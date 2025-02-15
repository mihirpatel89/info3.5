/*
 * Driver for Tegra Security Engine
 *
 * Copyright (c) 2015-2017, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _CRYPTO_TEGRA_SE_ELP_H
#define _CRYPTO_TEGRA_SE_ELP_H

#define TEGRA_SE_PKA_KEYSLOT_COUNT	4

#define TEGRA_SE_ELP_PKA_FLAGS_OFFSET			0xC024
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_F0_SHIFT		4
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_F0(x)	\
				(x << TEGRA_SE_ELP_PKA_FLAGS_FLAG_F0_SHIFT)
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_F1_SHIFT		5
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_F1(x)	\
				(x << TEGRA_SE_ELP_PKA_FLAGS_FLAG_F1_SHIFT)
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_F3_SHIFT		7
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_F3(x)	\
				(x << TEGRA_SE_ELP_PKA_FLAGS_FLAG_F3_SHIFT)
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_ZERO_SHIFT		0
#define TEGRA_SE_ELP_PKA_FLAGS_FLAG_ZERO(x)	\
				(x << TEGRA_SE_ELP_PKA_FLAGS_FLAG_ZERO_SHIFT)

#define TEGRA_SE_ELP_PKA_FSTACK_PTR_OFFSET		0xC010

#define ENABLE		1
#define DISABLE		0
#define FALSE		0
#define TRUE		1

#define TEGRA_SE_ELP_PKA_INT_ENABLE_OFFSET		0xC040
#define TEGRA_SE_ELP_PKA_INT_ENABLE_IE_IRQ_EN_SHIFT	30
#define TEGRA_SE_ELP_PKA_INT_ENABLE_IE_IRQ_EN(x)	\
			(x << TEGRA_SE_ELP_PKA_INT_ENABLE_IE_IRQ_EN_SHIFT)

#define TEGRA_SE_ELP_RNG_INT_EN_OFFSET		0xFC0
#define TEGRA_SE_ELP_RNG_IE_OFFSET		0xF10

#define TEGRA_SE_ELP_RNG_STATUS_OFFSET		0xF0C
#define TEGRA_SE_ELP_RNG_STATUS_BUSY_SHIFT	31
#define TEGRA_SE_ELP_RNG_STATUS_BUSY(x)		\
		(x << TEGRA_SE_ELP_RNG_STATUS_BUSY_SHIFT)

#define TEGRA_SE_ELP_RNG_STATUS_SECURE_SHIFT		6
#define STATUS_SECURE			1
#define STATUS_PROMISCUOUS		0
#define TEGRA_SE_ELP_RNG_STATUS_SECURE(x)	\
		(x << TEGRA_SE_ELP_RNG_STATUS_SECURE_SHIFT)

#define TEGRA_SE_ELP_RNG_ISTATUS_OFFSET			0xF14
#define ISTATUS_ACTIVE			1
#define ISTATUS_CLEAR			0

#define TEGRA_SE_ELP_RNG_ISTATUS_NOISE_RDY_SHIFT	2
#define TEGRA_SE_ELP_RNG_ISTATUS_NOISE_RDY(x)		\
			(x << TEGRA_SE_ELP_RNG_ISTATUS_NOISE_RDY_SHIFT)
#define TEGRA_SE_ELP_RNG_ISTATUS_DONE_SHIFT		4
#define TEGRA_SE_ELP_RNG_ISTATUS_DONE(x)		\
				(x << TEGRA_SE_ELP_RNG_ISTATUS_DONE_SHIFT)
#define TEGRA_SE_ELP_RNG_ISTATUS_KAT_COMPLETED_SHIFT	1
#define TEGRA_SE_ELP_RNG_ISTATUS_KAT_COMPLETED(x)	\
			(x << TEGRA_SE_ELP_RNG_ISTATUS_KAT_COMPLETED_SHIFT)
#define TEGRA_SE_ELP_RNG_ISTATUS_ZEROIZED_SHIFT		0
#define TEGRA_SE_ELP_RNG_ISTATUS_ZEROIZED(x)		\
			(x << TEGRA_SE_ELP_RNG_ISTATUS_ZEROIZED_SHIFT)

#define TEGRA_SE_ELP_RNG_INT_STATUS_OFFSET		0xFC4
#define STATUS_ACTIVE			1
#define STATUS_CLEAR			0
#define TEGRA_SE_ELP_RNG_INT_STATUS_EIP0_SHIFT		8
#define TEGRA_SE_ELP_RNG_INT_STATUS_EIP0(x)		\
			(x << TEGRA_SE_ELP_RNG_INT_STATUS_EIP0_SHIFT)

#define TEGRA_SE_ELP_RNG_NPA_DATA0_OFFSET	0xF34

#define TEGRA_SE_ELP_RNG_SE_MODE_OFFSET		0xF04
#define RNG1_MODE_ADDIN_PRESENT_SHIFT		4
#define RNG1_MODE_ADDIN_PRESENT			\
				(TRUE << RNG1_MODE_ADDIN_PRESENT_SHIFT)
#define RNG1_MODE_SEC_ALG_SHIFT			0
#define RNG1_MODE_SEC_ALG			\
					(TRUE << RNG1_MODE_SEC_ALG_SHIFT)
#define RNG1_MODE_PRED_RESIST_SHIFT			3
#define RNG1_MODE_PRED_RESIST			\
					(TRUE << RNG1_MODE_PRED_RESIST_SHIFT)

#define TEGRA_SE_ELP_RNG_SE_SMODE_OFFSET		0xF08
#define TEGRA_SE_ELP_RNG_SE_SMODE_SECURE_SHIFT		1
#define SMODE_SECURE			1
#define SMODE_PROMISCUOUS		0
#define TEGRA_SE_ELP_RNG_SE_SMODE_SECURE(x)		\
				(x << TEGRA_SE_ELP_RNG_SE_SMODE_SECURE_SHIFT)

#define TEGRA_SE_ELP_RNG_SE_SMODE_NONCE_SHIFT	0
#define TEGRA_SE_ELP_RNG_SE_SMODE_NONCE(x)	\
				(x << TEGRA_SE_ELP_RNG_SE_SMODE_NONCE_SHIFT)

#define TEGRA_SE_ELP_RNG_RAND0_OFFSET		0xF24
#define TEGRA_SE_ELP_RNG_ALARMS_OFFSET		0xF18

#define TEGRA_SE_ELP_PKA_STATUS_OFFSET			0xC020
#define TEGRA_SE_ELP_PKA_STATUS_IRQ_STAT_SHIFT		30
#define TEGRA_SE_ELP_PKA_STATUS_IRQ_STAT(x)	\
			(x << TEGRA_SE_ELP_PKA_STATUS_IRQ_STAT_SHIFT)

#define TEGRA_SE_ELP_PKA_CTRL_STATUS_OFFSET		0x810C
#define TEGRA_SE_ELP_PKA_CTRL_SE_STATUS_SHIFT		0
#define TEGRA_SE_ELP_PKA_CTRL_SE_STATUS_BUSY		1
#define TEGRA_SE_ELP_PKA_CTRL_SE_STATUS_IDLE		0
#define TEGRA_SE_ELP_PKA_CTRL_PKA_STATUS_SHIFT		1
#define TEGRA_SE_ELP_PKA_CTRL_PKA_STATUS_BUSY		1
#define TEGRA_SE_ELP_PKA_CTRL_PKA_STATUS_IDLE		0
#define TEGRA_SE_ELP_PKA_CTRL_PKA_STATUS(x)	\
				(x << TEGRA_SE_ELP_PKA_CTRL_PKA_STATUS_SHIFT)
#define TEGRA_SE_ELP_PKA_CTRL_SE_STATUS(x)	\
				(x << TEGRA_SE_ELP_PKA_CTRL_SE_STATUS_SHIFT)

#define TEGRA_SE_ELP_PKA_RETURN_CODE_OFFSET			0xC008
#define TEGRA_SE_ELP_PKA_RETURN_CODE_STOP_REASON_NORMAL		0x00
#define TEGRA_SE_ELP_PKA_RETURN_CODE_STOP_REASON_SHIFT		16
#define TEGRA_SE_ELP_PKA_RETURN_CODE_STOP_REASON(x)	\
			(x << TEGRA_SE_ELP_PKA_RETURN_CODE_STOP_REASON_SHIFT)

#define TEGRA_SE_ELP_PKA_MUTEX_WATCHDOG_OFFSET	0x8110
#define TEGRA_SE_ELP_PKA_MUTEX_OFFSET		0x8114
#define TEGRA_SE_ELP_PKA_MUTEX_RELEASE_OFFSET	0x8118

#define TEGRA_SE_ELP_PKA_MUTEX_TIMEOUT_ACTION_OFFSET	0x8128
#define TEGRA_SE_ELP_PKA_MUTEX_TIMEOUT_ACTION		0x2

#define TEGRA_SE_ELP_RNG_MUTEX_WATCHDOG_OFFSET		0xFD0
#define TEGRA_SE_ELP_RNG_MUTEX_OFFSET			0xFD8
#define TEGRA_SE_ELP_RNG_MUTEX_TIMEOUT_ACTION		0x2
#define TEGRA_SE_ELP_RNG_MUTEX_TIMEOUT_ACTION_OFFSET	0xFD4

#define TEGRA_SE_ELP_PKA_ERROR_CAPTURE_OFFSET	0x8134

#define TEGRA_SE_ELP_RNG_CTRL_OFFSET		0xF00

#define TEGRA_SE_ELP_PKA_CTRL_OFFSET		0xC000

#define TEGRA_SE_ELP_PKA_CTRL_GO_SHIFT		31
#define TEGRA_SE_ELP_PKA_CTRL_GO_START		1
#define TEGRA_SE_ELP_PKA_CTRL_GO_NOP		0
#define TEGRA_SE_ELP_PKA_CTRL_GO(x)	\
			(x << TEGRA_SE_ELP_PKA_CTRL_GO_SHIFT)

#define TEGRA_SE_ELP_PKA_CTRL_BASE_RADIX_SHIFT	8
#define TEGRA_SE_ELP_PKA_CTRL_BASE_256		2
#define TEGRA_SE_ELP_PKA_CTRL_BASE_512		3
#define TEGRA_SE_ELP_PKA_CTRL_BASE_1024		4
#define TEGRA_SE_ELP_PKA_CTRL_BASE_2048		5
#define TEGRA_SE_ELP_PKA_CTRL_BASE_4096		6
#define TEGRA_SE_ELP_PKA_CTRL_BASE_RADIX(x)	\
				(x << TEGRA_SE_ELP_PKA_CTRL_BASE_RADIX_SHIFT)

#define TEGRA_SE_ELP_PKA_CTRL_PARTIAL_RADIX_SHIFT	0

#define PKA_OP_SIZE_256		256
#define PKA_OP_SIZE_512		512
#define PKA_OP_SIZE_1024	1024
#define PKA_OP_SIZE_2048	2048
#define PKA_OP_SIZE_4096	4096

#define TEGRA_SE_ELP_MAX_DATA_SIZE	68

#define TEGRA_SE_ELP_PKA_CTRL_PARTIAL_RADIX(x)	\
				(x << TEGRA_SE_ELP_PKA_CTRL_PARTIAL_RADIX_SHIFT)

#define TEGRA_SE_ELP_PKA_TRNG_SMODE_OFFSET			0xBF0C
#define TEGRA_SE_ELP_PKA_TRNG_SMODE_SECURE_SHIFT		8
#define TEGRA_SE_ELP_PKA_TRNG_SMODE_SECURE(x)	\
				(x << TEGRA_SE_ELP_PKA_TRNG_SMODE_SECURE_SHIFT)

#define TEGRA_SE_ELP_PKA_TRNG_SMODE_NONCE_SHIFT			2
#define TEGRA_SE_ELP_PKA_TRNG_SMODE_NONCE(x)	\
				(x << TEGRA_SE_ELP_PKA_TRNG_SMODE_NONCE_SHIFT)

#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_OFFSET			0x8104
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_AUTO_RESEED_SHIFT		0
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_AUTO_RESEED(x)	\
			(x << TEGRA_SE_ELP_PKA_CTRL_CONTROL_AUTO_RESEED_SHIFT)

#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_LOAD_KEY_SHIFT		2
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_LOAD_KEY(x)	\
			(x << TEGRA_SE_ELP_PKA_CTRL_CONTROL_LOAD_KEY_SHIFT)

#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT_SHIFT	16
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT_0		0x00
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT_1		0x01
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT_2		0x02
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT_3		0x03
#define TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT(x)	\
			(x << TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT_SHIFT)

#define TEGRA_SE_ELP_PKA_TRNG_STATUS_OFFSET		0xBF04
#define TEGRA_SE_ELP_PKA_TRNG_STATUS_SECURE_SHIFT	8
#define TEGRA_SE_ELP_PKA_TRNG_STATUS_SECURE(x)	\
				(x << TEGRA_SE_ELP_PKA_TRNG_STATUS_SECURE_SHIFT)

#define TEGRA_SE_ELP_PKA_TRNG_STATUS_NONCE_SHIFT	2
#define TEGRA_SE_ELP_PKA_TRNG_STATUS_NONCE(x)	\
				(x << TEGRA_SE_ELP_PKA_TRNG_STATUS_NONCE_SHIFT)

#define TEGRA_SE_ELP_PKA_TRNG_STATUS_SEEDED_SHIFT	9
#define TEGRA_SE_ELP_PKA_TRNG_STATUS_SEEDED(x)	\
				(x << TEGRA_SE_ELP_PKA_TRNG_STATUS_SEEDED_SHIFT)

#define TEGRA_SE_ELP_PKA_TRNG_STATUS_LAST_RESEED_SHIFT		16
#define TRNG_LAST_RESEED_HOST					0
#define TRNG_LAST_RESEED_NONCE					3
#define TRNG_LAST_RESEED_RESEED					4
#define TRNG_LAST_RESEED_UNSEEDED				7
#define TEGRA_SE_ELP_PKA_TRNG_STATUS_LAST_RESEED(x)	\
			(x << TEGRA_SE_ELP_PKA_TRNG_STATUS_LAST_RESEED_SHIFT)

#define TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET	0xC004

#define TEGRA_SE_ELP_PKA_RSA_M_PRG_ENTRY_VAL		0x10
#define TEGRA_SE_ELP_PKA_RSA_R2_PRG_ENTRY_VAL		0x12
#define TEGRA_SE_ELP_PKA_RSA_RINV_PRG_ENTRY_VAL		0x11

#define TEGRA_SE_ELP_PKA_RSA_MOD_EXP_PRG_ENTRY_VAL		0x14
#define TEGRA_SE_ELP_PKA_ECC_ECPV_PRG_ENTRY_VAL			0x1E
#define TEGRA_SE_ELP_PKA_ECC_POINT_ADD_PRG_ENTRY_VAL		0x1C
#define TEGRA_SE_ELP_PKA_ECC_POINT_MUL_PRG_ENTRY_VAL		0x19
#define TEGRA_SE_ELP_PKA_ECC_POINT_DOUBLE_PRG_ENTRY_VAL		0x1A
#define TEGRA_SE_ELP_PKA_ECC_SHAMIR_TRICK_PRG_ENTRY_VAL		0x23

#define PKA_ADDR_OFFSET		0xC000

#define PKA_BANK_START_A	(PKA_ADDR_OFFSET + 0x400)
#define PKA_BANK_START_B	(PKA_ADDR_OFFSET + 0x800)
#define PKA_BANK_START_C	(PKA_ADDR_OFFSET + 0xC00)
#define PKA_BANK_START_D	(PKA_ADDR_OFFSET + 0x1000)

#define BANK_A	0
#define BANK_B	1
#define BANK_C	2
#define BANK_D	3

#define TEGRA_SE_ELP_PKA_RSA_MSG_BANK		BANK_A
#define TEGRA_SE_ELP_PKA_RSA_MSG_ID		0
#define TEGRA_SE_ELP_PKA_RSA_EXP_BANK		BANK_D
#define TEGRA_SE_ELP_PKA_RSA_EXP_ID		2
#define TEGRA_SE_ELP_PKA_RSA_RESULT_BANK	BANK_A
#define TEGRA_SE_ELP_PKA_RSA_RESULT_ID		0

#define	TEGRA_SE_ELP_PKA_M_BANK			BANK_D
#define	TEGRA_SE_ELP_PKA_M_ID			1
#define TEGRA_SE_ELP_PKA_RINV_BANK		BANK_C
#define TEGRA_SE_ELP_PKA_RINV_ID		0
#define TEGRA_SE_ELP_PKA_R2_BANK		BANK_D
#define TEGRA_SE_ELP_PKA_R2_ID			3
#define TEGRA_SE_ELP_PKA_MOD_BANK		BANK_D
#define TEGRA_SE_ELP_PKA_MOD_ID			0

#define TEGRA_SE_ELP_PKA_ECC_A_BANK		BANK_A
#define TEGRA_SE_ELP_PKA_ECC_A_ID		6
#define TEGRA_SE_ELP_PKA_ECC_B_BANK		BANK_A
#define TEGRA_SE_ELP_PKA_ECC_B_ID		7
#define TEGRA_SE_ELP_PKA_ECC_N_BANK		0
#define TEGRA_SE_ELP_PKA_ECC_N_ID		0
#define TEGRA_SE_ELP_PKA_ECC_XP_BANK		BANK_A
#define TEGRA_SE_ELP_PKA_ECC_XP_ID		2
#define TEGRA_SE_ELP_PKA_ECC_YP_BANK		BANK_B
#define TEGRA_SE_ELP_PKA_ECC_YP_ID		2
#define TEGRA_SE_ELP_PKA_ECC_XQ_BANK		BANK_A
#define TEGRA_SE_ELP_PKA_ECC_XQ_ID		3
#define TEGRA_SE_ELP_PKA_ECC_YQ_BANK		BANK_B
#define TEGRA_SE_ELP_PKA_ECC_YQ_ID		3
#define TEGRA_SE_ELP_PKA_ECC_K_BANK		BANK_D
#define TEGRA_SE_ELP_PKA_ECC_K_ID		7

#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(i)		(0x00008800+((i)*4))
#define TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(i)		(0x00008810+((i)*4))

#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_AUTO_INC_SHIFT	31
#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_AUTO_INC_SET	1
#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_AUTO_INC_CLEAR	0
#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_AUTO_INC(x)	\
			(x << TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_AUTO_INC_SHIFT)

#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD_SHIFT	8
#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(x)	\
				(x << TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD_SHIFT)

#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD_SHIFT	0
#define TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(x)	\
				(x << TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD_SHIFT)

#endif /* _CRYPTO_TEGRA_SE_ELP_H */
