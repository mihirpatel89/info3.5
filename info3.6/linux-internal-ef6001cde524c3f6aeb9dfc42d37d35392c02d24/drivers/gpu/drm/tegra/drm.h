/*
 * Copyright (C) 2012 Avionic Design GmbH
 * Copyright (C) 2012-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef HOST1X_DRM_H
#define HOST1X_DRM_H 1

#include <uapi/drm/tegra_drm.h>
#include <linux/host1x.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fixed.h>

#include "gem.h"

struct reset_control;

struct tegra_fb {
	struct drm_framebuffer base;
	struct tegra_bo **planes;
	unsigned int num_planes;
};

#ifdef CONFIG_DRM_TEGRA_FBDEV
struct tegra_fbdev {
	struct drm_fb_helper base;
	struct tegra_fb *fb;
};
#endif

struct tegra_drm {
	struct drm_device *drm;

	struct iommu_domain *domain;
	struct drm_mm mm;

	struct mutex clients_lock;
	struct list_head clients;

#ifdef CONFIG_DRM_TEGRA_FBDEV
	struct tegra_fbdev *fbdev;
#endif

	unsigned int pitch_align;
};

struct tegra_drm_client;

struct tegra_drm_context {
	struct tegra_drm_client *client;
	struct host1x_channel *channel;
	struct list_head list;

	struct host1x_bo *error_notifier_bo;
	u64 error_notifier_offset;
};

struct tegra_drm_client_ops {
	int (*open_channel)(struct tegra_drm_client *client,
			    struct tegra_drm_context *context);
	void (*close_channel)(struct tegra_drm_context *context);
	void (*reset)(struct device *dev);
	int (*is_addr_reg)(struct device *dev, u32 class, u32 offset, u32 val);
	int (*submit)(struct tegra_drm_context *context,
		      struct drm_tegra_submit *args, struct drm_device *drm,
		      struct drm_file *file);
	int (*load_regs)(struct tegra_drm_client *client);
	int (*finalize_poweron)(struct tegra_drm_client *client);
	int (*prepare_poweroff)(struct tegra_drm_client *client);
};

int tegra_drm_submit(struct tegra_drm_context *context,
		     struct drm_tegra_submit *args, struct drm_device *drm,
		     struct drm_file *file);

struct tegra_drm_client {
	struct host1x_client base;
	struct list_head list;

	const struct tegra_drm_client_ops *ops;
};

static inline struct tegra_drm_client *
host1x_to_drm_client(struct host1x_client *client)
{
	return container_of(client, struct tegra_drm_client, base);
}

int tegra_drm_register_client(struct tegra_drm *tegra,
			      struct tegra_drm_client *client);
int tegra_drm_unregister_client(struct tegra_drm *tegra,
				struct tegra_drm_client *client);

int tegra_drm_init(struct tegra_drm *tegra, struct drm_device *drm);
int tegra_drm_exit(struct tegra_drm *tegra);

struct tegra_dc_soc_info;
struct tegra_output;

struct tegra_dc {
	struct host1x_client client;
	struct device *dev;
	spinlock_t lock;

	struct drm_crtc base;
	int pipe;

	struct clk *clk;
	struct reset_control *rst;
	void __iomem *regs;
	int irq;

	struct tegra_output *rgb;

	struct list_head list;

	struct drm_info_list *debugfs_files;
	struct drm_minor *minor;
	struct dentry *debugfs;

	/* page-flip handling */
	struct drm_pending_vblank_event *event;

	const struct tegra_dc_soc_info *soc;

	struct iommu_domain *domain;
};

static inline struct tegra_dc *
host1x_client_to_dc(struct host1x_client *client)
{
	return container_of(client, struct tegra_dc, client);
}

static inline struct tegra_dc *to_tegra_dc(struct drm_crtc *crtc)
{
	return crtc ? container_of(crtc, struct tegra_dc, base) : NULL;
}

static inline void tegra_dc_writel(struct tegra_dc *dc, unsigned long value,
				   unsigned long reg)
{
	writel(value, dc->regs + (reg << 2));
}

static inline unsigned long tegra_dc_readl(struct tegra_dc *dc,
					   unsigned long reg)
{
	return readl(dc->regs + (reg << 2));
}

struct tegra_dc_window {
	struct {
		unsigned int x;
		unsigned int y;
		unsigned int w;
		unsigned int h;
	} src;
	struct {
		unsigned int x;
		unsigned int y;
		unsigned int w;
		unsigned int h;
	} dst;
	unsigned int bits_per_pixel;
	unsigned int format;
	unsigned int swap;
	unsigned int stride[2];
	unsigned long base[3];
	bool bottom_up;

	struct tegra_bo_tiling tiling;
};

/* from dc.c */
void tegra_dc_enable_vblank(struct tegra_dc *dc);
void tegra_dc_disable_vblank(struct tegra_dc *dc);
void tegra_dc_cancel_page_flip(struct drm_crtc *crtc, struct drm_file *file);

struct tegra_output_ops {
	int (*enable)(struct tegra_output *output);
	int (*disable)(struct tegra_output *output);
	int (*setup_clock)(struct tegra_output *output, struct clk *clk,
			   unsigned long pclk, unsigned int *div);
	int (*check_mode)(struct tegra_output *output,
			  struct drm_display_mode *mode,
			  enum drm_mode_status *status);
	enum drm_connector_status (*detect)(struct tegra_output *output);
};

enum tegra_output_type {
	TEGRA_OUTPUT_RGB,
	TEGRA_OUTPUT_HDMI,
	TEGRA_OUTPUT_DSI,
	TEGRA_OUTPUT_EDP,
};

struct tegra_output {
	struct device_node *of_node;
	struct device *dev;

	const struct tegra_output_ops *ops;
	enum tegra_output_type type;

	struct drm_panel *panel;
	struct i2c_adapter *ddc;
	const struct edid *edid;
	unsigned int hpd_irq;
	int hpd_gpio;

	struct drm_encoder encoder;
	struct drm_connector connector;
};

static inline struct tegra_output *encoder_to_output(struct drm_encoder *e)
{
	return container_of(e, struct tegra_output, encoder);
}

static inline struct tegra_output *connector_to_output(struct drm_connector *c)
{
	return container_of(c, struct tegra_output, connector);
}

static inline int tegra_output_enable(struct tegra_output *output)
{
	if (output && output->ops && output->ops->enable)
		return output->ops->enable(output);

	return output ? -ENOSYS : -EINVAL;
}

static inline int tegra_output_disable(struct tegra_output *output)
{
	if (output && output->ops && output->ops->disable)
		return output->ops->disable(output);

	return output ? -ENOSYS : -EINVAL;
}

static inline int tegra_output_setup_clock(struct tegra_output *output,
					   struct clk *clk, unsigned long pclk,
					   unsigned int *div)
{
	if (output && output->ops && output->ops->setup_clock)
		return output->ops->setup_clock(output, clk, pclk, div);

	return output ? -ENOSYS : -EINVAL;
}

static inline int tegra_output_check_mode(struct tegra_output *output,
					  struct drm_display_mode *mode,
					  enum drm_mode_status *status)
{
	if (output && output->ops && output->ops->check_mode)
		return output->ops->check_mode(output, mode, status);

	return output ? -ENOSYS : -EINVAL;
}

/* from rgb.c */
int tegra_dc_rgb_probe(struct tegra_dc *dc);
int tegra_dc_rgb_remove(struct tegra_dc *dc);
int tegra_dc_rgb_init(struct drm_device *drm, struct tegra_dc *dc);
int tegra_dc_rgb_exit(struct tegra_dc *dc);

/* from output.c */
int tegra_output_probe(struct tegra_output *output);
int tegra_output_remove(struct tegra_output *output);
int tegra_output_init(struct drm_device *drm, struct tegra_output *output);
int tegra_output_exit(struct tegra_output *output);

/* from dpaux.c */
struct tegra_dpaux;
struct drm_dp_link;

struct tegra_dpaux *tegra_dpaux_find_by_of_node(struct device_node *np);
enum drm_connector_status tegra_dpaux_detect(struct tegra_dpaux *dpaux);
int tegra_dpaux_attach(struct tegra_dpaux *dpaux, struct tegra_output *output);
int tegra_dpaux_detach(struct tegra_dpaux *dpaux);
int tegra_dpaux_enable(struct tegra_dpaux *dpaux);
int tegra_dpaux_disable(struct tegra_dpaux *dpaux);
int tegra_dpaux_prepare(struct tegra_dpaux *dpaux, u8 encoding);
int tegra_dpaux_train(struct tegra_dpaux *dpaux, struct drm_dp_link *link,
		      u8 pattern);

/* from fb.c */
struct tegra_bo *tegra_fb_get_plane(struct drm_framebuffer *framebuffer,
				    unsigned int index);
bool tegra_fb_is_bottom_up(struct drm_framebuffer *framebuffer);
int tegra_fb_get_tiling(struct drm_framebuffer *framebuffer,
			struct tegra_bo_tiling *tiling);
int tegra_drm_fb_prepare(struct drm_device *drm);
void tegra_drm_fb_free(struct drm_device *drm);
int tegra_drm_fb_init(struct drm_device *drm);
void tegra_drm_fb_exit(struct drm_device *drm);
#ifdef CONFIG_DRM_TEGRA_FBDEV
void tegra_fbdev_restore_mode(struct tegra_fbdev *fbdev);
#endif

extern struct platform_driver tegra_dc_driver;
extern struct platform_driver tegra_dsi_driver;
extern struct platform_driver tegra_sor_driver;
extern struct platform_driver tegra_hdmi_driver;
extern struct platform_driver tegra_dpaux_driver;
extern struct platform_driver tegra_gr2d_driver;
extern struct platform_driver tegra_gr3d_driver;
extern struct platform_driver tegra_vic_driver;
extern struct platform_driver tegra_nvdec_driver;
extern struct platform_driver tegra_nvjpg_driver;
extern struct platform_driver tegra_nvenc_driver;
extern struct platform_driver tegra_tsec_driver;
extern struct platform_driver tegra_isp_driver;
extern struct platform_driver tegra_vi_driver;

#endif /* HOST1X_DRM_H */
