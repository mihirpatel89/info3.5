/*
 * imx132.c - imx132 sensor driver
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/regmap.h>
#include <media/nvc.h>
#include <media/imx132.h>
#include <media/camera_common.h>
#include "nvc_utilities.h"


struct imx132_info {
	struct miscdevice		miscdev_info;
	int				mode;
	struct imx132_power_rail	power;
	struct nvc_fuseid		fuse_id;
	struct i2c_client		*i2c_client;
	struct imx132_platform_data	*pdata;
	atomic_t			in_use;
	struct clk			*mclk;
	struct regmap 		*regmap;
};

#define IMX132_TABLE_WAIT_MS 0
#define IMX132_TABLE_END 1

#define IMX132_WAIT_MS 5
#define IMX132_FUSE_ID_SIZE 7
#define IMX132_FUSE_ID_DELAY 5

static struct regulator *imx132_ext_reg1;
static struct regulator *imx132_ext_reg2;

static const struct reg_8 mode_1976x1144_one_lane[] = {
	/* Stand by */
	{0x0100, 0x00},
	{0x0101, 0x00},
	{IMX132_TABLE_WAIT_MS, IMX132_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x308B, 0x5A},
	{0x3094, 0x11},
	{0x309D, 0xA4},
	{0x30AA, 0x01},
	{0x30C6, 0x00},
	{0x30C7, 0x00},
	{0x3118, 0x2F},
	{0x312A, 0x00},
	{0x312B, 0x0B},
	{0x312C, 0x0B},
	{0x312D, 0x13},

	/* PLL Setting */
	{0x0305, 0x02},
	{0x0307, 0x42},
	{0x30A4, 0x02},
	{0x303C, 0x4B},

	/* Mode Setting */
	{0x0340, 0x04},
	{0x0341, 0x92},
	{0x0342, 0x08},
	{0x0343, 0xC8},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x1C},
	{0x0348, 0x07},
	{0x0349, 0xB7},
	{0x034A, 0x04},
	{0x034B, 0x93},
	{0x034C, 0x07},
	{0x034D, 0xB8},
	{0x034E, 0x04},
	{0x034F, 0x78},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x303D, 0x10},
	{0x303E, 0x5A},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x2F},
	{0x304D, 0x02},
	{0x3064, 0x92},
	{0x306A, 0x10},
	{0x309B, 0x00},
	{0x309E, 0x41},
	{0x30A0, 0x10},
	{0x30A1, 0x0B},
	{0x30B2, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30D8, 0x00},
	{0x30D9, 0x00},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x00},
	{0x3102, 0x0C},
	{0x3103, 0x33},
	{0x3104, 0x18},
	{0x3105, 0x00},
	{0x3106, 0x65},
	{0x3107, 0x00},
	{0x3108, 0x06},
	{0x3109, 0x04},
	{0x310A, 0x04},
	{0x315C, 0x3D},
	{0x315D, 0x3C},
	{0x316E, 0x3E},
	{0x316F, 0x3D},
	{0x3301, 0x01},
	{0x3304, 0x07},
	{0x3305, 0x06},
	{0x3306, 0x19},
	{0x3307, 0x03},
	{0x3308, 0x0F},
	{0x3309, 0x07},
	{0x330A, 0x0C},
	{0x330B, 0x06},
	{0x330C, 0x0B},
	{0x330D, 0x07},
	{0x330E, 0x03},
	{0x3318, 0x61},
	{0x3322, 0x09},
	{0x3342, 0x00},
	{0x3348, 0xE0},

	/* Streaming */
	{0x0100, 0x01},
	{IMX132_TABLE_WAIT_MS, IMX132_WAIT_MS},
	{IMX132_TABLE_END, 0x00}
};

static const struct reg_8 mode_1976x1144_two_lane[] = {
	/* Stand by */
	{0x0100, 0x00},
	{0x0101, 0x00},
	{IMX132_TABLE_WAIT_MS, IMX132_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x308B, 0x5A},
	{0x3094, 0x11},
	{0x309D, 0xA4},
	{0x30AA, 0x01},
	{0x30C6, 0x00},
	{0x30C7, 0x00},
	{0x3118, 0x2F},
	{0x312A, 0x00},
	{0x312B, 0x0B},
	{0x312C, 0x0B},
	{0x312D, 0x13},

	/* PLL Setting */
	{0x0305, 0x02},
	{0x0307, 0x21},
	{0x30A4, 0x02},
	{0x303C, 0x4B},

	/* Mode Setting */
	{0x0340, 0x04},
	{0x0341, 0x92},
	{0x0342, 0x08},
	{0x0343, 0xC8},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x1C},
	{0x0348, 0x07},
	{0x0349, 0xB7},
	{0x034A, 0x04},
	{0x034B, 0x93},
	{0x034C, 0x07},
	{0x034D, 0xB8},
	{0x034E, 0x04},
	{0x034F, 0x78},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x303D, 0x10},
	{0x303E, 0x4A},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x2F},
	{0x304D, 0x02},
	{0x3064, 0x92},
	{0x306A, 0x10},
	{0x309B, 0x00},
	{0x309E, 0x41},
	{0x30A0, 0x10},
	{0x30A1, 0x0B},
	{0x30B2, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30D8, 0x00},
	{0x30D9, 0x00},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x00},
	{0x3102, 0x0C},
	{0x3103, 0x33},
	{0x3104, 0x30},
	{0x3105, 0x00},
	{0x3106, 0xCA},
	{0x3107, 0x00},
	{0x3108, 0x06},
	{0x3109, 0x04},
	{0x310A, 0x04},
	{0x315C, 0x3D},
	{0x315D, 0x3C},
	{0x316E, 0x3E},
	{0x316F, 0x3D},
	{0x3301, 0x00},
	{0x3304, 0x07},
	{0x3305, 0x06},
	{0x3306, 0x19},
	{0x3307, 0x03},
	{0x3308, 0x0F},
	{0x3309, 0x07},
	{0x330A, 0x0C},
	{0x330B, 0x06},
	{0x330C, 0x0B},
	{0x330D, 0x07},
	{0x330E, 0x03},
	{0x3318, 0x67},
	{0x3322, 0x09},
	{0x3342, 0x00},
	{0x3348, 0xE0},

	/* Streaming */
	{0x0100, 0x01},
	{IMX132_TABLE_WAIT_MS, IMX132_WAIT_MS},
	{IMX132_TABLE_END, 0x00}
};

enum {
	IMX132_MODE_1976X1144_ONE_LANE,
	IMX132_MODE_1976X1144_TWO_LANE,
};

static const struct reg_8 *mode_table[] = {
	[IMX132_MODE_1976X1144_ONE_LANE] = mode_1976x1144_one_lane,
	[IMX132_MODE_1976X1144_TWO_LANE] = mode_1976x1144_two_lane,
};

static inline void
imx132_get_frame_length_regs(struct reg_8 *regs, u32 frame_length)
{
	regs->addr = IMX132_FRAME_LEN_LINES_15_8;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = IMX132_FRAME_LEN_LINES_7_0;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void
imx132_get_coarse_time_regs(struct reg_8 *regs, u32 coarse_time)
{
	regs->addr = IMX132_COARSE_INTEGRATION_TIME_15_8;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX132_COARSE_INTEGRATION_TIME_7_0;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void
imx132_get_gain_reg(struct reg_8 *regs, u16 gain)
{
	regs->addr = IMX132_ANA_GAIN_GLOBAL;
	regs->val = gain;
}

static int
imx132_set_mode(struct imx132_info *info, struct imx132_mode *mode)
{
	struct device *dev = &info->i2c_client->dev;
	int sensor_mode;
	int err;
	struct reg_8 reg_list[5];

	dev_info(dev, "%s: res [%ux%ux%d] framelength %u coarsetime %u gain %u\n",
		__func__, mode->xres, mode->yres, info->pdata->cap->data_lanes,
		mode->frame_length, mode->coarse_time, mode->gain);

	if ((mode->xres == 1976) && (mode->yres == 1144) && (info->pdata->cap->data_lanes == 1)) {
		sensor_mode = IMX132_MODE_1976X1144_ONE_LANE;
	} else if ((mode->xres == 1976) && (mode->yres == 1144) && (info->pdata->cap->data_lanes == 2)) {
		sensor_mode = IMX132_MODE_1976X1144_TWO_LANE;
	} else {
		dev_err(dev, "%s: invalid resolution to set mode %d %d\n",
			__func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	/*
	 * get a list of override regs for the asking frame length,
	 * coarse integration time, and gain.
	 */
	imx132_get_frame_length_regs(reg_list, mode->frame_length);
	imx132_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	imx132_get_gain_reg(reg_list + 4, mode->gain);

	err = regmap_util_write_table_8(info->regmap,
					mode_table[sensor_mode],
					reg_list, 5,
					IMX132_TABLE_WAIT_MS,
					IMX132_TABLE_END);
	if (err)
		return err;

	info->mode = sensor_mode;
	dev_info(dev, "[imx132]: stream on.\n");
	return 0;
}

static int
imx132_get_status(struct imx132_info *info, u8 *dev_status)
{
	/* TBD */
	*dev_status = 0;
	return 0;
}

static int
imx132_set_frame_length(struct imx132_info *info,
				u32 frame_length,
				bool group_hold)
{
	struct reg_8 reg_list[2];
	int i = 0;
	int ret;

	imx132_get_frame_length_regs(reg_list, frame_length);

	if (group_hold) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD, 0x01);
		if (ret)
			return ret;
	}

	for (i = 0; i < NUM_OF_FRAME_LEN_REG; i++) {
		ret = regmap_write(info->regmap, reg_list[i].addr,
			reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int
imx132_set_coarse_time(struct imx132_info *info,
				u32 coarse_time,
				bool group_hold)
{
	int ret;

	struct reg_8 reg_list[2];
	int i = 0;

	imx132_get_coarse_time_regs(reg_list, coarse_time);

	if (group_hold) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD,
					0x01);
		if (ret)
			return ret;
	}

	for (i = 0; i < NUM_OF_COARSE_TIME_REG; i++) {
		ret = regmap_write(info->regmap, reg_list[i].addr,
			reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int
imx132_set_gain(struct imx132_info *info, u16 gain, bool group_hold)
{
	int ret;
	struct reg_8 reg_list;

	imx132_get_gain_reg(&reg_list, gain);

	if (group_hold) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD, 0x1);
		if (ret)
			return ret;
	}

	ret = regmap_write(info->regmap, reg_list.addr, reg_list.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int
imx132_set_group_hold(struct imx132_info *info, struct imx132_ae *ae)
{
	int ret;
	int count = 0;
	bool groupHoldEnabled = false;

	if (ae->gain_enable)
		count++;
	if (ae->coarse_time_enable)
		count++;
	if (ae->frame_length_enable)
		count++;
	if (count >= 2)
		groupHoldEnabled = true;

	if (groupHoldEnabled) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD, 0x1);
		if (ret)
			return ret;
	}

	if (ae->gain_enable)
		imx132_set_gain(info, ae->gain, false);
	if (ae->coarse_time_enable)
		imx132_set_coarse_time(info, ae->coarse_time, false);
	if (ae->frame_length_enable)
		imx132_set_frame_length(info, ae->frame_length, false);

	if (groupHoldEnabled) {
		ret = regmap_write(info->regmap,
					IMX132_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int imx132_get_fuse_id(struct imx132_info *info)
{
	int ret = 0;
	int i;
	u8 bak = 0;

	if (info->fuse_id.size)
		return 0;

	/*
	 * TBD 1: If the sensor does not have power at this point
	 * Need to supply the power, e.g. by calling power on function
	 */
	msleep_range(IMX132_FUSE_ID_DELAY);

	for (i = 0; i < IMX132_FUSE_ID_SIZE ; i++) {
		ret |= regmap_read(info->regmap,
					IMX132_FUSE_ID_REG + i, (unsigned int *) &bak);
		info->fuse_id.data[i] = bak;
	}

	if (!ret)
		info->fuse_id.size = i;

	return ret;
}

static long
imx132_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err;
	struct imx132_info *info = file->private_data;
	struct device *dev = &info->i2c_client->dev;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(IMX132_IOCTL_SET_MODE):
	{
		struct imx132_mode mode;
		if (copy_from_user(&mode,
			(const void __user *)arg,
			sizeof(struct imx132_mode))) {
			dev_err(dev, "%s:Failed to get mode from user.\n",
			__func__);
			return -EFAULT;
		}
		return imx132_set_mode(info, &mode);
	}
	case _IOC_NR(IMX132_IOCTL_SET_FRAME_LENGTH):
		return imx132_set_frame_length(info, (u32)arg, true);
	case _IOC_NR(IMX132_IOCTL_SET_COARSE_TIME):
		return imx132_set_coarse_time(info, (u32)arg, true);
	case _IOC_NR(IMX132_IOCTL_SET_GAIN):
		return imx132_set_gain(info, (u16)arg, true);
	case _IOC_NR(IMX132_IOCTL_GET_STATUS):
	{
		u8 status;

		err = imx132_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status, 1)) {
			dev_err(dev, "%s:Failed to copy status to user.\n",
			__func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(IMX132_IOCTL_GET_FUSEID):
	{
		err = imx132_get_fuse_id(info);

		if (err) {
			dev_err(dev, "%s:Failed to get fuse id info.\n",
			__func__);
			return err;
		}
		if (copy_to_user((void __user *)arg,
				&info->fuse_id,
				sizeof(struct nvc_fuseid))) {
			dev_err(dev, "%s:Fail copy fuse id to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(IMX132_IOCTL_SET_GROUP_HOLD):
	{
		struct imx132_ae ae;
		if (copy_from_user(&ae, (const void __user *)arg,
				sizeof(struct imx132_ae))) {
			dev_err(dev, "%s:fail group hold\n", __func__);
			return -EFAULT;
		}
		return imx132_set_group_hold(info, &ae);
	}

	case _IOC_NR(IMX132_IOCTL_GET_CAPS):
		if (copy_to_user((void __user *)arg,
				 info->pdata->cap,
				 sizeof(struct nvc_imager_cap))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		return 0;
	default:
		dev_err(dev, "%s:unknown cmd.\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int imx132_get_extra_regulators(void)
{
	if (!imx132_ext_reg1) {
		imx132_ext_reg1 = devm_regulator_get(NULL, "imx132_reg1");
		if (WARN_ON(IS_ERR(imx132_ext_reg1))) {
			pr_err("%s: can't get regulator imx132_reg1: %ld\n",
				__func__, PTR_ERR(imx132_ext_reg1));
			imx132_ext_reg1 = NULL;
			return -ENODEV;
		}
	}

	if (!imx132_ext_reg2) {
		imx132_ext_reg2 = devm_regulator_get(NULL, "imx132_reg2");
		if (unlikely(WARN_ON(IS_ERR(imx132_ext_reg2)))) {
			pr_err("%s: can't get regulator imx132_reg2: %ld\n",
				__func__, PTR_ERR(imx132_ext_reg2));
			imx132_ext_reg2 = NULL;
			regulator_put(imx132_ext_reg1);
			return -ENODEV;
		}
	}

	return 0;
}

static void imx132_mclk_disable(struct imx132_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int imx132_mclk_enable(struct imx132_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static int imx132_power_on(struct imx132_info *info)
{
	int err;
	struct imx132_power_rail *pw = &info->power;
	unsigned int cam2_gpio = info->pdata->cam2_gpio;
	unsigned int reset_gpio = info->pdata->reset_gpio;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd)))
		return -EFAULT;

	if (info->pdata->ext_reg) {
		if (imx132_get_extra_regulators())
			goto imx132_poweron_fail;

		err = regulator_enable(imx132_ext_reg1);
		if (unlikely(err))
			goto imx132_i2c_fail;

		err = regulator_enable(imx132_ext_reg2);
		if (unlikely(err))
			goto imx132_vcm_fail;
	}

	gpio_set_value(cam2_gpio, 0);
	gpio_set_value(reset_gpio, 0);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto imx132_avdd_fail;

	err = regulator_enable(pw->dvdd);
	if (unlikely(err))
		goto imx132_dvdd_fail;

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto imx132_iovdd_fail;

	usleep_range(10, 20);

	gpio_set_value(cam2_gpio, 1);
	gpio_set_value(reset_gpio, 1);

	return 0;

imx132_iovdd_fail:
	regulator_disable(pw->dvdd);

imx132_dvdd_fail:
	regulator_disable(pw->avdd);

imx132_avdd_fail:
	if (info->pdata->ext_reg)
		regulator_disable(imx132_ext_reg2);

imx132_vcm_fail:
	if (info->pdata->ext_reg)
		regulator_disable(imx132_ext_reg1);

imx132_i2c_fail:
imx132_poweron_fail:
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int imx132_power_off(struct imx132_info *info)
{
	struct imx132_power_rail *pw = &info->power;
	unsigned int cam2_gpio = info->pdata->cam2_gpio;
	unsigned int reset_gpio = info->pdata->reset_gpio;

	if (!info->i2c_client->dev.of_node) {
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->power);
		goto imx132_pwroff_end;
	}

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd)))
		return -EFAULT;

	gpio_set_value(cam2_gpio, 0);
	gpio_set_value(reset_gpio, 0);

	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);
	regulator_disable(pw->avdd);

	if (info->pdata->ext_reg) {
		regulator_disable(imx132_ext_reg1);
		regulator_disable(imx132_ext_reg2);
	}

imx132_pwroff_end:
	return 0;
}

static int
imx132_open(struct inode *inode, struct file *file)
{
	int err;
	struct miscdevice	*miscdev = file->private_data;
	struct imx132_info	*info;

	info = container_of(miscdev, struct imx132_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		dev_err(&info->i2c_client->dev, "%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	err = imx132_mclk_enable(info);
	if (err < 0)
		return err;

	if (info->i2c_client->dev.of_node) {
		err = imx132_power_on(info);
	} else {
		if (info->pdata && info->pdata->power_on)
			err = info->pdata->power_on(&info->power);
		else {
			dev_err(&info->i2c_client->dev,
				"%s:no valid power_on function.\n", __func__);
			err = -EEXIST;
		}
	}
	if (err < 0)
		goto imx132_open_fail;

	return 0;

imx132_open_fail:
	imx132_mclk_disable(info);
	return err;
}

static int
imx132_release(struct inode *inode, struct file *file)
{
	struct imx132_info *info = file->private_data;

	imx132_power_off(info);

	imx132_mclk_disable(info);

	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int imx132_power_put(struct imx132_power_rail *pw)
{
	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	if (likely(imx132_ext_reg1))
		regulator_put(imx132_ext_reg1);

	if (likely(imx132_ext_reg2))
		regulator_put(imx132_ext_reg2);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;
	imx132_ext_reg1 = NULL;
	imx132_ext_reg2 = NULL;

	return 0;
}

static int imx132_regulator_get(struct imx132_info *info,
	struct regulator **vreg, const char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = devm_regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %p\n",
			__func__, vreg_name, reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int imx132_power_get(struct imx132_info *info)
{
	struct imx132_power_rail *pw = &info->power;

	imx132_regulator_get(info, &pw->dvdd, info->pdata->regulators.dvdd); /* digital 1.2v */
	imx132_regulator_get(info, &pw->avdd, info->pdata->regulators.avdd); /* analog 2.7v */
	imx132_regulator_get(info, &pw->iovdd, info->pdata->regulators.iovdd); /* interface 1.8v */

	return 0;
}

static const struct file_operations imx132_fileops = {
	.owner = THIS_MODULE,
	.open = imx132_open,
	.unlocked_ioctl = imx132_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = imx132_ioctl,
#endif
	.release = imx132_release,
};

static struct miscdevice imx132_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "imx132",
	.fops = &imx132_fileops,
};

static struct of_device_id imx132_of_match[] = {
	{ .compatible = "nvidia,imx132", },
	{ },
};

MODULE_DEVICE_TABLE(of, imx132_of_match);

static struct imx132_platform_data *imx132_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct imx132_platform_data *board_info_pdata;
	const char *sname;
	int ret;
	int num;

	board_info_pdata = devm_kzalloc(&client->dev, sizeof(*board_info_pdata)
			+ sizeof(*board_info_pdata->cap) + sizeof(*board_info_pdata->static_info),
			GFP_KERNEL);
	if (!board_info_pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return NULL;
	}

	board_info_pdata->cap = (void *)(board_info_pdata + 1);
	board_info_pdata->static_info = (void *)(board_info_pdata->cap + 1);

	board_info_pdata->cam2_gpio = of_get_named_gpio(np, "cam2-gpios", 0);
	board_info_pdata->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	board_info_pdata->ext_reg = of_property_read_bool(np, "nvidia,ext_reg");
	of_property_read_string(np, "clocks", &board_info_pdata->mclk_name);

	num = 0;
	do {
		ret = of_property_read_string_index(
			np, "regulators", num, &sname);
		if (ret < 0)
			break;
		switch (num) {
		case 0:
			board_info_pdata->regulators.avdd = sname;
			board_info_pdata->regulators.dvdd = NULL;
			board_info_pdata->regulators.iovdd = NULL;
			break;
		case 1:
			board_info_pdata->regulators.dvdd = sname;
			break;
		case 2:
			board_info_pdata->regulators.iovdd = sname;
			break;
		default:
			break;
		}
		num++;
	} while (num < 3);

	nvc_imager_parse_caps(np, board_info_pdata->cap, board_info_pdata->static_info);

	return board_info_pdata;
}

static int
imx132_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct imx132_info *info;
	const char *mclk_name;
	int err = 0;
	static struct regmap_config imx132_regmap_config = {
		.reg_bits = 16,
		.val_bits = 8,
	};

	pr_info("[imx132]: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
		sizeof(struct imx132_info), GFP_KERNEL);
	if (!info) {
		pr_err("[imx132]:%s:Unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	if (client->dev.of_node)
		info->pdata = imx132_parse_dt(client);
	else
		info->pdata = client->dev.platform_data;

	if (!info->pdata) {
		pr_err("[imx132]:%s:Unable to get platform data\n", __func__);
		return -EFAULT;
	}

	info->regmap = devm_regmap_init_i2c(client, &imx132_regmap_config);
	if (IS_ERR(info->regmap)) {
		err = PTR_ERR(info->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", err);
		return err;
	}

	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = -1;

	mclk_name = info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	i2c_set_clientdata(client, info);

	imx132_power_get(info);

	memcpy(&info->miscdev_info,
		&imx132_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		imx132_power_put(&info->power);
		pr_err("[imx132]:%s:Unable to register misc device!\n",
		__func__);
	}

	return err;
}

static int
imx132_remove(struct i2c_client *client)
{
	struct imx132_info *info = i2c_get_clientdata(client);
	imx132_power_put(&info->power);
	misc_deregister(&imx132_device);
	return 0;
}

static const struct i2c_device_id imx132_id[] = {
	{ "imx132", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, imx132_id);

static struct i2c_driver imx132_i2c_driver = {
	.driver = {
		.name = "imx132",
		.owner = THIS_MODULE,
		.of_match_table = imx132_of_match,
	},
	.probe = imx132_probe,
	.remove = imx132_remove,
	.id_table = imx132_id,
};

static int __init
imx132_init(void)
{
	pr_info("[imx132] sensor driver loading\n");
	return i2c_add_driver(&imx132_i2c_driver);
}

static void __exit
imx132_exit(void)
{
	i2c_del_driver(&imx132_i2c_driver);
}

module_init(imx132_init);
module_exit(imx132_exit);
