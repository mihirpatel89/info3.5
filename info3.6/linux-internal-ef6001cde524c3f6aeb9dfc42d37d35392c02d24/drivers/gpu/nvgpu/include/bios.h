/*
 * vbios tables support
 *
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_INCLUDE_BIOS_H
#define NVGPU_INCLUDE_BIOS_H

#include "gk20a/gk20a.h"

#define BIOS_GET_FIELD(value, name) ((value & name##_MASK) >> name##_SHIFT)

struct fll_descriptor_header {
	u8 version;
	u8 size;
} __packed;

#define FLL_DESCRIPTOR_HEADER_10_SIZE_4     4
#define FLL_DESCRIPTOR_HEADER_10_SIZE_6     6

struct fll_descriptor_header_10 {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u16 max_min_freq_mhz;
} __packed;

#define FLL_DESCRIPTOR_ENTRY_10_SIZE     15

struct fll_descriptor_entry_10 {
	u8 fll_device_type;
	u8 clk_domain;
	u8 fll_device_id;
	u16 lut_params;
	u8 vin_idx_logic;
	u8 vin_idx_sram;
	u16 fll_params;
	u8 min_freq_vfe_idx;
	u8 freq_ctrl_idx;
	u16 ref_freq_mhz;
	u16 ffr_cutoff_freq_mhz;
} __packed;

#define NV_FLL_DESC_FLL_PARAMS_MDIV_MASK 0x1F
#define NV_FLL_DESC_FLL_PARAMS_MDIV_SHIFT 0

#define NV_FLL_DESC_LUT_PARAMS_VSELECT_MASK 0x3
#define NV_FLL_DESC_LUT_PARAMS_VSELECT_SHIFT 0

#define NV_FLL_DESC_LUT_PARAMS_HYSTERISIS_THRESHOLD_MASK 0x3C
#define NV_FLL_DESC_LUT_PARAMS_HYSTERISIS_THRESHOLD_SHIFT 2

struct vin_descriptor_header_10 {
	u8 version;
	u8 header_sizee;
	u8 entry_size;
	u8 entry_count;
	u8 flags0;
	u32 vin_cal;
} __packed;

struct vin_descriptor_entry_10 {
	u8 vin_device_type;
	u8 volt_domain_vbios;
	u8 vin_device_id;
} __packed;

#define NV_VIN_DESC_FLAGS0_VIN_CAL_REVISION_MASK 0x7
#define NV_VIN_DESC_FLAGS0_VIN_CAL_REVISION_SHIFT 0

#define NV_VIN_DESC_FLAGS0_DISABLE_CONTROL_MASK 0x8
#define NV_VIN_DESC_FLAGS0_DISABLE_CONTROL_SHIFT 3

#define NV_VIN_DESC_VIN_CAL_SLOPE_FRACTION_MASK 0x1FF
#define NV_VIN_DESC_VIN_CAL_SLOPE_FRACTION_SHIFT 0

#define NV_VIN_DESC_VIN_CAL_SLOPE_INTEGER_MASK 0x3C00
#define NV_VIN_DESC_VIN_CAL_SLOPE_INTEGER_SHIFT  10

#define NV_VIN_DESC_VIN_CAL_INTERCEPT_FRACTION_MASK 0x3C000
#define NV_VIN_DESC_VIN_CAL_INTERCEPT_FRACTION_SHIFT 14

#define NV_VIN_DESC_VIN_CAL_INTERCEPT_INTEGER_MASK 0xFFC0000
#define NV_VIN_DESC_VIN_CAL_INTERCEPT_INTEGER_SHIFT 18

#define VBIOS_CLOCKS_TABLE_1X_HEADER_SIZE_07 0x07
struct vbios_clocks_table_1x_header {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u8 clocks_hal;
	u16 cntr_sampling_periodms;
} __packed;

#define VBIOS_CLOCKS_TABLE_1X_ENTRY_SIZE_09                                 0x09
struct vbios_clocks_table_1x_entry {
	u8 flags0;
	u16 param0;
	u32 param1;
	u16 param2;
} __packed;

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_MASK                    0x1F
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_SHIFT                   0
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_FIXED                   0x00
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_MASTER                  0x01
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_SLAVE                   0x02

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_FIRST_MASK  0xFF
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_FIRST_SHIFT  0
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_LAST_MASK  0xFF00
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_LAST_SHIFT 0x08

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_FIXED_FREQUENCY_MHZ_MASK        0xFFFF
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_FIXED_FREQUENCY_MHZ_SHIFT       0
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_MASTER_FREQ_OC_DELTA_MIN_MHZ_MASK 0xFFFF
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_MASTER_FREQ_OC_DELTA_MIN_MHZ_SHIFT 0

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_MASTER_FREQ_OC_DELTA_MAX_MHZ_MASK 0xFFFF0000
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_MASTER_FREQ_OC_DELTA_MAX_MHZ_SHIFT 0

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_SLAVE_MASTER_DOMAIN_MASK         0xF
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_SLAVE_MASTER_DOMAIN_SHIFT       0

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_UNAWARE_ORDERING_IDX_MASK 0xF
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_UNAWARE_ORDERING_IDX_SHIFT 0

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_AWARE_ORDERING_IDX_MASK     0xF0
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_AWARE_ORDERING_IDX_SHIFT   4

#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_FORCE_NOISE_UNAWARE_ORDERING_MASK 0x100
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_FORCE_NOISE_UNAWARE_ORDERING_SHIFT 8
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_FORCE_NOISE_UNAWARE_ORDERING_FALSE   0x00
#define NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_FORCE_NOISE_UNAWARE_ORDERING_TRUE    0x01

#define VBIOS_CLOCK_PROGRAMMING_TABLE_1X_HEADER_SIZE_08                              0x08
struct vbios_clock_programming_table_1x_header {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u8 slave_entry_size;
	u8 slave_entry_count;
	u8 vf_entry_size;
	u8 vf_entry_count;
} __packed;

#define VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_SIZE_05                      0x05
#define VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_SIZE_0D                      0x0D
struct vbios_clock_programming_table_1x_entry {
	u8 flags0;
	u16 freq_max_mhz;
	u8 param0;
	u8 param1;
	u32 rsvd;
	u32 rsvd1;
} __packed;

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASK          0xF
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_SHIFT         0
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_RATIO   0x00
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_TABLE   0x01
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_SLAVE          0x02

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_MASK          0x70
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_SHIFT         4
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_PLL          0x00
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_ONE_SOURCE   0x01
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_FLL        0x02

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_OVOC_ENABLED_MASK    0x80
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_OVOC_ENABLED_SHIFT   7
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_OVOC_ENABLED_FALSE  0x00
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_OVOC_ENABLED_TRUE   0x01

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_PARAM0_PLL_PLL_INDEX_MASK   0xFF
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_PARAM0_PLL_PLL_INDEX_SHIFT  0

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_PARAM1_PLL_FREQ_STEP_SIZE_MASK   0xFF
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_PARAM1_PLL_FREQ_STEP_SIZE_SHIFT  0

#define VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_SIZE_03              0x03
struct vbios_clock_programming_table_1x_slave_entry {
	u8 clk_dom_idx;
	u16 param0;
} __packed;

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_PARAM0_MASTER_RATIO_RATIO_MASK 0xFF
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_PARAM0_MASTER_RATIO_RATIO_SHIFT 0

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_PARAM0_MASTER_TABLE_FREQ_MASK  0x3FFF
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_PARAM0_MASTER_TABLE_FREQ_SHIFT  0

#define VBIOS_CLOCK_PROGRAMMING_TABLE_1X_VF_ENTRY_SIZE_02                   0x02
struct vbios_clock_programming_table_1x_vf_entry {
	u8 vfe_idx;
	u8 param0;
} __packed;

#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_VF_ENTRY_PARAM0_FLL_GAIN_VFE_IDX_MASK 0xFF
#define NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_VF_ENTRY_PARAM0_FLL_GAIN_VFE_IDX_SHIFT 0

struct vbios_vfe_3x_header_struct {
	u8 version;
	u8 header_size;
	u8 vfe_var_entry_size;
	u8 vfe_var_entry_count;
	u8 vfe_equ_entry_size;
	u8 vfe_equ_entry_count;
	u8 polling_periodms;
} __packed;

#define VBIOS_VFE_3X_VAR_ENTRY_SIZE_11                                      0x11
#define VBIOS_VFE_3X_VAR_ENTRY_SIZE_19                                      0x19
struct vbios_vfe_3x_var_entry_struct {
	u8 type;
	u32 out_range_min;
	u32 out_range_max;
	u32 param0;
	u32 param1;
	u32 param2;
	u32 param3;
} __packed;

#define VBIOS_VFE_3X_VAR_ENTRY_TYPE_DISABLED                                0x00
#define VBIOS_VFE_3X_VAR_ENTRY_TYPE_SINGLE_FREQUENCY                        0x01
#define VBIOS_VFE_3X_VAR_ENTRY_TYPE_SINGLE_VOLTAGE                          0x02
#define VBIOS_VFE_3X_VAR_ENTRY_TYPE_SINGLE_SENSED_TEMP                      0x03
#define VBIOS_VFE_3X_VAR_ENTRY_TYPE_SINGLE_SENSED_FUSE                      0x04
#define VBIOS_VFE_3X_VAR_ENTRY_TYPE_DERIVED_PRODUCT                         0x05
#define VBIOS_VFE_3X_VAR_ENTRY_TYPE_DERIVED_SUM                             0x06

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSTEMP_TH_CH_IDX_MASK 0xFF
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSTEMP_TH_CH_IDX_SHIFT 0

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSTEMP_HYS_POS_MASK 0xFF00
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSTEMP_HYS_POS_SHIFT 8

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSTEMP_HYS_NEG_MASK 0xFF0000
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSTEMP_HYS_NEG_SHIFT 16

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_VFIELD_ID_MASK 0xFF
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_VFIELD_ID_SHIFT 0

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_VFIELD_ID_VER_MASK 0xFF00
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_VFIELD_ID_VER_SHIFT 8

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_EXPECTED_VER_MASK 0xFF0000
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_EXPECTED_VER_SHIFT 16

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_USE_DEFAULT_ON_VER_CHECK_FAIL_MASK 0x1000000
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_USE_DEFAULT_ON_VER_CHECK_FAIL_SHIFT 24

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_USE_DEFAULT_ON_VER_CHECK_FAIL_YES 0x00000001
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_SSFUSE_USE_DEFAULT_ON_VER_CHECK_FAIL_NO 0x00000000
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DPROD_VFE_VAR_IDX_0_MASK 0xFF
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DPROD_VFE_VAR_IDX_0_SHIFT 0

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DPROD_VFE_VAR_IDX_1_MASK 0xFF00
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DPROD_VFE_VAR_IDX_1_SHIFT 8

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DSUM_VFE_VAR_IDX_0_MASK 0xFF
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DSUM_VFE_VAR_IDX_0_SHIFT 0

#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DSUM_VFE_VAR_IDX_1_MASK 0xFF00
#define VBIOS_VFE_3X_VAR_ENTRY_PAR0_DSUM_VFE_VAR_IDX_1_SHIFT 8

#define VBIOS_VFE_3X_VAR_ENTRY_PAR1_SSFUSE_DEFAULT_VAL_MASK 0xFFFFFFFF
#define VBIOS_VFE_3X_VAR_ENTRY_PAR1_SSFUSE_DEFAULT_VAL_SHIFT 0

#define VBIOS_VFE_3X_VAR_ENTRY_PAR1_SSFUSE_HW_CORRECTION_SCALE_MASK 0xFFFFFFFF
#define VBIOS_VFE_3X_VAR_ENTRY_PAR1_SSFUSE_HW_CORRECTION_SCALE_SHIFT 0

#define VBIOS_VFE_3X_VAR_ENTRY_PAR1_SSFUSE_HW_CORRECTION_OFFSET_MASK 0xFFFFFFFF
#define VBIOS_VFE_3X_VAR_ENTRY_PAR1_SSFUSE_HW_CORRECTION_OFFSET_SHIFT 0

#define VBIOS_VFE_3X_EQU_ENTRY_SIZE_17                                      0x17
#define VBIOS_VFE_3X_EQU_ENTRY_SIZE_18                                      0x18

struct vbios_vfe_3x_equ_entry_struct {
	u8 type;
	u8 var_idx;
	u8 equ_idx_next;
	u32 out_range_min;
	u32 out_range_max;
	u32 param0;
	u32 param1;
	u32 param2;
	u8 param3;
} __packed;


#define VBIOS_VFE_3X_EQU_ENTRY_TYPE_DISABLED                                0x00
#define VBIOS_VFE_3X_EQU_ENTRY_TYPE_QUADRATIC                               0x01
#define VBIOS_VFE_3X_EQU_ENTRY_TYPE_MINMAX                                  0x02
#define VBIOS_VFE_3X_EQU_ENTRY_TYPE_COMPARE                                 0x03
#define VBIOS_VFE_3X_EQU_ENTRY_TYPE_QUADRATIC_FXP                           0x04
#define VBIOS_VFE_3X_EQU_ENTRY_TYPE_MINMAX_FXP                              0x05

#define VBIOS_VFE_3X_EQU_ENTRY_IDX_INVALID                                  0xFF

#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_QUADRATIC_C0_MASK 0xFFFFFFFF
#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_QUADRATIC_C0_SHIFT 0

#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_VFE_EQU_IDX_0_MASK 0xFF
#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_VFE_EQU_IDX_0_SHIFT 0

#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_VFE_EQU_IDX_1_MASK 0xFF00
#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_VFE_EQU_IDX_1_SHIFT 8

#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_CRIT_MASK 0x10000
#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_CRIT_SHIFT 16
#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_CRIT_MIN 0x00000000
#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_CRIT_MAX 0x00000001

#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_COMPARE_CRIT_MASK 0xFFFFFFFF
#define VBIOS_VFE_3X_EQU_ENTRY_PAR0_COMPARE_CRIT_SHIFT 0

#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_QUADRATIC_C1_MASK 0xFFFFFFFF
#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_QUADRATIC_C1_SHIFT 0

#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_VFE_EQU_IDX_TRUE_MASK 0xFF
#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_VFE_EQU_IDX_TRUE_SHIFT 0

#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_VFE_EQU_IDX_FALSE_MASK 0xFF00
#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_VFE_EQU_IDX_FALSE_SHIFT 8

#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_MASK 0x70000
#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_SHIFT 16
#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_EQUAL 0x00000000
#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_GREATER_EQ 0x00000001
#define VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_GREATER 0x00000002

#define VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_MASK 0xF
#define VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_SHIFT 0
#define VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_UNITLESS                     0x0
#define VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_FREQ_MHZ                     0x1
#define VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_VOLT_UV                      0x2
#define VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_VF_GAIN                      0x3
#define VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_VOLT_DELTA_UV                0x4

#define NV_VFIELD_DESC_SIZE_BYTE            0x00000000
#define NV_VFIELD_DESC_SIZE_WORD            0x00000001
#define NV_VFIELD_DESC_SIZE_DWORD           0x00000002
#define VFIELD_SIZE(pvregentry) ((pvregentry->strap_reg_desc & 0x18) >> 3)

#define NV_PMU_BIOS_VFIELD_DESC_CODE_INVALID         0x00000000
#define NV_PMU_BIOS_VFIELD_DESC_CODE_REG             0x00000001
#define NV_PMU_BIOS_VFIELD_DESC_CODE_INDEX_REG       0x00000002

#define NV_VFIELD_DESC_CODE_INVALID         NV_PMU_BIOS_VFIELD_DESC_CODE_INVALID
#define NV_VFIELD_DESC_CODE_REG             NV_PMU_BIOS_VFIELD_DESC_CODE_REG
#define NV_VFIELD_DESC_CODE_INDEX_REG       NV_PMU_BIOS_VFIELD_DESC_CODE_INDEX_REG

#define VFIELD_CODE(pvregentry) ((pvregentry->strap_reg_desc & 0xE0) >> 5)

#define VFIELD_ID_STRAP_IDDQ                    0x09
#define VFIELD_ID_STRAP_IDDQ_1                  0x0B

#define VFIELD_REG_HEADER_SIZE 3
struct vfield_reg_header {
	u8 version;
	u8 entry_size;
	u8 count;
} __packed;

#define VBIOS_VFIELD_REG_TABLE_VERSION_1_0  0x10


#define VFIELD_REG_ENTRY_SIZE 13
struct vfield_reg_entry {
	u8 strap_reg_desc;
	u32 reg;
	u32 reg_index;
	u32 index;
} __packed;

#define VFIELD_HEADER_SIZE 3

struct vfield_header {
	u8 version;
	u8 entry_size;
	u8 count;
} __packed;

#define VBIOS_VFIELD_TABLE_VERSION_1_0  0x10

#define VFIELD_BIT_START(ventry) (ventry.strap_desc & 0x1F)
#define VFIELD_BIT_STOP(ventry)	((ventry.strap_desc & 0x3E0) >> 5)
#define VFIELD_BIT_REG(ventry) ((ventry.strap_desc & 0x3C00) >> 10)

#define VFIELD_ENTRY_SIZE 3

struct vfield_entry {
	u8 strap_id;
	u16 strap_desc;
} __packed;

#define PERF_CLK_DOMAINS_IDX_MAX		(32)
#define PERF_CLK_DOMAINS_IDX_INVALID		PERF_CLK_DOMAINS_IDX_MAX

#define VBIOS_PSTATE_TABLE_VERSION_5X		0x50
#define VBIOS_PSTATE_HEADER_5X_SIZE_10		(10)

struct vbios_pstate_header_5x {
	u8 version;
	u8 header_size;
	u8 base_entry_size;
	u8 base_entry_count;
	u8 clock_entry_size;
	u8 clock_entry_count;
	u8 flags0;
	u8 initial_pstate;
	u8 cpi_support_level;
u8 cpi_features;
} __packed;

#define VBIOS_PSTATE_CLOCK_ENTRY_5X_SIZE_6	6

#define VBIOS_PSTATE_BASE_ENTRY_5X_SIZE_2	0x2
#define VBIOS_PSTATE_BASE_ENTRY_5X_SIZE_3	0x3

struct vbios_pstate_entry_clock_5x {
	u16 param0;
	u32 param1;
} __packed;

struct vbios_pstate_entry_5x {
	u8 pstate_level;
	u8 flags0;
	u8 lpwr_entry_idx;
	struct vbios_pstate_entry_clock_5x clockEntry[PERF_CLK_DOMAINS_IDX_MAX];
} __packed;

#define VBIOS_PSTATE_5X_CLOCK_PROG_PARAM0_NOM_FREQ_MHZ_SHIFT	0
#define VBIOS_PSTATE_5X_CLOCK_PROG_PARAM0_NOM_FREQ_MHZ_MASK	0x00003FFF

#define VBIOS_PSTATE_5X_CLOCK_PROG_PARAM1_MIN_FREQ_MHZ_SHIFT	0
#define VBIOS_PSTATE_5X_CLOCK_PROG_PARAM1_MIN_FREQ_MHZ_MASK	0x00003FFF

#define VBIOS_PSTATE_5X_CLOCK_PROG_PARAM1_MAX_FREQ_MHZ_SHIFT	14
#define VBIOS_PSTATE_5X_CLOCK_PROG_PARAM1_MAX_FREQ_MHZ_MASK	0x0FFFC000

#define VBIOS_PERFLEVEL_SKIP_ENTRY				0xFF

#define VBIOS_MEMORY_CLOCK_HEADER_11_VERSION				0x11

#define VBIOS_MEMORY_CLOCK_HEADER_11_0_SIZE				16
#define VBIOS_MEMORY_CLOCK_HEADER_11_1_SIZE				21
#define VBIOS_MEMORY_CLOCK_HEADER_11_2_SIZE				26

struct vbios_memory_clock_header_1x {
	u8 version;
	u8 header_size;
	u8 base_entry_size;
	u8 strap_entry_size;
	u8 strap_entry_count;
	u8 entry_count;
	u8 flags;
	u8 fbvdd_settle_time;
	u32 cfg_pwrd_val;
	u16 fbvddq_high;
	u16 fbvddq_low;
	u32 script_list_ptr;
	u8 script_list_count;
	u32 cmd_script_list_ptr;
	u8 cmd_script_list_count;
} __packed;

#define VBIOS_MEMORY_CLOCK_BASE_ENTRY_11_2_SIZE				20

struct vbios_memory_clock_base_entry_11 {
	u16 minimum;
	u16 maximum;
	u32 script_pointer;
	u8 flags0;
	u32 fbpa_config;
	u32 fbpa_config1;
	u8 flags1;
	u8 ref_mpllssf_freq_delta;
	u8 flags2;
} __packed;

/* Script Pointer Index */
/* #define VBIOS_MEMORY_CLOCK_BASE_ENTRY_11_FLAGS1_SCRIPT_INDEX		3:2*/
#define VBIOS_MEMORY_CLOCK_BASE_ENTRY_11_FLAGS1_SCRIPT_INDEX_MASK	0xc
#define VBIOS_MEMORY_CLOCK_BASE_ENTRY_11_FLAGS1_SCRIPT_INDEX_SHIFT	2
/* #define VBIOS_MEMORY_CLOCK_BASE_ENTRY_12_FLAGS2_CMD_SCRIPT_INDEX	1:0*/
#define VBIOS_MEMORY_CLOCK_BASE_ENTRY_12_FLAGS2_CMD_SCRIPT_INDEX_MASK	0x3
#define VBIOS_MEMORY_CLOCK_BASE_ENTRY_12_FLAGS2_CMD_SCRIPT_INDEX_SHIFT	0

/* Voltage Rail Table */
struct vbios_voltage_rail_table_1x_header {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
	u8 volt_domain_hal;
} __packed;

#define NV_VBIOS_VOLTAGE_RAIL_1X_ENTRY_SIZE_07		0X00000007
#define NV_VBIOS_VOLTAGE_RAIL_1X_ENTRY_SIZE_08		0X00000008
#define NV_VBIOS_VOLTAGE_RAIL_1X_ENTRY_SIZE_09		0X00000009
#define NV_VBIOS_VOLTAGE_RAIL_1X_ENTRY_SIZE_0A		0X0000000A
#define NV_VBIOS_VOLTAGE_RAIL_1X_ENTRY_SIZE_0B		0X0000000B

struct vbios_voltage_rail_table_1x_entry {
	u32 boot_voltage_uv;
	u8 rel_limit_vfe_equ_idx;
	u8 alt_rel_limit_vfe_equidx;
	u8 ov_limit_vfe_equ_idx;
	u8 pwr_equ_idx;
	u8 boot_volt_vfe_equ_idx;
	u8 vmin_limit_vfe_equ_idx;
	u8 volt_margin_limit_vfe_equ_idx;
} __packed;

/* Voltage Device Table */
struct vbios_voltage_device_table_1x_header {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
} __packed;

struct vbios_voltage_device_table_1x_entry {
	u8 type;
	u8 volt_domain;
	u16 settle_time_us;
	u32 param0;
	u32 param1;
	u32 param2;
	u32 param3;
	u32 param4;
} __packed;

#define NV_VBIOS_VOLTAGE_DEVICE_1X_ENTRY_TYPE_INVALID	0x00
#define NV_VBIOS_VOLTAGE_DEVICE_1X_ENTRY_TYPE_PSV		0x02

#define NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_INPUT_FREQUENCY_MASK	\
		GENMASK(23, 0)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_INPUT_FREQUENCY_SHIFT	0
#define NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_EXT_DEVICE_INDEX_MASK	\
	GENMASK(31, 24)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_EXT_DEVICE_INDEX_SHIFT	24

#define NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_VOLTAGE_MINIMUM_MASK	\
		GENMASK(23, 0)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_VOLTAGE_MINIMUM_SHIFT	0
#define NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_MASK	\
	GENMASK(31, 24)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_SHIFT		24
#define NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_DEFAULT	0x00
#define NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_LPWR_STEADY_STATE \
		0x01
#define NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_LPWR_SLEEP_STATE \
		0x02
#define NV_VBIOS_VDT_1X_ENTRY_PARAM2_PSV_VOLTAGE_MAXIMUM_MASK	\
		GENMASK(23, 0)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM2_PSV_VOLTAGE_MAXIMUM_SHIFT	0
#define NV_VBIOS_VDT_1X_ENTRY_PARAM2_PSV_RSVD_MASK		\
		GENMASK(31, 24)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM2_PSV_RSVD_SHIFT		24

#define NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_BASE_MASK	\
		GENMASK(23, 0)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_BASE_SHIFT	0
#define NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_STEPS_MASK	\
		GENMASK(31, 24)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_STEPS_SHIFT	24

#define NV_VBIOS_VDT_1X_ENTRY_PARAM4_PSV_OFFSET_SCALE_MASK \
		GENMASK(23, 0)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM4_PSV_OFFSET_SCALE_SHIFT	0
#define NV_VBIOS_VDT_1X_ENTRY_PARAM4_PSV_RSVD_MASK		\
		GENMASK(31, 24)
#define NV_VBIOS_VDT_1X_ENTRY_PARAM4_PSV_RSVD_SHIFT	24

/* Voltage Policy Table */
struct vbios_voltage_policy_table_1x_header {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
	u8 perf_core_vf_seq_policy_idx;
} __packed;

struct vbios_voltage_policy_table_1x_entry {
	u8 type;
	u32 param0;
	u32 param1;
} __packed;

#define NV_VBIOS_VOLTAGE_POLICY_1X_ENTRY_TYPE_INVALID		0x00
#define NV_VBIOS_VOLTAGE_POLICY_1X_ENTRY_TYPE_SINGLE_RAIL	0x01
#define NV_VBIOS_VOLTAGE_POLICY_1X_ENTRY_TYPE_SR_MULTI_STEP	0x02
#define NV_VBIOS_VOLTAGE_POLICY_1X_ENTRY_TYPE_SR_SINGLE_STEP	0x03

#define NV_VBIOS_VPT_ENTRY_PARAM0_SINGLE_RAIL_VOLT_DOMAIN_MASK \
		GENMASK(7, 0)
#define NV_VBIOS_VPT_ENTRY_PARAM0_SINGLE_RAIL_VOLT_DOMAIN_SHIFT	0
#define NV_VBIOS_VPT_ENTRY_PARAM0_RSVD_MASK	GENMASK(8, 31)
#define NV_VBIOS_VPT_ENTRY_PARAM0_RSVD_SHIFT	8

#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_VD_MASTER_MASK \
		GENMASK(7, 0)
#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_VD_MASTER_SHIFT 0
#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_VD_SLAVE_MASK \
		GENMASK(15, 8)
#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_VD_SLAVE_SHIFT 8
#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_DELTA_SM_MIN_MASK \
		GENMASK(23, 16)
#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_DELTA_SM_MIN_SHIFT 16
#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_DELTA_SM_MAX_MASK \
		GENMASK(31, 24)
#define NV_VBIOS_VPT_ENTRY_PARAM0_SR_DELTA_SM_MAX_SHIFT 24

/* Type-Specific Parameter DWORD 0 - Type = _SR_MULTI_STEP */
#define NV_VBIOS_VPT_ENTRY_PARAM1_SR_SETTLE_TIME_INTERMEDIATE_MASK \
		GENMASK(15, 0)
#define NV_VBIOS_VPT_ENTRY_PARAM1_SR_SETTLE_TIME_INTERMEDIATE_SHIFT \
		0

#define VBIOS_POWER_SENSORS_VERSION_2X                                      0x20
#define VBIOS_POWER_SENSORS_2X_HEADER_SIZE_08                         0x00000008

struct pwr_sensors_2x_header {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
	u32 ba_script_pointer;
} __packed;

#define VBIOS_POWER_SENSORS_2X_ENTRY_SIZE_15                          0x00000015

struct pwr_sensors_2x_entry {
	u8 flags0;
	u32 class_param0;
	u32 sensor_param0;
	u32 sensor_param1;
	u32 sensor_param2;
	u32 sensor_param3;
} __packed;

#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_FLAGS0_CLASS_MASK                   0xF
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_FLAGS0_CLASS_SHIFT                    0
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_FLAGS0_CLASS_I2C              0x00000001

#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_CLASS_PARAM0_I2C_INDEX_MASK        0xFF
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_CLASS_PARAM0_I2C_INDEX_SHIFT          0
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_CLASS_PARAM0_I2C_USE_FXP8_8_MASK  0x100
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_CLASS_PARAM0_I2C_USE_FXP8_8_SHIFT  8

#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM0_INA3221_RSHUNT0_MOHM_MASK  0xFFFF
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM0_INA3221_RSHUNT0_MOHM_SHIFT  0
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM0_INA3221_RSHUNT1_MOHM_MASK  0xFFFF0000
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM0_INA3221_RSHUNT1_MOHM_SHIFT  16
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM1_INA3221_RSHUNT2_MOHM_MASK   0xFFFF
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM1_INA3221_RSHUNT2_MOHM_SHIFT   0
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM1_INA3221_CONFIGURATION_MASK  0xFFFF0000
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM1_INA3221_CONFIGURATION_SHIFT  16

#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM2_INA3221_MASKENABLE_MASK    0xFFFF
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM2_INA3221_MASKENABLE_SHIFT    0
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM2_INA3221_GPIOFUNCTION_MASK   0xFF0000
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM2_INA3221_GPIOFUNCTION_SHIFT   16
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM3_INA3221_CURR_CORRECT_M_MASK  0xFFFF
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM3_INA3221_CURR_CORRECT_M_SHIFT  0
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM3_INA3221_CURR_CORRECT_B_MASK  0xFFFF0000
#define NV_VBIOS_POWER_SENSORS_2X_ENTRY_SENSOR_PARAM3_INA3221_CURR_CORRECT_B_SHIFT  16

#define VBIOS_POWER_TOPOLOGY_VERSION_2X                                      0x20
#define VBIOS_POWER_TOPOLOGY_2X_HEADER_SIZE_06                         0x00000006

struct pwr_topology_2x_header {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
	u8 rel_entry_size;
	u8 num_rel_entries;
} __packed;

#define VBIOS_POWER_TOPOLOGY_2X_ENTRY_SIZE_16                          0x00000016

struct pwr_topology_2x_entry {
	u8 flags0;
	u8 pwr_rail;
	u32 param0;
	u32 curr_corr_slope;
	u32 curr_corr_offset;
	u32 param1;
	u32 param2;
} __packed;

#define NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_FLAGS0_CLASS_MASK                  0xF
#define NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_FLAGS0_CLASS_SHIFT                   0
#define NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_FLAGS0_CLASS_SENSOR                0x00000001

#define NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_PARAM1_SENSOR_INDEX_MASK          0xFF
#define NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_PARAM1_SENSOR_INDEX_SHIFT            0
#define NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_PARAM1_SENSOR_PROVIDER_INDEX_MASK 0xFF00
#define NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_PARAM1_SENSOR_PROVIDER_INDEX_SHIFT   8

#define VBIOS_POWER_POLICY_VERSION_3X                                       0x30
#define VBIOS_POWER_POLICY_3X_HEADER_SIZE_25                          0x00000025

struct pwr_policy_3x_header_struct {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
	u16 base_sample_period;
	u16 min_client_sample_period;
	u8 table_rel_entry_size;
	u8 num_table_rel_entries;
	u8 tgp_policy_idx;
	u8 rtp_policy_idx;
	u8 mxm_policy_idx;
	u8 dnotifier_policy_idx;
	u32 d2_limit;
	u32 d3_limit;
	u32 d4_limit;
	u32 d5_limit;
	u8 low_sampling_mult;
	u8 pwr_tgt_policy_idx;
	u8 pwr_tgt_floor_policy_idx;
	u8 sm_bus_policy_idx;
	u8 table_viol_entry_size;
	u8 num_table_viol_entries;
} __packed;

#define VBIOS_POWER_POLICY_3X_ENTRY_SIZE_2E                           0x0000002E

struct pwr_policy_3x_entry_struct {
	u8 flags0;
	u8 ch_idx;
	u32 limit_min;
	u32 limit_rated;
	u32 limit_max;
	u32 param0;
	u32 param1;
	u32 param2;
	u32 param3;
	u32 limit_batt;
	u8 flags1;
	u8 past_length;
	u8 next_length;
	u16 ratio_min;
	u16 ratio_max;
	u8 sample_mult;
	u32 filter_param;
} __packed;

#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS0_CLASS_MASK                    0xF
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS0_CLASS_SHIFT                    0
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS0_CLASS_HW_THRESHOLD        0x00000005
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS0_LIMIT_UNIT_MASK              0x10
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS0_LIMIT_UNIT_SHIFT                4

#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS1_FULL_DEFLECTION_LIMIT_MASK    0x1
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS1_FULL_DEFLECTION_LIMIT_SHIFT     0
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS1_INTEGRAL_CONTROL_MASK         0x2
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS1_INTEGRAL_CONTROL_SHIFT          1
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS1_FILTER_TYPE_MASK             0x3C
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_FLAGS1_FILTER_TYPE_SHIFT               2

#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM0_HW_THRESHOLD_THRES_IDX_MASK  0xFF
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM0_HW_THRESHOLD_THRES_IDX_SHIFT    0
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM0_HW_THRESHOLD_LOW_THRESHOLD_IDX_MASK 0xFF00
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM0_HW_THRESHOLD_LOW_THRESHOLD_IDX_SHIFT 8
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM0_HW_THRESHOLD_LOW_THRESHOLD_USE_MASK 0x10000
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM0_HW_THRESHOLD_LOW_THRESHOLD_USE_SHIFT 16

#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM1_HW_THRESHOLD_LOW_THRESHOLD_VAL_MASK 0xFFFF
#define NV_VBIOS_POWER_POLICY_3X_ENTRY_PARAM1_HW_THRESHOLD_LOW_THRESHOLD_VAL_SHIFT 0

#define VBIOS_THERM_DEVICE_VERSION_1X                                      0x10

#define VBIOS_THERM_DEVICE_1X_HEADER_SIZE_04                         0x00000004

struct therm_device_1x_header {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
} ;

struct therm_device_1x_entry {
	u8 class_id;
	u8 param0;
	u8 flags;
} ;

#define NV_VBIOS_THERM_DEVICE_1X_ENTRY_CLASS_GPU                               0x01

#define NV_VBIOS_THERM_DEVICE_1X_ENTRY_PARAM0_I2C_DEVICE_INDEX_MASK        0xFF
#define NV_VBIOS_THERM_DEVICE_1X_ENTRY_PARAM0_I2C_DEVICE_INDEX_SHIFT          0

#define VBIOS_THERM_CHANNEL_VERSION_1X                                     0x10

#define VBIOS_THERM_CHANNEL_1X_HEADER_SIZE_09                        0x00000009

struct therm_channel_1x_header {
	u8 version;
	u8 header_size;
	u8 table_entry_size;
	u8 num_table_entries;
	u8 gpu_avg_pri_ch_idx;
	u8 gpu_max_pri_ch_idx;
	u8 board_pri_ch_idx;
	u8 mem_pri_ch_idx;
	u8 pwr_supply_pri_ch_idx;
} __packed;

struct therm_channel_1x_entry {
	u8 class_id;
	u8 param0;
	u8 param1;
	u8 param2;
	u8 flags;
} __packed;

#define NV_VBIOS_THERM_CHANNEL_1X_ENTRY_CLASS_DEVICE                       0x01

#define NV_VBIOS_THERM_CHANNEL_1X_ENTRY_PARAM0_DEVICE_INDEX_MASK           0xFF
#define NV_VBIOS_THERM_CHANNEL_1X_ENTRY_PARAM0_DEVICE_INDEX_SHIFT             0

#define NV_VBIOS_THERM_CHANNEL_1X_ENTRY_PARAM1_DEVICE_PROVIDER_INDEX_MASK  0xFF
#define NV_VBIOS_THERM_CHANNEL_1X_ENTRY_PARAM1_DEVICE_PROVIDER_INDEX_SHIFT    0

/* Frequency Controller Table */
struct vbios_fct_1x_header {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u16 sampling_period_ms;
} __packed;

struct vbios_fct_1x_entry {
	u8 flags0;
	u8 clk_domain_idx;
	u16 param0;
	u16 param1;
	u32 param2;
	u32 param3;
	u32 param4;
	u32 param5;
	u32 param6;
	u32 param7;
	u32 param8;
} __packed;

#define NV_VBIOS_FCT_1X_ENTRY_FLAGS0_TYPE_MASK GENMASK(3, 0)
#define NV_VBIOS_FCT_1X_ENTRY_FLAGS0_TYPE_SHIFT 0
#define NV_VBIOS_FCT_1X_ENTRY_FLAGS0_TYPE_DISABLED 0x0
#define NV_VBIOS_FCT_1X_ENTRY_FLAGS0_TYPE_PI       0x1

#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_MASK GENMASK(7, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_SHIFT 0
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_SYS   0x00
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_LTC   0x01
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_XBAR  0x02
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_GPC0  0x03
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_GPC1  0x04
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_GPC2  0x05
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_GPC3  0x06
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_GPC4  0x07
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_GPC5  0x08
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID_GPCS  0x09

#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_FREQ_MODE_MASK GENMASK(9, 8)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_FREQ_MODE_SHIFT 8
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_FREQ_MODE_BCAST 0x0
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_FREQ_MODE_MIN   0x1
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_FREQ_MODE_MAX   0x2
#define NV_VBIOS_FCT_1X_ENTRY_PARAM0_FREQ_MODE_AVG   0x3

#define NV_VBIOS_FCT_1X_ENTRY_PARAM1_SLOWDOWN_PCT_MIN_MASK GENMASK(7, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM1_SLOWDOWN_PCT_MIN_SHIFT 0

#define NV_VBIOS_FCT_1X_ENTRY_PARAM1_POISON_MASK GENMASK(8, 8)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM1_POISON_SHIFT 8
#define NV_VBIOS_FCT_1X_ENTRY_PARAM1_POISON_NO  0x0
#define NV_VBIOS_FCT_1X_ENTRY_PARAM1_POISON_YES 0x1

#define NV_VBIOS_FCT_1X_ENTRY_PARAM2_PROP_GAIN_MASK GENMASK(31, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM2_PROP_GAIN_SHIFT 0

#define NV_VBIOS_FCT_1X_ENTRY_PARAM3_INTEG_GAIN_MASK GENMASK(31, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM3_INTEG_GAIN_SHIFT 0


#define NV_VBIOS_FCT_1X_ENTRY_PARAM4_INTEG_DECAY_MASK GENMASK(31, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM4_INTEG_DECAY_SHIFT 0

#define NV_VBIOS_FCT_1X_ENTRY_PARAM5_VOLT_DELTA_MIN_MASK GENMASK(31, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM5_VOLT_DELTA_MIN_SHIFT 0


#define NV_VBIOS_FCT_1X_ENTRY_PARAM6_VOLT_DELTA_MAX_MASK GENMASK(31, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM6_VOLT_DELTA_MAX_SHIFT 0

#define NV_VBIOS_FCT_1X_ENTRY_PARAM7_FREQ_CAP_VF_MASK GENMASK(15, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM7_FREQ_CAP_VF_SHIFT 0
#define NV_VBIOS_FCT_1X_ENTRY_PARAM7_FREQ_CAP_VMIN_MASK GENMASK(31, 16)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM7_FREQ_CAP_VMIN_SHIFT 16

#define NV_VBIOS_FCT_1X_ENTRY_PARAM8_FREQ_HYST_POS_MASK GENMASK(15, 0)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM8_FREQ_HYST_POS_SHIFT 0
#define NV_VBIOS_FCT_1X_ENTRY_PARAM8_FREQ_HYST_NEG_MASK GENMASK(31, 16)
#define NV_VBIOS_FCT_1X_ENTRY_PARAM8_FREQ_HYST_NEG_SHIFT 16

/* LPWR Index Table */
struct nvgpu_bios_lpwr_idx_table_1x_header {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u16 base_sampling_period;
} __packed;

struct nvgpu_bios_lpwr_idx_table_1x_entry {
	u8 pcie_idx;
	u8 gr_idx;
	u8 ms_idx;
	u8 di_idx;
	u8 gc6_idx;
} __packed;

/* LPWR MS Table*/
struct nvgpu_bios_lpwr_ms_table_1x_header {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u8 default_entry_idx;
	u16 idle_threshold_us;
} __packed;

struct nvgpu_bios_lpwr_ms_table_1x_entry {
	u32 feautre_mask;
	u16 dynamic_current_logic;
	u16 dynamic_current_sram;
} __packed;

#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_MASK    GENMASK(0, 0)
#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_SHIFT    0
#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_SWASR_MASK    GENMASK(2, 2)
#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_SWASR_SHIFT    2
#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_CLOCK_GATING_MASK    \
			GENMASK(3, 3)
#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_CLOCK_GATING_SHIFT    3
#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_RPPG_MASK    GENMASK(5, 5)
#define NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_RPPG_SHIFT    5

/* LPWR GR Table */
struct nvgpu_bios_lpwr_gr_table_1x_header {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u8 default_entry_idx;
	u16 idle_threshold_us;
	u8 adaptive_gr_multiplier;
} __packed;

struct nvgpu_bios_lpwr_gr_table_1x_entry {
	u32 feautre_mask;
} __packed;

#define NV_VBIOS_LPWR_GR_FEATURE_MASK_GR_MASK GENMASK(0, 0)
#define NV_VBIOS_LPWR_GR_FEATURE_MASK_GR_SHIFT 0

#define NV_VBIOS_LPWR_GR_FEATURE_MASK_GR_RPPG_MASK GENMASK(4, 4)
#define NV_VBIOS_LPWR_GR_FEATURE_MASK_GR_RPPG_SHIFT 4

#endif
