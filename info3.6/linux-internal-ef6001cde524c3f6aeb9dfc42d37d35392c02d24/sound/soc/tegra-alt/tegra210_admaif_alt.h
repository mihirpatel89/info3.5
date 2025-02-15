/*
 * tegra210_admaif_alt.h - Tegra210 ADMAIF registers
 *
 * Copyright (c) 2014-2015 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA210_ADMAIF_ALT_H__
#define __TEGRA210_ADMAIF_ALT_H__

#define TEGRA210_ADMAIF_CHANNEL_REG_STRIDE	0x40

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define TEGRA210_ADMAIF_CHANNEL_COUNT		10
#endif

#define TEGRA210_ADMAIF_XBAR_RX_ENABLE					0x0
#define TEGRA210_ADMAIF_XBAR_RX_SOFT_RESET				0x4
#define TEGRA210_ADMAIF_XBAR_RX_STATUS					0xc
#define TEGRA210_ADMAIF_XBAR_RX_INT_STATUS				0x10
#define TEGRA210_ADMAIF_XBAR_RX_INT_MASK				0x14
#define TEGRA210_ADMAIF_XBAR_RX_INT_SET					0x18
#define TEGRA210_ADMAIF_XBAR_RX_INT_CLEAR				0x1c
#define TEGRA210_ADMAIF_CHAN_ACIF_RX_CTRL				0x20
#define TEGRA210_ADMAIF_XBAR_RX_FIFO_CTRL				0x28
#define TEGRA210_ADMAIF_XBAR_RX_FIFO_READ				0x2c

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define TEGRA210_ADMAIF_XBAR_TX_ENABLE					0x300
#endif
#define TEGRA210_ADMAIF_XBAR_TX_SOFT_RESET				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x4)
#define TEGRA210_ADMAIF_XBAR_TX_STATUS					(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0xc)
#define TEGRA210_ADMAIF_XBAR_TX_INT_STATUS				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x10)
#define TEGRA210_ADMAIF_XBAR_TX_INT_MASK				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x14)
#define TEGRA210_ADMAIF_XBAR_TX_INT_SET				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x18)
#define TEGRA210_ADMAIF_XBAR_TX_INT_CLEAR				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x1c)
#define TEGRA210_ADMAIF_CHAN_ACIF_TX_CTRL				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x20)
#define TEGRA210_ADMAIF_XBAR_TX_FIFO_CTRL				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x28)
#define TEGRA210_ADMAIF_XBAR_TX_FIFO_WRITE				(TEGRA210_ADMAIF_XBAR_TX_ENABLE + 0x2c)

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define TEGRA210_ADMAIF_GLOBAL_ENABLE					0x700
#endif
#define TEGRA210_ADMAIF_GLOBAL_CG_0						(TEGRA210_ADMAIF_GLOBAL_ENABLE + 0x8)

#define TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN_SHIFT		31
#define TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN_MASK			(1 << TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN_SHIFT)
#define TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN			(1 << TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN_SHIFT)
#define TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN_SHIFT		30
#define TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN_MASK		(1 << TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN_SHIFT)
#define TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN			(1 << TEGRA210_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN_SHIFT)

#define TEGRA210_ADMAIF_XBAR_TX_ENABLE_SHIFT		0
#define TEGRA210_ADMAIF_XBAR_TX_EN			(1 << TEGRA210_ADMAIF_XBAR_TX_ENABLE_SHIFT)
#define TEGRA210_ADMAIF_XBAR_TX_ENABLE_MASK		(1 << TEGRA210_ADMAIF_XBAR_TX_ENABLE_SHIFT)

#define TEGRA210_ADMAIF_XBAR_RX_ENABLE_SHIFT		0
#define TEGRA210_ADMAIF_XBAR_RX_EN			(1 << TEGRA210_ADMAIF_XBAR_RX_ENABLE_SHIFT)
#define TEGRA210_ADMAIF_XBAR_RX_ENABLE_MASK		(1 << TEGRA210_ADMAIF_XBAR_RX_ENABLE_SHIFT)

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define TEGRA210_ADMAIF_XBAR_DMA_FIFO_START_ADDR_SHIFT	0
#define TEGRA210_ADMAIF_XBAR_DMA_FIFO_START_ADDR_MASK	(0x1f << TEGRA210_ADMAIF_XBAR_DMA_FIFO_START_ADDR_SHIFT)

#define TEGRA210_ADMAIF_XBAR_DMA_FIFO_SIZE_SHIFT	8
#define TEGRA210_ADMAIF_XBAR_DMA_FIFO_SIZE_MASK		(0x1f << TEGRA210_ADMAIF_XBAR_DMA_FIFO_SIZE_SHIFT)

#define TEGRA210_ADMAIF_XBAR_DMA_FIFO_THRESHOLD_SHIFT	20
#define TEGRA210_ADMAIF_XBAR_DMA_FIFO_THRESHOLD_MASK	(0x3ff << TEGRA210_ADMAIF_XBAR_DMA_FIFO_THRESHOLD_SHIFT)
#endif

#define TEGRA210_ADMAIF_XBAR_STATUS_TRANS_EN_SHIFT	0
#define TEGRA210_ADMAIF_XBAR_STATUS_TRANS_EN_MASK	(0x1 << TEGRA210_ADMAIF_XBAR_STATUS_TRANS_EN_SHIFT)

#define TEGRA210_ADMAIF_XBAR_CIF_CTRL_UNPACK8_ENABLE_SHIFT	31
#define TEGRA210_ADMAIF_XBAR_CIF_CTRL_UNPACK8_ENABLE		BIT(TEGRA210_ADMAIF_XBAR_CIF_CTRL_UNPACK8_ENABLE_SHIFT)

#define TEGRA210_ADMAIF_XBAR_CIF_CTRL_UNPACK16_ENABLE_SHIFT	30
#define TEGRA210_ADMAIF_XBAR_CIF_CTRL_UNPACK16_ENABLE		BIT(TEGRA210_ADMAIF_XBAR_CIF_CTRL_UNPACK16_ENABLE_SHIFT)

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define TEGRA210_ADMAIF_LAST_REG			0x75f

#define ADMAIF_RX1_FIFO_CTRL_REG_DEFAULT 0x00000300
#define ADMAIF_RX2_FIFO_CTRL_REG_DEFAULT 0x00000304
#define ADMAIF_RX3_FIFO_CTRL_REG_DEFAULT 0x00000208
#define ADMAIF_RX4_FIFO_CTRL_REG_DEFAULT 0x0000020b
#define ADMAIF_RX5_FIFO_CTRL_REG_DEFAULT 0x0000020e
#define ADMAIF_RX6_FIFO_CTRL_REG_DEFAULT 0x00000211
#define ADMAIF_RX7_FIFO_CTRL_REG_DEFAULT 0x00000214
#define ADMAIF_RX8_FIFO_CTRL_REG_DEFAULT 0x00000217
#define ADMAIF_RX9_FIFO_CTRL_REG_DEFAULT 0x0000021a
#define ADMAIF_RX10_FIFO_CTRL_REG_DEFAULT 0x0000021d
#define ADMAIF_RX11_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX12_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX13_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX14_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX15_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX16_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX17_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX18_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX19_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_RX20_FIFO_CTRL_REG_DEFAULT 0x00000000

#define ADMAIF_TX1_FIFO_CTRL_REG_DEFAULT 0x02000300
#define ADMAIF_TX2_FIFO_CTRL_REG_DEFAULT 0x02000304
#define ADMAIF_TX3_FIFO_CTRL_REG_DEFAULT 0x01800208
#define ADMAIF_TX4_FIFO_CTRL_REG_DEFAULT 0x0180020b
#define ADMAIF_TX5_FIFO_CTRL_REG_DEFAULT 0x0180020e
#define ADMAIF_TX6_FIFO_CTRL_REG_DEFAULT 0x01800211
#define ADMAIF_TX7_FIFO_CTRL_REG_DEFAULT 0x01800214
#define ADMAIF_TX8_FIFO_CTRL_REG_DEFAULT 0x01800217
#define ADMAIF_TX9_FIFO_CTRL_REG_DEFAULT 0x0180021a
#define ADMAIF_TX10_FIFO_CTRL_REG_DEFAULT 0x0180021d
#define ADMAIF_TX11_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX12_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX13_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX14_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX15_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX16_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX17_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX18_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX19_FIFO_CTRL_REG_DEFAULT 0x00000000
#define ADMAIF_TX20_FIFO_CTRL_REG_DEFAULT 0x00000000
#endif

enum {
	DATA_8BIT,
	DATA_16BIT,
	DATA_32BIT
};

struct tegra210_admaif_soc_data {
	unsigned int num_ch;
	unsigned int clk_list_mask;
	void (*set_audio_cif)(struct regmap *map,
			unsigned int reg,
			struct tegra210_xbar_cif_conf *cif_conf);
};

struct tegra210_admaif {
	/* regmap for admaif */
	struct regmap *regmap;
	int refcnt;
	struct tegra_alt_pcm_dma_params *capture_dma_data;
	struct tegra_alt_pcm_dma_params *playback_dma_data;
	const struct tegra210_admaif_soc_data *soc_data;
	int override_channels[TEGRA210_ADMAIF_CHANNEL_COUNT];
};

#endif
