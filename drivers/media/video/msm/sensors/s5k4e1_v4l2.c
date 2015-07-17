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
#define SENSOR_NAME "s5k4e1"
#define PLATFORM_DRIVER_NAME "msm_camera_s5k4e1"
#define s5k4e1_obj s5k4e1_##obj

#ifdef CONFIG_MACH_MSM8960_L1v 
#define LGIT_IEF_SWITCH 

#ifdef LGIT_IEF_SWITCH
extern int mipi_lgd_lcd_ief_off(void);
extern int mipi_lgd_lcd_ief_on(void);
#endif
#endif


DEFINE_MUTEX(s5k4e1_mut);
static struct msm_sensor_ctrl_t s5k4e1_s_ctrl;

static struct msm_camera_i2c_reg_conf s5k4e1_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e1_stop_settings[] = {
	{0x0100, 0x00},
};
static struct msm_camera_i2c_reg_conf s5k4e1_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e1_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k4e1_prev_settings[] = {
	{0x301B, 0x83},/* CDS option setting */
	{0x30BC, 0x98},/* 0xB0=>0x98 */
	/* PLL setting ... */
	/* (3) MIPI 2-lane Serial(TST = 0000b or TST = 0010b), 30 fps */
	{0x0305, 0x06},/* PLL P = 6 */
	{0x0306, 0x00},/* PLL M[8] = 0 */
	{0x0307, 0x64},/* 0x32=>0x64 */
	{0x30B5, 0x01},/* 0x00=>0x01 */
	{0x30E2, 0x02},
	{0x30F1, 0x70},
	/* output size (1304 x 980) */
	/* MIPI Size Setting ... */
	{0x30A9, 0x02},/* Horizontal Binning On */
	{0x300E, 0xEB},/* Vertical Binning On */
	{0x0344, 0x00},/* x_addr_start 0 */
	{0x0345, 0x00},
	{0x0348, 0x0A},/* x_addr_end 2607 */
	{0x0349, 0x2F},
	{0x0346, 0x00},/* y_addr_start 0 */
	{0x0347, 0x00},
	{0x034A, 0x07},/* y_addr_end 1959 */
	{0x034B, 0xA7},
	{0x0380, 0x00},/* x_even_inc 1 */
	{0x0381, 0x01},
	{0x0382, 0x00},/* x_odd_inc 1 */
	{0x0383, 0x01},
	{0x0384, 0x00},/* y_even_inc 1 */
	{0x0385, 0x01},
	{0x0386, 0x00},/* y_odd_inc 3 */
	{0x0387, 0x03},
	{0x034C, 0x05},/* x_output_size 1304 */
	{0x034D, 0x18},
	{0x034E, 0x03},/* y_output_size 980 */
	{0x034F, 0xd4},
	{0x30BF, 0xAB},/* outif_enable[7], data_type[5:0](2Bh = bayer 10bit} */
	{0x30C0, 0xA0},/* video_offset[7:4] 3260%12 */
	{0x30C8, 0x06},/* video_data_length 1600 = 1304 * 1.25 */
	{0x30C9, 0x5E},
	/* Integration setting */
#if 0 // randy@qctk
	{0x0202, 0x03},		//coarse integration time
	{0x0203, 0xD4},
	{0x0204, 0x00},		//analog gain[msb] 0100 x8 0080 x4
	{0x0205, 0x80},		//analog gain[lsb] 0040 x2 0020 x1
#else
	{0x0202, 0x03}, //coarse integration time
	{0x0203, 0xCE},
	{0x0204, 0x00}, //analog gain[msb]
	{0x0205, 0x30}, //analog gain[lsb]
#endif
};
/* LGE_CHANGE_S Full HD support, 2012.03.28 yt.kim@lge.com */
static struct msm_camera_i2c_reg_conf s5k4e1_video_settings[] = {
	{0x301B, 0x75 },/* CDS option setting */
	{0x30BC, 0xA8 },/* 0x98=>0xA8 */
	/* PLL setting ... */
	/* (3) MIPI 2-lane Serial(TST = 0000b or TST = 0010b), 30 fps */
	{0x0305, 0x04},/* PLL P = 6 */
	{0x0306, 0x00},/* PLL M[8] = 0 */
	{0x0307, 0x66},
	{0x30B5, 0x01},
	{0x30E2, 0x02},
	{0x30F1, 0xA0},

	/* output size (1936 x 1096) */
	/* MIPI Size Setting ... */
	// MIPI Size Setting
	{0x30A9, 0x03},/* Horizontal Binning On */
	{0x300E, 0xE8},/* Vertical Binning On */
	{0x034C, 0x07},/* x_output size */
	{0x034D, 0x90},
	{0x034E, 0x04},/* y_output size */
	{0x034F, 0x48},
	{0x0344, 0x01},/* x_addr_start 336 */
	{0x0345, 0x50},
	{0x0346, 0x01},/* y_addr_start 432 */
	{0x0347, 0xB0},
	{0x0348, 0x08},/* x_addr_end 2272 */
	{0x0349, 0xDF},//E0=>DF
	{0x034A, 0x05},/* y_addr_end 1528 */
	{0x034B, 0xF7},//0xF8=>0xF7 0829
    {0x0380, 0x00},/* x_even_inc 1 */
	{0x0381, 0x01},
	{0x0382, 0x00},/* x_odd_inc 1 */
	{0x0383, 0x01},
	{0x0384, 0x00},/* y_even_inc 1 */
	{0x0385, 0x01},
	{0x0386, 0x00},/* y_odd_inc 1 */
	{0x0387, 0x01},
	{0x30BF, 0xAB},
	{0x30C0, 0x80},
	{0x30C8, 0x09},
	{0x30C9, 0x74},
	/* Integration setting */
#if 0 // randy@qctk
	{0x0202, 0x04},  //coarse integration time
	{0x0203, 0x48},
	{0x0204, 0x00},  //analog gain[msb] 0100 x8 0080 x4
	{0x0205, 0x80},  //analog gain[lsb] 0040 x2 0020 x1
#else
	{0x0202, 0x05}, //coarse integration time
	{0x0203, 0xB7},
	{0x0204, 0x00}, //analog gain[msb]
	{0x0205, 0x20}, //analog gain[lsb]
#endif
};
/* LGE_CHANGE_E Full HD support, 2012.03.28 yt.kim@lge.com */

static struct msm_camera_i2c_reg_conf s5k4e1_snap_settings[] = {
	{0x301B, 0x75},/* CDS option setting */
	{0x30BC, 0x98},/* 0xB0=>0x98 */
	/* PLL setting ... */
	/* (3) MIPI 2-lane Serial(TST = 0000b or TST = 0010b), 30 fps */
	{0x0305, 0x06},/* PLL P = 6 */
	{0x0306, 0x00},/* PLL M[8] = 0 */
	{0x0307, 0x64},/* 0x32=>0x64 */
	{0x30B5, 0x01},/* 0x00=>0x01 */
	{0x30E2, 0x02},
	{0x30F1, 0x70},
	/* output size (2608 x 1960) */
	/* MIPI Size Setting ... */
	{0x30A9, 0x03},/*Horizontal Binning Off */
	{0x300E, 0xE8},/* Vertical Binning Off */
	{0x0344, 0x00},/* x_addr_start 0 */
	{0x0345, 0x00},
	{0x0348, 0x0A},/* x_addr_end 2607 */
	{0x0349, 0x2F},
	{0x0346, 0x00},/* y_addr_start 0 */
	{0x0347, 0x00},
	{0x034A, 0x07},/* y_addr_end 1959 */
	{0x034B, 0xA7},
	{0x0380, 0x00},/* x_even_inc 1 */
	{0x0381, 0x01},
	{0x0382, 0x00},/* x_odd_inc 1 */
	{0x0383, 0x01},
	{0x0384, 0x00},/* y_even_inc 1 */
	{0x0385, 0x01},
	{0x0386, 0x00},/* y_odd_inc 3 */
	{0x0387, 0x01},
	{0x034C, 0x0A},/* x_output_size */
	{0x034D, 0x30},
	{0x034E, 0x07},/* y_output_size */
	{0x034F, 0xA8},
	{0x30BF, 0xAB},/* outif_enable[7], data_type[5:0](2Bh = bayer 10bit} */
	{0x30C0, 0x80},/* video_offset[7:4] 3260%12 */
	{0x30C8, 0x0C},/* video_data_length 3260 = 2608 * 1.25 */
	{0x30C9, 0xBC},
	/* Integration setting */
#if 0 // randy@qctk
	{0x0202, 0x07}, 		 //coarse integration time
	{0x0203, 0xA8},
	{0x0204, 0x00}, 		 //analog gain[msb] 0100 x8 0080 x4
	{0x0205, 0x80}, 		 //analog gain[lsb] 0040 x2 0020 x1
#else
	{0x0202, 0x03}, //coarse integration time
	{0x0203, 0xCE},
	{0x0204, 0x00}, //analog gain[msb]
	{0x0205, 0x30}, //analog gain[lsb]
#endif
};

/* LGE_CHANGE_S Camera Zero shutter lag, 2012.03.12 yt.kim@lge.com */
static struct msm_camera_i2c_reg_conf s5k4e1_zsl_settings[] = {
	{0x301B, 0x75},/* CDS option setting */
	{0x30BC, 0x98},/* 0xB0=>0x98 */
	// PLL setting ...
	//// input clock 24MHz
	////// (3) MIPI 2-lane Serial(TST = 0000b or TST = 0010b), 16.5 fps
	{ 0x0305, 0x06 },	//PLL P = 6
	{ 0x0306, 0x00 },	//PLL M[8] = 0
	{ 0x0307, 0x6B },	//PLL M = 107
	{ 0x30B5, 0x01 },	//PLL S = 1
	{ 0x30E2, 0x02 },	//num lanes[1:0] = 2
	{ 0x30F1, 0x70 },	//DPHY BANDCTRL 856MHz=85.6MHz /2
	/* output size (2560 x 1920) */
	/* MIPI Size Setting ... */
	{ 0x30A9, 0x03 },	//Horizontal Binning Off
	{ 0x300E, 0xE8 },	//Vertical Binning Off
	{ 0x034C, 0x0A },	//x_output size
	{ 0x034D, 0x00 },
	{ 0x034E, 0x07 },	//y_output size
	{ 0x034F, 0x80 },
	{ 0x0344, 0x00 },	//x_addr_start
	{ 0x0345, 0x18 },
	{ 0x0346, 0x00 },	//y_addr_start
	{ 0x0347, 0x14 },
	{ 0x0348, 0x0A },	//x_addr_end
	{ 0x0349, 0x17 },
	{ 0x034A, 0x07 },	//y_addr_end
	{ 0x034B, 0x93 },
    {0x0380, 0x00},/* x_even_inc 1 */
	{0x0381, 0x01},
	{0x0382, 0x00},/* x_odd_inc 1 */
	{0x0383, 0x01},
	{0x0384, 0x00},/* y_even_inc 1 */
	{0x0385, 0x01},
	{0x0386, 0x00},/* y_odd_inc 1 */
	{0x0387, 0x01},
	{ 0x30BF, 0xAB },	//outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
	{ 0x30C0, 0x80 },	//video_offset[7:4] 3200%12
	{ 0x30C8, 0x0C },	//video_data_length 3200 = 2560 * 1.25
	{ 0x30C9, 0x80 },
	/* Integration setting */
#if 0 // randy@qctk
	{ 0x0202, 0x07 },		 //coarse integration time
	{ 0x0203, 0x80 },
	{ 0x0204, 0x00 },		 //analog gain[msb] 0100 x8 0080 x4
	{ 0x0205, 0x80 },		 //analog gain[lsb] 0040 x2 0020 x1
#else
	{0x0202, 0x04}, //coarse integration time
	{0x0203, 0x5A},
	{0x0204, 0x00}, //analog gain[msb]
	{0x0205, 0x2A}, //analog gain[lsb]
#endif
};
/* LGE_CHANGE_E Camera Zero shutter lag, 2012.03.12 yt.kim@lge.com */

/* If model uses H-V flip register for 180 rotation and EEPROM reading
 * You shoud set LGE_SENSOR_ROTATE_180_USED in Android.mk
 * See rolloff.c file in vendor
 */
static struct msm_camera_i2c_reg_conf s5k4e1_recommend_settings[] = {
	/* Analog Setting */
	{0x3030, 0x06},
	/* CDS timing setting ... */
	{0x3000, 0x05},
	{0x3001, 0x03},
	{0x3002, 0x08},
	{0x3003, 0x09},
	{0x3004, 0x2E},
	{0x3005, 0x06},
	{0x3006, 0x34},
	{0x3007, 0x00},
	{0x3008, 0x3C},
	{0x3009, 0x3C},
	{0x300A, 0x28},
	{0x300B, 0x04},
	{0x300C, 0x0A},
	{0x300D, 0x02},
	{0x300F, 0x82},
	/* CDS option setting ... */
	{0x3010, 0x00},
	{0x3011, 0x4C},
	{0x3012, 0x30},
	{0x3013, 0xC0},
	{0x3014, 0x00},
	{0x3015, 0x00},
	{0x3016, 0x2C},
	{0x3017, 0x94},
	{0x3018, 0x78},
	{0x301B, 0x83},
	{0x301D, 0xD4},
	{0x3021, 0x02},
	{0x3022, 0x24},
	{0x3024, 0x40},
	{0x3027, 0x08},
	{0x3029, 0xC6},
	{0x30BC, 0xB0},
	{0x302B, 0x01},
	/* Pixel option setting ... */
	{0x301C, 0x04},
	{0x30D8, 0x3F},
	/* ADLC setting ... */
	{0x3070, 0x5F},
	{0x3071, 0x00},
	{0x3080, 0x04},
	{0x3081, 0x38},
	/* H-V flip*/
	{0x0101, 0x03},
	/* MIPI setting */
	{0x30BD, 0x00},
	{0x3084, 0x15},
	{0x30BE, 0x15},
	{0x30C1, 0x01},
//	{0x30EE, 0x02},
    {0x30EE, 0x12},//MIPI_test_1130 {0x30EE, 0x02},
	{0x3111, 0x86},
		//MIPI_test_1130 added
		{0x30E3, 0x38},
		{0x30E4, 0x40},
		{0x3113, 0x70},
		{0x3114, 0x80},
		  {0x3115, 0x7B}

};


static struct v4l2_subdev_info s5k4e1_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order  = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array s5k4e1_init_conf[] = {
	{&s5k4e1_recommend_settings[0],
	ARRAY_SIZE(s5k4e1_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array s5k4e1_confs[] = {
	{&s5k4e1_snap_settings[0],
	ARRAY_SIZE(s5k4e1_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k4e1_prev_settings[0],
	ARRAY_SIZE(s5k4e1_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
/* LGE_CHANGE_S Full HD support, 2012.03.28 yt.kim@lge.com */
	{&s5k4e1_video_settings[0],
	ARRAY_SIZE(s5k4e1_video_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
/* LGE_CHANGE_E Full HD support, 2012.03.28 yt.kim@lge.com */
/* LGE_CHANGE_S Camera Zero shutter lag, 2012.03.12 yt.kim@lge.com */
	{&s5k4e1_zsl_settings[0],
	ARRAY_SIZE(s5k4e1_zsl_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
/* LGE_CHANGE_E Camera Zero shutter lag, 2012.03.12 yt.kim@lge.com */
};

static struct msm_sensor_output_info_t s5k4e1_dimensions[] = {
	{
		/*snapshot */
		.x_output = 0xA30,/*2608 => 2560(48)*/
		.y_output = 0x7A8,/*1960 => 1920(40)*/
		.line_length_pclk = 0xAB2,
		.frame_length_lines = 0x7B4,
		.vt_pixel_clk = 80000000,
		.op_pixel_clk = 80000000,
		.binning_factor = 1,
	},
	{
		/* preview */
		.x_output = 0x518,/*1304, =>1280(24)=>1296(8)*/
		.y_output = 0x3D4,/*980, =>960(20)=>972(8)*/
		.line_length_pclk = 0xAB2,
		.frame_length_lines = 0x3E0,
		.vt_pixel_clk = 80000000,
		.op_pixel_clk = 80000000,
		.binning_factor = 1,
	},
/* LGE_CHANGE_S Full HD support, 2012.03.28 yt.kim@lge.com */
	{
		/* video */
		.x_output = 0x790,/*1936, =>1920(16)*/
		.y_output = 0x448,/*1096, =>1088(8)*/
		.line_length_pclk = 0xAB2,
		.frame_length_lines = 0x5C8,
		.vt_pixel_clk = 122400000,
		.op_pixel_clk = 122400000,
		.binning_factor = 1,
	},
/* LGE_CHANGE_E Full HD support, 2012.03.28 yt.kim@lge.com */
/* LGE_CHANGE_S Camera Zero shutter lag, 2012.03.12 yt.kim@lge.com */
	{
		.x_output = 0xA00,//2569  0xA30,/*2608 => 2560(48)*/
		.y_output = 0x780,//1920  0x7A8,/*1960 => 1920(40)*/
		.line_length_pclk = 0xAB2,
		.frame_length_lines = 0x78C,
		.vt_pixel_clk = 85600000,
		.op_pixel_clk = 85600000,
		.binning_factor = 1,
	},
/* LGE_CHANGE_E Camera Zero shutter lag, 2012.03.12 yt.kim@lge.com */
};

static struct msm_camera_csid_vc_cfg s5k4e1_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params s5k4e1_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 2,
		.lut_params = {
			.num_cid = ARRAY_SIZE(s5k4e1_cid_cfg),
			.vc_cfg = s5k4e1_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 2,
		.settle_cnt = 0x18,
	},
};

static struct msm_camera_csi2_params *s5k4e1_csi_params_array[] = {
	&s5k4e1_csi_params,
	&s5k4e1_csi_params,
	&s5k4e1_csi_params,/* LGE_CHANGE Full HD support, 2012.03.28 yt.kim@lge.com */
	&s5k4e1_csi_params,/* LGE_CHANGE_S Camera Zero shutter lag, 2012.03.12 yt.kim@lge.com */
};

static struct msm_sensor_output_reg_addr_t s5k4e1_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t s5k4e1_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x4E10,
};

static struct msm_sensor_exp_gain_info_t s5k4e1_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x204,
	.vert_offset = 8,
};

#if 0
int32_t s5k4e1_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl, struct fps_cfg *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	s_ctrl->fps_divider = fps->fps_div;
	if (s_ctrl->curr_res != MSM_SENSOR_INVALID_RES) {
		uint16_t fl_read = 0;
		total_lines_per_frame = (uint16_t)
		((s_ctrl->curr_frame_length_lines) * s_ctrl->fps_divider/Q10);
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines,
		&fl_read, MSM_CAMERA_I2C_WORD_DATA);

		CDBG("%s: before_fl = %d, new_fl = %d", __func__, fl_read, total_lines_per_frame);
		if(fl_read < total_lines_per_frame) {
			pr_err("%s: Write new_fl (before_fl = %d, new_fl = %d)", __func__, fl_read, total_lines_per_frame);
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			total_lines_per_frame, MSM_CAMERA_I2C_WORD_DATA);
		}
	}
	return rc;
}
#endif

static int32_t s5k4e1_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{

	int32_t rc = 0;
	uint16_t max_legal_gain = 0x0200;
	static uint32_t fl_lines;
	uint16_t curr_fl_lines;
	uint8_t offset;

	curr_fl_lines = s_ctrl->curr_frame_length_lines;
	curr_fl_lines = (curr_fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;

	CDBG("#QCT s5k4e1_write_exp_gain1 = %d, %d \n", gain, line);

	if (gain > max_legal_gain) {
		pr_debug("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);

	/* Analogue Gain */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
			MSM_CAMERA_I2C_WORD_DATA);

	if (line > (curr_fl_lines - offset)) {
		fl_lines = line+offset;
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);

		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
		CDBG("#QCT 11 s5k4e1_write_exp_gain1 = %d, %d \n", line, fl_lines);

//	} else if (line < (fl_lines - offset)) {
	} else if (line < (curr_fl_lines - offset)) {
		fl_lines = line+offset;
		if (fl_lines < curr_fl_lines)
			fl_lines = curr_fl_lines;

		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
		CDBG("#QCT 22 s5k4e1_write_exp_gain1 = %d, %d \n", line, fl_lines);
	} else {
		fl_lines = line+offset;
		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
		CDBG("#QCT 33 s5k4e1_write_exp_gain1 = %d, %d \n", line, 0);
	}
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return rc;
}

/* LGE_CHANGE_S, L1 uses s5k4e1_write_exp_gain1 function, 2012-07-05, kyungjin.min@lge.com */
#if 0
static int32_t s5k4e1_write_exp_gain2(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	int32_t rc = 0;
	uint16_t max_legal_gain = 0x0200;
	uint8_t offset;


	pr_debug("s5k4e1_write_exp_gain2 : gain = %d line = %d\n", gain, line);
	CDBG("#QCT s5k4e1_write_exp_gain2 = %d, %d \n", gain, line);

	if (gain > max_legal_gain) {
			pr_debug("Max legal gain Line:%d\n", __LINE__);
			gain = max_legal_gain;
	}


	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;

	if (fl_lines - offset < line)
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
			MSM_CAMERA_I2C_WORD_DATA);

	/* Coarse Integration Time */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

	msleep(130);/* need to revisit */

	CDBG("#QCT s5k4e1_write_exp_gain2 after: gain,line,fl_lines, s_ctrl->curr_frame_length_lines"
		"0x%x, 0x%x, 0x%x, 0x%x ",gain,line,fl_lines, fl_lines);
	CDBG("#QCT s5k4e1_write_exp_gain2 after: gain,line,fl_lines, s_ctrl->curr_frame_length_lines"
		"%d, %d, %d, %d ",gain,line,fl_lines, fl_lines);

	return rc;


}
#endif
/* LGE_CHANGE_S, L1 uses s5k4e1_write_exp_gain1 function, 2012-07-05, kyungjin.min@lge.com */

/* LGE_CHANGE_S, Fixed flash saturation problem, 2012-10-16, kyungjin.min@lge.com */
static int32_t s5k4e1_write_exp_gain3(struct msm_sensor_ctrl_t *s_ctrl,
       uint16_t gain, uint32_t line)
{

       int32_t rc = 0;
       uint16_t max_legal_gain = 0x0200;
       static uint32_t fl_lines;
       uint16_t curr_fl_lines;
       uint8_t offset;

       curr_fl_lines = s_ctrl->curr_frame_length_lines;
       offset = s_ctrl->sensor_exp_gain_info->vert_offset;

       CDBG("#QCT s5k4e1_write_exp_gain1 = %d, %d \n", gain, line);

       if (gain > max_legal_gain) {
 	      pr_debug("Max legal gain Line:%d\n", __LINE__);
    	  gain = max_legal_gain;
       }
       s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);

       /* Analogue Gain */
       msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	       s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
	       MSM_CAMERA_I2C_WORD_DATA);

       if (line > (curr_fl_lines - offset)) {
       		fl_lines = line+offset;
     		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	       		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
	      		 MSM_CAMERA_I2C_WORD_DATA);

	       /* Coarse Integration Time */
	       msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		       s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		       MSM_CAMERA_I2C_WORD_DATA);
	       CDBG("#QCT 11 s5k4e1_write_exp_gain1 = %d, %d \n", line, fl_lines);

       } else if (line < (fl_lines - offset)) {
	       fl_lines = line+offset;
	       if (fl_lines < curr_fl_lines)
	       fl_lines = curr_fl_lines;

	       /* Coarse Integration Time */
	       msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		       s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		       MSM_CAMERA_I2C_WORD_DATA);
	       msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		       s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		       MSM_CAMERA_I2C_WORD_DATA);
		       CDBG("#QCT 22 s5k4e1_write_exp_gain1 = %d, %d \n", line, fl_lines);
       } else {
	       fl_lines = line+offset;
	       /* Coarse Integration Time */
	       msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		       s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		       MSM_CAMERA_I2C_WORD_DATA);
	       CDBG("#QCT 33 s5k4e1_write_exp_gain1 = %d, %d \n", line, 0);
       }
       s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);


       printk("[CHECK] 200ms delay inserted!");
//       msleep(200);/* need to revisit */
       msleep(50);/* need to revisit */

       return rc;
 }
 /* LGE_CHANGE_E, Fixed flash saturation problem, 2012-10-16, kyungjin.min@lge.com */
static const struct i2c_device_id s5k4e1_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k4e1_s_ctrl},
	{ }
};

static struct i2c_driver s5k4e1_i2c_driver = {
	.id_table = s5k4e1_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k4e1_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	printk("msm_sensor_init_module s5k4e1 \n");
	return i2c_add_driver(&s5k4e1_i2c_driver);
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static int32_t s5k4e1_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	printk("%s: E, %d\n", __func__, __LINE__); /* LGE_CHANGE, Show log always, 2012-05-24, sunkyoo.hwang@lge.com */
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
	printk("%s: msm_camera_enable_vreg, %d\n", __func__, __LINE__);
	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}
/* minimum sleep 20ms for EEPROM read */
	msleep(20);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	/* LGE_CHANGE_S, Turn IEF off in camera mode, 2012-10-23 */
	#ifdef LGIT_IEF_SWITCH
	if(system_state != SYSTEM_BOOTING){
		printk("[IEF_OFF] Camera \n");
			mipi_lgd_lcd_ief_off();
	}
	#endif
	/* LGE_CHANGE_E, Turn IEF off in camera mode, 2012-10-23 */

	printk("%s: X, %d\n", __func__, __LINE__); /* LGE_CHANGE, Show log always, 2012-05-24, sunkyoo.hwang@lge.com */
	return rc;
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
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
	kfree(s_ctrl->reg_ptr);
	return rc;
}
static int32_t s5k4e1_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	printk("%s: E, %d\n", __func__, __LINE__); /* LGE_CHANGE, Show log always, 2012-05-24, sunkyoo.hwang@lge.com */


	/* LGE_CHANGE_S, Turn IEF off in camera mode, 2012-10-23 */
		#ifdef LGIT_IEF_SWITCH
		if(system_state != SYSTEM_BOOTING){
			printk("[IEF_ON] Camera \n");
				mipi_lgd_lcd_ief_on();
		}
		#endif
		/* LGE_CHANGE_E, Turn IEF off in camera mode, 2012-10-23*/

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(20);
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	printk("%s: X, %d\n", __func__, __LINE__); /* LGE_CHANGE, Show log always, 2012-05-24, sunkyoo.hwang@lge.com */
	return 0;
}

#if 1
/*
 *Start :randy@qualcomm.com for calibration
 */
#define S5K4E1_EEPROM_SADDR 	0x28 >> 1
#ifdef LSC_CALIBRATION
#define S5K4E1_EEPROM_START_ADDR 0x0070
#define S5K4E1_EEPROM_DATA_SIZE	896
#else /* AWB CAL only! */
#define S5K4E1_EEPROM_START_ADDR 0x0060
#define S5K4E1_EEPROM_DATA_SIZE	6
#endif
#define RED_START 	0
#define GR_START 	224
#define GB_START 	448
#define BLUE_START 	672
// #define CRC_ADDR	0x7E

static int32_t s5k4e1_i2c_read_eeprom_burst(unsigned char saddr,
		unsigned char *rxdata, int length)
{
	int32_t rc = 0;
	unsigned char tmp_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = 2,
			.buf   = tmp_buf,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	tmp_buf[0] = (unsigned char)((S5K4E1_EEPROM_START_ADDR & 0xFF00) >> 8);
	tmp_buf[1] = (unsigned char)(S5K4E1_EEPROM_START_ADDR & 0xFF);
	//printk("msgs[0].buf = 0x%x,  msgs[0].buf[1]=0x%x\n", msgs[0].buf[0], msgs[0].buf[1] );
	//printk(" tmp_buf[0] = 0x%x,  tmp_buf[1]=0x%x\n", tmp_buf[0], tmp_buf[1] );

	rc = i2c_transfer(s5k4e1_s_ctrl.sensor_i2c_client->client->adapter, msgs, 2);
	if (rc < 0)
		pr_err("s5k4e1_i2c_read_eeprom_burst failed 0x%x\n", saddr);
	return rc;
}

static int s5k4e1_read_eeprom_data(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cfg)
{
	int32_t rc = 0;
	uint8_t eepromdata[S5K4E1_EEPROM_DATA_SIZE];
	printk("s5k4e1_read_eeprom_data==============================>start\n");
#ifdef LSC_CALIBRATION
	uint32_t crc_red= 0, crc_gr= 0, crc_gb= 0, crc_blue=0;
	int i;
	printk("s5k4e1_read_eeprom_data==============================>start\n");
	memset(eepromdata, 0, sizeof(eepromdata));
	// for LSC data
	//msleep(20);
	if(s5k4e1_i2c_read_eeprom_burst(S5K4E1_EEPROM_SADDR,
		eepromdata, S5K4E1_EEPROM_DATA_SIZE) < 0) {
		pr_err("%s: Error Reading EEPROM : page_no:0 \n", __func__);
		return rc;
	}

	for (i = 0; i < ROLLOFF_CALDATA_SIZE; i++) {
		cfg->cfg.calib_info.rolloff.r_gain[i] = eepromdata[RED_START + i];
		crc_red += eepromdata[RED_START + i];
		//~printk("[QCTK_EEPROM] R (0x%x, %d)\n", RED_START + i, eepromdata[RED_START + i]);

		cfg->cfg.calib_info.rolloff.gr_gain[i] = eepromdata[GR_START + i];
		crc_gr += eepromdata[GR_START + i];
		//~printk("[QCTK_EEPROM] GR (0x%x, %d)\n", GR_START + i, eepromdata[GR_START + i]);

		cfg->cfg.calib_info.rolloff.gb_gain[i] = eepromdata[GB_START + i];
		crc_gb += eepromdata[GB_START + i];
		//~printk("[QCTK_EEPROM] GB (0x%x, %d)\n", GB_START + i, eepromdata[GB_START + i]);

		cfg->cfg.calib_info.rolloff.b_gain[i] = eepromdata[BLUE_START + i];
		crc_blue += eepromdata[BLUE_START + i];
		//~printk("[QCTK_EEPROM] B (0x%x, %d)\n", BLUE_START + i, eepromdata[BLUE_START + i]);

	}

#if 0
	// CRC check
	if (((eepromdata[CRC_ADDR]<<8)+eepromdata[CRC_ADDR+1]) != crc_5100)
		{
			pr_err("%s: CRC error R(read crc:0x%x, cal crc:0x%x)\n", __func__,
			(eepromdata[CRC_ADDR]<<8)+eepromdata[CRC_ADDR+1], crc_red);
			// return -EFAULT;
		}
#endif
#if 1
	// for AWB data - from Rolloff data
	cfg->cfg.calib_info.r_over_g = (uint16_t)(((((uint32_t)cfg->cfg.calib_info.rolloff.r_gain[ROLLOFF_CALDATA_SIZE / 2])<<10)
			+ ((uint32_t)(cfg->cfg.calib_info.rolloff.gr_gain[ROLLOFF_CALDATA_SIZE / 2]) >>1))
			/ (uint32_t)cfg->cfg.calib_info.rolloff.gr_gain[ROLLOFF_CALDATA_SIZE / 2]);

	//~printk("[QCTK_EEPROM] r_over_g = 0x%4x\n", cfg->cfg.calib_info.r_over_g);
	cfg->cfg.calib_info.b_over_g = (uint16_t)(((((uint32_t)cfg->cfg.calib_info.rolloff.b_gain[ROLLOFF_CALDATA_SIZE / 2])<<10)
			+ ((uint32_t)(cfg->cfg.calib_info.rolloff.gb_gain[ROLLOFF_CALDATA_SIZE / 2]) >>1))
			/ (uint32_t)cfg->cfg.calib_info.rolloff.gb_gain[ROLLOFF_CALDATA_SIZE / 2]);
	//~printk("[QCTK_EEPROM] b_over_g = 0x%4x\n", cfg->cfg.calib_info.b_over_g);
	cfg->cfg.calib_info.gr_over_gb = (uint16_t)(((((uint32_t)cfg->cfg.calib_info.rolloff.gr_gain[ROLLOFF_CALDATA_SIZE / 2])<<10)
			+ ((uint32_t)(cfg->cfg.calib_info.rolloff.gb_gain[ROLLOFF_CALDATA_SIZE / 2]) >>1))
			/ (uint32_t)cfg->cfg.calib_info.rolloff.gb_gain[ROLLOFF_CALDATA_SIZE / 2]);
	//~printk("[QCTK_EEPROM] gr_over_gb = 0x%4x\n", cfg->cfg.calib_info.gr_over_gb);
#endif

#else
	if(s5k4e1_i2c_read_eeprom_burst(S5K4E1_EEPROM_SADDR,
		eepromdata, S5K4E1_EEPROM_DATA_SIZE) < 0) {
		pr_err("%s: Error Reading EEPROM : page_no:0 \n", __func__);
		return rc;
	}

	cfg->cfg.calib_info.r_over_g = eepromdata[0] + (eepromdata[1]<<8);
	//printk("[QCTK_EEPROM] r_over_g = 0x%4x\n", cfg->cfg.calib_info.r_over_g);
	cfg->cfg.calib_info.b_over_g = eepromdata[2] + (eepromdata[3]<<8);
	//printk("[QCTK_EEPROM] b_over_g = 0x%4x\n", cfg->cfg.calib_info.b_over_g);
	cfg->cfg.calib_info.gr_over_gb = eepromdata[4] + (eepromdata[5]<<8);
	//printk("[QCTK_EEPROM] gr_over_gb = 0x%4x\n", cfg->cfg.calib_info.gr_over_gb);
#endif
	printk("s5k4e1_read_eeprom_data==============================>end\n");
	return 0;
}
/*
 *End :randy@qualcomm.com for calibration 2012.06.04
 */
#endif
static struct v4l2_subdev_core_ops s5k4e1_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};
static struct v4l2_subdev_video_ops s5k4e1_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k4e1_subdev_ops = {
	.core = &s5k4e1_subdev_core_ops,
	.video  = &s5k4e1_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k4e1_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = s5k4e1_write_exp_gain1,
/* LGE_CHANGE_S, L1 uses s5k4e1_write_exp_gain1 function, 2012-07-05, kyungjin.min@lge.com */
//	.sensor_write_snapshot_exp_gain = s5k4e1_write_exp_gain1,
//	.sensor_write_snapshot_exp_gain = s5k4e1_write_exp_gain2,
	.sensor_write_snapshot_exp_gain = s5k4e1_write_exp_gain3,

/* LGE_CHANGE_E, L1 uses s5k4e1_write_exp_gain1 function, 2012-07-05, kyungjin.min@lge.com */
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = s5k4e1_sensor_power_up,
	.sensor_power_down = s5k4e1_sensor_power_down,
	/* randy@qualcomm.com for calibration 2012.06.04*/
	.sensor_get_eeprom_data = s5k4e1_read_eeprom_data,
/* LGE_CHANGE_S, L1 uses sensor_get_csi_params function, 2012-08-18, kyungjin.min@lge.com */
	.sensor_get_csi_params = msm_sensor_get_csi_params,
/* LGE_CHANGE_E, L1 uses sensor_get_csi_params function, 2012-08-18, kyungjin.min@lge.com */
};

static struct msm_sensor_reg_t s5k4e1_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = s5k4e1_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k4e1_start_settings),
	.stop_stream_conf = s5k4e1_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k4e1_stop_settings),
	.group_hold_on_conf = s5k4e1_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(s5k4e1_groupon_settings),
	.group_hold_off_conf = s5k4e1_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(s5k4e1_groupoff_settings),
	.init_settings = &s5k4e1_init_conf[0],
	.init_size = ARRAY_SIZE(s5k4e1_init_conf),
	.mode_settings = &s5k4e1_confs[0],
	.output_settings = &s5k4e1_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k4e1_confs),
};

static struct msm_sensor_ctrl_t s5k4e1_s_ctrl = {
	.msm_sensor_reg = &s5k4e1_regs,
	.sensor_i2c_client = &s5k4e1_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &s5k4e1_reg_addr,
	.sensor_id_info = &s5k4e1_id_info,
	.sensor_exp_gain_info = &s5k4e1_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &s5k4e1_csi_params_array[0],
	.msm_sensor_mutex = &s5k4e1_mut,
	.sensor_i2c_driver = &s5k4e1_i2c_driver,
	.sensor_v4l2_subdev_info = s5k4e1_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4e1_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k4e1_subdev_ops,
	.func_tbl = &s5k4e1_func_tbl,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Samsung 5 MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
