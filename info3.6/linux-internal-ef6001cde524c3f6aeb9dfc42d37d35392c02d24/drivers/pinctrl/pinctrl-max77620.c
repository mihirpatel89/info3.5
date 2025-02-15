/*
 * MAX77620 pin control driver.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Chaitanya Bandi <bandik@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/max77620.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include "core.h"
#include "pinconf.h"
#include "pinctrl-utils.h"

#define MAX77620_PIN_NUM 8

enum max77620_pin_ppdrv {
	MAX77620_PIN_UNCONFIG_DRV,
	MAX77620_PIN_OD_DRV,
	MAX77620_PIN_PP_DRV,
};

enum max77620_pinconf_param {
	MAX77620_FPS_SOURCE = PIN_CONFIG_END + 1,
	MAX77620_FPS_POWER_ON_PERIOD,
	MAX77620_FPS_POWER_OFF_PERIOD,
	MAX77620_SUSPEND_FPS_SOURCE,
	MAX77620_SUSPEND_FPS_POWER_ON_PERIOD,
	MAX77620_SUSPEND_FPS_POWER_OFF_PERIOD,
	MAX77620_POWER_OK,
};

struct max77620_pin_function {
	const char *name;
	const char * const *groups;
	unsigned ngroups;
	int mux_option;
};

struct max77620_cfg_param {
	const char *property;
	enum max77620_pinconf_param param;
};

static const struct max77620_cfg_param  max77620_cfg_params[] = {
	{
		.property = "maxim,active-fps-source",
		.param = MAX77620_FPS_SOURCE,
	}, {
		.property = "maxim,active-fps-power-up-period",
		.param = MAX77620_FPS_POWER_ON_PERIOD,
	}, {
		.property = "maxim,active-fps-power-up-slot",
		.param = MAX77620_FPS_POWER_ON_PERIOD,
	}, {
		.property = "maxim,active-fps-power-down-period",
		.param = MAX77620_FPS_POWER_OFF_PERIOD,
	}, {
		.property = "maxim,active-fps-power-down-slot",
		.param = MAX77620_FPS_POWER_OFF_PERIOD,
	}, {
		.property = "maxim,suspend-fps-source",
		.param = MAX77620_SUSPEND_FPS_SOURCE,
	}, {
		.property = "maxim,suspend-fps-power-down-slot",
		.param = MAX77620_SUSPEND_FPS_POWER_OFF_PERIOD,
	}, {
		.property = "maxim,suspend-fps-power-up-slot",
		.param = MAX77620_SUSPEND_FPS_POWER_ON_PERIOD,
	}, {
		.property = "maxim,power-ok",
		.param = MAX77620_POWER_OK,
	},
};

enum max77620_alternate_pinmux_option {
	MAX77620_PINMUX_GPIO				= 0,
	MAX77620_PINMUX_LOW_POWER_MODE_CONTROL_IN	= 1,
	MAX77620_PINMUX_FLEXIBLE_POWER_SEQUENCER_OUT	= 2,
	MAX77620_PINMUX_32K_OUT1			= 3,
	MAX77620_PINMUX_SD0_DYNAMIC_VOLTAGE_SCALING_IN	= 4,
	MAX77620_PINMUX_SD1_DYNAMIC_VOLTAGE_SCALING_IN	= 5,
	MAX77620_PINMUX_REFERENCE_OUT			= 6,
	MAX77620_PINMUX_POWER_OK			= 7,
};

struct max77620_pingroup {
	const char *name;
	const unsigned pins[1];
	unsigned npins;
	enum max77620_alternate_pinmux_option alt_option;
};

struct max77620_pin_info {
	enum max77620_pin_ppdrv drv_type;
	int pull_config;
};

struct max77620_fps_config {
	int active_fps_src;
	int active_power_up_slots;
	int active_power_down_slots;
	int suspend_fps_src;
	int suspend_power_up_slots;
	int suspend_power_down_slots;
};

struct max77620_pctrl_info {
	struct device *dev;
	struct pinctrl_dev *pctl;
	struct max77620_chip *max77620;
	int pins_current_opt[MAX77620_GPIO_NR];
	const struct max77620_pin_function *functions;
	unsigned num_functions;
	const struct max77620_pingroup *pin_groups;
	int num_pin_groups;
	const struct pinctrl_pin_desc *pins;
	unsigned num_pins;
	struct max77620_pin_info pin_info[MAX77620_PIN_NUM];
	struct max77620_fps_config fps_config[MAX77620_PIN_NUM];
};

static const struct pinctrl_pin_desc max77620_pins_desc[] = {
	PINCTRL_PIN(MAX77620_GPIO0, "gpio0"),
	PINCTRL_PIN(MAX77620_GPIO1, "gpio1"),
	PINCTRL_PIN(MAX77620_GPIO2, "gpio2"),
	PINCTRL_PIN(MAX77620_GPIO3, "gpio3"),
	PINCTRL_PIN(MAX77620_GPIO4, "gpio4"),
	PINCTRL_PIN(MAX77620_GPIO5, "gpio5"),
	PINCTRL_PIN(MAX77620_GPIO6, "gpio6"),
	PINCTRL_PIN(MAX77620_GPIO7, "gpio7"),
	PINCTRL_PIN(MAX77620_NRSTIO, "nrstio"),
};

static const char * const gpio_groups[] = {
	"gpio0",
	"gpio1",
	"gpio2",
	"gpio3",
	"gpio4",
	"gpio5",
	"gpio6",
	"gpio7",
	"nrstio",
};

#define FUNCTION_GROUP(fname, mux)			\
	{						\
		.name = #fname,				\
		.groups = gpio_groups,			\
		.ngroups = ARRAY_SIZE(gpio_groups),	\
		.mux_option = MAX77620_PINMUX_##mux,	\
	}

static const struct max77620_pin_function max77620_pin_function[] = {
	FUNCTION_GROUP(gpio, GPIO),
	FUNCTION_GROUP(lpm-control-in, LOW_POWER_MODE_CONTROL_IN),
	FUNCTION_GROUP(fps-out, FLEXIBLE_POWER_SEQUENCER_OUT),
	FUNCTION_GROUP(32k-out1, 32K_OUT1),
	FUNCTION_GROUP(sd0-dvs-in, SD0_DYNAMIC_VOLTAGE_SCALING_IN),
	FUNCTION_GROUP(sd1-dvs-in, SD1_DYNAMIC_VOLTAGE_SCALING_IN),
	FUNCTION_GROUP(reference-out, REFERENCE_OUT),
	FUNCTION_GROUP(power-ok, POWER_OK),
};

#define MAX77620_PINGROUP(pg_name, pin_id, option) \
	{								\
		.name = #pg_name,					\
		.pins = {MAX77620_##pin_id},				\
		.npins = 1,						\
		.alt_option = MAX77620_PINMUX_##option,			\
	}

static const struct max77620_pingroup max77620_pingroups[] = {
	MAX77620_PINGROUP(gpio0,	GPIO0,	LOW_POWER_MODE_CONTROL_IN),
	MAX77620_PINGROUP(gpio1,	GPIO1,	FLEXIBLE_POWER_SEQUENCER_OUT),
	MAX77620_PINGROUP(gpio2,	GPIO2,	FLEXIBLE_POWER_SEQUENCER_OUT),
	MAX77620_PINGROUP(gpio3,	GPIO3,	FLEXIBLE_POWER_SEQUENCER_OUT),
	MAX77620_PINGROUP(gpio4,	GPIO4,	32K_OUT1),
	MAX77620_PINGROUP(gpio5,	GPIO5,	SD0_DYNAMIC_VOLTAGE_SCALING_IN),
	MAX77620_PINGROUP(gpio6,	GPIO6,	SD1_DYNAMIC_VOLTAGE_SCALING_IN),
	MAX77620_PINGROUP(gpio7,	GPIO7,	REFERENCE_OUT),
	MAX77620_PINGROUP(nrstio,	NRSTIO,	POWER_OK),
};

static int max77620_pinctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	return max77620_pci->num_pin_groups;
}

static const char *max77620_pinctrl_get_group_name(struct pinctrl_dev *pctldev,
		unsigned group)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	return max77620_pci->pin_groups[group].name;
}

static int max77620_pinctrl_get_group_pins(struct pinctrl_dev *pctldev,
		unsigned group, const unsigned **pins, unsigned *num_pins)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	*pins = max77620_pci->pin_groups[group].pins;
	*num_pins = max77620_pci->pin_groups[group].npins;
	return 0;
}
static int max77620_pinctrl_max_cfg(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max77620_cfg_params);
}

static int max77620_pinctrl_parse_dt_config(struct pinctrl_dev *pctldev,
	struct device_node *np, unsigned long *configs, unsigned int *nconfigs)
{
	int max_cfg = max77620_pinctrl_max_cfg(pctldev);
	int i;
	int ncfg = 0;
	u32 val;
	int ret;
	unsigned long *cfg = configs;

	for (i = 0; i < max_cfg; i++) {
		const struct max77620_cfg_param *par = &max77620_cfg_params[i];
		ret = of_property_read_u32(np, par->property, &val);

		/* property not found */
		if (ret == -EINVAL)
			continue;
		if (ret)
			return -EINVAL;

		pr_debug("found %s with value %u\n", par->property, val);
		cfg[ncfg] = pinconf_to_config_packed(
				(enum pin_config_param)par->param, val);
		ncfg++;
	}

	*nconfigs = ncfg;
	return 0;
}

static const struct pinctrl_ops max77620_pinctrl_ops = {
	.get_groups_count = max77620_pinctrl_get_groups_count,
	.get_group_name = max77620_pinctrl_get_group_name,
	.get_group_pins = max77620_pinctrl_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinctrl_utils_dt_free_map,
	.dt_node_to_custom_config = max77620_pinctrl_parse_dt_config,
};

static int max77620_pinctrl_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	return max77620_pci->num_functions;
}

static const char *max77620_pinctrl_get_func_name(struct pinctrl_dev *pctldev,
			unsigned function)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	return max77620_pci->functions[function].name;
}

static int max77620_pinctrl_get_func_groups(struct pinctrl_dev *pctldev,
		unsigned function, const char * const **groups,
		unsigned * const num_groups)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	*groups = max77620_pci->functions[function].groups;
	*num_groups = max77620_pci->functions[function].ngroups;
	return 0;
}

static int max77620_pinctrl_enable(struct pinctrl_dev *pctldev,
		unsigned function, unsigned group)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	int ret;

	if (function == MAX77620_PINMUX_GPIO) {
		ret = max77620_reg_update(max77620_pci->max77620->dev,
			MAX77620_PWR_SLAVE, MAX77620_REG_AME_GPIO,
			1 << group, 0);
	} else if (function == max77620_pci->pin_groups[group].alt_option) {
		ret = max77620_reg_update(max77620_pci->max77620->dev,
			MAX77620_PWR_SLAVE, MAX77620_REG_AME_GPIO,
			1 << group, 1 << group);
	} else {
		dev_err(max77620_pci->dev, "%s(): GPIO %u doesn't have %u\n",
		__func__, group, function);
		return -EINVAL;
	}
	return ret;
}

static const struct pinmux_ops max77620_pinmux_ops = {
	.get_functions_count	= max77620_pinctrl_get_funcs_count,
	.get_function_name	= max77620_pinctrl_get_func_name,
	.get_function_groups	= max77620_pinctrl_get_func_groups,
	.set_mux		= max77620_pinctrl_enable,
};

static int max77620_pinconf_get(struct pinctrl_dev *pctldev,
			unsigned pin, unsigned long *config)
{
	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	enum pin_config_param param = pinconf_to_config_param(*config);
	u8 val;
	int arg = 0;
	int ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		if (max77620_pci->pin_info[pin].drv_type == MAX77620_PIN_OD_DRV)
			arg = 1;
		break;

	case PIN_CONFIG_DRIVE_PUSH_PULL:
		if (max77620_pci->pin_info[pin].drv_type == MAX77620_PIN_PP_DRV)
			arg = 1;
		break;

	case PIN_CONFIG_BIAS_PULL_UP:
		ret = max77620_reg_read(max77620_pci->max77620->dev,
			MAX77620_PWR_SLAVE, MAX77620_REG_PUE_GPIO, &val);
		if (ret < 0) {
			dev_err(max77620_pci->dev,
				"Reg PUE_GPIO read failed: %d\n", ret);
			return ret;
		}
		if (val & BIT(pin))
			arg = 1;
		break;

	case PIN_CONFIG_BIAS_PULL_DOWN:
		ret = max77620_reg_read(max77620_pci->max77620->dev,
			MAX77620_PWR_SLAVE, MAX77620_REG_PDE_GPIO, &val);
		if (ret < 0) {
			dev_err(max77620_pci->dev,
				"Reg PDE_GPIO read failed: %d\n", ret);
			return ret;
		}
		if (val & BIT(pin))
			arg = 1;
		break;

	default:
		dev_err(max77620_pci->dev, "Properties not supported\n");
		return -ENOTSUPP;
	}

	*config = pinconf_to_config_packed(param, (u16)arg);
	return 0;
}

static int max77620_get_default_fps(struct max77620_pctrl_info *max77620_pci,
	int addr, int *fps)
{
	u8 val;
	int ret;

	ret = max77620_reg_read(max77620_pci->max77620->dev,
			MAX77620_PWR_SLAVE, addr, &val);
	if (ret < 0) {
		dev_err(max77620_pci->dev, "Reg PUE_GPIO read failed: %d\n", ret);
		return ret;
	}
	val = (val & MAX77620_FPS_SRC_MASK) >> MAX77620_FPS_SRC_SHIFT;
	*fps = val;
	return 0;
}

static int max77620_set_fps_param(struct max77620_pctrl_info *max77620_pci,
	int pin, int param)
{
	struct max77620_fps_config *fps_config = &max77620_pci->fps_config[pin];
	int addr, ret;
	int param_val;
	int mask, shift;

	if ((pin < MAX77620_GPIO1) || (pin > MAX77620_GPIO3))
		return 0;

	addr = MAX77620_REG_FPS_GPIO1 + pin - 1;
	switch (param) {
	case MAX77620_FPS_SOURCE:
	case MAX77620_SUSPEND_FPS_SOURCE:
		mask = MAX77620_FPS_SRC_MASK;
		shift = MAX77620_FPS_SRC_SHIFT;
		param_val = fps_config->active_fps_src;
		if (param == MAX77620_SUSPEND_FPS_SOURCE)
			 param_val = fps_config->suspend_fps_src;
		break;

	case MAX77620_FPS_POWER_ON_PERIOD:
	case MAX77620_SUSPEND_FPS_POWER_ON_PERIOD:
		mask = MAX77620_FPS_PU_PERIOD_MASK;
		shift = MAX77620_FPS_PU_PERIOD_SHIFT;
		param_val = fps_config->active_power_up_slots;
		if (param == MAX77620_SUSPEND_FPS_POWER_ON_PERIOD)
			param_val = fps_config->suspend_power_up_slots;
		break;

	case MAX77620_FPS_POWER_OFF_PERIOD:
	case MAX77620_SUSPEND_FPS_POWER_OFF_PERIOD:
		mask = MAX77620_FPS_PD_PERIOD_MASK;
		shift = MAX77620_FPS_PD_PERIOD_SHIFT;
		param_val = fps_config->active_power_down_slots;
		if (param == MAX77620_SUSPEND_FPS_POWER_OFF_PERIOD)
			param_val = fps_config->suspend_power_down_slots;
		break;

	default:
		return -EINVAL;
	};

	if (param_val < 0)
		return 0;

	ret = max77620_reg_update(max77620_pci->max77620->dev,
		MAX77620_PWR_SLAVE, addr, mask, param_val << shift);
	if (ret < 0) {
		dev_err(max77620_pci->dev,
			"Reg 0x%02x update failed %d\n", addr, ret);
		return ret;
	}
	return 0;
}

static int max77620_pinconf_set(struct pinctrl_dev *pctldev,
		unsigned pin, unsigned long *configs,
		unsigned num_configs)
{

	struct max77620_pctrl_info *max77620_pci =
					pinctrl_dev_get_drvdata(pctldev);
	struct max77620_fps_config *fps_config;
	int param;
	u16 param_val;
	unsigned int val;
	unsigned int pu_val;
	unsigned int pd_val;
	unsigned int reg_val = 0, reg_mask = 0;
	int addr, ret;
	int i;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		param_val = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case PIN_CONFIG_DRIVE_OPEN_DRAIN:
			val = param_val ? 0 : 1;
			ret = max77620_reg_update(max77620_pci->max77620->dev,
				MAX77620_PWR_SLAVE, MAX77620_REG_GPIO0 + pin,
				MAX77620_CNFG_GPIO_DRV_MASK, val);
			if (ret < 0) {
				dev_err(max77620_pci->dev,
					"Reg GPIO%d update failed: %d\n",
					pin, ret);
				return ret;
			}

			max77620_pci->pin_info[pin].drv_type = val ?
				MAX77620_PIN_PP_DRV : MAX77620_PIN_OD_DRV;
			break;

		case PIN_CONFIG_DRIVE_PUSH_PULL:
			val = param_val ? 1 : 0;
			ret = max77620_reg_update(max77620_pci->max77620->dev,
				MAX77620_PWR_SLAVE, MAX77620_REG_GPIO0 + pin,
				MAX77620_CNFG_GPIO_DRV_MASK, val);
			if (ret < 0) {
				dev_err(max77620_pci->dev,
					"Reg GPIO%d update failed: %d\n",
					pin, ret);
				return ret;
			}

			max77620_pci->pin_info[pin].drv_type = val ?
				MAX77620_PIN_PP_DRV : MAX77620_PIN_OD_DRV;
			break;

		case MAX77620_FPS_SOURCE:
		case MAX77620_FPS_POWER_ON_PERIOD:
		case MAX77620_FPS_POWER_OFF_PERIOD:
			if ((pin < MAX77620_GPIO1) || (pin > MAX77620_GPIO3))
				return -EINVAL;

			fps_config = &max77620_pci->fps_config[pin];

			if ((param == MAX77620_FPS_SOURCE) &&
				(param_val == FPS_SRC_DEF)) {
				addr = MAX77620_REG_FPS_GPIO1 + pin - 1;
				ret = max77620_get_default_fps(max77620_pci,
					addr, &fps_config->active_fps_src);
				if (ret < 0)
					return ret;
				break;
			}

			if (param == MAX77620_FPS_SOURCE)
				fps_config->active_fps_src = param_val;
			else if (param == MAX77620_FPS_POWER_ON_PERIOD)
				fps_config->active_power_up_slots = param_val;
			else
				fps_config->active_power_down_slots = param_val;

			ret = max77620_set_fps_param(max77620_pci, pin, param);
			if (ret < 0) {
				dev_err(max77620_pci->dev,
					"Pin-%d Param %d config failed, %d\n",
					pin, param, ret);
				return ret;
			}
			break;

		case MAX77620_SUSPEND_FPS_SOURCE:
		case MAX77620_SUSPEND_FPS_POWER_ON_PERIOD:
		case MAX77620_SUSPEND_FPS_POWER_OFF_PERIOD:
			if ((pin < MAX77620_GPIO1) || (pin > MAX77620_GPIO3))
				return -EINVAL;

			fps_config = &max77620_pci->fps_config[pin];

			if ((param == MAX77620_SUSPEND_FPS_SOURCE) &&
				(param_val == FPS_SRC_DEF)) {
				addr = MAX77620_REG_FPS_GPIO1 + pin - 1;
				ret = max77620_get_default_fps(max77620_pci,
						addr, &fps_config->suspend_fps_src);
				if (ret < 0)
					return ret;
				break;
			}

			if (param == MAX77620_SUSPEND_FPS_SOURCE)
				fps_config->suspend_fps_src = param_val;
			else if (param == MAX77620_SUSPEND_FPS_POWER_ON_PERIOD)
				fps_config->suspend_power_up_slots = param_val;
			else
				fps_config->suspend_power_down_slots = param_val;
			break;

		case PIN_CONFIG_BIAS_PULL_UP:
		case PIN_CONFIG_BIAS_PULL_DOWN:
			pu_val = (param == PIN_CONFIG_BIAS_PULL_UP) ? BIT(pin) : 0;
			pd_val = (param == PIN_CONFIG_BIAS_PULL_DOWN) ? BIT(pin) : 0;

			ret = max77620_reg_update(max77620_pci->max77620->dev,
				MAX77620_PWR_SLAVE, MAX77620_REG_PUE_GPIO, BIT(pin),
				pu_val);
			if (ret < 0) {
				dev_err(max77620_pci->dev,
					"Reg PUE_GPIO update failed: %d\n", ret);
				return ret;
			}

			ret = max77620_reg_update(max77620_pci->max77620->dev,
				MAX77620_PWR_SLAVE, MAX77620_REG_PDE_GPIO, BIT(pin),
				pd_val);
			if (ret < 0) {
				dev_err(max77620_pci->dev,
					"Reg PDE_GPIO update failed: %d\n", ret);
				return ret;
			}
			break;

		case MAX77620_POWER_OK:
			if (max77620_pci->max77620->id != MAX20024)
				break;

			if (pin == MAX77620_NRSTIO) {
				reg_mask = MAX20024_ONOFFCNFG1_POK_BLOCK;
				if (param_val)
					reg_val = MAX20024_ONOFFCNFG1_POK_BLOCK;
				else
					reg_val = 0;
			} else if (pin == MAX77620_GPIO1) {
				reg_mask = MAX20024_ONOFFCNFG1_AME_GPIO;
				if (param_val)
					reg_val = MAX20024_ONOFFCNFG1_AME_GPIO;
				else
					reg_val = 0;
			}

			ret = max77620_reg_update(max77620_pci->max77620->dev,
					MAX77620_PWR_SLAVE,
					MAX77620_REG_ONOFFCNFG1, reg_mask, reg_val);
			if (ret < 0) {
				dev_err(max77620_pci->dev,
					"Reg 0x%02x update failed, %d\n",
					MAX77620_REG_ONOFFCNFG1, reg_val);
				return ret;
			}
			break;

		default:
			dev_err(max77620_pci->dev, "Properties not supported\n");
			return -ENOTSUPP;

		}
	}

	return 0;
}

static const struct pinconf_ops max77620_pinconf_ops = {
	.pin_config_get = max77620_pinconf_get,
	.pin_config_set = max77620_pinconf_set,
	.pin_config_get_max_custom_config = max77620_pinctrl_max_cfg,
};

static struct pinctrl_desc max77620_pinctrl_desc = {
	.pctlops = &max77620_pinctrl_ops,
	.pmxops = &max77620_pinmux_ops,
	.confops = &max77620_pinconf_ops,
	.owner = THIS_MODULE,
};

static int max77620_pinctrl_probe(struct platform_device *pdev)
{
	struct max77620_pctrl_info *max77620_pci;
	struct max77620_chip *max77620 = dev_get_drvdata(pdev->dev.parent);
	int i;

	max77620_pci = devm_kzalloc(&pdev->dev,
					sizeof(*max77620_pci), GFP_KERNEL);
	if (!max77620_pci) {
		dev_err(&pdev->dev, "Couldn't allocate mem\n");
		return -ENOMEM;
	}

	max77620_pci->dev = &pdev->dev;
	max77620_pci->dev->of_node = pdev->dev.parent->of_node;
	max77620_pci->max77620 = max77620;

	max77620_pci->pins = max77620_pins_desc;
	max77620_pci->num_pins = ARRAY_SIZE(max77620_pins_desc);
	max77620_pci->functions = max77620_pin_function;
	max77620_pci->num_functions = ARRAY_SIZE(max77620_pin_function);
	max77620_pci->pin_groups = max77620_pingroups;
	max77620_pci->num_pin_groups = ARRAY_SIZE(max77620_pingroups);
	platform_set_drvdata(pdev, max77620_pci);

	max77620_pinctrl_desc.name = dev_name(&pdev->dev);
	max77620_pinctrl_desc.pins = max77620_pins_desc;
	max77620_pinctrl_desc.npins = ARRAY_SIZE(max77620_pins_desc);

	for (i = 0; i < MAX77620_PIN_NUM; ++i) {
		max77620_pci->fps_config[i].active_fps_src = -1;
		max77620_pci->fps_config[i].active_power_up_slots = -1;
		max77620_pci->fps_config[i].active_power_down_slots = -1;
		max77620_pci->fps_config[i].suspend_fps_src = -1;
		max77620_pci->fps_config[i].suspend_power_up_slots = -1;
		max77620_pci->fps_config[i].suspend_power_down_slots = -1;
	}

	max77620_pci->pctl = pinctrl_register(&max77620_pinctrl_desc,
					&pdev->dev, max77620_pci);
	if (!max77620_pci->pctl) {
		dev_err(&pdev->dev, "Couldn't register pinctrl driver\n");
		return -EINVAL;
	}

	return 0;
}

static int max77620_pinctrl_remove(struct platform_device *pdev)
{
	struct max77620_pctrl_info *max77620_pci = platform_get_drvdata(pdev);

	pinctrl_unregister(max77620_pci->pctl);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77620_pinctrl_suspend(struct device *dev)
{
	struct max77620_pctrl_info *max77620_pci = dev_get_drvdata(dev);
	int pin, p;
	int ret;
	int param[] = {MAX77620_SUSPEND_FPS_SOURCE,
			MAX77620_SUSPEND_FPS_POWER_ON_PERIOD,
			MAX77620_SUSPEND_FPS_POWER_ON_PERIOD};

	for (pin = 0; pin < MAX77620_PIN_NUM; ++pin) {
		if ((pin < MAX77620_GPIO1) || (pin > MAX77620_GPIO3))
			continue;
		for (p = 0; p < 3; ++p) {
			ret = max77620_set_fps_param(max77620_pci,
					pin, param[p]);
			if (ret < 0)
				dev_err(dev, "Pin-%d config %d failed, %d\n",
					pin, param[p], ret);
		}
	}
	return 0;
};

static int max77620_pinctrl_resume(struct device *dev)
{
	struct max77620_pctrl_info *max77620_pci = dev_get_drvdata(dev);
	int pin, p;
	int ret;
	int param[] = {MAX77620_FPS_SOURCE,
			MAX77620_FPS_POWER_ON_PERIOD,
			MAX77620_FPS_POWER_OFF_PERIOD};

	for (pin = 0; pin < MAX77620_PIN_NUM; ++pin) {
		if ((pin < MAX77620_GPIO1) || (pin > MAX77620_GPIO3))
			continue;
		for (p = 0; p < 3; ++p) {
			ret = max77620_set_fps_param(max77620_pci, pin, param[p]);
			if (ret < 0)
				dev_err(dev, "Pin-%d config %d failed, %d\n",
					pin, param[p], ret);
		}
	}
	return 0;
}
#endif

static const struct dev_pm_ops max77620_pinctrl_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77620_pinctrl_suspend,
			max77620_pinctrl_resume)
};

static struct platform_device_id max77620_pinctrl_devtype[] = {
	{
		.name = "max77620-pinctrl",
	},
	{
		.name = "max20024-pinctrl",
	},
};

static struct platform_driver max77620_pinctrl_driver = {
	.driver = {
		.name = "max77620-pinctrl",
		.owner = THIS_MODULE,
		.pm = &max77620_pinctrl_pm_ops,
	},
	.probe = max77620_pinctrl_probe,
	.remove = max77620_pinctrl_remove,
	.id_table = max77620_pinctrl_devtype,
};

static int __init max77620_pinctrl_init(void)
{
	return platform_driver_register(&max77620_pinctrl_driver);
}
subsys_initcall(max77620_pinctrl_init);

static void __exit max77620_pinctrl_exit(void)
{
	platform_driver_unregister(&max77620_pinctrl_driver);
}
module_exit(max77620_pinctrl_exit);

MODULE_ALIAS("platform:max77620-pinctrl");
MODULE_DESCRIPTION("max77620 pin control driver");
MODULE_AUTHOR("Chaitanya Bandi<bandik@nvidia.com>");
MODULE_LICENSE("GPL v2");
