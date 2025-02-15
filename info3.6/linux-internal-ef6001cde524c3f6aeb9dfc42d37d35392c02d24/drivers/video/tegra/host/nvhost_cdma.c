/*
 * drivers/video/tegra/host/nvhost_cdma.c
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2010-2016, NVIDIA Corporation. All rights reserved.
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

#include "nvhost_cdma.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "dev.h"
#include "debug.h"
#include "chip_support.h"
#include <asm/cacheflush.h>
#include <nvhost_vm.h>

#include <linux/slab.h>
#include <linux/kfifo.h>
#include <trace/events/nvhost.h>
#include <linux/interrupt.h>

/*
 * TODO:
 *   stats
 *     - for figuring out what to optimize further
 *   resizable push buffer
 *     - some channels hardly need any, some channels (3d) could use more
 */

/*
 * push_buffer
 *
 * The push buffer is a circular array of words to be fetched by command DMA.
 * Note that it works slightly differently to the sync queue; fence == cur
 * means that the push buffer is full, not empty.
 */

/**
 * Allocate pushbuffer memory
 */
int nvhost_push_buffer_alloc(struct push_buffer *pb)
{
	struct nvhost_cdma *cdma = pb_to_cdma(pb);
	pb->mapped = NULL;
	pb->dma_addr = 0;

	pb->mapped = dma_alloc_writecombine(&cdma_to_dev(cdma)->dev->dev,
			PUSH_BUFFER_SIZE + 4,
			&pb->dma_addr,
			GFP_KERNEL);
	if (!pb->mapped) {
		pb->mapped = NULL;
		return -ENOMEM;
	}

	/* for now, map pushbuffer to all address spaces */
	nvhost_vm_map_static(cdma_to_dev(cdma)->dev, pb->mapped,
			     pb->dma_addr, PUSH_BUFFER_SIZE + 4);

	return 0;
}

/**
 * Clean up push buffer resources
 */
void nvhost_push_buffer_destroy(struct push_buffer *pb)
{
	struct nvhost_cdma *cdma = pb_to_cdma(pb);
	if (pb->mapped)
		dma_free_writecombine(&cdma_to_dev(cdma)->dev->dev,
					PUSH_BUFFER_SIZE + 4,
					pb->mapped,
					pb->dma_addr);

	pb->mapped = NULL;
	pb->dma_addr = 0;
}

/**
 * Push two words to the push buffer
 * Caller must ensure push buffer is not full
 */
static void nvhost_push_buffer_push_to(struct push_buffer *pb,
				u32 op1, u32 op2)
{
	u32 cur = pb->cur;
	u32 *p = (u32 *)((uintptr_t)pb->mapped + cur);
	WARN_ON(cur == pb->fence);
	*(p++) = op1;
	*(p++) = op2;
	pb->cur = (cur + 8) & (PUSH_BUFFER_SIZE - 1);
}

/**
 * Pop a number of two word slots from the push buffer
 * Caller must ensure push buffer is not empty
 */
static void nvhost_push_buffer_pop_from(struct push_buffer *pb,
		unsigned int slots)
{
	/* Advance the next write position */
	pb->fence = (pb->fence + slots * 8) & (PUSH_BUFFER_SIZE - 1);
}

/**
 * Return the number of two word slots free in the push buffer
 */
static u32 nvhost_push_buffer_space(struct push_buffer *pb)
{
	return ((pb->fence - pb->cur) & (PUSH_BUFFER_SIZE - 1)) / 8;
}

u32 nvhost_push_buffer_putptr(struct push_buffer *pb)
{
	return pb->dma_addr + pb->cur;
}

/**
 * Add an entry to the sync queue.
 */
static void add_to_sync_queue(struct nvhost_cdma *cdma,
			      struct nvhost_job *job,
			      u32 nr_slots,
			      u32 first_get)
{
	job->first_get = first_get;
	job->num_slots = nr_slots;
	nvhost_job_get(job);
	list_add_tail(&job->list, &cdma->sync_queue);
}

/**
 * Return the status of the cdma's sync queue or push buffer for the given event
 *  - sq empty: returns 1 for empty, 0 for not empty (as in "1 empty queue" :-)
 *  - pb space: returns the number of free slots in the channel's push buffer
 * Must be called with the cdma lock held.
 */
static unsigned int cdma_status_locked(struct nvhost_cdma *cdma,
		enum cdma_event event)
{
	switch (event) {
	case CDMA_EVENT_SYNC_QUEUE_EMPTY:
		return list_empty(&cdma->sync_queue) ? 1 : 0;
	case CDMA_EVENT_PUSH_BUFFER_SPACE: {
		struct push_buffer *pb = &cdma->push_buffer;
		return nvhost_push_buffer_space(pb);
	}
	default:
		return 0;
	}
}

/**
 * Sleep (if necessary) until the requested event happens
 *   - CDMA_EVENT_SYNC_QUEUE_EMPTY : sync queue is completely empty.
 *     - Returns 1
 *   - CDMA_EVENT_PUSH_BUFFER_SPACE : there is space in the push buffer
 *     - Return the amount of space (> 0)
 * Must be called with the cdma lock held.
 */
unsigned int nvhost_cdma_wait_locked(struct nvhost_cdma *cdma,
		enum cdma_event event)
{
	for (;;) {
		unsigned int space = cdma_status_locked(cdma, event);
		if (space)
			return space;

		trace_nvhost_wait_cdma(cdma_to_channel(cdma)->dev->name,
				event);

		/* If somebody has managed to already start waiting, yield */
		if (cdma->event != CDMA_EVENT_NONE) {
			mutex_unlock(&cdma->lock);
			schedule();
			mutex_lock(&cdma->lock);
			continue;
		}
		cdma->event = event;

		mutex_unlock(&cdma->lock);
		down(&cdma->sem);
		mutex_lock(&cdma->lock);
	}
	return 0;
}

/**
 * Start timer for a buffer submition that has completed yet.
 * Must be called with the cdma lock held.
 */
static void cdma_start_timer_locked(struct nvhost_cdma *cdma,
		struct nvhost_job *job)
{
	/* In the virtual case, timeouts are handled by the server */
	if (nvhost_dev_is_virtual(cdma_to_dev(cdma)->dev))
		return;

	if (cdma->timeout.clientid) {
		/* timer already started */
		return;
	}

	cdma->timeout.clientid = job->clientid;
	cdma->timeout.sp = job->sp;
	cdma->timeout.num_syncpts = job->num_syncpts;
	cdma->timeout.start_ktime = ktime_get();
	cdma->timeout.timeout_debug_dump = job->timeout_debug_dump;
	cdma->timeout.timeout = job->timeout;
	cdma->timeout.allow_dependency = true;

	if (job->timeout)
		schedule_delayed_work(&cdma->timeout.wq,
				msecs_to_jiffies(cdma->timeout.timeout));
}

/**
 * Stop timer when a buffer submition completes.
 * Must be called with the cdma lock held.
 */
static void stop_cdma_timer_locked(struct nvhost_cdma *cdma)
{
	bool ret = true;

	while (ret)
		ret = cancel_delayed_work_sync(&cdma->timeout.wq);

	cdma->timeout.clientid = 0;
}

/**
 * For all sync queue entries that have already finished according to the
 * current sync point registers:
 *  - unpin & unref their mems
 *  - pop their push buffer slots
 *  - remove them from the sync queue
 * This is normally called from the host code's worker thread, but can be
 * called manually if necessary.
 * Must be called with the cdma lock held.
 */
static void update_cdma_locked(struct nvhost_cdma *cdma)
{
	bool signal = false;
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_syncpt *sp = &dev->syncpt;
	struct nvhost_job *job, *n;

	/* If CDMA is stopped, queue is cleared and we can return */
	if (!cdma->running)
		return;

	/*
	 * Walk the sync queue, reading the sync point registers as necessary,
	 * to consume as many sync queue entries as possible without blocking
	 */
	list_for_each_entry_safe(job, n, &cdma->sync_queue, list) {
		bool completed = true;
		int i;

		/* Check whether this syncpt has completed, and bail if not */
		for (i = 0; completed && i < job->num_syncpts; ++i)
			completed &= nvhost_syncpt_is_expired(sp,
				job->sp[i].id, job->sp[i].fence);

		if (!completed) {
			/* Start timer on next pending syncpt */
			cdma_start_timer_locked(cdma, job);
			break;
		}

		/* Cancel timeout, when a buffer completes */
		if (cdma->timeout.clientid)
			stop_cdma_timer_locked(cdma);

		/* Drop syncpoint references from this job */
		for (i = 0; i < job->num_syncpts; ++i)
			nvhost_syncpt_put_ref(sp, job->sp[i].id);

		/* Unpin the memory */
		nvhost_job_unpin(job);

		/* Pop push buffer slots */
		if (job->num_slots) {
			struct push_buffer *pb = &cdma->push_buffer;
			nvhost_push_buffer_pop_from(pb, job->num_slots);
			if (cdma->event == CDMA_EVENT_PUSH_BUFFER_SPACE)
				signal = true;
		}

		list_del(&job->list);

		nvhost_job_put(job);
	}

	if (list_empty(&cdma->sync_queue) &&
				cdma->event == CDMA_EVENT_SYNC_QUEUE_EMPTY)
			signal = true;

	/* Wake up CdmaWait() if the requested event happened */
	if (signal) {
		cdma->event = CDMA_EVENT_NONE;
		up(&cdma->sem);
	}
}


void nvhost_cdma_finalize_job_incrs(struct nvhost_syncpt *syncpt,
					struct nvhost_job_syncpt *sp)
{
	u32 id = sp->id;
	u32 fence = sp->fence;

	atomic_set(&syncpt->min_val[id], fence);
	syncpt_op().reset(syncpt, id);
	nvhost_syncpt_update_min(syncpt, id);
}

void nvhost_cdma_update_sync_queue(struct nvhost_cdma *cdma,
		struct nvhost_syncpt *syncpt, struct platform_device *dev)
{
	u32 get_restart;
	struct nvhost_job *job = NULL;
	int nb_pts = nvhost_syncpt_nb_hw_pts(syncpt);
	DECLARE_BITMAP(syncpt_used, nb_pts);

	bitmap_zero(syncpt_used, nb_pts);

	/* ensure that no-one in CPU updates syncpoint values */
	mutex_lock(&syncpt->cpu_increment_mutex);

	/*
	 * Move the sync_queue read pointer to the first entry that hasn't
	 * completed based on the current HW syncpt value. It's likely there
	 * won't be any (i.e. we're still at the head), but covers the case
	 * where a syncpt incr happens just prior/during the teardown.
	 */

	dev_dbg(&dev->dev,
		"%s: skip completed buffers still in sync_queue\n",
		__func__);

	list_for_each_entry(job, &cdma->sync_queue, list) {
		int i;
		for (i = 0; i < job->num_syncpts; ++i) {
			u32 id = job->sp[i].id;

			if (!test_bit(id, syncpt_used))
				nvhost_syncpt_update_min(syncpt, id);

			set_bit(id, syncpt_used);

			if (!nvhost_syncpt_is_expired(syncpt, id,
				job->sp[i].fence))
				goto out;
		}

		if (nvhost_debug_force_timeout_dump ||
				cdma->timeout.timeout_debug_dump)
			nvhost_job_dump(&dev->dev, job);
	}
out:
	/*
	 * Walk the sync_queue, first incrementing with the CPU syncpts that
	 * are partially executed (the first buffer) or fully skipped while
	 * still in the current context (slots are also NOP-ed).
	 *
	 * At the point contexts are interleaved, syncpt increments must be
	 * done inline with the pushbuffer from a GATHER buffer to maintain
	 * the order (slots are modified to be a GATHER of syncpt incrs).
	 *
	 * Note: save in get_restart the location where the timed out buffer
	 * started in the PB, so we can start the refetch from there (with the
	 * modified NOP-ed PB slots). This lets things appear to have completed
	 * properly for this buffer and resources are freed.
	 */

	dev_dbg(&dev->dev,
		"%s: perform CPU incr on pending same ctx buffers\n",
		__func__);

	get_restart = cdma->last_put;
	if (!list_empty(&cdma->sync_queue))
		get_restart = job->first_get;

	/* do CPU increments as long as this context continues */
	list_for_each_entry_from(job, &cdma->sync_queue, list) {
		int i;

		/* different context, gets us out of this loop */
		if (job->clientid != cdma->timeout.clientid)
			break;

		if (nvhost_debug_force_timeout_dump ||
				cdma->timeout.timeout_debug_dump)
			nvhost_job_dump(&dev->dev, job);

		/* won't need a timeout when replayed */
		job->timeout = 0;

		/* set notifier to userspace about submit timeout */
		nvhost_job_set_notifier(job, NVHOST_CHANNEL_SUBMIT_TIMEOUT);

		for (i = 0; i < job->num_syncpts; ++i)
			nvhost_cdma_finalize_job_incrs(syncpt, job->sp + i);

		/* cleanup push buffer */
		cdma_op().timeout_pb_cleanup(cdma, job->first_get,
			job->num_slots);
	}

	mutex_unlock(&syncpt->cpu_increment_mutex);

	list_for_each_entry_from(job, &cdma->sync_queue, list)
		if (job->clientid == cdma->timeout.clientid)
			job->timeout = min(job->timeout, 500);

	dev_dbg(&dev->dev,
		"%s: finished sync_queue modification\n", __func__);

	/* roll back DMAGET and start up channel again */
	cdma_op().timeout_teardown_end(cdma, get_restart);
}

/**
 * Create a cdma
 */
int nvhost_cdma_init(struct platform_device *pdev,
		     struct nvhost_cdma *cdma)
{
	int err;
	struct push_buffer *pb = &cdma->push_buffer;
	mutex_init(&cdma->lock);
	sema_init(&cdma->sem, 0);

	INIT_LIST_HEAD(&cdma->sync_queue);

	cdma->event = CDMA_EVENT_NONE;
	cdma->running = false;
	cdma->torndown = false;
	cdma->pdev = pdev;

	err = cdma_pb_op().init(pb);
	if (err)
		return err;
	return 0;
}

/**
 * Destroy a cdma
 */
void nvhost_cdma_deinit(struct nvhost_cdma *cdma)
{
	struct push_buffer *pb = &cdma->push_buffer;

	WARN_ON(cdma->running);
	nvhost_push_buffer_destroy(pb);
	cdma_op().timeout_destroy(cdma);
}

/**
 * Begin a cdma submit
 */
int nvhost_cdma_begin(struct nvhost_cdma *cdma, struct nvhost_job *job)
{
	mutex_lock(&cdma->lock);

	if (job->timeout) {
		/* init state on first submit with timeout value */
		if (!cdma->timeout.initialized) {
			int err;
			err = cdma_op().timeout_init(cdma,
				job->sp->id);
			if (err) {
				mutex_unlock(&cdma->lock);
				return err;
			}
		}
	}
	if (!cdma->running) {
		cdma_op().start(cdma);
	}
	cdma->slots_free = 0;
	cdma->slots_used = 0;
	cdma->first_get = nvhost_push_buffer_putptr(&cdma->push_buffer);
	return 0;
}

static void trace_write_gather(struct nvhost_cdma *cdma,
		u32 *cpuva, dma_addr_t iova,
		u32 offset, u32 words)
{
	if (iova) {
		u32 i;
		/*
		 * Write in batches of 128 as there seems to be a limit
		 * of how much you can output to ftrace at once.
		 */
		for (i = 0; i < words; i += TRACE_MAX_LENGTH) {
			trace_nvhost_cdma_push_gather(
				cdma_to_channel(cdma)->dev->name,
				(u32)((uintptr_t)iova),
				min(words - i, TRACE_MAX_LENGTH),
				offset + i * sizeof(u32),
				cpuva);
		}
	}
}

/**
 * Push two words into a push buffer slot
 * Blocks as necessary if the push buffer is full.
 */
void nvhost_cdma_push(struct nvhost_cdma *cdma, u32 op1, u32 op2)
{
	if (nvhost_debug_trace_cmdbuf)
		trace_nvhost_cdma_push(cdma_to_channel(cdma)->dev->name,
				op1, op2);

	nvhost_cdma_push_gather(cdma, NULL, 0, 0, op1, op2);
}

/**
 * Push two words into a push buffer slot
 * Blocks as necessary if the push buffer is full.
 */
void nvhost_cdma_push_gather(struct nvhost_cdma *cdma,
			u32 *cpuva, dma_addr_t iova,
			u32 offset, u32 op1, u32 op2)
{
	u32 slots_free = cdma->slots_free;
	struct push_buffer *pb = &cdma->push_buffer;

	if (cpuva)
		trace_write_gather(cdma, cpuva, iova, offset, op1 & 0x1fff);

	if (slots_free == 0) {
		slots_free = nvhost_cdma_wait_locked(cdma,
				CDMA_EVENT_PUSH_BUFFER_SPACE);
	}
	cdma->slots_free = slots_free - 1;
	cdma->slots_used++;
	nvhost_push_buffer_push_to(pb, op1, op2);
}

/**
 * End a cdma submit
 * Kick off DMA, add job to the sync queue, and a number of slots to be freed
 * from the pushbuffer. The handles for a submit must all be pinned at the same
 * time, but they can be unpinned in smaller chunks.
 */
void nvhost_cdma_end(struct nvhost_cdma *cdma,
		struct nvhost_job *job)
{
	bool was_idle = list_empty(&cdma->sync_queue);

	add_to_sync_queue(cdma,
			job,
			cdma->slots_used,
			cdma->first_get);

	cdma_op().kick(cdma);

	/* start timer on idle -> active transitions */
	if (was_idle)
		cdma_start_timer_locked(cdma, job);

	trace_nvhost_cdma_end(job->ch->dev->name);

	mutex_unlock(&cdma->lock);
}

/**
 * Update cdma state according to current sync point values
 */
void nvhost_cdma_update(struct nvhost_cdma *cdma)
{
	mutex_lock(&cdma->lock);
	update_cdma_locked(cdma);
	mutex_unlock(&cdma->lock);
}

/**
 * Wait for push buffer to be empty.
 * @cdma pointer to channel cdma
 * @timeout timeout in ms
 * Returns -ETIME if timeout was reached, zero if push buffer is empty.
 */
int nvhost_cdma_flush(struct nvhost_cdma *cdma, int timeout)
{
	unsigned int space, err = 0;
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);

	trace_nvhost_cdma_flush(cdma_to_channel(cdma)->dev->name, timeout);

	/*
	 * Wait for at most timeout ms. Recalculate timeout at each iteration
	 * to better keep within given timeout.
	 */
	while(!err && time_before(jiffies, end_jiffies)) {
		int timeout_jiffies = end_jiffies - jiffies;

		mutex_lock(&cdma->lock);
		space = cdma_status_locked(cdma,
				CDMA_EVENT_SYNC_QUEUE_EMPTY);
		if (space) {
			mutex_unlock(&cdma->lock);
			return 0;
		}

		/*
		 * Wait for sync queue to become empty. If there is already
		 * an event pending, we need to poll.
		 */
		if (cdma->event != CDMA_EVENT_NONE) {
			mutex_unlock(&cdma->lock);
			schedule();
		} else {
			cdma->event = CDMA_EVENT_SYNC_QUEUE_EMPTY;

			mutex_unlock(&cdma->lock);
			err = down_timeout(&cdma->sem,
					jiffies_to_msecs(timeout_jiffies));
		}
	}
	return err;
}
