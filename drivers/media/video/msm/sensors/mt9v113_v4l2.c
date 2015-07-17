/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_ispif.h"
#define SENSOR_NAME "mt9v113"
#define PLATFORM_DRIVER_NAME "msm_camera_mt9v113"
#define mt9v113_obj mt9v113_##obj
// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
#define SENSOR_FIXED_FPS_15		1
#define SENSOR_FIXED_FPS_30		2
#define SENSOR_AUTO_FPS_1030	3
#define SENSOR_AUTO_FPS_0730	4
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }

DEFINE_MUTEX(mt9v113_mut);
static struct msm_sensor_ctrl_t mt9v113_s_ctrl;

// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
#ifdef CONFIG_MACH_LGE
static int camera_started;
static int prev_balance_mode;
static int prev_effect_mode;
static int prev_brightness_mode;
static int prev_fps_mode;
#endif
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }

static struct msm_camera_i2c_reg_conf mt9v113_start_settings[] = {
	{0x0018, 0x4028},	// john.park, 2011-05-09, To replace Polling check
	{0xFFFF, 0x0064},	// Delay = 100ms
//	{0xFFFD, 0x4028},	// For polling
};

static struct msm_camera_i2c_reg_conf mt9v113_stop_settings[] = {
	{0x0018, 0x4029},	// john.park, 2011-05-09, To replace Polling check
	{0xFFFF, 0x0064},	// Delay = 100ms
//	{0xFFFD, 0x4029},	// For polling
};

static struct msm_camera_i2c_reg_conf mt9v113_groupon_settings[] = {
//	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf mt9v113_groupoff_settings[] = {
//	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf mt9v113_prev_settings[] = {
};

static struct msm_camera_i2c_reg_conf mt9v113_recommend_settings[] = {
	{0x0018, 0x4028},	// STANDBY_CONTROL
	{0x001A, 0x0013},	// RESET_AND_MISC_CONTROL
	{0xFFFF, 0x000A},	// Delay
	{0x001A, 0x0010},	// RESET_AND_MISC_CONTROL
	{0xFFFF, 0x000A},	// Delay
	{0x0018, 0x4028},	// STANDBY_CONTROL
	{0x098C, 0x02F0},	{0x0990, 0x0000},
	{0x098C, 0x02F2},	{0x0990, 0x0210},
	{0x098C, 0x02F4},	{0x0990, 0x001A},
	{0x098C, 0x2145},	{0x0990, 0x02F4},
	{0x098C, 0xA134},	{0x0990, 0x0001},
	{0x31E0, 0x0001},	// CORE_31E0
	{0xFFFF, 0x000A},	// Delay
	{0x3400, 0x783C},	// MIPI_CONTROL
	{0x001A, 0x0210},	// RESET_AND_MISC_CONTROL
	{0x001E, 0x0400},	// PAD_SLEW
	{0x0016, 0x42DF},	// CLOCKS_CONTROL
	{0x0014, 0x2145},	// PLL_CONTROL
	{0x0014, 0x2145},	// PLL_CONTROL
	{0x0010, 0x021A},	// 21c PLL_DIVIDERS
	{0x0012, 0x0000},	// PLL_P_DIVIDERS
	{0x0014, 0x244B},	// PLL_CONTROL
	{0xFFFF, 0x000A},	// Delay
	{0x0014, 0x304B},	// PLL_CONTROL
	{0xFFFF, 0x0032},	// Delay
	{0x0014, 0xB04A},	// PLL_CONTROL
	{0xFFFF, 0x0012},	// Delay
	{0x3400, 0x7A3C},	// MIPI_CONTROL
	{0x001A, 0x0010},
	{0x001A, 0x0018},
	{0x321C, 0x0003},	// OFIFO_CONTROL_STATUS
	{0x098C, 0x2703},	{0x0990, 0x0280},	// Output Width (A) = 640
	{0x098C, 0x2705},	{0x0990, 0x01E0},	// Output Height (A) = 480
	{0x098C, 0x2707},	{0x0990, 0x0280},	// Output Width (B) = 640
	{0x098C, 0x2709},	{0x0990, 0x01E0},	// Output Height (B) = 480
	{0x098C, 0x270D},	{0x0990, 0x0000},	// Row Start (A) = 0
	{0x098C, 0x270F},	{0x0990, 0x0000},	// Column Start (A) = 0
	{0x098C, 0x2711},	{0x0990, 0x01E7},	// Row End (A) = 487
	{0x098C, 0x2713},	{0x0990, 0x0287},	// Column End (A) = 647
	{0x098C, 0x2715},	{0x0990, 0x0001},	// Row Speed (A) = 1
#ifdef CONFIG_MACH_MSM8960_L0
	{0x098C, 0x2717},	{0x0990, 0x0026},	// Read Mode (A) = 38
#else
	{0x098C, 0x2717},	{0x0990, 0x0025},	// Read Mode (A) = 37
#endif
	{0x098C, 0x2719},	{0x0990, 0x001A},	// sensor_fine_correction (A) = 26
	{0x098C, 0x271B},	{0x0990, 0x006B},	// sensor_fine_IT_min (A) = 107
	{0x098C, 0x271D},	{0x0990, 0x006B},	// sensor_fine_IT_max_margin (A) = 107
	{0x098C, 0x271F},	{0x0990, 0x01FF},	// Frame Lines (A) = 554
	{0x098C, 0x2721},	{0x0990, 0x0350},	// Line Length (A) = 842
	{0x098C, 0x2723},	{0x0990, 0x0000},	// Row Start (B) = 0
	{0x098C, 0x2725},	{0x0990, 0x0000},	// Column Start (B) = 0
	{0x098C, 0x2727},	{0x0990, 0x01E7},	// Row End (B) = 487
	{0x098C, 0x2729},	{0x0990, 0x0287},	// Column End (B) = 647
	{0x098C, 0x272B},	{0x0990, 0x0001},	// Row Speed (B) = 1
#ifdef CONFIG_MACH_MSM8960_L0
	{0x098C, 0x272D},	{0x0990, 0x0026},	// Read Mode (B) = 38
#else
	{0x098C, 0x272D},	{0x0990, 0x0025},	// Read Mode (B) = 37
#endif
	{0x098C, 0x272F},	{0x0990, 0x001A},	// sensor_fine_correction (B) = 26
	{0x098C, 0x2731},	{0x0990, 0x006B},	// sensor_fine_IT_min (B) = 107
	{0x098C, 0x2733},	{0x0990, 0x006B},	// sensor_fine_IT_max_margin (B) = 107
	{0x098C, 0x2735},	{0x0990, 0x01FF},	// Frame Lines (B) = 1108
	{0x098C, 0x2737},	{0x0990, 0x0350},	// Line Length (B) = 842
	{0x098C, 0x2739},	{0x0990, 0x0000},	// Crop_X0 (A) = 0
	{0x098C, 0x273B},	{0x0990, 0x027F},	// Crop_X1 (A) = 639
	{0x098C, 0x273D},	{0x0990, 0x0000},	// Crop_Y0 (A) = 0
	{0x098C, 0x273F},	{0x0990, 0x01DF},	// Crop_Y1 (A) = 479
	{0x098C, 0x2747},	{0x0990, 0x0000},	// Crop_X0 (B) = 0
	{0x098C, 0x2749},	{0x0990, 0x027F},	// Crop_X1 (B) = 639
	{0x098C, 0x274B},	{0x0990, 0x0000},	// Crop_Y0 (B) = 0
	{0x098C, 0x274D},	{0x0990, 0x01DF},	// Crop_Y1 (B) = 479
	{0x098C, 0x222D},	{0x0990, 0x0080},	// R9 Step = 139
	{0x098C, 0xA408},	{0x0990, 0x001F},	// search_f1_50 = 33
	{0x098C, 0xA409},	{0x0990, 0x0021},	// search_f2_50 = 35
	{0x098C, 0xA40A},	{0x0990, 0x0025},	// search_f1_60 = 40
	{0x098C, 0xA40B},	{0x0990, 0x0027},	// search_f2_60 = 42
	{0x098C, 0x2411},	{0x0990, 0x0080},	// R9_Step_60 (A) = 139
	{0x098C, 0x2413},	{0x0990, 0x0099},	// R9_Step_50 (A) = 166
	{0x098C, 0x2415},	{0x0990, 0x0080},	// R9_Step_60 (B) = 139
	{0x098C, 0x2417},	{0x0990, 0x0099},	// R9_Step_50 (B) = 166
	{0x098C, 0xA404},	{0x0990, 0x0010},	// FD Mode = 16
	{0x098C, 0xA40D},	{0x0990, 0x0001},	// Stat_min = 2
	{0x098C, 0xA40E},	{0x0990, 0x0003},	// Stat_max = 3
	{0x098C, 0xA410},	{0x0990, 0x000A},	// Min_amplitude = 10
	{0x098C, 0xA20C},	{0x0990, 0x000C},	// AE_MAX_INDEX (12fps~: 0x000A, 10fps~: 0x000C, 7.5fps~: 0x0010)
	{0x098C, 0xAB37},	{0x0990, 0x0001},
	{0x098C, 0x2B38},	{0x0990, 0x1000},
	{0x098C, 0x2B3A},	{0x0990, 0x2000},
// AE_1_S
	{0x098C, 0xAB3C},	{0x0990, 0x0000},
	{0x098C, 0xAB3D},	{0x0990, 0x0017},
	{0x098C, 0xAB3E},	{0x0990, 0x0028},
	{0x098C, 0xAB3F},	{0x0990, 0x003D},
	{0x098C, 0xAB40},	{0x0990, 0x005B},
	{0x098C, 0xAB41},	{0x0990, 0x0074},
	{0x098C, 0xAB42},	{0x0990, 0x0089},
	{0x098C, 0xAB43},	{0x0990, 0x009B},
	{0x098C, 0xAB44},	{0x0990, 0x00AA},
	{0x098C, 0xAB45},	{0x0990, 0x00B7},
	{0x098C, 0xAB46},	{0x0990, 0x00C3},
	{0x098C, 0xAB47},	{0x0990, 0x00CD},
	{0x098C, 0xAB48},	{0x0990, 0x00D6},
	{0x098C, 0xAB49},	{0x0990, 0x00DE},
	{0x098C, 0xAB4A},	{0x0990, 0x00E6},
	{0x098C, 0xAB4B},	{0x0990, 0x00ED},
	{0x098C, 0xAB4C},	{0x0990, 0x00F3},
	{0x098C, 0xAB4D},	{0x0990, 0x00F9},
	{0x098C, 0xAB4E},	{0x0990, 0x00FF},
// AE_1_E
// AE_2_S
//	{0x098C, 0xAB3C},	{0x0990, 0x0000},
//	{0x098C, 0xAB3D},	{0x0990, 0x0017},
//	{0x098C, 0xAB3E},	{0x0990, 0x0027},
//	{0x098C, 0xAB3F},	{0x0990, 0x003B},
//	{0x098C, 0xAB40},	{0x0990, 0x005A},
//	{0x098C, 0xAB41},	{0x0990, 0x0073},
//	{0x098C, 0xAB42},	{0x0990, 0x008A},
//	{0x098C, 0xAB43},	{0x0990, 0x009C},
//	{0x098C, 0xAB44},	{0x0990, 0x00AB},
//	{0x098C, 0xAB45},	{0x0990, 0x00B8},
//	{0x098C, 0xAB46},	{0x0990, 0x00C4},
//	{0x098C, 0xAB47},	{0x0990, 0x00CE},
//	{0x098C, 0xAB48},	{0x0990, 0x00D7},
//	{0x098C, 0xAB49},	{0x0990, 0x00DF},
//	{0x098C, 0xAB4A},	{0x0990, 0x00E6},
//	{0x098C, 0xAB4B},	{0x0990, 0x00ED},
//	{0x098C, 0xAB4C},	{0x0990, 0x00F4},
//	{0x098C, 0xAB4D},	{0x0990, 0x00F9},
//	{0x098C, 0xAB4E},	{0x0990, 0x00FF},
	{0x098C, 0xAB3C},	{0x0990, 0x0000},
	{0x098C, 0xAB3D},	{0x0990, 0x0017},
	{0x098C, 0xAB3E},	{0x0990, 0x0028},
	{0x098C, 0xAB3F},	{0x0990, 0x003D},
	{0x098C, 0xAB40},	{0x0990, 0x005B},
	{0x098C, 0xAB41},	{0x0990, 0x0074},
	{0x098C, 0xAB42},	{0x0990, 0x0089},
	{0x098C, 0xAB43},	{0x0990, 0x009B},
	{0x098C, 0xAB44},	{0x0990, 0x00AA},
	{0x098C, 0xAB45},	{0x0990, 0x00B7},
	{0x098C, 0xAB46},	{0x0990, 0x00C3},
	{0x098C, 0xAB47},	{0x0990, 0x00CD},
	{0x098C, 0xAB48},	{0x0990, 0x00D6},
	{0x098C, 0xAB49},	{0x0990, 0x00DE},
	{0x098C, 0xAB4A},	{0x0990, 0x00E6},
	{0x098C, 0xAB4B},	{0x0990, 0x00ED},
	{0x098C, 0xAB4C},	{0x0990, 0x00F3},
	{0x098C, 0xAB4D},	{0x0990, 0x00F9},
	{0x098C, 0xAB4E},	{0x0990, 0x00FF},
// AE_2_E
	{0x3658, 0x0130},
	{0x365A, 0x030D},
	{0x365C, 0x6B92},
	{0x365E, 0xE62E},
	{0x3660, 0x53B4},
	{0x3680, 0x1AED},
	{0x3682, 0x2A6A},
	{0x3684, 0xBA2B},
	{0x3686, 0x392F},
	{0x3688, 0xAD53},
	{0x36A8, 0x1453},
	{0x36AA, 0xC78D},
	{0x36AC, 0x4C35},
	{0x36AE, 0x7791},
	{0x36B0, 0x6797},
	{0x36D0, 0xE3F0},
	{0x36D2, 0xF8F0},
	{0x36D4, 0x5934},
	{0x36D6, 0x6AF2},
	{0x36D8, 0xFC98},
	{0x36F8, 0x2EB4},
	{0x36FA, 0x9972},
	{0x36FC, 0x5E33},
	{0x36FE, 0x07D8},
	{0x3700, 0x51DC},
/* LGE_CHANGE_S, Lens shading tuning, 2012-06-11, yt.kim@lge.com */
#ifdef CONFIG_MACH_MSM8960_L0
/* 85% Lens shading*/
	{ 0x364E, 0x01B0},// P_GR_P0Q0
	{ 0x3650, 0xB08B},// P_GR_P0Q1
	{ 0x3652, 0x0DF2},// P_GR_P0Q2
	{ 0x3654, 0xF010},// P_GR_P0Q3
	{ 0x3656, 0x28F2},// P_GR_P0Q4
	{ 0x3658, 0x00B0},// P_RD_P0Q0
	{ 0x365A, 0x7C67},// P_RD_P0Q1
	{ 0x365C, 0x2352},// P_RD_P0Q2
	{ 0x365E, 0xF2B0},// P_RD_P0Q3
	{ 0x3660, 0x4032},// P_RD_P0Q4
	{ 0x3662, 0x0190},// P_BL_P0Q0
	{ 0x3664, 0xD32B},// P_BL_P0Q1
	{ 0x3666, 0x0A52},// P_BL_P0Q2
	{ 0x3668, 0x9091},// P_BL_P0Q3
	{ 0x366A, 0x93CF},// P_BL_P0Q4
	{ 0x366C, 0x0050},// P_GB_P0Q0
	{ 0x366E, 0x912B},// P_GB_P0Q1
	{ 0x3670, 0x1112},// P_GB_P0Q2
	{ 0x3672, 0xDF90},// P_GB_P0Q3
	{ 0x3674, 0x6CF1},// P_GB_P0Q4
	{ 0x3676, 0x9AEE},// P_GR_P1Q0
	{ 0x3678, 0x0CEE},// P_GR_P1Q1
	{ 0x367A, 0xFE50},// P_GR_P1Q2
	{ 0x367C, 0x996D},// P_GR_P1Q3
	{ 0x367E, 0x2073},// P_GR_P1Q4
	{ 0x3680, 0x9BEE},// P_RD_P1Q0
	{ 0x3682, 0x240D},// P_RD_P1Q1
	{ 0x3684, 0x9C91},// P_RD_P1Q2
	{ 0x3686, 0x6610},// P_RD_P1Q3
	{ 0x3688, 0x3194},// P_RD_P1Q4
	{ 0x368A, 0xA40E},// P_BL_P1Q0
	{ 0x368C, 0x586E},// P_BL_P1Q1
	{ 0x368E, 0xE2D0},// P_BL_P1Q2
	{ 0x3690, 0x99CF},// P_BL_P1Q3
	{ 0x3692, 0x08D2},// P_BL_P1Q4
	{ 0x3694, 0x8C6E},// P_GB_P1Q0
	{ 0x3696, 0x052D},// P_GB_P1Q1
	{ 0x3698, 0x9DB1},// P_GB_P1Q2
	{ 0x369A, 0x2CF1},// P_GB_P1Q3
	{ 0x369C, 0x31F4},// P_GB_P1Q4
	{ 0x369E, 0x7332},// P_GR_P2Q0
	{ 0x36A0, 0xCB51},// P_GR_P2Q1
	{ 0x36A2, 0x8355},// P_GR_P2Q2
	{ 0x36A4, 0x48D5},// P_GR_P2Q3
	{ 0x36A6, 0x6EF8},// P_GR_P2Q4
	{ 0x36A8, 0x79D2},// P_RD_P2Q0
	{ 0x36AA, 0xDF11},// P_RD_P2Q1
	{ 0x36AC, 0x90D3},// P_RD_P2Q2
	{ 0x36AE, 0x6854},// P_RD_P2Q3
	{ 0x36B0, 0x2F58},// P_RD_P2Q4
	{ 0x36B2, 0x52D2},// P_BL_P2Q0
	{ 0x36B4, 0xCA11},// P_BL_P2Q1
	{ 0x36B6, 0xF8D4},// P_BL_P2Q2
	{ 0x36B8, 0x64B5},// P_BL_P2Q3
	{ 0x36BA, 0x14D8},// P_BL_P2Q4
	{ 0x36BC, 0x6AD2},// P_GB_P2Q0
	{ 0x36BE, 0xED71},// P_GB_P2Q1
	{ 0x36C0, 0x8F75},// P_GB_P2Q2
	{ 0x36C2, 0x58B5},// P_GB_P2Q3
	{ 0x36C4, 0x0839},// P_GB_P2Q4
	{ 0x36C6, 0x522F},// P_GR_P3Q0
	{ 0x36C8, 0x93F3},// P_GR_P3Q1
	{ 0x36CA, 0x21D5},// P_GR_P3Q2
	{ 0x36CC, 0x7035},// P_GR_P3Q3
	{ 0x36CE, 0xC937},// P_GR_P3Q4
	{ 0x36D0, 0x1190},// P_RD_P3Q0
	{ 0x36D2, 0xDB72},// P_RD_P3Q1
	{ 0x36D4, 0x3795},// P_RD_P3Q2
	{ 0x36D6, 0x4614},// P_RD_P3Q3
	{ 0x36D8, 0xB2D8},// P_RD_P3Q4
	{ 0x36DA, 0x1AD1},// P_BL_P3Q0
	{ 0x36DC, 0x95D3},// P_BL_P3Q1
	{ 0x36DE, 0x5FD4},// P_BL_P3Q2
	{ 0x36E0, 0x1636},// P_BL_P3Q3
	{ 0x36E2, 0x1276},// P_BL_P3Q4
	{ 0x36E4, 0x10AF},// P_GB_P3Q0
	{ 0x36E6, 0xC3D2},// P_GB_P3Q1
	{ 0x36E8, 0x5415},// P_GB_P3Q2
	{ 0x36EA, 0x93D5},// P_GB_P3Q3
	{ 0x36EC, 0xA199},// P_GB_P3Q4
	{ 0x36EE, 0xF634},// P_GR_P4Q0
	{ 0x36F0, 0x5315},// P_GR_P4Q1
	{ 0x36F2, 0x1299},// P_GR_P4Q2
	{ 0x36F4, 0xA8DA},// P_GR_P4Q3
	{ 0x36F6, 0xBA9B},// P_GR_P4Q4
	{ 0x36F8, 0x9AF4},// P_RD_P4Q0
	{ 0x36FA, 0x30F5},// P_RD_P4Q1
	{ 0x36FC, 0x4437},// P_RD_P4Q2
	{ 0x36FE, 0xD5B9},// P_RD_P4Q3
	{ 0x3700, 0x2F3A},// P_RD_P4Q4
	{ 0x3702, 0xBB94},// P_BL_P4Q0
	{ 0x3704, 0x5E55},// P_BL_P4Q1
	{ 0x3706, 0x22F7},// P_BL_P4Q2
	{ 0x3708, 0x9D5A},// P_BL_P4Q3
	{ 0x370A, 0x3B5B},// P_BL_P4Q4
	{ 0x370C, 0xC0D4},// P_GB_P4Q0
	{ 0x370E, 0x5855},// P_GB_P4Q1
	{ 0x3710, 0x0359},// P_GB_P4Q2
	{ 0x3712, 0x8F1A},// P_GB_P4Q3
	{ 0x3714, 0xABDB},// P_GB_P4Q4
	{ 0x3644, 0x0148},// POLY_ORIGIN_C
	{ 0x3642, 0x00F0},// POLY_ORIGIN_R
#else
	{0x364E, 0x0170},
	{0x3650, 0x570C},
	{0x3652, 0x6652},
	{0x3654, 0xB5B0},
	{0x3656, 0x3274},
	{0x3676, 0x214D},
	{0x3678, 0x3D46},
	{0x367A, 0x218F},
	{0x367C, 0x13D0},
	{0x367E, 0x8C34},
	{0x369E, 0x1253},
	{0x36A0, 0x98B0},
	{0x36A2, 0x3275},
	{0x36A4, 0x4FD0},
	{0x36A6, 0x0896},
	{0x36C6, 0x82D0},
	{0x36C8, 0x8292},
	{0x36CA, 0x0CD5},
	{0x36CC, 0x2632},
	{0x36CE, 0xC659},
	{0x36EE, 0x2374},
	{0x36F0, 0xDC32},
	{0x36F2, 0xCBF7},
	{0x36F4, 0x1918},
	{0x36F6, 0x675C},
	{0x3662, 0x00D0},
	{0x3664, 0x67AC},
	{0x3666, 0x6CD2},
	{0x3668, 0xB350},
	{0x366A, 0x0CD4},
	{0x368A, 0x400C},
	{0x368C, 0x07ED},
	{0x368E, 0x57CF},
	{0x3690, 0x3C50},
	{0x3692, 0xD654},
	{0x36B2, 0x0213},
	{0x36B4, 0x2CCF},
	{0x36B6, 0x1636},
	{0x36B8, 0x8EB5},
	{0x36BA, 0xF338},
	{0x36DA, 0xD310},
	{0x36DC, 0x22B0},
	{0x36DE, 0x3273},
	{0x36E0, 0x9BF6},
	{0x36E2, 0x82D8},
	{0x3702, 0x4854},
	{0x3704, 0x9AD5},
	{0x3706, 0xD519},
	{0x3708, 0x5D99},
	{0x370A, 0x685D},
	{0x366C, 0x00D0},
	{0x366E, 0x498C},
	{0x3670, 0x6B32},
	{0x3672, 0xA910},
	{0x3674, 0x3614},
	{0x3694, 0x3FAC},
	{0x3696, 0xB68A},
	{0x3698, 0x0FB0},
	{0x369A, 0x5E70},
	{0x369C, 0xD6B4},
	{0x36BC, 0x09F3},
	{0x36BE, 0xB9F1},
	{0x36C0, 0x3C55},
	{0x36C2, 0x2035},
	{0x36C4, 0x5B55},
	{0x36E4, 0x9450},
	{0x36E6, 0xF171},
	{0x36E8, 0x16D4},
	{0x36EA, 0x8234},
	{0x36EC, 0x98F9},
	{0x370C, 0x3854},
	{0x370E, 0x3754},
	{0x3710, 0xBED7},
	{0x3712, 0xF557},
	{0x3714, 0x4B7C},
	{0x3644, 0x0148},
	{0x3642, 0x00F0},
#endif
/* LGE_CHANGE_E, Lens shading tuning, 2012-06-11, yt.kim@lge.com */
	{0x3210, 0x09B8},
	{0x098C, 0x2306},	{0x0990, 0x0133},
	{0x098C, 0x2308},	{0x0990, 0xFFC4},
	{0x098C, 0x230A},	{0x0990, 0x0014},
	{0x098C, 0x230C},	{0x0990, 0xFF64},
	{0x098C, 0x230E},	{0x0990, 0x01E3},
	{0x098C, 0x2310},	{0x0990, 0xFFB2},
	{0x098C, 0x2312},	{0x0990, 0xFF9A},
	{0x098C, 0x2314},	{0x0990, 0xFEDB},
	{0x098C, 0x2316},	{0x0990, 0x0213},
	{0x098C, 0x2318},	{0x0990, 0x001C},
	{0x098C, 0x231A},	{0x0990, 0x003A},
	{0x098C, 0x231C},	{0x0990, 0x0064},
	{0x098C, 0x231E},	{0x0990, 0xFF7D},
	{0x098C, 0x2320},	{0x0990, 0xFFFF},
	{0x098C, 0x2322},	{0x0990, 0x001A},
	{0x098C, 0x2324},	{0x0990, 0xFF94},
	{0x098C, 0x2326},	{0x0990, 0x0048},
	{0x098C, 0x2328},	{0x0990, 0x001B},
	{0x098C, 0x232A},	{0x0990, 0x0166},
	{0x098C, 0x232C},	{0x0990, 0xFEE3},
	{0x098C, 0x232E},	{0x0990, 0x0004},
	{0x098C, 0x2330},	{0x0990, 0xFFDC},
	{0x098C, 0xA348},	{0x0990, 0x0008},
	{0x098C, 0xA349},	{0x0990, 0x0002},
// AWB_1_S
	{0x098C, 0xA34A},	{0x0990, 0x0059},
	{0x098C, 0xA34B},	{0x0990, 0x00E6},
	{0x098C, 0xA34C},	{0x0990, 0x0059},
	{0x098C, 0xA34D},	{0x0990, 0x00A6},
// AWB_1_E
	{0x098C, 0xA34E},	{0x0990, 0x0080},
	{0x098C, 0xA34F},	{0x0990, 0x0080},
	{0x098C, 0xA350},	{0x0990, 0x0080},
// AWB_1_S
	{0x098C, 0xA351},	{0x0990, 0x0020},
	{0x098C, 0xA352},	{0x0990, 0x007F},
// AWB_1_E
	{0x098C, 0xA354},	{0x0990, 0x0060},
	{0x098C, 0xA354},	{0x0990, 0x0060},
	{0x098C, 0xA355},	{0x0990, 0x0001},
	{0x098C, 0xA35D},	{0x0990, 0x0078},
	{0x098C, 0xA35E},	{0x0990, 0x0086},
	{0x098C, 0xA35F},	{0x0990, 0x007E},
	{0x098C, 0xA360},	{0x0990, 0x0082},
	{0x098C, 0xA302},	{0x0990, 0x0000},
	{0x098C, 0xA303},	{0x0990, 0x00EF},
#ifdef CONFIG_MACH_MSM8960_L0 //AWB_TG_MAX AWB boundary changed.
	{0x098C, 0xA364},	{0x0990, 0x00F6},
#else
	{0x098C, 0xA364},	{0x0990, 0x00E4},
#endif
// AWB_1_S
	{0x098C, 0xA365},	{0x0990, 0x0000},
// AWB_1_E
	{0x098C, 0xA366},	{0x0990, 0x0080},
	{0x098C, 0xA367},	{0x0990, 0x0080},
	{0x098C, 0xA368},	{0x0990, 0x0080},
// AWB_1_S
	{0x098C, 0xA369},	{0x0990, 0x0083},
	{0x098C, 0xA36A},	{0x0990, 0x0082},
	{0x098C, 0xA36B},	{0x0990, 0x0082},
// AWB_1_E
	{0x098C, 0xAB1F},	{0x0990, 0x00C6},
	{0x098C, 0xAB20},	{0x0990, 0x0060},
	{0x098C, 0xAB21},	{0x0990, 0x001F},
	{0x098C, 0xAB22},	{0x0990, 0x0003},
	{0x098C, 0xAB23},	{0x0990, 0x0005},
	{0x098C, 0xAB24},	{0x0990, 0x0030},
	{0x098C, 0xAB25},	{0x0990, 0x0060},
	{0x098C, 0xAB26},	{0x0990, 0x0000},
	{0x098C, 0xAB27},	{0x0990, 0x0006},
	{0x098C, 0x2B28},	{0x0990, 0x1800},
	{0x098C, 0x2B2A},	{0x0990, 0x3000},
	{0x098C, 0xAB2C},	{0x0990, 0x0006},
	{0x098C, 0xAB2D},	{0x0990, 0x000A},
	{0x098C, 0xAB2E},	{0x0990, 0x0006},
	{0x098C, 0xAB2F},	{0x0990, 0x0006},
	{0x098C, 0xAB30},	{0x0990, 0x001E},
	{0x098C, 0xAB31},	{0x0990, 0x000E},
	{0x098C, 0xAB32},	{0x0990, 0x001E},
	{0x098C, 0xAB33},	{0x0990, 0x001E},
	{0x098C, 0xAB34},	{0x0990, 0x0008},
	{0x098C, 0xAB35},	{0x0990, 0x0080},
	{0x098C, 0xA202},	{0x0990, 0x0021},
	{0x098C, 0xA203},	{0x0990, 0x00DD},
	{0x098C, 0xA11D},	{0x0990, 0x0002},
	{0x098C, 0xA208},	{0x0990, 0x0003},
	{0x098C, 0xA209},	{0x0990, 0x0002},
	{0x098C, 0xA20A},	{0x0990, 0x001F},
	{0x098C, 0xA216},	{0x0990, 0x003A},
	{0x098C, 0xA244},	{0x0990, 0x0008},
// AE_S
	{0x098C, 0xA24F},	{0x0990, 0x0042},
// AE_E
	{0x098C, 0xA207},	{0x0990, 0x0005},	// AE_GATE
	{0x098C, 0xA20D},	{0x0990, 0x0020},
	{0x098C, 0xA20E},	{0x0990, 0x0080},
	{0x098C, 0xAB04},	{0x0990, 0x0014},
	{0x098C, 0x2361},	{0x0990, 0x0a00},
	{0x3244, 0x0303},
	{0x098C, 0x2212},	{0x0990, 0x00F0},
	{0x326C, 0x1305},
//	{0x098C, 0xA103},	{0x0990, 0x0006},
//	{0x098C, 0xA103},	{0x0990, 0x0005},
//	{0xFFFF, 0x0064},	// Delay
};

// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
#ifdef CONFIG_MACH_LGE
/* White balance register settings */
static struct msm_camera_i2c_reg_conf wb_default_tbl_sub[]=
{
	{0x098C, 0xA102}, {0x0990, 0x000F},	// Mode(AWB/Flicker/AE driver enable)
	{0x098C, 0xA34A}, {0x0990, 0x0059},	// AWB_GAINMIN_R
	{0x098C, 0xA34B}, {0x0990, 0x00E6},	// AWB_GAINMAX_R
	{0x098C, 0xA34C}, {0x0990, 0x0059},	// AWB_GAINMIN_B
	{0x098C, 0xA34D}, {0x0990, 0x00A6},	// AWB_GAINMAX_B
	{0x098C, 0xA351}, {0x0990, 0x0020},	// AWB_CCM_POSITION_MIN
	{0x098C, 0xA352}, {0x0990, 0x007F},	// AWB_CCM_POSITION_MAX
	{0x098C, 0xA365}, {0x0990, 0x0000},	// AWB_X0
	{0x098C, 0xA369}, {0x0990, 0x0083},	// AWB_KR_R
	{0x098C, 0xA36A}, {0x0990, 0x0082},	// AWB_KG_R
	{0x098C, 0xA36B}, {0x0990, 0x0082},	// AWB_KB_R
};
static struct msm_camera_i2c_reg_conf wb_sunny_tbl_sub[]=
{
	{0x098C, 0xA102}, {0x0990, 0x000B},	// Mode(AWB disable)
	{0x098C, 0xA34A}, {0x0990, 0x00B0},	// AWB_GAIN_MIN
	{0x098C, 0xA34B}, {0x0990, 0x00B0},	// AWB_GAIN_MAX
	{0x098C, 0xA34C}, {0x0990, 0x0080},	// AWB_GAINMIN_B
	{0x098C, 0xA34D}, {0x0990, 0x0080},	// AWB_GAINMAX_B
	{0x098C, 0xA351}, {0x0990, 0x004B},	// AWB_CCM_POSITION_MIN
	{0x098C, 0xA352}, {0x0990, 0x004B},	// AWB_CCM_POSITION_MAX
	{0x098C, 0xA353}, {0x0990, 0x004B},	// AWB_CCM_POSITION
	{0x098C, 0xA369}, {0x0990, 0x0095},	// AWB_KR_R
	{0x098C, 0xA36A}, {0x0990, 0x0075},	// AWB_KG_R
	{0x098C, 0xA36B}, {0x0990, 0x008A},	// AWB_KB_R
};
static struct msm_camera_i2c_reg_conf wb_cloudy_tbl_sub[]=
{
	{0x098C, 0xA102}, {0x0990, 0x000B},	// Mode(AWB disable)
	{0x098C, 0xA34A}, {0x0990, 0x00B0},	// AWB_GAIN_MIN
	{0x098C, 0xA34B}, {0x0990, 0x00B0},	// AWB_GAIN_MAX
	{0x098C, 0xA34C}, {0x0990, 0x0080},	// AWB_GAINMIN_B
	{0x098C, 0xA34D}, {0x0990, 0x0080},	// AWB_GAINMAX_B
	{0x098C, 0xA351}, {0x0990, 0x004B},	// AWB_CCM_POSITION_MIN
	{0x098C, 0xA352}, {0x0990, 0x004B},	// AWB_CCM_POSITION_MAX
	{0x098C, 0xA353}, {0x0990, 0x004B},	// AWB_CCM_POSITION
	{0x098C, 0xA369}, {0x0990, 0x00A5},	// AWB_KR_R
	{0x098C, 0xA36A}, {0x0990, 0x0080},	// AWB_KG_R
	{0x098C, 0xA36B}, {0x0990, 0x0065},	// AWB_KB_R
};
static struct msm_camera_i2c_reg_conf wb_fluorescent_tbl_sub[]=
{
	{0x098C, 0xA102}, {0x0990, 0x000B},	// Mode(AWB disable)
	{0x098C, 0xA34A}, {0x0990, 0x0059},	// AWB_GAINMIN_R
	{0x098C, 0xA34B}, {0x0990, 0x00E6},	// AWB_GAINMAX_R
	{0x098C, 0xA34C}, {0x0990, 0x0059},	// AWB_GAINMIN_B
	{0x098C, 0xA34D}, {0x0990, 0x00A6},	// AWB_GAINMAX_B
	{0x098C, 0xA351}, {0x0990, 0x0020},	// AWB_CCM_POSITION_MIN
	{0x098C, 0xA352}, {0x0990, 0x007F},	// AWB_CCM_POSITION_MAX
	{0x098C, 0xA365}, {0x0990, 0x0000},	// AWB_X0
	{0x098C, 0xA369}, {0x0990, 0x0075},	// AWB_KR_R
	{0x098C, 0xA36A}, {0x0990, 0x0082},	// AWB_KG_R
	{0x098C, 0xA36B}, {0x0990, 0x00A5},	// AWB_KB_R
};
static struct msm_camera_i2c_reg_conf wb_incandescent_tbl_sub[]=
{
	{0x098C, 0xA102}, {0x0990, 0x000B},	// Mode(AWB disable)
	{0x098C, 0xA34A}, {0x0990, 0x0092},	// AWB_GAIN_MIN
	{0x098C, 0xA34B}, {0x0990, 0x0092},	// AWB_GAIN_MAX
	{0x098C, 0xA34C}, {0x0990, 0x0096},	// AWB_GAINMIN_B
	{0x098C, 0xA34D}, {0x0990, 0x0096},	// AWB_GAINMAX_B
	{0x098C, 0xA351}, {0x0990, 0x001F},	// AWB_CCM_POSITION_MIN
	{0x098C, 0xA352}, {0x0990, 0x001F},	// AWB_CCM_POSITION_MAX
	{0x098C, 0xA353}, {0x0990, 0x001F},	// AWB_CCM_POSITION
	{0x098C, 0xA369}, {0x0990, 0x0072},	// AWB_KR_R
	{0x098C, 0xA36A}, {0x0990, 0x0082},	// AWB_KG_R
	{0x098C, 0xA36B}, {0x0990, 0x007C},	// AWB_KB_R
};

/* Effect register settings */
static struct msm_camera_i2c_reg_conf effect_default_tbl_sub[]=
{
	{0x098C, 0x2759},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6440},	// MODE_A_SPECIAL_EFFECT_OFF
	{0x098C, 0x275B},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6440},	// MODE_B_SPECIAL_EFFECT_OFF
};
static struct msm_camera_i2c_reg_conf effect_mono_tbl_sub[]=
{
	{0x098C, 0x2759},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6441},	// MODE_A_MONO_ON
	{0x098C, 0x275B},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6441},	// MODE_B_MONO_ON
};
static struct msm_camera_i2c_reg_conf effect_sepia_tbl_sub[]=
{
	{0x098C, 0x2759},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6442},	// MODE_A_SEPIA_ON
	{0x098C, 0x275B},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6442},	// MODE_B_SEPIA_ON
//Start LGE_BSP_CAMERA::elin.lee@lge.com 2011-07-14  Fix the Sepia effect
	{0x098C, 0x2763},	// LOGICAL_ADDRESS_ACCESS [MODE_COMMONMODESETTINGS_FX_SEPIA_SETTINGS]
	{0x0990, 0xE817},	// MCU_DATA_0
//End LGE_BSP_CAMERA::elin.lee@lge.com 2011-07-14  Fix the Sepia effect
};
static struct msm_camera_i2c_reg_conf effect_negative_tbl_sub[]=
{
	{0x098C, 0x2759},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6443},	// MODE_A_NEGATIVE_ON
	{0x098C, 0x275B},	// LOGICAL_ADDRESS_ACCESS			===== 8
	{0x0990, 0x6443},	// MODE_B_NEGATIVE_ON
};

static struct msm_camera_i2c_reg_conf fps_fixed_15_tbl_sub[]=
{
	{0x098C, 0x271F},	{0x0990, 0x03F6},	// MODE_SENSOR_FRAME_LENGTH_A
	{0x098C, 0xA20B},	{0x0990, 0x0005},	// AE_MIN_INDEX
	{0x098C, 0xA20C},	{0x0990, 0x0005},	// AE_MAX_INDEX
};
static struct msm_camera_i2c_reg_conf fps_fixed_30_tbl_sub[]=
{
	{0x098C, 0x271F},	{0x0990, 0x01FF},	// MODE_SENSOR_FRAME_LENGTH_A
	{0x098C, 0xA20C},	{0x0990, 0x0004},	// AE_MIN_INDEX
	{0x098C, 0xA20B},	{0x0990, 0x0004},	// AE_MAX_INDEX
};
static struct msm_camera_i2c_reg_conf fps_auto_1030_tbl_sub[]=
{
	{0x098C, 0x271F},	{0x0990, 0x01FF},	// MODE_SENSOR_FRAME_LENGTH_A
	{0x098C, 0xA20B},	{0x0990, 0x0000},	// AE_MIN_INDEX
	{0x098C, 0xA20C},	{0x0990, 0x000C},	// AE_MAX_INDEX
};
static struct msm_camera_i2c_reg_conf fps_auto_730_tbl_sub[]=
{
	{0x098C, 0x271F},	{0x0990, 0x01FF},	// MODE_SENSOR_FRAME_LENGTH_A
	{0x098C, 0xA20B},	{0x0990, 0x0000},	// AE_MIN_INDEX
	{0x098C, 0xA20C},	{0x0990, 0x0010},	// AE_MAX_INDEX
};

static struct msm_camera_i2c_reg_conf refreshmode_sub[]=
{
	{0x098C, 0xA103},	{0x0990, 0x0006},
};

static struct msm_camera_i2c_reg_conf refresh_sub[]=
{
	{0x098C, 0xA103},	{0x0990, 0x0005},
};
#endif
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }

static struct v4l2_subdev_info mt9v113_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8, /* For YUV type sensor (YUV422) */
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order  = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array mt9v113_init_conf[] = {
	{&mt9v113_recommend_settings[0],
	ARRAY_SIZE(mt9v113_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_camera_i2c_conf_array mt9v113_confs[] = {
	{&mt9v113_prev_settings[0],
	ARRAY_SIZE(mt9v113_prev_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_sensor_output_info_t mt9v113_dimensions[] = {
	{
		.x_output = 0x280,
		.y_output = 0x1E0,
		.line_length_pclk = 0xD06,
		.frame_length_lines = 0x04ED,
		.vt_pixel_clk = 45600000,
		.op_pixel_clk = 45600000,
	},
};

static struct msm_camera_csid_vc_cfg mt9v113_cid_cfg[] = {
	{0, CSI_YUV422_8, CSI_DECODE_8BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params mt9v113_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 1,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = mt9v113_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 1,
		.settle_cnt = 0x14,
	},
};

static struct msm_camera_csi2_params *mt9v113_csi_params_array[] = {
	&mt9v113_csi_params,
};

static struct msm_sensor_output_reg_addr_t mt9v113_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t mt9v113_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x2280,
};

static struct msm_sensor_exp_gain_info_t mt9v113_exp_gain_info = {
	.coarse_int_time_addr = 0x3012,
	.global_gain_addr = 0x305E,
	.vert_offset = 0x00,
};

//static struct sensor_calib_data mt9v113_calib_data;
static const struct i2c_device_id mt9v113_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&mt9v113_s_ctrl},
	{ }
};

static struct i2c_driver mt9v113_i2c_driver = {
	.id_table = mt9v113_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9v113_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&mt9v113_i2c_driver);
}

static struct v4l2_subdev_core_ops mt9v113_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

#define GPIO_VGACAM_LDO_EN      (64)
#ifdef CONFIG_MACH_MSM8960_L0
#define GPIO_CAM2_PWRDOWN       (54)
#endif
int mt9v113_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	pr_err("%s: E: %s\n", __func__, data->sensor_name); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}
	
	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

#ifdef CONFIG_MACH_MSM8960_L0
	/* POWER DOWN PIN */
	rc = gpio_request(GPIO_CAM2_PWRDOWN, "CAM_PWRDOWN");
	if (rc) {
		pr_err("gpio_request(GPIO_CAM2_PWRDOWN) error\n");
	} else {
		rc = gpio_direction_output(GPIO_CAM2_PWRDOWN, 0);
		if (rc) {
			pr_err("gpio_direction_output(GPIO_CAM2_PWRDOWN) error\n");
		}
	}
#endif

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}
#ifndef CONFIG_MACH_MSM8960_L0
	/* CAMVDD */
	rc = gpio_request(GPIO_VGACAM_LDO_EN, "CAM_GPIO");
	if (rc) {
		pr_err("gpio_request(GPIO_VGACAM_LDO_EN) error\n");
	} else {
		rc = gpio_direction_output(GPIO_VGACAM_LDO_EN, 1);
		if (rc) {
			pr_err("gpio_direction_output(GPIO_VGACAM_LDO_EN) error\n");
		}
	}
#endif
	rc = msm_camera_config_gpio_table(data, 0);

	msleep(10);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

	/* Recommanded delay for mt9v113 */
	mdelay(10);
	rc = msm_camera_config_gpio_table(data, 1);
	mdelay(10);
	/* Recommanded delay for mt9v113 */

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	pr_err("%s: X: %s\n", __func__, data->sensor_name); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	return rc;
enable_clk_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
}

int mt9v113_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s\n", __func__);
	pr_err("%s: E: %s\n", __func__, data->sensor_name); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
#ifdef CONFIG_MACH_MSM8960_L0
	/* POWER DOWN PIN */
	gpio_free(GPIO_CAM2_PWRDOWN);
#endif

	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
#ifndef CONFIG_MACH_MSM8960_L0
	/* CAMVDD */
	if (gpio_direction_output(GPIO_VGACAM_LDO_EN, 0)) {
		pr_err("gpio_direction_output(GPIO_VGACAM_LDO_EN, 0) error\n");
	} else {
		gpio_free(GPIO_VGACAM_LDO_EN);
	}
#endif
	msm_camera_request_gpio_table(data, 0);
	pr_err("%s: X: %s\n", __func__, data->sensor_name); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	kfree(s_ctrl->reg_ptr);
	return 0;
}

int32_t mt9v113_camera_i2c_write_tbl(struct msm_camera_i2c_client *client,
                                     struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
                                     enum msm_camera_i2c_data_type data_type)
{
	int i;
	int32_t rc = -EIO;
#if 1 // seonghyon.cho@lge.com 2011.10.19
	printk(KERN_ERR "### %s %d %d size: %d \n",
		__func__,client->addr_type,data_type, size);
#endif

	for (i = 0; i < size; i++) {
		if (reg_conf_tbl->reg_addr == 0xFFFF) {
			//CDBG("%s: SLEEP for %d ms\n", __func__, reg_conf_tbl->reg_data);
			msleep(reg_conf_tbl->reg_data);
			rc = 0;
		} else if (reg_conf_tbl->reg_addr == 0xFFFE) {
			unsigned short test_data = 0;
			for (i = 0; i < 50; i++) {
				/* max delay ==> 500 ms */
				rc  = msm_camera_i2c_read(client, 0x0080, &test_data, data_type);
				if (rc < 0)
					return rc;
				if((test_data & reg_conf_tbl->reg_data) == 0)
					break;
				else
					mdelay(10);

				/*	printk(KERN_ERR "### %s :  Polling set, 0x0080 Reg : 0x%x\n", __func__, test_data);  */
			}
		} else if (reg_conf_tbl->reg_addr == 0x301A) {
			unsigned short test_data = 0;
			rc  = msm_camera_i2c_read(client, 0x301A, &test_data, data_type);
			if (rc < 0)
				return rc;
			rc = msm_camera_i2c_write(client, 0x301A, test_data | 0x0200, data_type);
			if (rc < 0)
				return rc;

			/*	 printk(KERN_ERR "### %s : Reset reg check, 0x301A Reg : 0x%x\n", __func__, test_data|0x0200);  */
		} else if ((reg_conf_tbl->reg_addr == 0x0080) &&
		                ((reg_conf_tbl->reg_data == 0x8000) || (reg_conf_tbl->reg_data == 0x0001))) {
			unsigned short test_data = 0;
			rc  = msm_camera_i2c_read(client, 0x0080, &test_data, data_type);
			if (rc < 0)
				return rc;

			test_data = test_data | reg_conf_tbl->reg_data;
			rc = msm_camera_i2c_write(client, 0x0080, test_data, data_type);
			if (rc < 0)
				return rc;

			/*	printk(KERN_ERR "### %s : Patch check, 0x0080 Reg : 0x%x\n", __func__,test_data);  */
		} else {
			//CDBG("%s: i2c write ADDR=0x%x, DATA=0x%x\n", __func__, reg_conf_tbl->reg_addr, reg_conf_tbl->reg_data);
			rc = msm_camera_i2c_write(client, reg_conf_tbl->reg_addr, reg_conf_tbl->reg_data, data_type);
			if (rc < 0)
				return rc;
			/*	rc = mt9v113_i2c_write(mt9v113_client->addr, reg_conf_tbl->waddr, reg_conf_tbl->wdata, reg_conf_tbl->width);  */
		}

		if (rc < 0)
			break;

		reg_conf_tbl++;
	}

	return rc;
}

int32_t mt9v113_sensor_write_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0, i;
	pr_err("%s: E: \n", __func__); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	for (i = 0; i < s_ctrl->msm_sensor_reg->init_size; i++) {
		rc = mt9v113_camera_i2c_write_tbl(
		             s_ctrl->sensor_i2c_client,
		             s_ctrl->msm_sensor_reg->init_settings[i].conf,
		             s_ctrl->msm_sensor_reg->init_settings[i].size,
		             s_ctrl->msm_sensor_reg->init_settings[i].data_type);

		if (rc < 0)
			break;
	}

	return rc;
}

int32_t mt9v113_sensor_write_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
                uint16_t res)
{
	int32_t rc = 0;
/*
	rc = mt9v113_camera_i2c_write_tbl(
	             s_ctrl->sensor_i2c_client,
	             s_ctrl->msm_sensor_reg->mode_settings[res].conf,
	             s_ctrl->msm_sensor_reg->mode_settings[res].size,
	             s_ctrl->msm_sensor_reg->mode_settings[res].data_type);
	if (rc < 0)
		return rc;

	rc = msm_sensor_write_output_settings(s_ctrl, res);
*/
	return rc;
}


void mt9v113_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	pr_err("%s: E: \n", __func__); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	mt9v113_camera_i2c_write_tbl(
	        s_ctrl->sensor_i2c_client,
	        s_ctrl->msm_sensor_reg->start_stream_conf,
	        s_ctrl->msm_sensor_reg->start_stream_conf_size,
	        s_ctrl->msm_sensor_reg->default_data_type);
}

void mt9v113_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	pr_err("%s: E: \n", __func__); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	mt9v113_camera_i2c_write_tbl(
	        s_ctrl->sensor_i2c_client,
	        s_ctrl->msm_sensor_reg->stop_stream_conf,
	        s_ctrl->msm_sensor_reg->stop_stream_conf_size,
	        s_ctrl->msm_sensor_reg->default_data_type);
}

void mt9v113_sensor_group_hold_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	return;
}

void mt9v113_sensor_group_hold_off(struct msm_sensor_ctrl_t *s_ctrl)
{
	return;
}

int mt9v113_check_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl);  /* LGE_CHANGE, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */
static int32_t mt9v113_set_wb(struct msm_sensor_ctrl_t *s_ctrl, uint8_t mode);
static int32_t mt9v113_set_effect(struct msm_sensor_ctrl_t *s_ctrl, uint8_t mode);
static int mt9v113_set_brightness(struct msm_sensor_ctrl_t *s_ctrl, uint8_t mode);
static int32_t mt9v113_set_fps(struct msm_sensor_ctrl_t *s_ctrl, uint8_t minfps, uint8_t maxfps);

int32_t mt9v113_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
                               int update_type, int res)
{
	int32_t rc = 0;

	pr_err("%s: update_type = %d, resolution = %d\n", __func__, update_type, res); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
	//s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);		/* LGE_CHANGE, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
/* LGE_CHANGE_S, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */
#if 0
		mt9v113_sensor_write_init_settings(s_ctrl);
		// Refresh mode
		rc = mt9v113_camera_i2c_write_tbl(
				s_ctrl->sensor_i2c_client,
				refreshmode_sub,
				ARRAY_SIZE(refreshmode_sub),
				MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0)
			return rc;
		rc = mt9v113_check_sensor_mode(s_ctrl);

		// Refresh
		rc = mt9v113_camera_i2c_write_tbl(
				s_ctrl->sensor_i2c_client,
				refresh_sub,
				ARRAY_SIZE(refresh_sub),
				MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0)
			return rc;
		rc = mt9v113_check_sensor_mode(s_ctrl);

		/* LGE_CHANGE_S, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
		camera_started = 1;
		if(prev_balance_mode != 1)
		{
			int mode = prev_balance_mode;
			prev_balance_mode = 1;
			mt9v113_set_wb(&mt9v113_s_ctrl, mode);
		}
		if(prev_effect_mode != 0)
		{
			int mode = prev_effect_mode;
			prev_effect_mode = 0;
			mt9v113_set_effect(&mt9v113_s_ctrl, mode);
		}
		if(prev_brightness_mode != 6)
		{
			int mode = prev_brightness_mode;
			prev_brightness_mode = 6;
			mt9v113_set_brightness(&mt9v113_s_ctrl, mode);
		}
		if (prev_fps_mode != 3) {
			int mode = prev_fps_mode;
			prev_fps_mode = 3;
			if (mode == SENSOR_FIXED_FPS_15) mt9v113_set_fps(&mt9v113_s_ctrl, 15, 15);
			else if(mode == SENSOR_FIXED_FPS_30) mt9v113_set_fps(&mt9v113_s_ctrl, 30, 30);
			else if(mode == SENSOR_AUTO_FPS_0730) mt9v113_set_fps(&mt9v113_s_ctrl, 5, 30);
		}
		/* LGE_CHANGE_E, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
#endif
#ifdef CONFIG_MACH_LGE
		CDBG("mt9v113: Reset all mode values.\n");
		camera_started = 0;
		prev_balance_mode = 1;
		prev_effect_mode = 0;
		prev_brightness_mode = 6;
// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
		prev_fps_mode = 3;
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }
#endif
/* LGE_CHANGE_E, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		if (s_ctrl->curr_csi_params != s_ctrl->csi_params[res]) {
			pr_err("%s: in (s_ctrl->curr_csi_params != s_ctrl->csi_params[res])\n", __func__); /* LGE_CHANGE, For debugging, 2012-09-24, sungsik.kim@lge.com */
			s_ctrl->curr_csi_params = s_ctrl->csi_params[res];
// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
			s_ctrl->curr_csi_params->csid_params.lane_assign =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_assign;
			s_ctrl->curr_csi_params->csiphy_params.lane_mask =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_mask;
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSID_CFG,
				&s_ctrl->curr_csi_params->csid_params);
			mb();
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIPHY_CFG,
				&s_ctrl->curr_csi_params->csiphy_params);
			mb();
			msleep(20);
		}

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
/* LGE_CHANGE_S, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */
//		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		mt9v113_sensor_write_init_settings(s_ctrl);

		// Refresh mode
		rc = mt9v113_camera_i2c_write_tbl(
				s_ctrl->sensor_i2c_client,
				refreshmode_sub,
				ARRAY_SIZE(refreshmode_sub),
				MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0)
			return rc;
		rc = mt9v113_check_sensor_mode(s_ctrl);

		// Refresh
		rc = mt9v113_camera_i2c_write_tbl(
				s_ctrl->sensor_i2c_client,
				refresh_sub,
				ARRAY_SIZE(refresh_sub),
				MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0)
			return rc;
		rc = mt9v113_check_sensor_mode(s_ctrl);
		/* LGE_CHANGE_S, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
		camera_started = 1;
		if(prev_balance_mode != 1)
		{
			int mode = prev_balance_mode;
			prev_balance_mode = 1;
			mt9v113_set_wb(&mt9v113_s_ctrl, mode);
		}
		if(prev_effect_mode != 0)
		{
			int mode = prev_effect_mode;
			prev_effect_mode = 0;
			mt9v113_set_effect(&mt9v113_s_ctrl, mode);
		}
		if(prev_brightness_mode != 6)
		{
			int mode = prev_brightness_mode;
			prev_brightness_mode = 6;
			mt9v113_set_brightness(&mt9v113_s_ctrl, mode);
		}
		if (prev_fps_mode != 3) {
			int mode = prev_fps_mode;
			prev_fps_mode = 3;
			if (mode == SENSOR_FIXED_FPS_15) mt9v113_set_fps(&mt9v113_s_ctrl, 15, 15);
			else if(mode == SENSOR_FIXED_FPS_30) mt9v113_set_fps(&mt9v113_s_ctrl, 30, 30);
			else if(mode == SENSOR_AUTO_FPS_0730) mt9v113_set_fps(&mt9v113_s_ctrl, 5, 30);
		}
		/* LGE_CHANGE_E, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
		msleep(30);
	}

	return rc;
}


/* ES4 ku.kwon@lge.com
static int __init msm_sensor_init_module(void)
{
	printk("yongjin1.kim: %s\n", __func__);
	return platform_driver_register(&mt9v113_driver);
}
*/

// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
#ifdef CONFIG_MACH_LGE
/*
static int mt9v113_polling_reg(unsigned short waddr, unsigned short wcondition, unsigned short result, int delay, int time_out)
{
	int rc = -EFAULT;
	int i=0;
	unsigned short wdata;
	printk("### yongjin1.kim, %s \n", __func__);

	for(i=0; i<time_out; i++){
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, waddr, &wdata, MSM_CAMERA_I2C_WORD_DATA);

		if (rc<0) {
			CDBG("polling reg failed to read\n");
			return rc;
		}

		CDBG("polling reg 0x%x ==> 0x%x\n", waddr, wdata);

		if ((wdata&wcondition)==result) {
			CDBG("polling register meets condition\n");
			break;
		}
		else
			mdelay(delay);
	}

	return rc;
}
//rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x0990, &mcu_data, MSM_CAMERA_I2C_WORD_DATA);
//rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0004, MSM_CAMERA_I2C_WORD_DATA);
*/
int mt9v113_check_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl)
{
	unsigned short mcu_address_sts =0, mcu_data =0;
	int i, rc=-EFAULT;

	CDBG("mt9v113 : check_sensor_mode E\n");

	for(i=0; i<50; i++){

		/* MCU_ADDRESS : check mcu_address */
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x098C, &mcu_address_sts, MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0){
			CDBG("mt9v113: reading mcu_address_sts fail\n");
			return rc;
		}

		/* MCU_DATA_0 : check mcu_data */
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x0990, &mcu_data, MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0){
			CDBG("mt9v113: reading mcu_data fail\n");
			return rc;
		}


		if( ((mcu_address_sts & 0xA103) == 0xA103) && (mcu_data == 0x00)){
			CDBG("mt9v113: sensor refresh mode success!!\n");
			break;
		}
		msleep(10);
	}

	CDBG("mt9v113: check_sensor_mode X\n");

	return rc;
}

static int32_t mt9v113_set_wb(struct msm_sensor_ctrl_t *s_ctrl, uint8_t mode)
{
	int32_t rc = 0;
	CDBG("ku.kwon: %s %d\n", __func__, mode);

	/* LGE_CHANGE_S, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
	if(!camera_started) {
		CDBG("### [CHECK]%s: camera is not started yet, delay work to the end of initialization -> %d\n", __func__, (int)mode);
		prev_balance_mode = mode;
		return rc;
	}
	/* LGE_CHANGE_E, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */

	if(prev_balance_mode == mode)
	{
		CDBG("###  [CHECK]%s: skip this function, wb_mode -> %d\n", __func__, mode);
		return rc;
	}
       CDBG("###  [CHECK]%s: mode -> %d\n", __func__, mode);

	switch (mode) {
		case CAMERA_WB_AUTO:
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 wb_default_tbl_sub,
						 ARRAY_SIZE(wb_default_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		case CAMERA_WB_DAYLIGHT:	// sunny
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 wb_sunny_tbl_sub,
						 ARRAY_SIZE(wb_sunny_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		case CAMERA_WB_CLOUDY_DAYLIGHT:  // cloudy
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 wb_cloudy_tbl_sub,
						 ARRAY_SIZE(wb_cloudy_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		case CAMERA_WB_FLUORESCENT:
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 wb_fluorescent_tbl_sub,
						 ARRAY_SIZE(wb_fluorescent_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		case CAMERA_WB_INCANDESCENT:
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 wb_incandescent_tbl_sub,
						 ARRAY_SIZE(wb_incandescent_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		default:
			return -EINVAL;
	}
 	rc = mt9v113_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client,
			refresh_sub,
			ARRAY_SIZE(refresh_sub),
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;

	rc = mt9v113_check_sensor_mode(s_ctrl);

	if (rc<0)
	{
		CDBG("###[ERROR]%s: failed to check sensor mode\n", __func__);
		return rc;
	}

	prev_balance_mode = mode;
	return rc;
}

static int32_t mt9v113_set_effect(struct msm_sensor_ctrl_t *s_ctrl, uint8_t mode)
{
	int32_t rc = 0;
	CDBG("yongjin1.kim: %s %d\n", __func__, mode);

	/* LGE_CHANGE_S, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
	if(!camera_started) {
		CDBG("### [CHECK]%s: camera is not started yet, delay work to the end of initialization -> %d\n", __func__, (int)mode);
		prev_effect_mode = mode;
		return rc;
	}
	/* LGE_CHANGE_E, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */

	if(prev_effect_mode == mode)
	{
		CDBG("###  [CHECK]%s: skip this function, effect_mode -> %d\n", __func__, mode);
		return rc;
	}
       CDBG("###  [CHECK]%s: mode -> %d\n", __func__, mode);

	switch (mode) {
		case CAMERA_EFFECT_OFF:
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 effect_default_tbl_sub,
						 ARRAY_SIZE(effect_default_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		case CAMERA_EFFECT_MONO:		// mono
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 effect_mono_tbl_sub,
						 ARRAY_SIZE(effect_mono_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		case CAMERA_EFFECT_SEPIA:		// sepia
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 effect_sepia_tbl_sub,
						 ARRAY_SIZE(effect_sepia_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		case CAMERA_EFFECT_NEGATIVE:	// negative
			rc = mt9v113_camera_i2c_write_tbl(
						 s_ctrl->sensor_i2c_client,
						 effect_negative_tbl_sub,
						 ARRAY_SIZE(effect_negative_tbl_sub),
						 MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0)
				return rc;
			break;
		default:
			return -EINVAL;
	}
 	rc = mt9v113_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client,
			refresh_sub,
			ARRAY_SIZE(refresh_sub),
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;

	rc = mt9v113_check_sensor_mode(s_ctrl);

	if (rc<0)
	{
		CDBG("###[ERROR]%s: failed to check sensor mode\n", __func__);
		return rc;
	}

	prev_effect_mode = mode;
	return rc;
}

 static int brightness_table[] = {0x000F, 0x0017, 0x0020, 0x0028, 0x0031, 0x0039, 0x0042, 0x004B, 0x0053, 0x005C, 0x0064, 0x006D, 0x0075};
// {0x001E, 0x0021, 0x0024, 0x0027, 0x002A, 0x002D, 0x0030, 0x0033, 0x0036, 0x0039, 0x003C, 0x003F, 0x0042, 0x0045, 0x0048, 0x004B, 0x004E, 0x0051, 0x0054, 0x0057, 0x005A, 0x005D, 0x0060, 0x0063, 0x0066};

// {0x0015, 0x001C, 0x0022, 0x0029, 0x0032, 0x0039, 0x0042, 0x0049, 0x0052, 0x0059, 0x0062, 0x0069, 0x0072};

static int mt9v113_set_brightness(struct msm_sensor_ctrl_t *s_ctrl, uint8_t mode)
{
	int32_t rc = 0;
	CDBG("### yongjin1.kim, %s: mode = %d(2)\n", __func__, mode);

	/* LGE_CHANGE_S, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
	if(!camera_started) {
		CDBG("### [CHECK]%s: camera is not started yet, delay work to the end of initialization -> %d\n", __func__, (int)mode);
		prev_brightness_mode = mode;
		return rc;
	}
	/* LGE_CHANGE_E, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */

	if (prev_brightness_mode==mode){
		CDBG("###%s: skip mt9v113_set_brightness\n", __func__);
		return rc;
	}
	if(mode < 0 || mode > 12){
		CDBG("###[ERROR]%s: Invalid Mode value\n", __func__);
		return -EINVAL;
	}

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA217, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0004, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA24F, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, brightness_table[mode], MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA217, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0004, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;

	rc = mt9v113_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client,
			refresh_sub,
			ARRAY_SIZE(refresh_sub),
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;

	rc = mt9v113_check_sensor_mode(s_ctrl);

	if (rc<0)
	{
		CDBG("###[ERROR]%s: failed to check sensor mode\n", __func__);
		return rc;
	}
	prev_brightness_mode = mode;

	return rc;
}

static int32_t mt9v113_set_fps(struct msm_sensor_ctrl_t *s_ctrl, uint8_t minfps, uint8_t maxfps) {
    int32_t rc = 0;
    int mode;	/* LGE_CHANGE_S, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */

    printk("### yongjin1.kim ### %s: minfps = %d, maxfps = %d \n", __func__, minfps, maxfps);

    if(minfps == 15 && maxfps == 15) { mode = SENSOR_FIXED_FPS_15; }
    else if(minfps == 30 && maxfps == 30) { mode = SENSOR_FIXED_FPS_30; }
    else if(minfps == 10 && maxfps == 30) { mode = SENSOR_AUTO_FPS_1030; }
    else if(minfps == 5 && maxfps == 30) { mode = SENSOR_AUTO_FPS_0730; }
    else { mode = prev_fps_mode; }

    /* LGE_CHANGE_S, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */
    if(!camera_started) {
        CDBG("###  [CHECK]%s: camera is not started yet, delay work to the end of initialization -> %d\n", __func__, mode);
        prev_fps_mode = mode;
        return rc;
    }
    /* LGE_CHANGE_E, fix reset & AE/AWB start side effect, 2012-06-08 ku.kwon@lge.com */

    if(prev_fps_mode == mode)
    {
        CDBG("###  [CHECK]%s: skip this function, fps_mode -> %d\n", __func__, mode);
        return rc;
    }
    CDBG("###  [CHECK]%s: mode -> %d\n", __func__, mode);

    switch (mode) {
        case SENSOR_FIXED_FPS_15:	// CAMERA_FPS_15
            rc = mt9v113_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
                                              fps_fixed_15_tbl_sub,
                                              ARRAY_SIZE(fps_fixed_15_tbl_sub),
                                              MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                return rc;
            break;
        case SENSOR_FIXED_FPS_30:	// CAMERA_FPS_30		// sunny
            rc = mt9v113_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
                                              fps_fixed_30_tbl_sub,
                                              ARRAY_SIZE(fps_fixed_30_tbl_sub),
                                              MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                return rc;
            break;
        case SENSOR_AUTO_FPS_1030:	// CAMERA_FPS_10to30
            rc = mt9v113_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
                                              fps_auto_1030_tbl_sub,
                                              ARRAY_SIZE(fps_auto_1030_tbl_sub),
                                              MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                return rc;
            break;
        case SENSOR_AUTO_FPS_0730:	// CAMERA_FPS_7to30	 // cloudy
            rc = mt9v113_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
                                              fps_auto_730_tbl_sub,
                                              ARRAY_SIZE(fps_auto_730_tbl_sub),
                                              MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                return rc;
            break;
        default:
            return -EINVAL;
    }

/* LGE_CHANGE_S, Disabling because this refresh method is not used yet, 2012-05-27, yongjin1.kim@lge.com
    rc = mt9v113_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client, refreshmode_sub, ARRAY_SIZE(refreshmode_sub), MSM_CAMERA_I2C_WORD_DATA);
    if (rc < 0)
        return rc;

    rc = mt9v113_check_sensor_mode(s_ctrl);
   LGE_CHANGE_S, Disabling because this refresh method is not used yet, 2012-05-27, yongjin1.kim@lge.com */

    rc = mt9v113_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client, refresh_sub, ARRAY_SIZE(refresh_sub), MSM_CAMERA_I2C_WORD_DATA);
    if (rc < 0)
        return rc;

    rc = mt9v113_check_sensor_mode(s_ctrl);

    if (rc<0)
    {
        CDBG("###[ERROR]%s: failed to check sensor mode\n", __func__);
        return rc;
    }

    prev_fps_mode = mode;

    return rc;
}

#endif
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }

static struct v4l2_subdev_video_ops mt9v113_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops mt9v113_subdev_ops = {
	.core = &mt9v113_subdev_core_ops,
	.video  = &mt9v113_subdev_video_ops,
};

static struct msm_sensor_fn_t mt9v113_func_tbl = {
/* LGE_CHANGE_S, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */
	.sensor_start_stream = mt9v113_sensor_start_stream,
	.sensor_stop_stream = mt9v113_sensor_stop_stream,
	.sensor_group_hold_on = mt9v113_sensor_group_hold_on,
	.sensor_group_hold_off = mt9v113_sensor_group_hold_off,
/* LGE_CHANGE_E, fix reset & AE/AWB start, 2012-06-07 ku.kwon@lge.com */
#if 0 /* removed at M8960AAAAANLYA1022 */
	.sensor_get_prev_lines_pf = msm_sensor_get_prev_lines_pf,
	.sensor_get_prev_pixels_pl = msm_sensor_get_prev_pixels_pl,
	.sensor_get_pict_lines_pf = msm_sensor_get_pict_lines_pf,
	.sensor_get_pict_pixels_pl = msm_sensor_get_pict_pixels_pl,
	.sensor_get_pict_max_exp_lc = msm_sensor_get_pict_max_exp_lc,
	.sensor_get_pict_fps = msm_sensor_get_pict_fps,
#endif
	/* .sensor_set_fps = msm_sensor_set_fps, */
	.sensor_write_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = mt9v113_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = mt9v113_sensor_power_up,
	.sensor_power_down = mt9v113_sensor_power_down,
// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
	.sensor_set_fps = msm_sensor_set_fps,
#ifdef CONFIG_MACH_LGE
	.sensor_set_wb = mt9v113_set_wb,
	.sensor_set_effect = mt9v113_set_effect,
	.sensor_set_brightness = mt9v113_set_brightness,
	.sensor_set_soc_minmax_fps = mt9v113_set_fps,
#endif
	.sensor_get_csi_params = msm_sensor_get_csi_params,
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }
};

static struct msm_sensor_reg_t mt9v113_regs = {
	.default_data_type = MSM_CAMERA_I2C_WORD_DATA,
	.start_stream_conf = mt9v113_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(mt9v113_start_settings),
	.stop_stream_conf = mt9v113_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(mt9v113_stop_settings),
	.group_hold_on_conf = mt9v113_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(mt9v113_groupon_settings),
	.group_hold_off_conf = mt9v113_groupoff_settings,
	.group_hold_off_conf_size =
	ARRAY_SIZE(mt9v113_groupoff_settings),
	.init_settings = &mt9v113_init_conf[0],
	.init_size = ARRAY_SIZE(mt9v113_init_conf),
	.mode_settings = &mt9v113_confs[0],
	.output_settings = &mt9v113_dimensions[0],
	.num_conf = ARRAY_SIZE(mt9v113_confs),
};

static struct msm_sensor_ctrl_t mt9v113_s_ctrl = {
	.msm_sensor_reg = &mt9v113_regs,
	.sensor_i2c_client = &mt9v113_sensor_i2c_client,
	.sensor_i2c_addr = 0x7A,
	.sensor_output_reg_addr = &mt9v113_reg_addr,
	.sensor_id_info = &mt9v113_id_info,
	.sensor_exp_gain_info = &mt9v113_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &mt9v113_csi_params_array[0],
	.msm_sensor_mutex = &mt9v113_mut,
	.sensor_i2c_driver = &mt9v113_i2c_driver,
	.sensor_v4l2_subdev_info = mt9v113_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9v113_subdev_info),
	.sensor_v4l2_subdev_ops = &mt9v113_subdev_ops,
	.func_tbl = &mt9v113_func_tbl,
// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("mt9v113 VT sensor driver");
MODULE_LICENSE("GPL v2");

