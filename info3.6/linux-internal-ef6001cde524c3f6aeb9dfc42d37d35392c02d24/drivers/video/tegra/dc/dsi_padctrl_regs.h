/*
 * drivers/video/tegra/dc/dsi_padctrl.h
 *
 * Copyright (c) 2015, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define DSI_PADCTRL_GLOBAL_CNTRLS		0x2
#define DSI_PADCTRL_PDVCLAMP_CD			BIT(4)
#define DSI_PADCTRL_PDVCLAMP_AB			BIT(3)

#define DSI_PADCTRL_PWR_DOWN_PD_IO_1_EN		BIT(2)
#define DSI_PADCTRL_PWR_DOWN_PD_IO_0_EN		BIT(1)
#define DSI_PADCTRL_PWR_DOWN_PD_CLK_EN		BIT(0)

#define DSI_PADCTRL_A_LANES_PWR_DOWN		0x5
#define DSI_PADCTRL_B_LANES_PWR_DOWN		0x11
#define DSI_PADCTRL_C_LANES_PWR_DOWN		0x1D
#define DSI_PADCTRL_D_LANES_PWR_DOWN		0x29

#define DSI_PADCTRL_E_PULL_DWN_PD_IO_1_EN	BIT(2)
#define DSI_PADCTRL_E_PULL_DWN_PD_IO_0_EN	BIT(1)
#define DSI_PADCTRL_E_PULL_DWN_PD_CLK_EN	BIT(0)

#define DSI_PADCTRL_A_PULL_DOWN			0x6
#define DSI_PADCTRL_B_PULL_DOWN			0x12
#define DSI_PADCTRL_C_PULL_DOWN			0x1E
#define DSI_PADCTRL_D_PULL_DOWN			0x2A

#define DSI_PADCTRL_INT_LP_BIAS_OVRRIDE_E_ATE_IO1_EN	BIT(2)
#define DSI_PADCTRL_INT_LP_BIAS_OVRRIDE_E_ATE_IO0_EN	BIT(1)
#define DSI_PADCTRL_INT_LP_BIAS_OVRRIDE_E_ATE_CLK_EN	BIT(0)

#define DSI_PADCTRL_A_INTERNAL_LP_BIAS_OVRRIDE	0x7
#define DSI_PADCTRL_B_INTERNAL_LP_BIAS_OVRRIDE	0x13
#define DSI_PADCTRL_C_INTERNAL_LP_BIAS_OVRRIDE	0x1F
#define DSI_PADCTRL_D_INTERNAL_LP_BIAS_OVRRIDE	0x2B

#define DSI_PADCTRL_DPHY_CLK_CONFIG_REV_CLK_EN	BIT(0)

#define DSI_PADCTRL_A_DPHY_CLK_CONFIG		0x8
#define DSI_PADCTRL_B_DPHY_CLK_CONFIG		0x14
#define DSI_PADCTRL_C_DPHY_CLK_CONFIG		0x20
#define DSI_PADCTRL_D_DPHY_CLK_CONFIG		0x2C

#define DSI_PADCTRL_VALID_OUTPUT_DELAY_TRIMMER_OUTADJ_IO1(x)	(((x) & 0x3F) << 20)
#define DSI_PADCTRL_VALID_OUTPUT_DELAY_TRIMMER_OUTADJ_IO0(x)	(((x) & 0x3F) << 10)
#define DSI_PADCTRL_VALID_OUTPUT_DELAY_TRIMMER_OUTADJ_CLK(x)	((x) & 0x3F)

#define DSI_PADCTRL_A_OUTPUT_DELAY_TRIMMER	0x9
#define DSI_PADCTRL_B_OUTPUT_DELAY_TRIMMER	0x15
#define DSI_PADCTRL_C_OUTPUT_DELAY_TRIMMER	0x21
#define DSI_PADCTRL_D_OUTPUT_DELAY_TRIMMER	0x2D

#define DSI_PADCTRL_VALID_PEMPD_IO1(x)	(((x) & 0x3) << 20)
#define DSI_PADCTRL_VALID_PEMPD_IO0(x)	(((x) & 0x3) << 16)
#define DSI_PADCTRL_VALID_PEMPD_CLK(x)	(((x) & 0x3) << 12)
#define DSI_PADCTRL_VALID_PEMPU_IO1(x)	(((x) & 0x3) << 8)
#define DSI_PADCTRL_VALID_PEMPU_IO0(x)	(((x) & 0x3) << 4)
#define DSI_PADCTRL_VALID_PEMPU_CLK(x)	((x) & 0x3)

#define DSI_PADCTRL_A_PRE_EMPHASIS	0xA
#define DSI_PADCTRL_B_PRE_EMPHASIS	0x16
#define DSI_PADCTRL_C_PRE_EMPHASIS	0x22

#define DSI_PADCTRL_VALID_CPHY_MID_STR_TRIO1_CTRL_EN	BIT(1)
#define DSI_PADCTRL_VALID_CPHY_MID_STR_TRIO0_CTRL_EN	BIT(0)

#define DSI_PADCTRL_A_CPHY_MID_STRENGTH	0xB
#define DSI_PADCTRL_B_CPHY_MID_STRENGTH	0x17
#define DSI_PADCTRL_C_CPHY_MID_STRENGTH	0x23
#define DSI_PADCTRL_D_CPHY_MID_STRENGTH	0x2F

#define DSI_PADCTRL_VALID_IMP_CTRL_LP_DWN_ADJ_IO1(x)	(((x) & 0xF) << 20)
#define DSI_PADCTRL_VALID_IMP_CTRL_LP_DWN_ADJ_IO0(x)	(((x) & 0xF) << 16)
#define DSI_PADCTRL_VALID_IMP_CTRL_LP_DWN_ADJ_CLK(x)	(((x) & 0xF) << 12)
#define DSI_PADCTRL_VALID_IMP_CTRL_LP_UP_ADJ_IO1(x)	(((x) & 0xF) << 8)
#define DSI_PADCTRL_VALID_IMP_CTRL_LP_UP_ADJ_IO0(x)	(((x) & 0xF) << 4)
#define DSI_PADCTRL_VALID_IMP_CTRL_LP_UP_ADJ_CLK(x)	((x) & 0xF)

#define DSI_PADCTRL_A_LP_DRVR_IMP_CTRL	0xC
#define DSI_PADCTRL_B_LP_DRVR_IMP_CTRL	0x18
#define DSI_PADCTRL_C_LP_DRVR_IMP_CTRL	0x24
#define DSI_PADCTRL_D_LP_DRVR_IMP_CTRL	0x30

#define DSI_PADCTRL_VALID_SLEW_DOWN_IO1(x)	(((x) & 0xF) << 20)
#define DSI_PADCTRL_VALID_SLEW_DOWN_IO0(x)	(((x) & 0xF) << 16)
#define DSI_PADCTRL_VALID_SLEW_DOWN_CLK(x)	(((x) & 0xF) << 12)
#define DSI_PADCTRL_VALID_SLEW_UP_IO1(x)	(((x) & 0xF) << 8)
#define DSI_PADCTRL_VALID_SLEW_UP_IO0(x)	(((x) & 0xF) << 4)
#define DSI_PADCTRL_VALID_SLEW_UP_CLK(x)	((x) & 0xF)

#define DSI_PADCTRL_A_LP_DRVR_SLEW_RATE_CTRL	0xD
#define DSI_PADCTRL_B_LP_DRVR_SLEW_RATE_CTRL	0x19
#define DSI_PADCTRL_C_LP_DRVR_SLEW_RATE_CTRL	0x25
#define DSI_PADCTRL_D_LP_DRVR_SLEW_RATE_CTRL	0x31

#define DSI_PADCTRL_CONTENTION_DET_E_CD_IO1_EN	BIT(2)
#define DSI_PADCTRL_CONTENTION_DET_E_CD_IO0_EN	BIT(1)
#define DSI_PADCTRL_CONTENTION_DET_E_CD_CLK_EN	BIT(0)

#define DSI_PADCTRL_A_CONTENTION_DET_CTRL	0xE
#define DSI_PADCTRL_B_CONTENTION_DET_CTRL	0x1A
#define DSI_PADCTRL_C_CONTENTION_DET_CTRL	0x26
#define DSI_PADCTRL_D_CONTENTION_DET_CTRL	0x32

#define DSI_PADCTRL_CDN_IO1_DETECTED	BIT(5)
#define DSI_PADCTRL_CDP_IO1_DETECTED	BIT(4)
#define DSI_PADCTRL_CDN_IO0_DETECTED	BIT(3)
#define DSI_PADCTRL_CDP_IO0_DETECTED	BIT(2)
#define DSI_PADCTRL_CDN_CLK_DETECTED	BIT(1)
#define DSI_PADCTRL_CDP_CLK_DETECTED	BIT(0)

#define DSI_PADCTRL_A_CONTENTION_DET_STATUS	0XF
#define DSI_PADCTRL_B_CONTENTION_DET_STATUS	0X1B
#define DSI_PADCTRL_C_CONTENTION_DET_STATUS	0X27
#define DSI_PADCTRL_D_CONTENTION_DET_STATUS	0X33

#define DSI_PAPCTRL_VALID_SPARE_IO1(x)		(((x) & 0xF) << 8)
#define DSI_PAPCTRL_VALID_SPARE_IO0(x)		(((x) & 0xF) << 4)
#define DSI_PAPCTRL_VALID_SPARE_CLK(x)		((x) & 0xF)

#define DSI_PADCTRL_A_SPARE_BITS			0x10
#define DSI_PADCTRL_B_SPARE_BITS			0x1C
#define DSI_PADCTRL_C_SPARE_BITS			0x28
#define DSI_PADCTRL_D_SPARE_BITS			0x34
