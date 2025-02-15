/*
 * Copyright (C) 2012-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HOST1X_DEV_H
#define HOST1X_DEV_H

#include <linux/platform_device.h>
#include <linux/device.h>

#include "channel.h"
#include "syncpt.h"
#include "intr.h"
#include "cdma.h"
#include "job.h"

struct host1x_syncpt;
struct host1x_syncpt_base;
struct host1x_channel;
struct host1x_cdma;
struct host1x_job;
struct push_buffer;
struct output;
struct dentry;

struct host1x_dev_ops {
	int (*load_regs)(struct host1x *host);
};

struct host1x_channel_ops {
	int (*init)(struct host1x_channel *channel, struct host1x *host,
		    unsigned int id);
	int (*submit)(struct host1x_job *job);
	void (*push_wait)(struct host1x_channel *ch, u32 id, u32 thresh);
	void (*enable_gather_filter)(struct host1x_channel *ch);
};

struct host1x_cdma_ops {
	void (*start)(struct host1x_cdma *cdma);
	void (*stop)(struct host1x_cdma *cdma);
	void (*flush)(struct  host1x_cdma *cdma);
	int (*timeout_init)(struct host1x_cdma *cdma);
	void (*timeout_destroy)(struct host1x_cdma *cdma);
	void (*freeze)(struct host1x_cdma *cdma);
	void (*resume)(struct host1x_cdma *cdma, u32 getptr);
	void (*timeout_handle)(struct host1x_cdma *cdma, u32 getptr,
			       u32 nr_slots);
};

struct host1x_pushbuffer_ops {
	void (*init)(struct push_buffer *pb);
};

struct host1x_debug_ops {
	void (*debug_init)(struct dentry *de);
	void (*show_channel_cdma)(struct host1x *host,
				  struct host1x_channel *ch,
				  struct output *o);
	void (*show_channel_fifo)(struct host1x *host,
				  struct host1x_channel *ch,
				  struct output *o);
	void (*show_mlocks)(struct host1x *host, struct output *output);

};

struct host1x_syncpt_ops {
	void (*restore)(struct host1x_syncpt *syncpt);
	void (*restore_wait_base)(struct host1x_syncpt *syncpt);
	void (*load_wait_base)(struct host1x_syncpt *syncpt);
	u32 (*load)(struct host1x_syncpt *syncpt);
	int (*cpu_incr)(struct host1x_syncpt *syncpt);
	int (*patch_wait)(struct host1x_syncpt *syncpt, void *patch_addr);
	void (*get_mutex_owner)(struct host1x_syncpt *syncpt,
				unsigned int mutex_id, bool *cpu, bool *ch,
				unsigned int *chid);
};

struct host1x_intr_ops {
	int (*init_host_sync)(struct host1x *host, u32 cpm,
		void (*syncpt_thresh_work)(struct work_struct *work));
	void (*set_syncpt_threshold)(
		struct host1x *host, u32 id, u32 thresh);
	void (*enable_syncpt_intr)(struct host1x *host, u32 id);
	void (*disable_syncpt_intr)(struct host1x *host, u32 id);
	void (*disable_all_syncpt_intrs)(struct host1x *host);
	int (*free_syncpt_irq)(struct host1x *host);
	int (*request_host_general_irq)(struct host1x *host);
	void (*free_host_general_irq)(struct host1x *host);
};

struct host1x_info {
	int	nb_channels;		/* host1x: num channels supported */
	int	nb_pts;			/* host1x: num syncpoints supported */
	int	nb_bases;		/* host1x: num syncpoints supported */
	int	nb_mlocks;		/* host1x: number of mlocks */
	int	(*init)(struct host1x *); /* initialize per SoC ops */
	int	sync_offset;
	bool	gather_filter_enabled;
};

struct host1x {
	const struct host1x_info *info;

	void __iomem *regs;
	struct host1x_syncpt *syncpt;
	struct host1x_syncpt_base *bases;
	struct device *dev;
	struct clk *clk;
	struct reset_control *reset_control;

	struct mutex intr_mutex;
	struct workqueue_struct *intr_wq;
	int intr_syncpt_irq;

	/* For general interrupts */
	int intr_general_irq;

	const struct host1x_dev_ops *dev_op;
	const struct host1x_syncpt_ops *syncpt_op;
	const struct host1x_intr_ops *intr_op;
	const struct host1x_channel_ops *channel_op;
	const struct host1x_cdma_ops *cdma_op;
	const struct host1x_pushbuffer_ops *cdma_pb_op;
	const struct host1x_debug_ops *debug_op;

	struct host1x_syncpt *nop_sp;

	struct mutex syncpt_mutex;
	struct mutex chlist_mutex;
	struct host1x_channel chlist;
	unsigned long allocated_channels;
	unsigned int num_allocated_channels;

	struct dentry *debugfs;

	struct mutex devices_lock;
	struct list_head devices;

	struct list_head list;
	struct device_dma_parameters dma_parms;

	struct host1x_characteristics host1x_chara;
};

void host1x_sync_writel(struct host1x *host1x, u32 v, u32 r);
u32 host1x_sync_readl(struct host1x *host1x, u32 r);
void host1x_ch_writel(struct host1x_channel *ch, u32 v, u32 r);
u32 host1x_ch_readl(struct host1x_channel *ch, u32 r);

static inline void host1x_hw_load_regs(struct host1x *host)
{
	if (host->dev_op && host->dev_op->load_regs)
		host->dev_op->load_regs(host);
}

static inline void host1x_hw_syncpt_restore(struct host1x *host,
					    struct host1x_syncpt *sp)
{
	host->syncpt_op->restore(sp);
}

static inline void host1x_hw_syncpt_restore_wait_base(struct host1x *host,
						      struct host1x_syncpt *sp)
{
	host->syncpt_op->restore_wait_base(sp);
}

static inline void host1x_hw_syncpt_load_wait_base(struct host1x *host,
						   struct host1x_syncpt *sp)
{
	host->syncpt_op->load_wait_base(sp);
}

static inline u32 host1x_hw_syncpt_load(struct host1x *host,
					struct host1x_syncpt *sp)
{
	return host->syncpt_op->load(sp);
}

static inline int host1x_hw_syncpt_cpu_incr(struct host1x *host,
					    struct host1x_syncpt *sp)
{
	return host->syncpt_op->cpu_incr(sp);
}

static inline int host1x_hw_syncpt_patch_wait(struct host1x *host,
					      struct host1x_syncpt *sp,
					      void *patch_addr)
{
	return host->syncpt_op->patch_wait(sp, patch_addr);
}

static inline int host1x_hw_intr_init_host_sync(struct host1x *host, u32 cpm,
			void (*syncpt_thresh_work)(struct work_struct *))
{
	return host->intr_op->init_host_sync(host, cpm, syncpt_thresh_work);
}

static inline void host1x_hw_intr_set_syncpt_threshold(struct host1x *host,
						       u32 id, u32 thresh)
{
	host->intr_op->set_syncpt_threshold(host, id, thresh);
}

static inline void host1x_hw_intr_enable_syncpt_intr(struct host1x *host,
						     u32 id)
{
	host->intr_op->enable_syncpt_intr(host, id);
}

static inline void host1x_hw_intr_disable_syncpt_intr(struct host1x *host,
						      u32 id)
{
	host->intr_op->disable_syncpt_intr(host, id);
}

static inline void host1x_hw_intr_disable_all_syncpt_intrs(struct host1x *host)
{
	host->intr_op->disable_all_syncpt_intrs(host);
}

static inline int host1x_hw_intr_free_syncpt_irq(struct host1x *host)
{
	return host->intr_op->free_syncpt_irq(host);
}

static inline int host1x_hw_intr_request_host_general_irq(struct host1x *host)
{
	return host->intr_op->request_host_general_irq(host);
}

static inline void host1x_hw_intr_free_host_general_irq(struct host1x *host)
{
	host->intr_op->free_host_general_irq(host);
}

static inline int host1x_hw_channel_init(struct host1x *host,
					 struct host1x_channel *channel,
					 int chid)
{
	return host->channel_op->init(channel, host, chid);
}

static inline int host1x_hw_channel_submit(struct host1x *host,
					   struct host1x_job *job)
{
	return host->channel_op->submit(job);
}

static inline void host1x_hw_channel_push_wait(struct host1x *host,
					       struct host1x_channel *channel,
					       u32 id, u32 thresh)
{
	host->channel_op->push_wait(channel, id, thresh);
}

static inline void host1x_hw_channel_enable_gather_filter(struct host1x *host,
						   struct host1x_channel *channel)
{
	return host->channel_op->enable_gather_filter(channel);
}

static inline void host1x_hw_cdma_start(struct host1x *host,
					struct host1x_cdma *cdma)
{
	host->cdma_op->start(cdma);
}

static inline void host1x_hw_cdma_stop(struct host1x *host,
				       struct host1x_cdma *cdma)
{
	host->cdma_op->stop(cdma);
}

static inline void host1x_hw_cdma_flush(struct host1x *host,
					struct host1x_cdma *cdma)
{
	host->cdma_op->flush(cdma);
}

static inline int host1x_hw_cdma_timeout_init(struct host1x *host,
					      struct host1x_cdma *cdma)
{
	return host->cdma_op->timeout_init(cdma);
}

static inline void host1x_hw_cdma_timeout_destroy(struct host1x *host,
						  struct host1x_cdma *cdma)
{
	host->cdma_op->timeout_destroy(cdma);
}

static inline void host1x_hw_cdma_freeze(struct host1x *host,
					 struct host1x_cdma *cdma)
{
	host->cdma_op->freeze(cdma);
}

static inline void host1x_hw_cdma_resume(struct host1x *host,
					 struct host1x_cdma *cdma, u32 getptr)
{
	host->cdma_op->resume(cdma, getptr);
}

static inline void host1x_hw_cdma_timeout_handle(struct host1x *host,
						   struct host1x_cdma *cdma,
						   u32 getptr, u32 nr_slots)
{
	host->cdma_op->timeout_handle(cdma, getptr, nr_slots);
}

static inline void host1x_hw_pushbuffer_init(struct host1x *host,
					     struct push_buffer *pb)
{
	host->cdma_pb_op->init(pb);
}

static inline void host1x_hw_debug_init(struct host1x *host, struct dentry *de)
{
	if (host->debug_op && host->debug_op->debug_init)
		host->debug_op->debug_init(de);
}

static inline void host1x_hw_show_channel_cdma(struct host1x *host,
					       struct host1x_channel *channel,
					       struct output *o)
{
	host->debug_op->show_channel_cdma(host, channel, o);
}

static inline void host1x_hw_show_channel_fifo(struct host1x *host,
					       struct host1x_channel *channel,
					       struct output *o)
{
	host->debug_op->show_channel_fifo(host, channel, o);
}

static inline void host1x_hw_show_mlocks(struct host1x *host, struct output *o)
{
	host->debug_op->show_mlocks(host, o);
}

extern struct platform_driver tegra_mipi_driver;

#endif
