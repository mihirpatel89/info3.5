/*
 * Tegra host1x Syncpoints
 *
 * Copyright (C) 2010-2015 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <trace/events/host1x.h>

#include "syncpt.h"
#include "dev.h"
#include "intr.h"
#include "debug.h"

#define SYNCPT_CHECK_PERIOD (2 * HZ)
#define MAX_STUCK_CHECK_COUNT 15

static struct host1x_syncpt_base *
host1x_syncpt_base_request(struct host1x *host)
{
	struct host1x_syncpt_base *bases = host->bases;
	unsigned int i;

	for (i = 0; i < host->info->nb_bases; i++)
		if (!bases[i].requested)
			break;

	if (i >= host->info->nb_bases)
		return NULL;

	bases[i].requested = true;
	return &bases[i];
}

static void host1x_syncpt_base_free(struct host1x_syncpt_base *base)
{
	if (base)
		base->requested = false;
}

static struct host1x_syncpt *host1x_syncpt_alloc(struct host1x *host,
						 struct device *dev,
						 unsigned long flags)
{
	int i;
	struct host1x_syncpt *sp = host->syncpt;
	char *name;

	mutex_lock(&host->syncpt_mutex);

	for (i = 0; i < host->info->nb_pts && sp->name; i++, sp++)
		;

	if (i >= host->info->nb_pts)
		goto err_alloc_syncpt;

	if (flags & HOST1X_SYNCPT_HAS_BASE) {
		sp->base = host1x_syncpt_base_request(host);
		if (!sp->base)
			goto err_alloc_base;
	}

	name = kasprintf(GFP_KERNEL, "%02d-%s", sp->id,
			dev ? dev_name(dev) : NULL);
	if (!name)
		goto err_alloc_name;

	sp->dev = dev;
	sp->name = name;

	if (flags & HOST1X_SYNCPT_CLIENT_MANAGED)
		sp->client_managed = true;
	else
		sp->client_managed = false;

	mutex_unlock(&host->syncpt_mutex);
	return sp;

err_alloc_name:
	host1x_syncpt_base_free(sp->base);
	sp->base = NULL;
err_alloc_base:
	sp = NULL;
err_alloc_syncpt:
	mutex_unlock(&host->syncpt_mutex);
	return NULL;
}

u32 host1x_syncpt_id(struct host1x_syncpt *sp)
{
	return sp->id;
}
EXPORT_SYMBOL(host1x_syncpt_id);

/*
 * Updates the value sent to hardware.
 */
u32 host1x_syncpt_incr_max(struct host1x_syncpt *sp, u32 incrs)
{
	return (u32)atomic_add_return(incrs, &sp->max_val);
}
EXPORT_SYMBOL(host1x_syncpt_incr_max);

 /*
 * Write cached syncpoint and waitbase values to hardware.
 */
void host1x_syncpt_restore(struct host1x *host)
{
	struct host1x_syncpt *sp_base = host->syncpt;
	u32 i;

	for (i = 0; i < host1x_syncpt_nb_pts(host); i++)
		host1x_hw_syncpt_restore(host, sp_base + i);
	for (i = 0; i < host1x_syncpt_nb_bases(host); i++)
		host1x_hw_syncpt_restore_wait_base(host, sp_base + i);
	wmb();
}

/*
 * Update the cached syncpoint and waitbase values by reading them
 * from the registers.
  */
void host1x_syncpt_save(struct host1x *host)
{
	struct host1x_syncpt *sp_base = host->syncpt;
	u32 i;

	for (i = 0; i < host1x_syncpt_nb_pts(host); i++) {
		if (host1x_syncpt_client_managed(sp_base + i))
			host1x_hw_syncpt_load(host, sp_base + i);
		else
			WARN_ON(!host1x_syncpt_idle(sp_base + i));
	}

	for (i = 0; i < host1x_syncpt_nb_bases(host); i++)
		host1x_hw_syncpt_load_wait_base(host, sp_base + i);
}

/*
 * Updates the cached syncpoint value by reading a new value from the hardware
 * register
 */
u32 host1x_syncpt_load(struct host1x_syncpt *sp)
{
	u32 val;
	val = host1x_hw_syncpt_load(sp->host, sp);
	trace_host1x_syncpt_load_min(sp->id, val);

	return val;
}

/*
 * Get the current syncpoint base
 */
u32 host1x_syncpt_load_wait_base(struct host1x_syncpt *sp)
{
	u32 val;
	host1x_hw_syncpt_load_wait_base(sp->host, sp);
	val = sp->base_val;
	return val;
}

/*
 * Increment syncpoint value from cpu, updating cache
 */
int host1x_syncpt_incr(struct host1x_syncpt *sp)
{
	return host1x_hw_syncpt_cpu_incr(sp->host, sp);
}
EXPORT_SYMBOL(host1x_syncpt_incr);

void host1x_syncpt_reset(struct host1x_syncpt *sp)
{
	struct host1x *host = sp->host;

	atomic_set(&sp->min_val, atomic_read(&sp->max_val));
	host1x_hw_syncpt_restore(host, sp);
}
EXPORT_SYMBOL(host1x_syncpt_reset);

/*
 * Updated sync point form hardware, and returns true if syncpoint is expired,
 * false if we may need to wait
 */
static bool syncpt_load_min_is_expired(struct host1x_syncpt *sp, u32 thresh)
{
	host1x_hw_syncpt_load(sp->host, sp);
	return host1x_syncpt_is_expired(sp, thresh);
}

/*
 * Main entrypoint for syncpoint value waits.
 */
int host1x_syncpt_wait(struct host1x_syncpt *sp, u32 thresh, long timeout,
			u32 *value)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	void *ref;
	struct host1x_waitlist *waiter;
	int err = 0, check_count = 0;
	u32 val;

	if (value)
		*value = 0;

	/* first check cache */
	if (host1x_syncpt_is_expired(sp, thresh)) {
		if (value)
			*value = host1x_syncpt_load(sp);
		return 0;
	}

	/* try to read from register */
	val = host1x_hw_syncpt_load(sp->host, sp);
	if (host1x_syncpt_is_expired(sp, thresh)) {
		if (value)
			*value = val;
		goto done;
	}

	if (!timeout) {
		err = -EAGAIN;
		goto done;
	}

	/* allocate a waiter */
	waiter = kzalloc(sizeof(*waiter), GFP_KERNEL);
	if (!waiter) {
		err = -ENOMEM;
		goto done;
	}

	/* schedule a wakeup when the syncpoint value is reached */
	err = host1x_intr_add_action(sp->host, sp->id, thresh,
				     HOST1X_INTR_ACTION_WAKEUP_INTERRUPTIBLE,
				     &wq, waiter, &ref);
	if (err)
		goto done;

	err = -EAGAIN;
	/* Caller-specified timeout may be impractically low */
	if (timeout < 0)
		timeout = LONG_MAX;

	/* wait for the syncpoint, or timeout, or signal */
	while (timeout) {
		long check = min_t(long, SYNCPT_CHECK_PERIOD, timeout);
		int remain = wait_event_interruptible_timeout(wq,
				syncpt_load_min_is_expired(sp, thresh),
				check);
		if (remain > 0 || host1x_syncpt_is_expired(sp, thresh)) {
			if (value)
				*value = host1x_syncpt_load(sp);
			err = 0;
			break;
		}
		if (remain < 0) {
			err = remain;
			break;
		}
		timeout -= check;
		if (timeout && check_count <= MAX_STUCK_CHECK_COUNT) {
			dev_warn(sp->host->dev,
				"%s: syncpoint id %d (%s) stuck waiting %d, timeout=%ld\n",
				 current->comm, sp->id, sp->name,
				 thresh, timeout);

			host1x_debug_dump_syncpts(sp->host);
			if (check_count == MAX_STUCK_CHECK_COUNT)
				host1x_debug_dump(sp->host);
			check_count++;
		}
	}
	host1x_intr_put_ref(sp->host, sp->id, ref);

done:
	return err;
}
EXPORT_SYMBOL(host1x_syncpt_wait);

static bool syncpt_is_expired(struct host1x_syncpt *sp, u32 current_val,
			      u32 future_val, u32 thresh)
{
	/* Note the use of unsigned arithmetic here (mod 1<<32).
	 *
	 * c = current_val = min_val	= the current value of the syncpoint.
	 * t = thresh			= the value we are checking
	 * f = future_val  = max_val	= the value c will reach when all
	 *				  outstanding increments have completed.
	 *
	 * Note that c always chases f until it reaches f.
	 *
	 * Dtf = (f - t)
	 * Dtc = (c - t)
	 *
	 *  Consider all cases:
	 *
	 *	A) .....c..t..f.....	Dtf < Dtc	need to wait
	 *	B) .....c.....f..t..	Dtf > Dtc	expired
	 *	C) ..t..c.....f.....	Dtf > Dtc	expired	   (Dct very large)
	 *
	 *  Any case where f==c: always expired (for any t).	Dtf == Dcf
	 *  Any case where t==c: always expired (for any f).	Dtf >= Dtc (because Dtc==0)
	 *  Any case where t==f!=c: always wait.		Dtf <  Dtc (because Dtf==0,
	 *							Dtc!=0)
	 *
	 *  Other cases:
	 *
	 *	A) .....t..f..c.....	Dtf < Dtc	need to wait
	 *	A) .....f..c..t.....	Dtf < Dtc	need to wait
	 *	A) .....f..t..c.....	Dtf > Dtc	expired
	 *
	 *   So:
	 *	   Dtf >= Dtc implies EXPIRED	(return true)
	 *	   Dtf <  Dtc implies WAIT	(return false)
	 *
	 * Note: If t is expired then we *cannot* wait on it. We would wait
	 * forever (hang the system).
	 *
	 * Note: do NOT get clever and remove the -thresh from both sides. It
	 * is NOT the same.
	 *
	 * If future valueis zero, we have a client managed sync point. In that
	 * case we do a direct comparison.
	 */

	if (!host1x_syncpt_client_managed(sp))
		return future_val - thresh >= current_val - thresh;

	return (s32)(current_val - thresh) >= 0;
}

/*
 * Returns true if syncpoint is expired, false if we may need to wait
 */
bool host1x_syncpt_is_expired(struct host1x_syncpt *sp, u32 thresh)
{
	u32 current_val, future_val;

	/* ensure that syncpoint shadow is up-to-date */
	smp_rmb();

	current_val = (u32)atomic_read(&sp->min_val);
	future_val = (u32)atomic_read(&sp->max_val);

	return syncpt_is_expired(sp, current_val, future_val, thresh);
}

int host1x_syncpt_compare(struct host1x_syncpt *syncpt, u32 thresh_a,
			  u32 thresh_b)
{
	u32 current_val, future_val;
	u32 a_n, b_n;
	bool a_expired, b_expired;

	if (thresh_a == thresh_b)
		return 0;

	/* ensure that syncpoint shadow is up-to-date */
	smp_rmb();

	current_val = (u32)atomic_read(&syncpt->min_val);
	future_val = (u32)atomic_read(&syncpt->max_val);

	a_expired = syncpt_is_expired(syncpt, current_val, future_val,
				      thresh_a);
	b_expired = syncpt_is_expired(syncpt, current_val, future_val,
				      thresh_b);

	if (a_expired && !b_expired) {
		/* Only A is expired so it must be earlier */
		return -1;
	}

	if (!a_expired && b_expired) {
		/* Likewise, B must be earlier */
		return 1;
	}

	/*
	 * Both a and b are expired (trigger before current_val) or not
	 * expired (trigger after current_val), so we can use
	 * current_val as a reference value.
	 *
	 * We normalize both a and b by subtracting ref from them.
	 * Denote the normalized values by a_n and b_n. Note that
	 * because of wrapping, a_n and/or b_n may be negative.
	 *
	 * The normalized values a_n and b_n satisfy:
	 * - a positive value triggers before a negative value
	 * - a smaller positive value triggers before a greater
	 *   positive value
	 * - a smaller negative value (greater in absolute value)
	 *   triggers before a greater negative value (smaller in
	 *   absolute value).
	 *
	 * Thus we can just stick to unsigned arithmetic and compare
	 * (u32)a_n to (u32)b_n.
	 *
	 * Just to reiterate the possible cases:
	 *
	 *	1A) ...ref..a....b....
	 *	1B) ...ref..b....a....
	 *	2A) ...b....ref..a....              b_n < 0
	 *	2B) ...a....ref..b....     a_n > 0
	 *	3A) ...a....b....ref..     a_n < 0, b_n < 0
	 *	3A) ...b....a....ref..     a_n < 0, b_n < 0
	 */

	a_n = thresh_a - current_val;
	b_n = thresh_b - current_val;

	if (a_n < b_n)
		return -1;

	if (a_n > b_n)
		return 1;

	return 0;
}

/* remove a wait pointed to by patch_addr */
int host1x_syncpt_patch_wait(struct host1x_syncpt *sp, void *patch_addr)
{
	return host1x_hw_syncpt_patch_wait(sp->host, sp, patch_addr);
}

int host1x_syncpt_init(struct host1x *host)
{
	struct host1x_syncpt_base *bases;
	struct host1x_syncpt *syncpt;
	int i;

	syncpt = devm_kzalloc(host->dev, sizeof(*syncpt) * host->info->nb_pts,
			      GFP_KERNEL);
	if (!syncpt)
		return -ENOMEM;

	bases = devm_kzalloc(host->dev, sizeof(*bases) * host->info->nb_bases,
			     GFP_KERNEL);
	if (!bases)
		return -ENOMEM;

	for (i = 0; i < host->info->nb_pts; i++) {
		syncpt[i].id = i;
		syncpt[i].host = host;

		syncpt[i].timeline =
			host1x_sync_timeline_create(host, &syncpt[i]);
	}

	for (i = 0; i < host->info->nb_bases; i++)
		bases[i].id = i;

	mutex_init(&host->syncpt_mutex);
	host->syncpt = syncpt;
	host->bases = bases;

	host1x_syncpt_restore(host);

	/* Allocate sync point to use for clearing waits for expired fences */
	host->nop_sp = host1x_syncpt_alloc(host, NULL, 0);
	if (!host->nop_sp)
		return -ENOMEM;

#ifdef CONFIG_TEGRA_HOST1X_DOWNSTREAM
	syncpt[10].name = "avp";
	syncpt[10].client_managed = true;
	syncpt[26].name = "vblank0";
	syncpt[26].client_managed = true;
	syncpt[27].name = "vblank1";
	syncpt[27].client_managed = true;
#endif

	return 0;
}

struct host1x_syncpt *host1x_syncpt_request(struct device *dev,
					    unsigned long flags)
{
	struct host1x *host;

	if (dev->parent && dev->parent != &platform_bus)
		dev = dev->parent;

	host = dev_get_drvdata(dev);

	return host1x_syncpt_alloc(host, dev, flags);
}
EXPORT_SYMBOL(host1x_syncpt_request);

void host1x_syncpt_free(struct host1x_syncpt *sp)
{
	if (!sp)
		return;

	mutex_lock(&sp->host->syncpt_mutex);

	host1x_syncpt_base_free(sp->base);
	kfree(sp->name);
	sp->base = NULL;
	sp->dev = NULL;
	sp->name = NULL;
	sp->client_managed = false;

	mutex_unlock(&sp->host->syncpt_mutex);
}
EXPORT_SYMBOL(host1x_syncpt_free);

void host1x_syncpt_deinit(struct host1x *host)
{
	int i;
	struct host1x_syncpt *sp = host->syncpt;
	for (i = 0; i < host->info->nb_pts; i++, sp++) {
		host1x_sync_timeline_destroy(sp->timeline);
		kfree(sp->name);
	}
}

/*
 * Read max. It indicates how many operations there are in queue, either in
 * channel or in a software thread.
 * */
u32 host1x_syncpt_read_max(struct host1x_syncpt *sp)
{
	smp_rmb();
	return (u32)atomic_read(&sp->max_val);
}
EXPORT_SYMBOL(host1x_syncpt_read_max);

void host1x_syncpt_update_min(struct host1x_syncpt *sp)
{
	host1x_hw_syncpt_load(sp->host, sp);
}
EXPORT_SYMBOL(host1x_syncpt_update_min);

/*
 * Read min, which is a shadow of the current sync point value in hardware.
 */
u32 host1x_syncpt_read_min(struct host1x_syncpt *sp)
{
	smp_rmb();
	return (u32)atomic_read(&sp->min_val);
}
EXPORT_SYMBOL(host1x_syncpt_read_min);

u32 host1x_syncpt_read(struct host1x_syncpt *sp)
{
	return host1x_syncpt_load(sp);
}
EXPORT_SYMBOL(host1x_syncpt_read);

int host1x_syncpt_nb_pts(struct host1x *host)
{
	return host->info->nb_pts;
}

int host1x_syncpt_nb_bases(struct host1x *host)
{
	return host->info->nb_bases;
}

int host1x_syncpt_nb_mlocks(struct host1x *host)
{
	return host->info->nb_mlocks;
}

struct host1x_syncpt *host1x_syncpt_get(struct host1x *host, u32 id)
{
	if (host->info->nb_pts < id)
		return NULL;
	return host->syncpt + id;
}
EXPORT_SYMBOL(host1x_syncpt_get);

struct host1x_syncpt_base *host1x_syncpt_get_base(struct host1x_syncpt *sp)
{
	return sp ? sp->base : NULL;
}
EXPORT_SYMBOL(host1x_syncpt_get_base);

u32 host1x_syncpt_base_id(struct host1x_syncpt_base *base)
{
	return base->id;
}
EXPORT_SYMBOL(host1x_syncpt_base_id);
