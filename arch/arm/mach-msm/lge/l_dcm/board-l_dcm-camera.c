/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <mach/msm_bus_board.h>
#include <mach/gpiomux.h>
#include "devices.h"
#include "board-l_dcm.h"
#include <mach/board_lge.h>
#include CONFIG_BOARD_HEADER_FILE

#ifdef CONFIG_MSM_CAMERA

/*
 * QCT Original
 *
 * NOT USED!
 *
 */
#if !defined(CONFIG_MACH_LGE)
#if (defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)) && \
	defined(CONFIG_I2C)

static struct i2c_board_info cam_expander_i2c_info[] = {
	{
		I2C_BOARD_INFO("sx1508q", 0x22),
		.platform_data = &msm8960_sx150x_data[SX150X_CAM]
	},
};

static struct msm_cam_expander_info cam_expander_info[] = {
	{
		cam_expander_i2c_info,
		MSM_8960_GSBI4_QUP_I2C_BUS_ID,
	},
};
#endif
#endif /* QCT Original */

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 1*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 2*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 3*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_5, /*active 4*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_6, /*active 5*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_2, /*active 6*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_3, /*active 7*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*i2c suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},

};

/*
 * QCT Original
 *
 * Flash config is merged into msm8960_cam_common_configs.
 *
 */
#if !defined(CONFIG_MACH_LGE)
static struct msm_gpiomux_config msm8960_cdp_flash_configs[] = {
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};
#endif


static struct msm_gpiomux_config msm8960_cam_common_configs[] = {
	{
		.gpio = GPIO_VGACAM_LDO_EN, /* 64 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_FLASH_EN, /* 1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_VCM_EN, /* 2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MCLK0, /* 4 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MCLK1, /* 5 */
		.settings = {
			[GPIOMUX_ACTIVE]	= &cam_settings[6],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM2_RST_N, /* 76 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM1_RST_N, /* 107 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};

static struct msm_gpiomux_config msm8960_cam_2d_configs[] = {
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 19,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_I2C_SDA, /* 20 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_I2C_SCL, /* 21 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};

#define VFE_CAMIF_TIMER1_GPIO 2
#define VFE_CAMIF_TIMER2_GPIO 3
#define VFE_CAMIF_TIMER3_GPIO_INT 4
static struct msm_camera_sensor_strobe_flash_data strobe_flash_xenon = {
	.flash_trigger = VFE_CAMIF_TIMER2_GPIO,
	.flash_charge = VFE_CAMIF_TIMER1_GPIO,
	.flash_charge_done = VFE_CAMIF_TIMER3_GPIO_INT,
	.flash_recharge_duration = 50000,
	.irq = MSM_GPIO_TO_INT(VFE_CAMIF_TIMER3_GPIO_INT),
};

/*
 * QCT Original
 *
 * NOT USED!
 *
 */
#if !defined(CONFIG_MACH_LGE)
#ifdef CONFIG_MSM_CAMERA_FLASH
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_EXT,
	._fsrc.ext_driver_src.led_en = VFE_CAMIF_TIMER1_GPIO,
	._fsrc.ext_driver_src.led_flash_en = VFE_CAMIF_TIMER2_GPIO,
	._fsrc.ext_driver_src.flash_id = MAM_CAMERA_EXT_LED_FLASH_SC628A,
};
#endif
#endif /* QCT Original */

static struct msm_bus_vectors cam_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 27648000,
		.ib  = 110592000,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 154275840,
		.ib  = 617103360,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 274423680,
		.ib  = 1097694720,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
};

static struct msm_bus_vectors cam_zsl_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 302071680,
/* Start LGE_BSP_CAMERA : jongkyung.kim@lge.com 2012-02-20 Continuos Shot Preveiw& Capture Abnormal Phenomenon with ZSL, Case: 00754853 */
#if 0
		.ib  = 1208286720,
#else
		.ib  = 1812430080,
#endif
/* End LGE_BSP_CAMERA   : jongkyung.kim@lge.com 2012-02-20 Continuos Shot Preveiw& Capture Abnormal Phenomenon with ZSL, Case: 00754853 */
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
/* Start LGE_BSP_CAMERA : jongkyung.kim@lge.com 2012-02-20 Continuos Shot Preveiw& Capture Abnormal Phenomenon with ZSL, Case: 00754853 */
#if 0
		.ib  = 1350000000,
#else
		.ib  = 2025000000,
#endif
/* End LGE_BSP_CAMERA	: jongkyung.kim@lge.com 2012-02-20 Continuos Shot Preveiw& Capture Abnormal Phenomenon with ZSL, Case: 00754853 */
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
};

static struct msm_bus_paths cam_bus_client_config[] = {
	{
		ARRAY_SIZE(cam_init_vectors),
		cam_init_vectors,
	},
	{
		ARRAY_SIZE(cam_preview_vectors),
		cam_preview_vectors,
	},
	{
		ARRAY_SIZE(cam_video_vectors),
		cam_video_vectors,
	},
	{
		ARRAY_SIZE(cam_snapshot_vectors),
		cam_snapshot_vectors,
	},
	{
		ARRAY_SIZE(cam_zsl_vectors),
		cam_zsl_vectors,
	},
};

static struct msm_bus_scale_pdata cam_bus_client_pdata = {
		cam_bus_client_config,
		ARRAY_SIZE(cam_bus_client_config),
		.name = "msm_camera",
};

static struct msm_camera_device_platform_data msm_camera_csi_device_data[] = {
	{
		.csid_core = 0,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
	{
		.csid_core = 1,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
	{
		.csid_core = 2,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
};

static struct camera_vreg_t msm_8960_back_cam_vreg[] = {
	{"cam1_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam1_vio", REG_VS, 0, 0, 0},
	{"cam1_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam1_vaf", REG_LDO, 2800000, 2800000, 300000},
};

static struct camera_vreg_t msm_8960_front_cam_vreg[] = {
	{"cam2_vio", REG_VS, 0, 0, 0},
	{"cam2_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam2_vdig", REG_LDO, 1200000, 1200000, 105000},
};

static struct gpio msm8960_common_cam_gpio[] = {
	{4, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{5, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{20, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{21, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
};

static struct gpio msm8960_front_cam_gpio[] = {
	{76, GPIOF_DIR_OUT, "CAM_RESET"},
};

static struct gpio msm8960_back_cam_gpio[] = {
	{107, GPIOF_DIR_OUT, "CAM_RESET"},
	{53, GPIOF_DIR_OUT, "VCM_ENABLE"},
};

static struct msm_gpio_set_tbl msm8960_front_cam_gpio_set_tbl[] = {
	{76, GPIOF_OUT_INIT_LOW, 1000},
	{76, GPIOF_OUT_INIT_HIGH, 4000},
};

static struct msm_gpio_set_tbl msm8960_back_cam_gpio_set_tbl[] = {
	{107, GPIOF_OUT_INIT_LOW, 1000},
	{107, GPIOF_OUT_INIT_HIGH, 4000},
	{53, GPIOF_OUT_INIT_HIGH, 1000},
};

static struct msm_camera_gpio_conf msm_8960_front_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio),
	.cam_gpio_req_tbl = msm8960_front_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio),
	.cam_gpio_set_tbl = msm8960_front_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio_set_tbl),
};

static struct msm_camera_gpio_conf msm_8960_back_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio),
	.cam_gpio_req_tbl = msm8960_back_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio),
	.cam_gpio_set_tbl = msm8960_back_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio_set_tbl),
};

static struct i2c_board_info msm_act_main_cam_i2c_info = {
	I2C_BOARD_INFO("msm_actuator", 0x18),
};

static struct msm_actuator_info msm_act_main_cam_0_info = {
	.board_info     = &msm_act_main_cam_i2c_info,
	.cam_name   = MSM_ACTUATOR_MAIN_CAM_1,
	.bus_id         = MSM_8960_GSBI4_QUP_I2C_BUS_ID,
	.vcm_pwd        = 0,
	.vcm_enable     = 1,
};

/*
static struct i2c_board_info msm_act_main_cam1_i2c_info = {
	I2C_BOARD_INFO("msm_actuator", 0x18),
};

static struct msm_actuator_info msm_act_main_cam_1_info = {
	.board_info     = &msm_act_main_cam1_i2c_info,
	.cam_name       = MSM_ACTUATOR_MAIN_CAM_1,
	.bus_id         = MSM_8960_GSBI4_QUP_I2C_BUS_ID,
	.vcm_pwd        = 0,
	.vcm_enable     = 0,
};
*/

#ifdef CONFIG_IMX111
static struct msm_camera_sensor_flash_data flash_imx111 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
#if !defined(CONFIG_MACH_LGE)
#ifdef CONFIG_MSM_CAMERA_FLASH
	.flash_src	= &msm_flash_src
#endif
#endif
};

static struct msm_camera_csi_lane_params imx111_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx111 = {
	.mount_angle	= 90,
	.cam_vreg = msm_8960_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_back_cam_vreg),
	.gpio_conf = &msm_8960_back_cam_gpio_conf,
	.csi_lane_params = &imx111_csi_lane_params,
};


static struct i2c_board_info imx111_eeprom_i2c_info = {
	I2C_BOARD_INFO("imx111_eeprom", I2C_SLAVE_ADDR_IMX111),
};

static struct msm_eeprom_info imx111_eeprom_info = {
	.board_info     = &imx111_eeprom_i2c_info,
	.bus_id         = MSM_8960_GSBI4_QUP_I2C_BUS_ID,
};


static struct msm_camera_sensor_info msm_camera_sensor_imx111_data = {
	.sensor_name	= "imx111",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_imx111,
	.strobe_flash_data = &strobe_flash_xenon,
	.sensor_platform_info = &sensor_board_info_imx111,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
	.actuator_info = &msm_act_main_cam_0_info,
	.eeprom_info = &imx111_eeprom_info,
};
#endif /* CONFIG_IMX111 */

#ifdef CONFIG_MT9V113
static struct msm_camera_sensor_flash_data flash_mt9v113 = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_csi_lane_params mt9v113_csi_lane_params = {
	.csi_lane_assign = 0xE4,
// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
#if 1
	.csi_lane_mask = 0x1, // 1111b:4lane, 0011b:2lane, 0001b:1lane
#else
	.csi_lane_mask = 0x3,
#endif
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }
};

static struct msm_camera_sensor_platform_info sensor_board_info_mt9v113 = {
	.mount_angle	= 270,
	.cam_vreg = msm_8960_front_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_front_cam_vreg),
	.gpio_conf = &msm_8960_front_cam_gpio_conf,
	.csi_lane_params = &mt9v113_csi_lane_params,
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9v113_data = {
	.sensor_name	= "mt9v113",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data	= &flash_mt9v113,
	.sensor_platform_info = &sensor_board_info_mt9v113,
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
// LGE_CHNAGE_S sungsik.kim 2012/11/07 {
// for YUV sensor[JB]
#if 1
	.sensor_type = YUV_SENSOR,
#else
	.sensor_type = BAYER_SENSOR,
#endif
// LGE_CHNAGE_E sungsik.kim 2012/11/07 }
};
#endif /* CONFIG_MT9V113 */

/*
 * QCT Original
 *
 * privacy_light_info is related to front cam indicator LED. (NOT USED)
 *
 */
#if !defined(CONFIG_MACH_LGE)
static struct pm8xxx_mpp_config_data privacy_light_on_config = {
	.type		= PM8XXX_MPP_TYPE_SINK,
	.level		= PM8XXX_MPP_CS_OUT_5MA,
	.control	= PM8XXX_MPP_CS_CTRL_MPP_LOW_EN,
};

static struct pm8xxx_mpp_config_data privacy_light_off_config = {
	.type		= PM8XXX_MPP_TYPE_SINK,
	.level		= PM8XXX_MPP_CS_OUT_5MA,
	.control	= PM8XXX_MPP_CS_CTRL_DISABLE,
};

static int32_t msm_camera_8960_ext_power_ctrl(int enable)
{
	int rc = 0;
	if (enable) {
		rc = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(12),
			&privacy_light_on_config);
	} else {
		rc = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(12),
			&privacy_light_off_config);
	}
	return rc;
}
#endif /* CONFIG_MACH_LGE */

static struct platform_device msm_camera_server = {
	.name = "msm_cam_server",
	.id = 0,
};

void __init msm8960_init_cam(void)
{
	msm_gpiomux_install(msm8960_cam_common_configs,
			ARRAY_SIZE(msm8960_cam_common_configs));

#if !defined(CONFIG_MACH_LGE)
	if (machine_is_msm8960_cdp()) {
		msm_gpiomux_install(msm8960_cdp_flash_configs,
			ARRAY_SIZE(msm8960_cdp_flash_configs));
		msm_flash_src._fsrc.ext_driver_src.led_en =
			GPIO_CAM_GP_LED_EN1;
		msm_flash_src._fsrc.ext_driver_src.led_flash_en =
			GPIO_CAM_GP_LED_EN2;
		#if defined(CONFIG_I2C) && (defined(CONFIG_GPIO_SX150X) || \
		defined(CONFIG_GPIO_SX150X_MODULE))
		msm_flash_src._fsrc.ext_driver_src.expander_info =
			cam_expander_info;
		#endif
	}
#endif

/*
 * QCT Original
 *
 * mount_angle change is not required.
 * privacy_light is related to front cam indicator LED. (NOT USED)
 *
 */
#if !defined(CONFIG_MACH_LGE)
	if (machine_is_msm8960_liquid()) {
		struct msm_camera_sensor_info *s_info;
		s_info = &msm_camera_sensor_imx111_data;
		s_info->sensor_platform_info->mount_angle = 180;
		s_info = &msm_camera_sensor_mt9v113_data;
		s_info->sensor_platform_info->ext_power_ctrl =
			msm_camera_8960_ext_power_ctrl;
	}

	if (machine_is_msm8960_fluid()) {
		msm_camera_sensor_imx119_data.sensor_platform_info->
			mount_angle = 270;
	}
#endif /* QCT Original */

	platform_device_register(&msm_camera_server);
	platform_device_register(&msm8960_device_csiphy0);
	platform_device_register(&msm8960_device_csiphy1);
	platform_device_register(&msm8960_device_csiphy2);
	platform_device_register(&msm8960_device_csid0);
	platform_device_register(&msm8960_device_csid1);
	platform_device_register(&msm8960_device_csid2);
	platform_device_register(&msm8960_device_ispif);
	platform_device_register(&msm8960_device_vfe);
	platform_device_register(&msm8960_device_vpe);
}

#ifdef CONFIG_I2C
static struct i2c_board_info msm8960_camera_i2c_boardinfo[] = {
#ifdef CONFIG_IMX111
	{
	I2C_BOARD_INFO("imx111", I2C_SLAVE_ADDR_IMX111), /* 0x0D */
	.platform_data = &msm_camera_sensor_imx111_data,
	},
#endif /* CONFIG_IMX111 */
#ifdef CONFIG_MT9V113
	{
	I2C_BOARD_INFO("mt9v113", I2C_SLAVE_ADDR_MT9V113), /* 0x6E */
	.platform_data = &msm_camera_sensor_mt9v113_data,
	},
#endif /* CONFIG_MT9V113 */
};

struct msm_camera_board_info msm8960_camera_board_info = {
	.board_info = msm8960_camera_i2c_boardinfo,
	.num_i2c_board_info = ARRAY_SIZE(msm8960_camera_i2c_boardinfo),
};

#ifdef CONFIG_MSM_CAMERA_FLASH_LM3559
struct led_flash_platform_data {
	int gpio_en;
};

static struct led_flash_platform_data lm3559_flash_pdata = {
	.gpio_en = GPIO_CAM_FLASH_EN,
};

static struct i2c_board_info cam_i2c_flash_info[] = {
	{
	I2C_BOARD_INFO("lm3559", I2C_SLAVE_ADDR_LM3559), /* 0x53 */
	.type = "lm3559",
	.platform_data = &lm3559_flash_pdata,
	}
};
#endif /* CONFIG_MSM_CAMERA_FLASH_LM3559 */

/*
 * The body of lge_add_camera_devices() is from
 * register_i2c_devices(void) function,
 * which is defined in board-8960.c file of QCT original.
 *
 */
void __init lge_add_camera_devices(void)
{
	struct i2c_registry msm8960_camera_i2c_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8960_GSBI4_QUP_I2C_BUS_ID,
		msm8960_camera_board_info.board_info,
		msm8960_camera_board_info.num_i2c_board_info,
	};
#ifdef CONFIG_MSM_CAMERA_FLASH_LM3559
	struct i2c_registry msm8960_camera_flash_i2c_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI8_QUP_I2C_BUS_ID,
		cam_i2c_flash_info,
		ARRAY_SIZE(cam_i2c_flash_info),
	};
#endif

	pr_err("%s: E\n", __func__);

	msm8960_init_cam();

	i2c_register_board_info(msm8960_camera_i2c_devices.bus,
		msm8960_camera_i2c_devices.info,
		msm8960_camera_i2c_devices.len);

#ifdef CONFIG_MSM_CAMERA_FLASH_LM3559
	i2c_register_board_info(msm8960_camera_flash_i2c_devices.bus,
		msm8960_camera_flash_i2c_devices.info,
		msm8960_camera_flash_i2c_devices.len);
#endif

	pr_err("%s: X\n", __func__);
}

#endif /* CONFIG_I2C */

#endif /* CONFIG_MSM_CAMERA at line 29 */
