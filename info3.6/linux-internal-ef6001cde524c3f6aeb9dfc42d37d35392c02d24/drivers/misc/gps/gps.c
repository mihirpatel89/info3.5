/* drivers/misc/gps/gps.c
 *
 * Implementation of the GPS driver.
 
 * Copyright (C) 2004,2005 TomTom BV <http://www.tomtom.com/>
 * Authors:
 * Jeroen Taverne <jeroen.taverne@tomtom.com>
 * Dimitry Andric <dimitry.andric@tomtom.com>
 
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>

 */

/*******************************************************************************
* Dependency
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/version.h>

#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif


struct mtk_gps_hardware{
	int (*ext_power_on)(void);
	int (*ext_power_off)(void);
	int ext_force_on;
};


struct mtk_gps_hardware mt3332_gps_hw = {
	.ext_power_on = NULL,
	.ext_power_off = NULL,
	.ext_force_on = -1,
};


struct mtk_gps_gpios {
	int force_on;
};

struct mtk_gps_gpios mt3332_gpios = {
	.force_on = -1,
};

/******************************************************************************
* Function Configuration
******************************************************************************/
/*#define FAKE_DATA*/
#define GPS_SUSPEND_RESUME
#define GPS_CONFIGURABLE_RESET_DELAY
/******************************************************************************
* Definition
******************************************************************************/
/* device name and major number */
#define GPS_DEVNAME			   "mt3332-gps"
/******************************************************************************
 * Debug configuration
******************************************************************************/
#define GPS_DBG_NONE(fmt, arg...)	 do {} while (0)
#define GPS_DBG_FUNC(fmt, arg...)	 printk(KERN_INFO PFX "%s: " fmt, __FUNCTION__, ##arg)
#define GPS_ERR(fmt, arg...)		 printk(KERN_ERR PFX "%s: " fmt, __FUNCTION__ , ##arg)
#define GPS_WARN(fmt, arg...)		 printk(KERN_WARNING PFX "%s: " fmt, __FUNCTION__, ##arg)
#define GPS_NOTICE(fmt, arg...)		 printk(KERN_NOTICE PFX "%s: " fmt, __FUNCTION__, ##arg)
#define GPS_INFO(fmt, arg...)		 printk(KERN_INFO PFX "%s: " fmt, __FUNCTION__, ##arg)
#define GPS_TRC_FUNC(f)				 printk(PFX "<%s>\n", __FUNCTION__);
#define GPS_TRC_VERBOSE(fmt, arg...) printk(PFX fmt, ##arg)

#define PFX "GPS: "
#define GPS_DBG GPS_DBG_FUNC
#define GPS_TRC GPS_DBG_NONE /*GPS_TRC_FUNC*/
#define GPS_VER GPS_DBG_NONE /*GPS_TRC_VERBOSE*/
#define IH_DBG GPS_DBG_NONE
/*******************************************************************************
* structure & enumeration
*******************************************************************************/
enum {
	GPS_PWRCTL_UNSUPPORTED	= 0xFF,
	GPS_PWRCTL_OFF			= 0x00,
	GPS_PWRCTL_ON			= 0x01,
	GPS_PWRCTL_RST			= 0x02,
	GPS_PWRCTL_OFF_FORCE	= 0x03,
	GPS_PWRCTL_RST_FORCE	= 0x04,
	GPS_PWRCTL_MAX			= 0x05,
};
enum {
	GPS_PWR_UNSUPPORTED		= 0xFF,
	GPS_PWR_RESUME			= 0x00,
	GPS_PWR_SUSPEND			= 0x01,
	GPS_PWR_MAX				= 0x02,
};
enum {
	GPS_STATE_UNSUPPORTED	= 0xFF,
	GPS_STATE_OFF			= 0x00, /*cleanup/power off, default state*/
	GPS_STATE_INIT			= 0x01, /*init*/
	GPS_STATE_START			= 0x02, /*start navigating*/
	GPS_STATE_STOP			= 0x03, /*stop navigating*/
	GPS_STATE_DEC_FREQ		= 0x04,
	GPS_STATE_SLEEP			= 0x05,
	GPS_STATE_MAX			= 0x06,
};
enum {
	GPS_PWRSAVE_UNSUPPORTED = 0xFF,
	GPS_PWRSAVE_DEC_FREQ	= 0x00,
	GPS_PWRSAVE_SLEEP		= 0x01,
	GPS_PWRSAVE_OFF			= 0x02,
	GPS_PWRSAVE_MAX			= 0x03,
};
/*---------------------------------------------------------------------------*/
struct gps_data{
	int  dat_len;
	int  dat_pos;
	char dat_buf[4096];
	spinlock_t lock;
	wait_queue_head_t read_wait;
	struct semaphore sem;
};
/*---------------------------------------------------------------------------*/
struct gps_sta_itm {	/*gps status record*/
	unsigned char year;		/*current year - 1900*/
	unsigned char month;	/*1~12*/
	unsigned char day;		/*1~31*/
	unsigned char hour;		/*0~23*/
	unsigned char minute;	   /*0~59*/
	unsigned char sec;		/*0~59*/
	unsigned char count;	/*reborn count*/
	unsigned char reason;	/*reason: 0: timeout; 1: force*/
};
/*---------------------------------------------------------------------------*/
struct gps_sta_obj {
	int index;
	struct gps_sta_itm items[32];
};
/*---------------------------------------------------------------------------*/
struct gps_drv_obj {
	unsigned char pwrctl;
	unsigned char suspend;
	unsigned char state;
	unsigned char pwrsave;
	int rdelay;   /*power reset delay*/
	struct kobject *kobj;
	struct mutex sem;
	struct gps_sta_obj status;
	struct mtk_gps_hardware *hw;
};
/*---------------------------------------------------------------------------*/
struct gps_dev_obj {
	struct class	*cls;
	struct device	*dev;
	dev_t			devno;
	struct cdev		chdev;
	struct mtk_gps_hardware *hw;
};
/******************************************************************************
 * local variables
******************************************************************************/
static struct gps_data gps_private = {0};

#if defined(FAKE_DATA)
static char fake_data[] = {
"$GPGGA,135036.000,2446.3713,N,12101.3605,E,1,5,1.61,191.1,M,15.1,M,,*51\r\n"
"$GPGSA,A,3,22,18,14,30,31,,,,,,,,1.88,1.61,0.98*09\r\n"
"$GPGSV,2,1,6,18,83,106,32,22,58,324,35,30,45,157,35,14,28,308,32*44\r\n"
"$GPGSV,2,2,6,40,21,254,,31,17,237,29*42\r\n"
"$GPRMC,135036.000,A,2446.37125,N,12101.36054,E,0.243,56.48,140109,,A*46\r\n"
"$GPVTG,56.48,T,,M,0.243,N,0.451,K,A*07\r\n"
};
#endif /*FAKE_DATA*/

/*this should be synchronous with mnld.c
enum {
	MNL_RESTART_NONE			= 0x00, //recording the 1st of mnld
	MNL_RESTART_TIMEOUT_INIT	= 0x01, //restart due to timeout
	MNL_RESTART_TIMEOUT_MONITOR = 0x02, //restart due to timeout
	MNL_RESTART_TIMEOUT_WAKEUP	= 0x03, //restart due to timeout
	MNL_RESTART_TIMEOUT_TTFF	= 0x04, //restart due to TTFF timeout
	MNL_RESTART_FORCE			= 0x04, //restart due to external command
};
*/
/*---------------------------------------------------------------------------*/
static char *str_reason[] = {"none", "init", "monitor", "wakeup", "TTFF", "force", "unknown"};
/******************************************************************************
* Functions
******************************************************************************/
static int mt3332_gps_ext_power_on(void)
{
	int ret = 0;

	if (gpio_is_valid(mt3332_gpios.force_on)) {
		gpio_set_value(mt3332_gpios.force_on, 1);
		GPS_DBG("(enable gps)\n");
		mdelay(10);
	} else {
		GPS_ERR("enable gpio not init\n");
	}
	return ret;
}

int mt3332_gps_ext_power_off(void)
{
	int ret = 0;

	if (gpio_is_valid(mt3332_gpios.force_on)) {
		gpio_set_value(mt3332_gpios.force_on, 0);
		GPS_DBG("(disable gps)\n");
		mdelay(10);
	} else {
		GPS_ERR("disable gpio not init\n");
	}
	return ret;
}

static inline void mt3332_gps_power(struct mtk_gps_hardware *hw,
				unsigned int on, unsigned int force)
{
	int err;
	GPS_DBG("Switching GPS device %s\n", on ? "on" : "off");
	if (!hw) {
		GPS_ERR("null pointer!!\n");
		return;
	}

	if (on) {
		if (hw->ext_power_on) {
			err = hw->ext_power_on();
			if (err)
				GPS_ERR("ext_power_on fail\n");
		}

		mdelay(120);
	} else {

		if (hw->ext_power_off) {
				err = hw->ext_power_off();
			if (err)
				GPS_ERR("ext_power_off fail\n");
		}
		mdelay(10);

	}
}


/*****************************************************************************/
static inline void mt3332_gps_reset(struct mtk_gps_hardware *hw, int delay, int force)
{
	mt3332_gps_power(hw, 1, FALSE);
	mdelay(delay);
	mt3332_gps_power(hw, 0, force);
	mdelay(delay);
	mt3332_gps_power(hw, 1, FALSE);
}

/******************************************************************************/

static inline int mt3332_gps_set_suspend(struct gps_drv_obj *obj,
										  unsigned char suspend)
{
	if (!obj)
		return -1;
	mutex_lock(&obj->sem);
	if (obj->suspend != suspend) {
		GPS_DBG("issue sysfs_notify : %p\n", obj->kobj->sd);
		sysfs_notify(obj->kobj, NULL, "suspend");
	}
	obj->suspend = suspend;
	mutex_unlock(&obj->sem);
	return 0;
}
/******************************************************************************/
static inline int mt3332_gps_set_pwrctl(struct gps_drv_obj *obj,
										 unsigned char pwrctl)
{
	int err = 0;
	if (!obj)
		return -1;
	mutex_lock(&obj->sem);

	if ((pwrctl == GPS_PWRCTL_ON) || (pwrctl == GPS_PWRCTL_OFF)) {
		obj->pwrctl = pwrctl;
		mt3332_gps_power(obj->hw, pwrctl, FALSE);
	} else if (pwrctl == GPS_PWRCTL_OFF_FORCE) {
		obj->pwrctl = pwrctl;
		mt3332_gps_power(obj->hw, pwrctl, TRUE);
	} else if (pwrctl == GPS_PWRCTL_RST) {
		mt3332_gps_reset(obj->hw, obj->rdelay, FALSE);
		obj->pwrctl = GPS_PWRCTL_ON;
	} else if (pwrctl == GPS_PWRCTL_RST_FORCE) {
		mt3332_gps_reset(obj->hw, obj->rdelay, TRUE);
		obj->pwrctl = GPS_PWRCTL_ON;
	} else {
		err = -1;
	}
	mutex_unlock(&obj->sem);
	return err;
}
 /******************************************************************************/
static inline int mt3332_gps_set_status(struct gps_drv_obj *obj,
										const char *buf, size_t count)
{
	int err = 0;
	int year, mon, day, hour, minute, sec, cnt, reason, idx;
	if (!obj)
		return -1;

	mutex_lock(&obj->sem);
	if (sscanf(buf, "(%d/%d/%d %d:%d:%d) - %d/%d", &year, &mon, &day,
			   &hour, &minute, &sec, &cnt, &reason) == 8) {
		int number = (int)(sizeof(obj->status.items)/sizeof(obj->status.items[0]));
		idx = obj->status.index % number;
		obj->status.items[idx].year  = (unsigned char)year;
		obj->status.items[idx].month = (unsigned char)mon;
		obj->status.items[idx].day	 = (unsigned char)day;
		obj->status.items[idx].hour  = (unsigned char)hour;
		obj->status.items[idx].minute = (unsigned char)minute;
		obj->status.items[idx].sec	 = (unsigned char)sec;
		obj->status.items[idx].count = (unsigned char)cnt;
		obj->status.items[idx].reason = (unsigned char)reason;
		obj->status.index++;
	} else {
		err = -1;
	}
	mutex_unlock(&obj->sem);
	return err;
}
/******************************************************************************/
static inline int mt3332_gps_set_state(struct gps_drv_obj *obj,
									   unsigned char state)
{
	int err = 0;
	if (!obj)
		return -1;
	mutex_lock(&obj->sem);
	if (state < GPS_STATE_MAX)
		obj->state = state;
	else
		err = -1;
	mutex_unlock(&obj->sem);
	return err;
}
/******************************************************************************/
static inline int mt3332_gps_set_pwrsave(struct gps_drv_obj *obj,
										 unsigned char pwrsave)
{
	int err = 0;

	if (!obj)
		return -1;
	mutex_lock(&obj->sem);
	if (pwrsave < GPS_PWRSAVE_MAX)
		obj->pwrsave = pwrsave;
	else
		err = -1;
	mutex_unlock(&obj->sem);
	return err;
}
/******************************************************************************/
static inline int mt3332_gps_dev_suspend(struct gps_drv_obj *obj)
{
#if defined(GPS_SUSPEND_RESUME)
	int err;
	err = mt3332_gps_set_suspend(obj, GPS_PWR_SUSPEND);
	if (err)
		GPS_DBG("set suspend fail: %d\n", err);
	err = mt3332_gps_set_pwrctl(obj, GPS_PWRCTL_OFF);
	if (err)
		GPS_DBG("set pwrctl fail: %d\n", err);
	return err;
#endif
}
/******************************************************************************/
static inline int mt3332_gps_dev_resume(struct gps_drv_obj *obj)
{
#if defined(GPS_SUSPEND_RESUME)
	int err;
	err = mt3332_gps_set_suspend(obj, GPS_PWR_RESUME);
	if (err)
		GPS_DBG("set suspend fail: %d\n", err);
	/*don't power on device automatically*/
	return err;
#endif
}
/******************************************************************************/
static ssize_t mt3332_show_pwrctl(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct gps_drv_obj *obj;
	ssize_t res;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	mutex_lock(&obj->sem);
	res = snprintf(buf, PAGE_SIZE, "%d\n", obj->pwrctl);
	mutex_unlock(&obj->sem);
	return res;
}
/******************************************************************************/
static ssize_t mt3332_store_pwrctl(struct device *dev, struct device_attribute *attr,
								  const char *buf, size_t count)
{
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	if ((count == 1) ||
		((count == 2) && (buf[1] == '\n'))) {
		unsigned char pwrctl = buf[0] - '0';
		if (!mt3332_gps_set_pwrctl(obj, pwrctl))
			return count;
	 }
	GPS_DBG("invalid content: '%s', length = %d\n", buf, count);
	return count;
}
/******************************************************************************/
static ssize_t mt3332_show_suspend(struct device *dev,
								   struct device_attribute *attr, char *buf)
{
	struct gps_drv_obj *obj;
	ssize_t res;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	mutex_lock(&obj->sem);
	res = snprintf(buf, PAGE_SIZE, "%d\n", obj->suspend);
	mutex_unlock(&obj->sem);
	return res;
}
/******************************************************************************/
static ssize_t mt3332_store_suspend(struct device *dev, struct device_attribute *attr,
									const char *buf, size_t count)
{
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	if ((count == 1) ||
		((count == 2) && (buf[1] == '\n'))) {
		unsigned char suspend = buf[0] - '0';
		if (suspend == GPS_PWR_SUSPEND) {
			if (!mt3332_gps_dev_suspend(obj))
				return count;
		} else if (suspend == GPS_PWR_RESUME) {
			if (!mt3332_gps_dev_resume(obj))
				return count;
		}
	}
	GPS_DBG("invalid content: '%s', length = %d\n", buf, count);
	return count;
}
/******************************************************************************/
static ssize_t mt3332_show_status(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	int res, idx, num, left, cnt, len;
	struct gps_drv_obj *obj;
	char *reason = NULL;
	int reason_max = (int)(sizeof(str_reason)/sizeof(str_reason[0]));

	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	mutex_lock(&obj->sem);
	num = (int)(sizeof(obj->status.items)/sizeof(obj->status.items[0]));
	left = PAGE_SIZE;
	cnt = 0;
	len = 0;
	for (idx = 0; idx < num; idx++) {
		if (obj->status.items[idx].month == 0)
			continue;
		if (obj->status.items[idx].reason >= reason_max)
			reason = str_reason[reason_max-1];
		else
			reason = str_reason[obj->status.items[idx].reason];
		cnt = snprintf(buf+len, left, "[%d] %.4d/%.2d/%.2d %.2d:%.2d:%.2d - %d, %s\n", idx,
			  obj->status.items[idx].year + 1900, obj->status.items[idx].month,
			  obj->status.items[idx].day, obj->status.items[idx].hour,
			  obj->status.items[idx].minute, obj->status.items[idx].sec,
			  obj->status.items[idx].count, reason);
		left -= cnt;
		len += cnt;
	}
	res = PAGE_SIZE - left;
	mutex_unlock(&obj->sem);
	return res;
}
/******************************************************************************/
static ssize_t mt3332_store_status(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t count)
{
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	if (!mt3332_gps_set_status(obj, buf, count))
		return count;
	GPS_DBG("invalid content: '%s', length = %d\n", buf, count);
	return count;
}
/******************************************************************************/
static ssize_t mt3332_show_state(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	ssize_t res;
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
		if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	mutex_lock(&obj->sem);
	res = snprintf(buf, PAGE_SIZE, "%d\n", obj->state);
	mutex_unlock(&obj->sem);
	return res;
}
/******************************************************************************/
static ssize_t mt3332_store_state(struct device *dev, struct device_attribute *attr,
								  const char *buf, size_t count)
{
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	if ((count == 1) ||
		((count == 2) && (buf[1] == '\n'))) {	/*To Do: dynamic change according to input*/
		unsigned char state = buf[0] - '0';
		if (!mt3332_gps_set_state(obj, state))
			return count;
	}
	GPS_DBG("invalid content: '%s', length = %d\n", buf, count);
	return count;
}
/******************************************************************************/
static ssize_t mt3332_show_pwrsave(struct device *dev,
								   struct device_attribute *attr, char *buf)
{
	ssize_t res;
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	mutex_lock(&obj->sem);
	res = snprintf(buf, PAGE_SIZE, "%d\n", obj->pwrsave);
	mutex_unlock(&obj->sem);
	return res;
}
/******************************************************************************/
static ssize_t mt3332_store_pwrsave(struct device *dev, struct device_attribute *attr,
									const char *buf, size_t count)
{
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	if ((count == 1) ||
		((count == 2) && (buf[1] == '\n'))) {
		unsigned char pwrsave = buf[0] - '0';
		if (!mt3332_gps_set_pwrsave(obj, pwrsave))
			return count;
	}
	GPS_DBG("invalid content: '%s', length = %d\n", buf, count);
	return count;
}
/******************************************************************************/
#if defined(GPS_CONFIGURABLE_RESET_DELAY)
/******************************************************************************/
static ssize_t mt3332_show_rdelay(struct device *dev,
								   struct device_attribute *attr, char *buf)
{
	ssize_t res;
	struct gps_drv_obj *obj;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	mutex_lock(&obj->sem);
	res = snprintf(buf, PAGE_SIZE, "%d\n", obj->rdelay);
	mutex_unlock(&obj->sem);
	return res;
}
/******************************************************************************/
static ssize_t mt3332_store_rdelay(struct device *dev, struct device_attribute *attr,
									const char *buf, size_t count)
{
	struct gps_drv_obj *obj;
	int rdelay;
	char *end;
	if (!dev) {
		GPS_DBG("dev is null!!\n");
		return 0;
	}
	obj = (struct gps_drv_obj *)dev_get_drvdata(dev);
	if (!obj) {
		GPS_DBG("drv data is null!!\n");
		return 0;
	}
	end = (char *)buf+count;
	rdelay = (int)simple_strtol(buf, &end, 10);
	if (rdelay < 2000) {
		mutex_lock(&obj->sem);
		obj->rdelay = rdelay;
		mutex_unlock(&obj->sem);
		return count;
	}
	GPS_DBG("invalid content: '%s', length = %d\n", buf, count);
	return count;
}
/******************************************************************************/
#endif
/******************************************************************************/
DEVICE_ATTR(pwrctl,		S_IWUSR | S_IWGRP | S_IRUGO, mt3332_show_pwrctl, mt3332_store_pwrctl);
DEVICE_ATTR(suspend,	S_IWUSR | S_IWGRP | S_IRUGO, mt3332_show_suspend, mt3332_store_suspend);
DEVICE_ATTR(status,		S_IWUSR | S_IWGRP | S_IRUGO, mt3332_show_status, mt3332_store_status);
DEVICE_ATTR(state,		S_IWUSR | S_IWGRP | S_IRUGO, mt3332_show_state, mt3332_store_state);
DEVICE_ATTR(pwrsave,	S_IWUSR | S_IWGRP | S_IRUGO, mt3332_show_pwrsave, mt3332_store_pwrsave);
#if defined(GPS_CONFIGURABLE_RESET_DELAY)
DEVICE_ATTR(rdelay,		S_IWUSR | S_IWGRP | S_IRUGO, mt3332_show_rdelay, mt3332_store_rdelay);
#endif
static struct device_attribute *gps_attr_list[] = {
	&dev_attr_pwrctl,
	&dev_attr_suspend,
	&dev_attr_status,
	&dev_attr_state,
	&dev_attr_pwrsave,
#if defined(GPS_CONFIGURABLE_RESET_DELAY)
	&dev_attr_rdelay,
#endif
};
/******************************************************************************/
static int mt3332_gps_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = (int)(sizeof(gps_attr_list)/sizeof(gps_attr_list[0]));
	if (!dev)
		return -EINVAL;

	GPS_TRC();
	for (idx = 0; idx < num; idx++) {
		err = device_create_file(dev, gps_attr_list[idx]);
		if (err) {
			GPS_DBG("device_create_file (%s) = %d\n", gps_attr_list[idx]->attr.name, err);
			break;
		}
	}

	return err;
}
/******************************************************************************/
static int mt3332_gps_delete_attr(struct device *dev)
{
	int idx, err = 0;
	int num = (int)(sizeof(gps_attr_list)/sizeof(gps_attr_list[0]));

	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		device_remove_file(dev, gps_attr_list[idx]);

	return err;
}
/******************************************************************************/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36))
static int mt3332_gps_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	GPS_DBG("mt3332_gps_ioctl!!\n");
	return -ENOIOCTLCMD;
}
#else
long mt3332_gps_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	GPS_DBG("mt3332_gps_unlocked_ioctl!!\n");
	return -ENOIOCTLCMD;
}
#endif
/*****************************************************************************/
static int mt3332_gps_open(struct inode *inode, struct file *file)
{
	GPS_TRC();
	file->private_data = &gps_private;	/*all files share the same buffer*/
	return nonseekable_open(inode, file);
}
/*****************************************************************************/
static int mt3332_gps_release(struct inode *inode, struct file *file)
{
	struct gps_data *dev = file->private_data;

	GPS_TRC();

	if (dev)
		file->private_data = NULL;

	return 0;
}
/******************************************************************************/
static ssize_t mt3332_gps_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct gps_data *dev = file->private_data;
	ssize_t ret = 0;
	int copy_len = 0;

	GPS_TRC();

	if (!dev)
		return -EINVAL;

	if (signal_pending(current))
		return -ERESTARTSYS;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	if (dev->dat_len == 0) { /*no data to be read*/
		up(&dev->sem);
		if (file->f_flags & O_NONBLOCK) /*non-block mode*/
			return -EAGAIN;
		do {/*block mode*/
			ret = wait_event_interruptible(dev->read_wait, (dev->dat_len > 0));
			if (ret == -ERESTARTSYS)
				return -ERESTARTSYS;
		} while (ret == 0);
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}

	/*data is available*/
	copy_len = (dev->dat_len < (int)count) ? (dev->dat_len) : (int)(count);
	if (copy_to_user(buf, dev->dat_buf+dev->dat_pos, (unsigned long)copy_len)) {
		GPS_DBG("copy_to_user error: 0x%p 0x%p, %d\n", buf, dev->dat_buf, dev->dat_len);
		ret = -EFAULT;
	} else {
		GPS_VER("mt3332_gps_read(%d,%d,%d) = %d\n", count, dev->dat_pos, dev->dat_len, copy_len);
		if (dev->dat_len > (copy_len+dev->dat_pos)) {
			dev->dat_pos += copy_len;
		} else {
			dev->dat_len = 0;
			dev->dat_pos = 0;
		}
		ret = copy_len;
	}

	up(&dev->sem);
	GPS_VER("%s return %d bytes\n", __FUNCTION__, ret);
	return ret;
}
/******************************************************************************/
static ssize_t mt3332_gps_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct gps_data *dev = file->private_data;
	ssize_t ret = 0;

	GPS_TRC();

	if (!dev)
		return -EINVAL;

	if (!count)		/*no data written*/
		return 0;

	if (signal_pending(current))
		return -ERESTARTSYS;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	if (copy_from_user(dev->dat_buf, buf, count)) {
		GPS_DBG("copy_from_user error");
		ret = -EFAULT;
	} else {
		dev->dat_len = count;
		dev->dat_pos = 0;
		ret = count;
	}
	up(&dev->sem);
	wake_up_interruptible(&dev->read_wait);
	GPS_VER("%s: write %d bytes\n", __FUNCTION__, dev->dat_len);
	return ret;
}
/******************************************************************************/
static unsigned int mt3332_gps_poll(struct file *file, poll_table *wait)
{
	struct gps_data *dev = file->private_data;
	unsigned int mask = 0;

	GPS_TRC();

	if (!dev)
		return 0;

	down(&dev->sem);
	poll_wait(file, &dev->read_wait, wait);
	if (dev->dat_len != 0) /*readable if data is available*/
		mask = (POLLIN|POLLRDNORM) | (POLLOUT|POLLWRNORM);
	else /*always writable*/
		mask = (POLLOUT|POLLWRNORM);
	up(&dev->sem);
	GPS_VER("%s: mask : 0x%X\n", __FUNCTION__, mask);
	return mask;
}
/*****************************************************************************/
/* Kernel interface */
static struct file_operations mt3332_gps_fops = {
	.owner		= THIS_MODULE,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36))
	.ioctl		= mt3332_gps_ioctl,
#else
	.unlocked_ioctl		 = mt3332_gps_unlocked_ioctl,
#endif
	.open		= mt3332_gps_open,
	.read		= mt3332_gps_read,
	.write		= mt3332_gps_write,
	.release	= mt3332_gps_release,
	.poll		= mt3332_gps_poll,
};
/*****************************************************************************/
extern unsigned int reset_state;
/*****************************************************************************/
static void mt3332_gps_hw_init(struct mtk_gps_hardware *hw)
{
	mt3332_gps_power(hw, 1, FALSE);
}
/*****************************************************************************/
static void mt3332_gps_hw_exit(struct mtk_gps_hardware *hw)
{
	mt3332_gps_power(hw, 0, FALSE);
}
/*****************************************************************************/

static struct of_device_id mt3332_of_match[] = {
	{.compatible = "mtk,mt3332", },
	{},
};
MODULE_DEVICE_TABLE(of, mt3332_of_match);

static int mtk_request_gpio(struct mtk_gps_gpios *pgpios)
{

	int ret = 0;

	if (gpio_is_valid(pgpios->force_on)) {
		GPS_DBG("enable gpio = %d\n", pgpios->force_on);

		ret = gpio_request(pgpios->force_on, "force-on-gpios");
		GPS_DBG("gpio_request ret = %d\n", ret);

		if (ret)
			return ret;
		GPS_DBG("gpio_request ok\n");

		/* output mode */
		gpio_direction_output(pgpios->force_on, 1);
	} else {
		pgpios->force_on = -1;
		GPS_ERR("enable gpio  is not registered\n");
		return -EINVAL;
	}
	return 0;
}

static int mt3332_gps_parse_dt(struct platform_device *pdev,
					struct mtk_gps_gpios *pgpios)
{
	struct device_node *np = pdev->dev.of_node;

	pgpios->force_on = of_get_named_gpio(np, "force-on-gpios", 0);
	return mtk_request_gpio(pgpios);
}

static int mt3332_gps_probe(struct platform_device *dev)
{
	struct mtk_gps_hardware *hw = NULL;
	struct mtk_gps_gpios *p_mtk_gps_gpios = NULL;
	int ret = 0, err = 0;
	struct gps_drv_obj *drvobj = NULL;
	struct gps_dev_obj *devobj = NULL;
	/* init mtk_gps_gpios */
	p_mtk_gps_gpios = &mt3332_gpios;

	if (dev->dev.of_node) {

		if (mt3332_gps_parse_dt(dev, &mt3332_gpios)) {
			GPS_ERR("fail to find gpio\n");
			goto free_res;
		}
	} else if (dev->dev.platform_data) {
		p_mtk_gps_gpios->force_on =
			((struct mtk_gps_hardware *)
				dev->dev.platform_data)->ext_force_on;
		if (gpio_is_valid(p_mtk_gps_gpios->force_on)) {
			ret = mtk_request_gpio(p_mtk_gps_gpios);
			if (ret) {
				GPS_ERR("fail to request gpio\n");
				goto free_res;
			}
		} else {
			GPS_ERR("could not find gpio\n");
			goto free_res;
		}
	} else {
		goto free_res;
	}
	/*set drv data */
	mt3332_gps_hw.ext_power_on = &mt3332_gps_ext_power_on;
	mt3332_gps_hw.ext_power_off = &mt3332_gps_ext_power_off;
	hw = &mt3332_gps_hw;

	devobj = kzalloc(sizeof(*devobj), GFP_KERNEL);
	if (!devobj) {
		GPS_ERR("-ENOMEM\n");
		err = -ENOMEM;
		goto error;
	}
	mt3332_gps_hw_init(hw);

	/* init devobj */
	GPS_DBG("Registering chardev\n");
	ret = alloc_chrdev_region(&devobj->devno, 0, 1, GPS_DEVNAME);
	if (ret) {
		GPS_ERR("alloc_chrdev_region fail: %d\n", ret);
		goto error;
	} else {
		GPS_DBG("major: %d, minor: %d\n", MAJOR(devobj->devno), MINOR(devobj->devno));
	}
	cdev_init(&devobj->chdev, &mt3332_gps_fops);
	devobj->chdev.owner = THIS_MODULE;
	err = cdev_add(&devobj->chdev, devobj->devno, 1);
	if (err) {
		GPS_ERR("cdev_add fail: %d\n", err);
		goto error;
	}
	drvobj = kmalloc(sizeof(*drvobj), GFP_KERNEL);
	if (!drvobj) {
		err = -ENOMEM;
		goto error;
	}
	memset(drvobj, 0, sizeof(*drvobj));

	devobj->cls = class_create(THIS_MODULE, "gpsdrv");
	if (IS_ERR(devobj->cls)) {
		GPS_ERR("Unable to create class, err = %d\n", (int)PTR_ERR(devobj->cls));
		goto error;
	}
	devobj->dev = device_create(devobj->cls, NULL, devobj->devno, drvobj, "gps");
	drvobj->hw = hw;
	drvobj->pwrctl = 0;
	drvobj->suspend = 0;
	drvobj->state = GPS_STATE_UNSUPPORTED;
	drvobj->pwrsave = GPS_PWRSAVE_UNSUPPORTED;
	drvobj->rdelay = 50;
	drvobj->kobj = &devobj->dev->kobj;
	mutex_init(&drvobj->sem);

	err = mt3332_gps_create_attr(devobj->dev);
	if (err)
		goto error;

	/*initialize members*/
	spin_lock_init(&gps_private.lock);
	init_waitqueue_head(&gps_private.read_wait);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36))
	init_MUTEX(&gps_private.sem);
#else
	sema_init(&gps_private.sem, 1);
#endif
	gps_private.dat_len = 0;
	gps_private.dat_pos = 0;
	memset(gps_private.dat_buf, 0x00, sizeof(gps_private.dat_buf));

	/*set platform data:
	  a new device created for gps */
	platform_set_drvdata(dev, devobj);

	GPS_DBG("Done\n");
	return 0;

error:
	mt3332_gps_hw.ext_power_on = NULL;
	mt3332_gps_hw.ext_power_off = NULL;
	if (err == 0)
		cdev_del(&devobj->chdev);
	if (ret == 0)
		unregister_chrdev_region(devobj->devno, 1);
	return -1;
free_res:
	if (gpio_is_valid(p_mtk_gps_gpios->force_on)) {
		gpio_free(p_mtk_gps_gpios->force_on);
		p_mtk_gps_gpios->force_on =  -1;
	}
	return -1;
}
/*****************************************************************************/
static int mt3332_gps_remove(struct platform_device *dev)
{
	struct gps_dev_obj *devobj = (struct gps_dev_obj *)platform_get_drvdata(dev);
	struct gps_drv_obj *drvobj = (struct gps_drv_obj *)dev_get_drvdata(devobj->dev);
	int err;

	if (!devobj || !drvobj) {
		GPS_ERR("null pointer: %p, %p\n", devobj, drvobj);
		return -1;
	}

	GPS_DBG("Unregistering chardev\n");
	kfree(devobj);

	cdev_del(&devobj->chdev);
	unregister_chrdev_region(devobj->devno, 1);

	mt3332_gps_hw_exit(devobj->hw);

	err = mt3332_gps_delete_attr(devobj->dev);
	if (err)
		GPS_ERR("delete attr fails: %d\n", err);
	device_destroy(devobj->cls, devobj->devno);
	class_destroy(devobj->cls);
	GPS_DBG("Done\n");
	return 0;
}
/*****************************************************************************/
static void mt3332_gps_shutdown(struct platform_device *dev)
{
	struct gps_dev_obj *devobj = (struct gps_dev_obj *)platform_get_drvdata(dev);
	GPS_DBG("Shutting down\n");
	mt3332_gps_hw_exit(devobj->hw);
}
/*****************************************************************************/
#ifdef CONFIG_PM
/*****************************************************************************/
static int mt3332_gps_suspend(struct platform_device *dev, pm_message_t state)
{
	int err = 0;
	struct gps_dev_obj *devobj = (struct gps_dev_obj *)platform_get_drvdata(dev);
	struct gps_drv_obj *drvobj = (struct gps_drv_obj *)dev_get_drvdata(devobj->dev);

	if (!devobj || !drvobj) {
		GPS_ERR("null pointer: %p, %p\n", devobj, drvobj);
		return -1;
	}

	GPS_DBG("dev = %p, event = %u,", dev, state.event);
	if (state.event == PM_EVENT_SUSPEND) {
		err = mt3332_gps_dev_suspend(drvobj);
	}
	return err;
}
/*****************************************************************************/
static int mt3332_gps_resume(struct platform_device *dev)
{
	struct gps_dev_obj *devobj = (struct gps_dev_obj *)platform_get_drvdata(dev);
	struct gps_drv_obj *drvobj = (struct gps_drv_obj *)dev_get_drvdata(devobj->dev);

	GPS_DBG("");
	return mt3332_gps_dev_resume(drvobj);
}
/*****************************************************************************/
#endif /* CONFIG_PM */
/*****************************************************************************/

static struct platform_driver mt3332_gps_driver = {
	.probe		= mt3332_gps_probe,
	.remove		= mt3332_gps_remove,
	.shutdown	= mt3332_gps_shutdown,
#if defined(CONFIG_PM)
	.suspend	= mt3332_gps_suspend,
	.resume		= mt3332_gps_resume,
#endif
	.driver		= {
		.name = GPS_DEVNAME,
		.of_match_table = of_match_ptr(mt3332_of_match),
		.bus	= &platform_bus_type,
	},
};
/*****************************************************************************/
static int __init mt3332_gps_mod_init(void)
{
	int ret = 0;
	GPS_TRC();

	/*ret = driver_register(&mt3332_gps_driver);*/
	ret = platform_driver_register(&mt3332_gps_driver);
	GPS_DBG("platform_driver_register ret = %d\n", ret);
	return ret;
}


/*****************************************************************************/
/*void mtk_gps_register_test(void)
{
	mtk_gps_register();
}*/

/*****************************************************************************/
static void __exit mt3332_gps_mod_exit(void)
{
	GPS_TRC();
	platform_driver_unregister(&mt3332_gps_driver);
}
/*****************************************************************************/
module_init(mt3332_gps_mod_init);
module_exit(mt3332_gps_mod_exit);
/*****************************************************************************/
MODULE_DESCRIPTION("MT3332 GPS Driver");
MODULE_LICENSE("GPL");



