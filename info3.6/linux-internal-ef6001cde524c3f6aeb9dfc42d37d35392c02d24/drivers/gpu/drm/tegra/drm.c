/*
 * Copyright (C) 2012 Avionic Design GmbH
 * Copyright (C) 2012-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/host1x.h>
#include <linux/iommu.h>
#include <drm/tegra_drm.h>

#include "drm.h"
#include "gem.h"
#include "../../../staging/android/sync.h"

#define DRIVER_NAME "tegra"
#define DRIVER_DESC "NVIDIA Tegra graphics"
#define DRIVER_DATE "20120330"
#define DRIVER_MAJOR 0
#define DRIVER_MINOR 0
#define DRIVER_PATCHLEVEL 0

struct tegra_drm_file {
	struct list_head contexts;
};

static int tegra_drm_load(struct drm_device *drm, unsigned long flags)
{
	struct host1x_device *device = to_host1x_device(drm->dev);
	struct tegra_drm *tegra;
	int err;

	tegra = kzalloc(sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	if (!IS_ENABLED(CONFIG_DRM_TEGRA_DOWNSTREAM) &&
	    iommu_present(&platform_bus_type)) {
		struct iommu_domain_geometry *geometry;
		u64 start, end;

		tegra->domain = iommu_domain_alloc(&platform_bus_type);
		if (!tegra->domain) {
			err = -ENOMEM;
			goto free;
		}

		geometry = &tegra->domain->geometry;
		start = geometry->aperture_start;
		end = geometry->aperture_end;

		DRM_DEBUG("IOMMU context initialized (aperture: %#llx-%#llx)\n",
			  start, end);
		drm_mm_init(&tegra->mm, start, end - start + 1);
	}

	mutex_init(&tegra->clients_lock);
	INIT_LIST_HEAD(&tegra->clients);
	drm->dev_private = tegra;
	tegra->drm = drm;

	drm_mode_config_init(drm);

	err = tegra_drm_fb_prepare(drm);
	if (err < 0)
		goto config;

	drm_kms_helper_poll_init(drm);

	err = host1x_device_init(device);
	if (err < 0)
		goto fbdev;

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	drm->irq_enabled = true;

	err = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (err < 0)
		goto device;

	err = tegra_drm_fb_init(drm);
	if (err < 0)
		goto vblank;

	return 0;

vblank:
	drm_vblank_cleanup(drm);
device:
	host1x_device_exit(device);
fbdev:
	drm_kms_helper_poll_fini(drm);
	tegra_drm_fb_free(drm);
config:
	drm_mode_config_cleanup(drm);

	if (tegra->domain) {
		iommu_domain_free(tegra->domain);
		drm_mm_takedown(&tegra->mm);
	}
free:
	kfree(tegra);
	return err;
}

static int tegra_drm_unload(struct drm_device *drm)
{
	struct host1x_device *device = to_host1x_device(drm->dev);
	struct tegra_drm *tegra = drm->dev_private;
	int err;

	drm_kms_helper_poll_fini(drm);
	tegra_drm_fb_exit(drm);
	drm_vblank_cleanup(drm);
	drm_mode_config_cleanup(drm);

	err = host1x_device_exit(device);
	if (err < 0)
		return err;

	if (tegra->domain) {
		iommu_domain_free(tegra->domain);
		drm_mm_takedown(&tegra->mm);
	}

	kfree(tegra);

	return 0;
}

static int tegra_drm_open(struct drm_device *drm, struct drm_file *filp)
{
	struct tegra_drm_file *fpriv;

	fpriv = kzalloc(sizeof(*fpriv), GFP_KERNEL);
	if (!fpriv)
		return -ENOMEM;

	INIT_LIST_HEAD(&fpriv->contexts);
	filp->driver_priv = fpriv;

	return 0;
}

static void tegra_drm_context_free(struct tegra_drm_context *context)
{
	context->client->ops->close_channel(context);
	kfree(context);
}

static void tegra_drm_lastclose(struct drm_device *drm)
{
#ifdef CONFIG_DRM_TEGRA_FBDEV
	struct tegra_drm *tegra = drm->dev_private;

	tegra_fbdev_restore_mode(tegra->fbdev);
#endif
}

static struct host1x_bo *
host1x_bo_lookup(struct drm_device *drm, struct drm_file *file, u32 handle)
{
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	gem = drm_gem_object_lookup(drm, file, handle);
	if (!gem)
		return NULL;

	mutex_lock(&drm->struct_mutex);
	drm_gem_object_unreference(gem);
	mutex_unlock(&drm->struct_mutex);

	bo = to_tegra_bo(gem);
	return &bo->base;
}

static int host1x_reloc_copy_from_user(struct host1x_reloc *dest,
				       struct drm_tegra_reloc __user *src,
				       struct drm_device *drm,
				       struct drm_file *file)
{
	u32 cmdbuf, target;
	int err;

	err = get_user(cmdbuf, &src->cmdbuf.handle);
	if (err < 0)
		return err;

	err = get_user(dest->cmdbuf.offset, &src->cmdbuf.offset);
	if (err < 0)
		return err;

	err = get_user(target, &src->target.handle);
	if (err < 0)
		return err;

	err = get_user(dest->target.offset, &src->target.offset);
	if (err < 0)
		return err;

	err = get_user(dest->shift, &src->shift);
	if (err < 0)
		return err;

	dest->cmdbuf.bo = host1x_bo_lookup(drm, file, cmdbuf);
	if (!dest->cmdbuf.bo)
		return -ENOENT;

	dest->target.bo = host1x_bo_lookup(drm, file, target);
	if (!dest->target.bo)
		return -ENOENT;

	return 0;
}

int tegra_drm_submit(struct tegra_drm_context *context,
		     struct drm_tegra_submit *args, struct drm_device *drm,
		     struct drm_file *file)
{
	struct host1x *host1x = dev_get_drvdata(drm->dev->parent);
	unsigned int num_cmdbufs = args->num_cmdbufs;
	unsigned int num_relocs = args->num_relocs;
	unsigned int num_waitchks = args->num_waitchks;
	struct drm_tegra_cmdbuf __user *cmdbufs =
		(void __user *)(uintptr_t)args->cmdbufs;
	struct drm_tegra_reloc __user *relocs =
		(void __user *)(uintptr_t)args->relocs;
	struct drm_tegra_waitchk __user *waitchks =
		(void __user *)(uintptr_t)args->waitchks;
	struct drm_tegra_syncpt __user *syncpts =
		(void __user *)(uintptr_t)args->syncpts;
	u32 __user *fences = (void __user *)(uintptr_t)args->fences;
	u32 __user *class_ids = (u32 __user *)(uintptr_t)args->class_ids;
	u32 *local_class_ids = NULL;
	struct host1x_job *job;
	unsigned int i;
	int err;

	job = host1x_job_alloc(context->channel, args->num_cmdbufs,
			       args->num_relocs, args->num_waitchks,
			       args->num_syncpts);
	if (!job)
		return -ENOMEM;

	job->num_syncpts = args->num_syncpts;
	job->num_relocs = args->num_relocs;
	job->num_waitchk = args->num_waitchks;
	job->client = (u32)args->context;
	job->serialize = true;

	if (context->error_notifier_bo) {
		job->error_notifier_bo = context->error_notifier_bo;
		job->error_notifier_offset = context->error_notifier_offset;
	}

	if (class_ids) {
		local_class_ids = kzalloc(sizeof(u32) * num_cmdbufs,
					GFP_KERNEL);
		if (!local_class_ids) {
			err = -ENOMEM;
			goto fail;
		}

		err = copy_from_user(local_class_ids, class_ids,
				sizeof(u32) * num_cmdbufs);
		if (err)
			goto fail;
	}

	for (i = 0; i < num_cmdbufs; i++) {
		struct sync_fence *pre_fence = NULL;
		struct drm_tegra_cmdbuf cmdbuf;
		struct host1x_bo *bo;
		u32 class_id;

		if (class_ids && local_class_ids[i])
			class_id = local_class_ids[i];
		else
			class_id = context->client->base.class;

		if (copy_from_user(&cmdbuf, cmdbufs, sizeof(cmdbuf))) {
			err = -EFAULT;
			goto fail;
		}

		bo = host1x_bo_lookup(drm, file, cmdbuf.handle);
		if (!bo) {
			err = -ENOENT;
			goto fail;
		}

		if (cmdbuf.pre_fence > 0)
			pre_fence = sync_fence_fdget(cmdbuf.pre_fence);

		host1x_job_add_gather(job, bo, cmdbuf.words, cmdbuf.offset,
				      pre_fence, class_id);
		cmdbufs++;
	}

	kfree(local_class_ids);
	local_class_ids = NULL;

	/* copy and resolve relocations from submit */
	while (num_relocs--) {
		err = host1x_reloc_copy_from_user(&job->relocarray[num_relocs],
						  &relocs[num_relocs], drm,
						  file);
		if (err < 0)
			goto fail;
	}

	if (copy_from_user(job->waitchk, waitchks,
			   sizeof(*waitchks) * num_waitchks)) {
		err = -EFAULT;
		goto fail;
	}

	for (i = 0; i < args->num_syncpts; i++) {
		struct drm_tegra_syncpt syncpt;

		if (copy_from_user(&syncpt, &syncpts[i],
				   sizeof(syncpt))) {
			err = -EFAULT;
			goto fail;
		}

		job->syncpts[i].incrs = syncpt.incrs;
		job->syncpts[i].id = syncpt.id;
	}

	job->is_addr_reg = context->client->ops->is_addr_reg;
	job->reset = context->client->ops->reset;
	job->timeout = 10000;

	if (args->timeout && args->timeout < 10000)
		job->timeout = args->timeout;

	err = host1x_job_pin(job, context->client->base.dev);
	if (err)
		goto fail;

	err = host1x_job_submit(job);
	if (err)
		goto fail_submit;

	if (args->flags & DRM_TEGRA_SUBMIT_FLAGS_SYNC_FD) {
		struct host1x_syncpt_fence *syncpt_fences;

		syncpt_fences = kmalloc_array(args->num_syncpts,
				       sizeof(*syncpt_fences),
				       GFP_KERNEL);
		if (!syncpt_fences) {
			err = -ENOMEM;
			goto fail;
		}

		for (i = 0; i < args->num_syncpts; i++) {
			syncpt_fences[i].id = job->syncpts[i].id;
			syncpt_fences[i].threshold = job->syncpts[i].end;
		}

		err = host1x_sync_create_fence_fd(host1x, syncpt_fences,
						      args->num_syncpts,
						      "fence_drm",
						      &args->fence);
		kfree(syncpt_fences);
		if (err)
			goto fail;

	} else {
		args->fence = job->syncpts[0].end;
	}

	/* Deliver multiple fences back to the userspace */
	if (fences) {
		for (i = 0; i < args->num_syncpts; ++i) {
			u32 fence = job->syncpts[i].end;

			err = copy_to_user(fences, &fence, sizeof(u32));
			if (err)
				break;
			fences++;
		}
	}

	host1x_job_put(job);
	return 0;

fail_submit:
	host1x_job_unpin(job);
fail:
	host1x_job_put(job);
	return err;
}


#ifdef CONFIG_DRM_TEGRA_STAGING
static struct tegra_drm_context *tegra_drm_get_context(__u64 context)
{
	return (struct tegra_drm_context *)(uintptr_t)context;
}

static bool tegra_drm_file_owns_context(struct tegra_drm_file *file,
					struct tegra_drm_context *context)
{
	struct tegra_drm_context *ctx;

	list_for_each_entry(ctx, &file->contexts, list)
		if (ctx == context)
			return true;

	return false;
}

static int tegra_gem_create(struct drm_device *drm, void *data,
			    struct drm_file *file)
{
	struct drm_tegra_gem_create *args = data;
	struct tegra_bo *bo;

	bo = tegra_bo_create_with_handle(file, drm, args->size, args->flags,
					 &args->handle);
	if (IS_ERR(bo))
		return PTR_ERR(bo);

	return 0;
}

static int tegra_gem_mmap(struct drm_device *drm, void *data,
			  struct drm_file *file)
{
	struct drm_tegra_gem_mmap *args = data;
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	gem = drm_gem_object_lookup(drm, file, args->handle);
	if (!gem)
		return -EINVAL;

	bo = to_tegra_bo(gem);

	args->offset = drm_vma_node_offset_addr(&bo->gem.vma_node);

	drm_gem_object_unreference(gem);

	return 0;
}

static int tegra_syncpt_read(struct drm_device *drm, void *data,
			     struct drm_file *file)
{
	struct host1x *host = dev_get_drvdata(drm->dev->parent);
	struct drm_tegra_syncpt_read *args = data;
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get(host, args->id);
	if (!sp)
		return -EINVAL;

	args->value = host1x_syncpt_read_min(sp);
	return 0;
}

static int tegra_syncpt_incr(struct drm_device *drm, void *data,
			     struct drm_file *file)
{
	struct host1x *host1x = dev_get_drvdata(drm->dev->parent);
	struct drm_tegra_syncpt_incr *args = data;
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get(host1x, args->id);
	if (!sp)
		return -EINVAL;

	return host1x_syncpt_incr(sp);
}

static int tegra_syncpt_wait(struct drm_device *drm, void *data,
			     struct drm_file *file)
{
	struct host1x *host1x = dev_get_drvdata(drm->dev->parent);
	struct drm_tegra_syncpt_wait *args = data;
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get(host1x, args->id);
	if (!sp)
		return -EINVAL;

	return host1x_syncpt_wait(sp, args->thresh, args->timeout,
				  &args->value);
}

static int tegra_fence_create(struct drm_device *drm, void *data,
			      struct drm_file *file)
{
	int err;
	struct host1x *host1x = dev_get_drvdata(drm->dev->parent);
	struct drm_tegra_fence_create *args = data;
	struct host1x_syncpt_fence *fences;
	struct drm_tegra_fence_info pt;
	char name[32];
	const char __user *args_name =
		(const char __user *)(uintptr_t)args->name;
	const void __user *args_pts =
		(const void __user *)(uintptr_t)args->pts;
	int i;

	if (args_name) {
		if (strncpy_from_user(name, args_name, sizeof(name)) < 0)
			return -EFAULT;
		name[sizeof(name) - 1] = '\0';
	} else {
		name[0] = '\0';
	}

	fences = kmalloc_array(args->num_pts, sizeof(*fences), GFP_KERNEL);
	if (!fences) {
		err = -ENOMEM;
		goto out;
	}

	for (i = 0; i < args->num_pts; i++) {
		err = copy_from_user(&pt, args_pts + i, sizeof(pt));
		if (err < 0)
			goto out;

		fences[i].id = pt.id;
		fences[i].threshold = pt.thresh;
	}

	err = host1x_sync_create_fence_fd(host1x, fences, args->num_pts,
					  name, &args->fence_fd);
out:
	kfree(fences);

	return err;
}

static int tegra_fence_set_name(struct drm_device *drm, void *data,
				struct drm_file *file)
{
	struct drm_tegra_fence_set_name *args = data;
	int err;
	char name[32];
	const char __user *args_name =
		(const char __user *)(uintptr_t)args->name;

	if (args_name) {
		if (strncpy_from_user(name, args_name, sizeof(name)) < 0)
			return -EFAULT;
		name[sizeof(name) - 1] = '\0';
	} else {
		name[0] = '\0';
	}

	err = host1x_sync_fence_set_name(args->fence_fd, name);

	return err;
}

static int tegra_set_error_notifier(struct drm_device *drm, void *data,
				    struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_set_error_notifier *args = data;
	struct tegra_drm_context *context;
	struct host1x_bo *bo;
	void *notifier;

	context = tegra_drm_get_context(args->context);
	if (!tegra_drm_file_owns_context(fpriv, context))
		return -EINVAL;

	if (!args->handle) {
		if (context->error_notifier_bo)
			host1x_bo_put(context->error_notifier_bo);
		context->error_notifier_bo = NULL;
		return 0;
	}

	if (context->error_notifier_bo) {
		host1x_bo_put(context->error_notifier_bo);
		context->error_notifier_bo = NULL;
	}

	bo = host1x_bo_lookup(drm, file, args->handle);
	if (!bo)
		return -ENOENT;

	notifier = host1x_bo_mmap(bo);
	if (!notifier)
		return -EINVAL;

	host1x_bo_get(bo);

	memset(notifier + args->offset, 0,
			sizeof(struct drm_tegra_notification));
	host1x_bo_munmap(bo, notifier);

	context->error_notifier_bo = bo;
	context->error_notifier_offset = args->offset;

	return 0;
}

static int tegra_get_characteristics(struct drm_device *drm, void *data,
				struct drm_file *file)
{
	struct drm_tegra_get_characteristics *args = data;
	struct host1x *host1x = dev_get_drvdata(drm->dev->parent);
	struct host1x_characteristics *host1x_char;
	struct drm_tegra_characteristics drm_tegra_char;
	int err = 0;

	/* get characterisitcs from host1x and update to drm fields */
	host1x_char = host1x_get_chara(host1x);
	drm_tegra_char.flags = host1x_char->flags;
	drm_tegra_char.num_syncpts = host1x_char->num_syncpts;

	if (host1x_channel_gather_filter_enabled(host1x))
		drm_tegra_char.flags |= DRM_TEGRA_CHARA_GFILTER;


	if (args->drm_tegra_chara_buf_size > 0) {
		size_t write_size = sizeof(drm_tegra_char);

		if (write_size > args->drm_tegra_chara_buf_size)
			write_size = args->drm_tegra_chara_buf_size;

		err = copy_to_user((void __user *)(uintptr_t)
				   args->drm_tegra_chara_buf_addr,
				   &drm_tegra_char, write_size);
	}

	if (err == 0)
		args->drm_tegra_chara_buf_size =
			sizeof(drm_tegra_char);

	return err;
}

static int tegra_open_channel(struct drm_device *drm, void *data,
			      struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct tegra_drm *tegra = drm->dev_private;
	struct drm_tegra_open_channel *args = data;
	struct tegra_drm_context *context;
	struct tegra_drm_client *client;
	int err = -ENODEV;

	context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;

	list_for_each_entry(client, &tegra->clients, list)
		if (client->base.class == args->client) {
			err = client->ops->open_channel(client, context);
			if (err)
				break;

			list_add(&context->list, &fpriv->contexts);
			args->context = (uintptr_t)context;
			context->client = client;
			return 0;
		}

	kfree(context);
	return err;
}

static int tegra_close_channel(struct drm_device *drm, void *data,
			       struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_close_channel *args = data;
	struct tegra_drm_context *context;

	context = tegra_drm_get_context(args->context);

	if (!tegra_drm_file_owns_context(fpriv, context))
		return -EINVAL;

	if (context->error_notifier_bo) {
		host1x_bo_put(context->error_notifier_bo);
		context->error_notifier_bo = NULL;
	}

	list_del(&context->list);
	tegra_drm_context_free(context);

	return 0;
}

static int tegra_get_syncpt(struct drm_device *drm, void *data,
			    struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_get_syncpt *args = data;
	struct tegra_drm_context *context;
	struct host1x_syncpt *syncpt;

	context = tegra_drm_get_context(args->context);

	if (!tegra_drm_file_owns_context(fpriv, context))
		return -ENODEV;

	if (args->index >= context->client->base.num_syncpts)
		return -EINVAL;

	syncpt = context->client->base.syncpts[args->index];
	args->id = host1x_syncpt_id(syncpt);

	return 0;
}

static int tegra_submit(struct drm_device *drm, void *data,
			struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_submit *args = data;
	struct tegra_drm_context *context;

	context = tegra_drm_get_context(args->context);

	if (!tegra_drm_file_owns_context(fpriv, context))
		return -ENODEV;

	return context->client->ops->submit(context, args, drm, file);
}

static int tegra_get_syncpt_base(struct drm_device *drm, void *data,
				 struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_get_syncpt_base *args = data;
	struct tegra_drm_context *context;
	struct host1x_syncpt_base *base;
	struct host1x_syncpt *syncpt;

	context = tegra_drm_get_context(args->context);

	if (!tegra_drm_file_owns_context(fpriv, context))
		return -ENODEV;

	if (args->syncpt >= context->client->base.num_syncpts)
		return -EINVAL;

	syncpt = context->client->base.syncpts[args->syncpt];

	base = host1x_syncpt_get_base(syncpt);
	if (!base)
		return -ENXIO;

	args->id = host1x_syncpt_base_id(base);

	return 0;
}

static int tegra_gem_set_tiling(struct drm_device *drm, void *data,
				struct drm_file *file)
{
	struct drm_tegra_gem_set_tiling *args = data;
	enum tegra_bo_tiling_mode mode;
	struct drm_gem_object *gem;
	unsigned long value = 0;
	struct tegra_bo *bo;

	switch (args->mode) {
	case DRM_TEGRA_GEM_TILING_MODE_PITCH:
		mode = TEGRA_BO_TILING_MODE_PITCH;

		if (args->value != 0)
			return -EINVAL;

		break;

	case DRM_TEGRA_GEM_TILING_MODE_TILED:
		mode = TEGRA_BO_TILING_MODE_TILED;

		if (args->value != 0)
			return -EINVAL;

		break;

	case DRM_TEGRA_GEM_TILING_MODE_BLOCK:
		mode = TEGRA_BO_TILING_MODE_BLOCK;

		if (args->value > 5)
			return -EINVAL;

		value = args->value;
		break;

	default:
		return -EINVAL;
	}

	gem = drm_gem_object_lookup(drm, file, args->handle);
	if (!gem)
		return -ENOENT;

	bo = to_tegra_bo(gem);

	bo->tiling.mode = mode;
	bo->tiling.value = value;

	drm_gem_object_unreference(gem);

	return 0;
}

static int tegra_gem_get_tiling(struct drm_device *drm, void *data,
				struct drm_file *file)
{
	struct drm_tegra_gem_get_tiling *args = data;
	struct drm_gem_object *gem;
	struct tegra_bo *bo;
	int err = 0;

	gem = drm_gem_object_lookup(drm, file, args->handle);
	if (!gem)
		return -ENOENT;

	bo = to_tegra_bo(gem);

	switch (bo->tiling.mode) {
	case TEGRA_BO_TILING_MODE_PITCH:
		args->mode = DRM_TEGRA_GEM_TILING_MODE_PITCH;
		args->value = 0;
		break;

	case TEGRA_BO_TILING_MODE_TILED:
		args->mode = DRM_TEGRA_GEM_TILING_MODE_TILED;
		args->value = 0;
		break;

	case TEGRA_BO_TILING_MODE_BLOCK:
		args->mode = DRM_TEGRA_GEM_TILING_MODE_BLOCK;
		args->value = bo->tiling.value;
		break;

	default:
		err = -EINVAL;
		break;
	}

	drm_gem_object_unreference(gem);

	return err;
}

static int tegra_gem_set_flags(struct drm_device *drm, void *data,
			       struct drm_file *file)
{
	struct drm_tegra_gem_set_flags *args = data;
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	if (args->flags & ~DRM_TEGRA_GEM_FLAGS)
		return -EINVAL;

	gem = drm_gem_object_lookup(drm, file, args->handle);
	if (!gem)
		return -ENOENT;

	bo = to_tegra_bo(gem);
	bo->flags = 0;

	if (args->flags & DRM_TEGRA_GEM_BOTTOM_UP)
		bo->flags |= TEGRA_BO_BOTTOM_UP;

	drm_gem_object_unreference(gem);

	return 0;
}

static int tegra_gem_get_flags(struct drm_device *drm, void *data,
			       struct drm_file *file)
{
	struct drm_tegra_gem_get_flags *args = data;
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	gem = drm_gem_object_lookup(drm, file, args->handle);
	if (!gem)
		return -ENOENT;

	bo = to_tegra_bo(gem);
	args->flags = 0;

	if (bo->flags & TEGRA_BO_BOTTOM_UP)
		args->flags |= DRM_TEGRA_GEM_BOTTOM_UP;

	drm_gem_object_unreference(gem);

	return 0;
}

static int tegra_get_clk_rate(struct drm_device *drm, void *data,
					struct drm_file *file)
{
	struct tegra_drm *tegra = drm->dev_private;
	struct tegra_drm_client *client;
	struct host1x_client *base;
	struct drm_tegra_get_clk_rate *args = data;

	list_for_each_entry(client, &tegra->clients, list) {
		base = &client->base;
		if (base->class == args->id) {
			if (!base->ops || !base->ops->get_clk_rate) {
				DRM_INFO("%s:%s No ops to get clk\n",
					 __func__, dev_name(base->dev));
				return -EINVAL;
			}
			return base->ops->get_clk_rate(base, &args->data,
					args->type);
		}
	}

	return -EINVAL;
}

static int tegra_set_clk_rate(struct drm_device *drm, void *data,
					struct drm_file *file)
{
	struct tegra_drm *tegra = drm->dev_private;
	struct tegra_drm_client *client;
	struct host1x_client *base;
	struct drm_tegra_set_clk_rate *args = data;

	list_for_each_entry(client, &tegra->clients, list) {
		base = &client->base;
		if (base->class == args->id) {
			if (!base->ops || !base->ops->set_clk_rate) {
				DRM_INFO("%s:%s No ops to set clk\n",
					 __func__, dev_name(base->dev));
				return -EINVAL;
			}
			return base->ops->set_clk_rate(base, args->data,
					args->type);
		}
	}
	return -EINVAL;
}
#endif

static const struct drm_ioctl_desc tegra_drm_ioctls[] = {
#ifdef CONFIG_DRM_TEGRA_STAGING
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_CREATE, tegra_gem_create, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_MMAP, tegra_gem_mmap, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_SYNCPT_READ, tegra_syncpt_read, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_SYNCPT_INCR, tegra_syncpt_incr, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_SYNCPT_WAIT, tegra_syncpt_wait, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_OPEN_CHANNEL, tegra_open_channel, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_CLOSE_CHANNEL, tegra_close_channel, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_GET_SYNCPT, tegra_get_syncpt, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_SUBMIT, tegra_submit, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_GET_SYNCPT_BASE, tegra_get_syncpt_base, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_SET_TILING, tegra_gem_set_tiling, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_GET_TILING, tegra_gem_get_tiling, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_SET_FLAGS, tegra_gem_set_flags, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_GET_FLAGS, tegra_gem_get_flags, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(TEGRA_FENCE_CREATE, tegra_fence_create, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_FENCE_SET_NAME, tegra_fence_set_name, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_SET_ERROR_NOTIFIER, tegra_set_error_notifier, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_GET_CHARACTERISTICS, tegra_get_characteristics, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_GET_CLK_RATE, tegra_get_clk_rate, DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_SET_CLK_RATE, tegra_set_clk_rate, DRM_UNLOCKED|DRM_RENDER_ALLOW),
#endif
};

static const struct file_operations tegra_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = tegra_drm_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
	.llseek = noop_llseek,
};

static struct drm_crtc *tegra_crtc_from_pipe(struct drm_device *drm, int pipe)
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &drm->mode_config.crtc_list, head) {
		struct tegra_dc *dc = to_tegra_dc(crtc);

		if (dc->pipe == pipe)
			return crtc;
	}

	return NULL;
}

static u32 tegra_drm_get_vblank_counter(struct drm_device *dev, int crtc)
{
	/* TODO: implement real hardware counter using syncpoints */
	return drm_vblank_count(dev, crtc);
}

static int tegra_drm_enable_vblank(struct drm_device *drm, int pipe)
{
	struct drm_crtc *crtc = tegra_crtc_from_pipe(drm, pipe);
	struct tegra_dc *dc = to_tegra_dc(crtc);

	if (!crtc)
		return -ENODEV;

	tegra_dc_enable_vblank(dc);

	return 0;
}

static void tegra_drm_disable_vblank(struct drm_device *drm, int pipe)
{
	struct drm_crtc *crtc = tegra_crtc_from_pipe(drm, pipe);
	struct tegra_dc *dc = to_tegra_dc(crtc);

	if (crtc)
		tegra_dc_disable_vblank(dc);
}

static void tegra_drm_preclose(struct drm_device *drm, struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct tegra_drm_context *context, *tmp;
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &drm->mode_config.crtc_list, head)
		tegra_dc_cancel_page_flip(crtc, file);

	list_for_each_entry_safe(context, tmp, &fpriv->contexts, list)
		tegra_drm_context_free(context);

	kfree(fpriv);
}

#ifdef CONFIG_DEBUG_FS
static int tegra_debugfs_framebuffers(struct seq_file *s, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)s->private;
	struct drm_device *drm = node->minor->dev;
	struct drm_framebuffer *fb;

	mutex_lock(&drm->mode_config.fb_lock);

	list_for_each_entry(fb, &drm->mode_config.fb_list, head) {
		seq_printf(s, "%3d: user size: %d x %d, depth %d, %d bpp, refcount %d\n",
			   fb->base.id, fb->width, fb->height, fb->depth,
			   fb->bits_per_pixel,
			   atomic_read(&fb->refcount.refcount));
	}

	mutex_unlock(&drm->mode_config.fb_lock);

	return 0;
}

static struct drm_info_list tegra_debugfs_list[] = {
	{ "framebuffers", tegra_debugfs_framebuffers, 0 },
};

static int tegra_debugfs_init(struct drm_minor *minor)
{
	return drm_debugfs_create_files(tegra_debugfs_list,
					ARRAY_SIZE(tegra_debugfs_list),
					minor->debugfs_root, minor);
}

static void tegra_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(tegra_debugfs_list,
				 ARRAY_SIZE(tegra_debugfs_list), minor);
}
#endif

static struct drm_driver tegra_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME |
			   DRIVER_RENDER,
	.load = tegra_drm_load,
	.unload = tegra_drm_unload,
	.open = tegra_drm_open,
	.preclose = tegra_drm_preclose,
	.lastclose = tegra_drm_lastclose,

	.get_vblank_counter = tegra_drm_get_vblank_counter,
	.enable_vblank = tegra_drm_enable_vblank,
	.disable_vblank = tegra_drm_disable_vblank,

#if defined(CONFIG_DEBUG_FS)
	.debugfs_init = tegra_debugfs_init,
	.debugfs_cleanup = tegra_debugfs_cleanup,
#endif

	.gem_free_object = tegra_bo_free_object,
	.gem_vm_ops = &tegra_bo_vm_ops,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = tegra_gem_prime_export,
	.gem_prime_import = tegra_gem_prime_import,

	.dumb_create = tegra_bo_dumb_create,
	.dumb_map_offset = tegra_bo_dumb_map_offset,
	.dumb_destroy = drm_gem_dumb_destroy,

	.ioctls = tegra_drm_ioctls,
	.num_ioctls = ARRAY_SIZE(tegra_drm_ioctls),
	.fops = &tegra_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

int tegra_drm_register_client(struct tegra_drm *tegra,
			      struct tegra_drm_client *client)
{
	mutex_lock(&tegra->clients_lock);
	list_add_tail(&client->list, &tegra->clients);
	mutex_unlock(&tegra->clients_lock);

	return 0;
}

int tegra_drm_unregister_client(struct tegra_drm *tegra,
				struct tegra_drm_client *client)
{
	mutex_lock(&tegra->clients_lock);
	list_del_init(&client->list);
	mutex_unlock(&tegra->clients_lock);

	return 0;
}

static int host1x_drm_probe(struct host1x_device *dev)
{
	struct drm_driver *driver = &tegra_drm_driver;
	struct drm_device *drm;
	int err;

	drm = drm_dev_alloc(driver, &dev->dev);
	if (!drm)
		return -ENOMEM;

	drm_dev_set_unique(drm, dev_name(&dev->dev));
	dev_set_drvdata(&dev->dev, drm);

	err = drm_dev_register(drm, 0);
	if (err < 0)
		goto unref;

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n", driver->name,
		 driver->major, driver->minor, driver->patchlevel,
		 driver->date, drm->primary->index);

	return 0;

unref:
	drm_dev_unref(drm);
	return err;
}

static int host1x_drm_remove(struct host1x_device *dev)
{
	struct drm_device *drm = dev_get_drvdata(&dev->dev);

	drm_dev_unregister(drm);
	drm_dev_unref(drm);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int host1x_drm_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_kms_helper_poll_disable(drm);

	return 0;
}

static int host1x_drm_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_kms_helper_poll_enable(drm);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(host1x_drm_pm_ops, host1x_drm_suspend,
			 host1x_drm_resume);

static const struct of_device_id host1x_drm_subdevs[] = {
#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	{ .compatible = "nvidia,tegra20-dc", },
	{ .compatible = "nvidia,tegra20-hdmi", },
	{ .compatible = "nvidia,tegra20-gr2d", },
	{ .compatible = "nvidia,tegra20-gr3d", },
	{ .compatible = "nvidia,tegra30-dc", },
	{ .compatible = "nvidia,tegra30-hdmi", },
	{ .compatible = "nvidia,tegra30-gr2d", },
	{ .compatible = "nvidia,tegra30-gr3d", },
	{ .compatible = "nvidia,tegra114-dsi", },
	{ .compatible = "nvidia,tegra114-hdmi", },
	{ .compatible = "nvidia,tegra114-gr3d", },
	{ .compatible = "nvidia,tegra124-dc", },
	{ .compatible = "nvidia,tegra124-sor", },
	{ .compatible = "nvidia,tegra124-hdmi", },
#endif
	{ .compatible = "nvidia,tegra124-vic", },
	{ .compatible = "nvidia,tegra210-vic", },
	{ .compatible = "nvidia,tegra210-nvdec", },
	{ .compatible = "nvidia,tegra210-nvjpg", },
	{ .compatible = "nvidia,tegra210-nvenc", },
	{ .compatible = "nvidia,tegra210-tsec", },
	{ .compatible = "nvidia,tegra210-isp", },
	{ .compatible = "nvidia,tegra210-vi", },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-vic", },
	{ .compatible = "nvidia,tegra186-nvenc", },
	{ .compatible = "nvidia,tegra186-nvdec", },
	{ .compatible = "nvidia,tegra186-nvjpg", },
	{ .compatible = "nvidia,tegra186-tsec", },
#endif
	{ /* sentinel */ }
};

static struct host1x_driver host1x_drm_driver = {
	.driver = {
		.name = "drm",
		.pm = &host1x_drm_pm_ops,
	},
	.probe = host1x_drm_probe,
	.remove = host1x_drm_remove,
	.subdevs = host1x_drm_subdevs,
};

static int __init host1x_drm_init(void)
{
	int err;

	err = host1x_driver_register(&host1x_drm_driver);
	if (err < 0)
		return err;

#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	err = platform_driver_register(&tegra_dc_driver);
	if (err < 0)
		goto unregister_host1x;

	err = platform_driver_register(&tegra_dsi_driver);
	if (err < 0)
		goto unregister_dc;

	err = platform_driver_register(&tegra_sor_driver);
	if (err < 0)
		goto unregister_dsi;

	err = platform_driver_register(&tegra_hdmi_driver);
	if (err < 0)
		goto unregister_sor;

	err = platform_driver_register(&tegra_dpaux_driver);
	if (err < 0)
		goto unregister_hdmi;

	err = platform_driver_register(&tegra_gr2d_driver);
	if (err < 0)
		goto unregister_dpaux;

	err = platform_driver_register(&tegra_gr3d_driver);
	if (err < 0)
		goto unregister_gr2d;
#endif

	err = platform_driver_register(&tegra_vic_driver);
	if (err < 0)
		goto unregister_gr3d;

	err = platform_driver_register(&tegra_nvdec_driver);
	if (err < 0)
		goto unregister_vic;

	err = platform_driver_register(&tegra_nvjpg_driver);
	if (err < 0)
		goto unregister_nvdec;

	err = platform_driver_register(&tegra_nvenc_driver);
	if (err < 0)
		goto unregister_nvjpg;

	err = platform_driver_register(&tegra_tsec_driver);
	if (err < 0)
		goto unregister_nvenc;

	err = platform_driver_register(&tegra_isp_driver);
	if (err < 0)
		goto unregister_tsec;

	err = platform_driver_register(&tegra_vi_driver);
	if (err < 0)
		goto unregister_isp;

	return 0;

unregister_isp:
	platform_driver_unregister(&tegra_isp_driver);
unregister_tsec:
	platform_driver_unregister(&tegra_tsec_driver);
unregister_nvenc:
	platform_driver_unregister(&tegra_nvenc_driver);
unregister_nvjpg:
	platform_driver_unregister(&tegra_nvjpg_driver);
unregister_nvdec:
	platform_driver_unregister(&tegra_nvdec_driver);
unregister_vic:
	platform_driver_unregister(&tegra_vic_driver);
unregister_gr3d:
#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	platform_driver_unregister(&tegra_gr3d_driver);
unregister_gr2d:
	platform_driver_unregister(&tegra_gr2d_driver);
unregister_dpaux:
	platform_driver_unregister(&tegra_dpaux_driver);
unregister_hdmi:
	platform_driver_unregister(&tegra_hdmi_driver);
unregister_sor:
	platform_driver_unregister(&tegra_sor_driver);
unregister_dsi:
	platform_driver_unregister(&tegra_dsi_driver);
unregister_dc:
	platform_driver_unregister(&tegra_dc_driver);
unregister_host1x:
#endif
	host1x_driver_unregister(&host1x_drm_driver);
	return err;
}
module_init(host1x_drm_init);

static void __exit host1x_drm_exit(void)
{
	platform_driver_unregister(&tegra_vi_driver);
	platform_driver_unregister(&tegra_isp_driver);
	platform_driver_unregister(&tegra_tsec_driver);
	platform_driver_unregister(&tegra_nvenc_driver);
	platform_driver_unregister(&tegra_nvjpg_driver);
	platform_driver_unregister(&tegra_nvdec_driver);
	platform_driver_unregister(&tegra_vic_driver);
#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	platform_driver_unregister(&tegra_gr3d_driver);
	platform_driver_unregister(&tegra_gr2d_driver);
	platform_driver_unregister(&tegra_dpaux_driver);
	platform_driver_unregister(&tegra_hdmi_driver);
	platform_driver_unregister(&tegra_sor_driver);
	platform_driver_unregister(&tegra_dsi_driver);
	platform_driver_unregister(&tegra_dc_driver);
#endif
	host1x_driver_unregister(&host1x_drm_driver);
}
module_exit(host1x_drm_exit);

MODULE_AUTHOR("Thierry Reding <thierry.reding@avionic-design.de>");
MODULE_DESCRIPTION("NVIDIA Tegra DRM driver");
MODULE_LICENSE("GPL v2");
