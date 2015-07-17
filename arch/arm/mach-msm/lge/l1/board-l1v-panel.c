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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/ion.h>
#include <asm/mach-types.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/socinfo.h>

#include "devices.h"
#ifndef CONFIG_MACH_LGE

#endif

#include <linux/fb.h>
#include "../../../../drivers/video/msm/msm_fb.h"
#include "../../../../drivers/video/msm/msm_fb_def.h"
#include "../../../../drivers/video/msm/mipi_dsi.h"

#include <mach/board_lge.h>
#include CONFIG_BOARD_HEADER_FILE

#ifdef CONFIG_LGE_KCAL
#ifdef CONFIG_LGE_QC_LCDC_LUT
extern int set_qlut_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_qlut_display(void);
#else
#error only kcal by Qucalcomm LUT is supported now!!!
#endif
#endif

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_PRIM_BUF_SIZE (LCD_RESOLUTION_X * LCD_RESOLUTION_Y * 4 * 3)
/*  4(bpp) x 3(pages) */
#else
#define MSM_FB_PRIM_BUF_SIZE (LCD_RESOLUTION_X * LCD_RESOLUTION_Y * 4 * 2)
/*  4(bpp) x 2(pages) */
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#define MSM_FB_EXT_BUF_SIZE	(1920 * 1088 * 2 * 1) /* 2 bpp x 1 page */
#elif defined(CONFIG_FB_MSM_TVOUT)
#define MSM_FB_EXT_BUF_SIZE (720 * 576 * 2 * 2) /* 2 bpp x 2 pages */
#else
#define MSM_FB_EXT_BUF_SIZE	0
#endif

/* Note: must be multiple of 4096 */
#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE + MSM_FB_EXT_BUF_SIZE, 4096)

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((LCD_RESOLUTION_X * LCD_RESOLUTION_Y * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */


#define MDP_VSYNC_GPIO 0

#define MIPI_CMD_NOVATEK_QHD_PANEL_NAME	"mipi_cmd_novatek_qhd"
#define MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME	"mipi_video_novatek_qhd"
#define MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME	"mipi_video_toshiba_wsvga"
#define MIPI_VIDEO_TOSHIBA_WUXGA_PANEL_NAME	"mipi_video_toshiba_wuxga"
#define MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME	"mipi_video_chimei_wxga"
#define MIPI_VIDEO_CHIMEI_WUXGA_PANEL_NAME	"mipi_video_chimei_wuxga"
#define MIPI_VIDEO_SIMULATOR_VGA_PANEL_NAME	"mipi_video_simulator_vga"
#define MIPI_CMD_RENESAS_FWVGA_PANEL_NAME	"mipi_cmd_renesas_fwvga"
#define MIPI_VIDEO_ORISE_720P_PANEL_NAME	"mipi_video_orise_720p"
#define MIPI_CMD_ORISE_720P_PANEL_NAME		"mipi_cmd_orise_720p"
#define HDMI_PANEL_NAME	"hdmi_msm"
#define TVOUT_PANEL_NAME	"tvout_msm"

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
unsigned char hdmi_is_primary = 1;
//static unsigned char hdmi_is_primary = 1;
#else
unsigned char hdmi_is_primary;
//static unsigned char hdmi_is_primary;
#endif
#define TUNING_BUFSIZE 4096
#define TUNING_REGSIZE 40
#define TUNING_REGNUM 10
#define LCD_GAMMA 1     /*  lx_kr is LCD_GAMMA ON  */
unsigned char msm8960_hdmi_as_primary_selected(void)
{
	return hdmi_is_primary;
}

static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

#ifndef CONFIG_MACH_LGE
#ifndef CONFIG_FB_MSM_MIPI_PANEL_DETECT
static void set_mdp_clocks_for_wuxga(void);
#endif
#endif

static int msm_fb_detect_panel(const char *name)
{
#ifndef CONFIG_MACH_LGE
	if (machine_is_msm8960_liquid()) {
		u32 ver = socinfo_get_platform_version();
		if (SOCINFO_VERSION_MAJOR(ver) == 3) {
			if (!strncmp(name, MIPI_VIDEO_CHIMEI_WUXGA_PANEL_NAME,
				     strnlen(MIPI_VIDEO_CHIMEI_WUXGA_PANEL_NAME,
						PANEL_NAME_MAX_LEN))) {
				set_mdp_clocks_for_wuxga();
				return 0;
			}
		} else {
			if (!strncmp(name, MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME,
				     strnlen(MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME,
						PANEL_NAME_MAX_LEN)))
				return 0;
		}
	} else {
		if (!strncmp(name, MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
				strnlen(MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

#if !defined(CONFIG_FB_MSM_LVDS_MIPI_PANEL_DETECT) && \
	!defined(CONFIG_FB_MSM_MIPI_PANEL_DETECT)
		if (!strncmp(name, MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME,
				strnlen(MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

		if (!strncmp(name, MIPI_CMD_NOVATEK_QHD_PANEL_NAME,
				strnlen(MIPI_CMD_NOVATEK_QHD_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

		if (!strncmp(name, MIPI_VIDEO_SIMULATOR_VGA_PANEL_NAME,
				strnlen(MIPI_VIDEO_SIMULATOR_VGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

		if (!strncmp(name, MIPI_CMD_RENESAS_FWVGA_PANEL_NAME,
				strnlen(MIPI_CMD_RENESAS_FWVGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

		if (!strncmp(name, MIPI_VIDEO_TOSHIBA_WUXGA_PANEL_NAME,
				strnlen(MIPI_VIDEO_TOSHIBA_WUXGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN))) {
			set_mdp_clocks_for_wuxga();
			return 0;
		}

		if (!strncmp(name, MIPI_VIDEO_ORISE_720P_PANEL_NAME,
				strnlen(MIPI_VIDEO_ORISE_720P_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

		if (!strncmp(name, MIPI_CMD_ORISE_720P_PANEL_NAME,
				strnlen(MIPI_CMD_ORISE_720P_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;
#endif
	}

	if (!strncmp(name, HDMI_PANEL_NAME,
			strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
		if (hdmi_is_primary)
			set_mdp_clocks_for_wuxga();
		return 0;
	}

	if (!strncmp(name, TVOUT_PANEL_NAME,
				strnlen(TVOUT_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
		return 0;

	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
#else
	return 0;
#endif
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

static bool dsi_power_on;
static char mipi_dsi_splash_is_enabled(void);

#if defined(CONFIG_MACH_MSM8960_D1LSK) || defined(CONFIG_MACH_MSM8960_D1LKT) || defined(CONFIG_MACH_MSM8960_D1LU) || defined(CONFIG_MACH_MSM8960_L1U) || defined(CONFIG_MACH_MSM8960_L1SK) || defined(CONFIG_MACH_MSM8960_L1KT) || defined(CONFIG_MACH_MSM8960_L0) || defined(CONFIG_MACH_MSM8960_L1m) || defined(CONFIG_MACH_MSM8960_L1v) 
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_l8, *reg_l2, *reg_lvs6;
	static int gpio43;
	int rc;

	struct pm_gpio gpio43_param = {
		.direction = PM_GPIO_DIR_OUT,
		.output_buffer = PM_GPIO_OUT_BUF_CMOS,
		.output_value = 0,
		.pull = PM_GPIO_PULL_NO,
		.vin_sel = 2,
		.out_strength = PM_GPIO_STRENGTH_HIGH,
		.function = PM_GPIO_FUNC_PAIRED,
		.inv_int_pol = 0,
		.disable_pin = 0,
	};

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdc");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8921_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}

		reg_lvs6 = regulator_get(&msm_mipi_dsi1_device.dev,
				"8921_lvs6");
		if (IS_ERR(reg_lvs6)) {
			pr_err("could not get 8921_lvs6, rc = %ld\n",
				PTR_ERR(reg_lvs6));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l8, 3000000, 3000000);
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		gpio43 = PM8921_GPIO_PM_TO_SYS(43);
		rc = gpio_request(gpio43, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 43 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		dsi_power_on = true;
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_l8, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
        /*  START :  lx_kr panel power lvs6  */
		rc = regulator_set_optimum_mode(reg_lvs6, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode lvs6 failed, rc=%d\n", rc);
			return -EINVAL;
		}
        /*  END :  lx_kr panel power lvs6  */
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l8);
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_enable(reg_lvs6);
		if (rc) {
			pr_err("enable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio43_param.pull = PM_GPIO_PULL_NO;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (1), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_UP_30;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (2), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_NO;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (3), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_UP_30;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (4), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio_set_value_cansleep(gpio43, 1);
		mdelay(20);
		gpio_set_value_cansleep(gpio43, 0);
		mdelay(20);
		gpio_set_value_cansleep(gpio43, 1);
		mdelay(20);
	} else {
		gpio_set_value_cansleep(gpio43, 0);
		rc = regulator_disable(reg_l8);
		if (rc) {
			pr_err("disable reg_l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_lvs6);
		if (rc) {
			pr_err("disable reg_lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
        /*  START :  lx_kr panel power lvs6  */
		rc = regulator_set_optimum_mode(reg_lvs6, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode lvs6 failed, rc=%d\n", rc);
			return -EINVAL;
		}
        /*  END :  lx_kr panel power lvs6  */
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	return 0;
}
#elif defined(CONFIG_FB_MSM_MIPI_LGD_VIDEO_QHD_PT_PANEL)
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_l8, *reg_l2, *reg_lvs6;
	static int gpio43;
	int rc;

	struct pm_gpio gpio43_param = {
		.direction = PM_GPIO_DIR_OUT,
		.output_buffer = PM_GPIO_OUT_BUF_CMOS,
		.output_value = 0,
		.pull = PM_GPIO_PULL_NO,
		.vin_sel = 2,
		.out_strength = PM_GPIO_STRENGTH_HIGH,
		.function = PM_GPIO_FUNC_PAIRED,
		.inv_int_pol = 0,
		.disable_pin = 0,
	};

	pr_debug("%s: state : %d\n", __func__, on);
	printk(KERN_ERR "mipi_dsi_panel_power ON\n");

	if (!dsi_power_on) {

		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdc");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8921_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}

		reg_lvs6 = regulator_get(&msm_mipi_dsi1_device.dev,
				"8921_lvs6");
		if (IS_ERR(reg_lvs6)) {
			pr_err("could not get 8921_lvs6, rc = %ld\n",
				PTR_ERR(reg_lvs6));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l8, 3000000, 3000000);
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		gpio43 = PM8921_GPIO_PM_TO_SYS(43);
		rc = gpio_request(gpio43, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 43 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		dsi_power_on = true;
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_lvs6, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode lvs6 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l8, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_lvs6);
		if (rc) {
			pr_err("enable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_enable(reg_l8);
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio43_param.pull = PM_GPIO_PULL_NO;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (1), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_UP_30;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (2), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_NO;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (3), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_UP_30;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (4), rc=%d\n", rc);
			return -EINVAL;
		}
		mdelay(2);
		gpio_set_value_cansleep(gpio43, 1);
		mdelay(7);
	} else {
		gpio_set_value_cansleep(gpio43, 0);
		rc = regulator_disable(reg_l8);
		if (rc) {
			pr_err("disable reg_l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_lvs6);
		if (rc) {
			pr_err("disable reg_lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_lvs6, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode lvs6 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	return 0;
}
#elif defined(CONFIG_MACH_MSM8960_D1LV)
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_l8, *reg_l29, *reg_l2;
	static int gpio43;
	int rc;

	struct pm_gpio gpio43_param = {
		.direction = PM_GPIO_DIR_OUT,
		.output_buffer = PM_GPIO_OUT_BUF_CMOS,
		.output_value = 0,
		.pull = PM_GPIO_PULL_NO,
		.vin_sel = 2,
		.out_strength = PM_GPIO_STRENGTH_HIGH,
		.function = PM_GPIO_FUNC_PAIRED,
		.inv_int_pol = 0,
		.disable_pin = 0,
	};

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdc");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8921_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}

		reg_l29 = regulator_get(&msm_mipi_dsi1_device.dev,
				"8921_l29");
		if (IS_ERR(reg_l29)) {
			pr_err("could not get 8921_l29, rc = %ld\n",
				PTR_ERR(reg_l29));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l8, 2800000, 2800000);
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_voltage(reg_l29, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage l29 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		gpio43 = PM8921_GPIO_PM_TO_SYS(43);
		rc = gpio_request(gpio43, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 43 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		dsi_power_on = true;
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_l8, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l29, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l29 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l8);
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		mdelay(5);

		rc = regulator_enable(reg_l29);
		if (rc) {
			pr_err("enable l29 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		mdelay(5);

		rc = gpio_request(LCD_VCI_EN_GPIO, "LCD_VCI_EN_GPIO");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"LCD_VCI_EN_GPIO", LCD_VCI_EN_GPIO, rc);
			gpio_free(LCD_VCI_EN_GPIO);
		}
		rc = gpio_direction_output(LCD_VCI_EN_GPIO, 1);
		mdelay(5);

		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio43_param.pull = PM_GPIO_PULL_NO;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (1), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_UP_30;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (2), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_NO;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (3), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio43_param.pull = PM_GPIO_PULL_UP_30;
		rc = pm8xxx_gpio_config(gpio43, &gpio43_param);
		if (rc) {
			pr_err("gpio_config 43 failed (4), rc=%d\n", rc);
			return -EINVAL;
		}
		gpio_set_value_cansleep(gpio43, 1);
		mdelay(20);
		gpio_set_value_cansleep(gpio43, 0);
		mdelay(20);
		gpio_set_value_cansleep(gpio43, 1);
		mdelay(20);
	} else {
		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = gpio_direction_output(LCD_VCI_EN_GPIO, 0);
		gpio_free(LCD_VCI_EN_GPIO);

		rc = regulator_set_optimum_mode(reg_l29, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l29 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	return 0;
}
#else
static int mipi_dsi_panel_power(int on)
{
	int ret;

	pr_debug("%s: on=%d\n", __func__, on);

	if (machine_is_msm8960_liquid())
		ret = mipi_dsi_liquid_panel_power(on);
	else
		ret = mipi_dsi_cdp_panel_power(on);

	return ret;
}
#endif

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = MDP_VSYNC_GPIO,
	.dsi_power_save = mipi_dsi_panel_power,
	.splash_is_enabled = mipi_dsi_splash_is_enabled,
};

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors mdp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors mdp_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 230400000 * 2,
		.ib = 288000000 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 334080000 * 2,
		.ib = 417600000 * 2,
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

#endif

static int mdp_core_clk_rate_table[] = {
	128000000,/*85330000,*/
	128000000,/*85330000,*/
	160000000,
	200000000,
};

struct msm_fb_info_st {
	unsigned int width_mm;
	unsigned int height_mm;
};

static struct msm_fb_info_st msm_fb_info_data = {
	.width_mm = MSM_FB_WIDTH_MM,
	.height_mm = MSM_FB_HEIGHT_MM
};

static int msm_fb_event_notify(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct fb_event *event = data;
	struct fb_info *info = event->info;
	struct msm_fb_info_st *fb_info_mm = &msm_fb_info_data;
	int ret = 0;

	switch (action) {
	case FB_EVENT_FB_REGISTERED:
		info->var.width = fb_info_mm->width_mm;
		info->var.height = fb_info_mm->height_mm;
		break;
	}
	return ret;
}

static struct notifier_block msm_fb_event_notifier = {
	.notifier_call = msm_fb_event_notify,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_core_clk_rate = 128000000, /*85330000,*/
	.mdp_core_clk_table = mdp_core_clk_rate_table,
	.num_mdp_clk = ARRAY_SIZE(mdp_core_clk_rate_table),
#ifdef CONFIG_MSM_BUS_SCALING
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
#endif
	.mdp_rev = MDP_REV_42,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
};

#ifndef CONFIG_MACH_LGE
#ifndef CONFIG_FB_MSM_MIPI_PANEL_DETECT
/**
 * Set MDP clocks to high frequency to avoid DSI underflow
 * when using high resolution 1200x1920 WUXGA panels
 */
static void set_mdp_clocks_for_wuxga(void)
{
	int i;

	mdp_ui_vectors[0].ab = 2000000000;
	mdp_ui_vectors[0].ib = 2000000000;
	mdp_vga_vectors[0].ab = 2000000000;
	mdp_vga_vectors[0].ib = 2000000000;
	mdp_720p_vectors[0].ab = 2000000000;
	mdp_720p_vectors[0].ib = 2000000000;
	mdp_1080p_vectors[0].ab = 2000000000;
	mdp_1080p_vectors[0].ib = 2000000000;

	mdp_pdata.mdp_core_clk_rate = 200000000;

	for (i = 0; i < ARRAY_SIZE(mdp_core_clk_rate_table); i++)
		mdp_core_clk_rate_table[i] = 200000000;

}
#endif
#endif

void __init msm8960_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}
static char mipi_dsi_splash_is_enabled(void)
{
	return mdp_pdata.cont_splash_enabled;
}
#if defined(CONFIG_LGE_BACKLIGHT_LM3533)
extern void lm3533_lcd_backlight_set_level(int level);
#elif defined (CONFIG_LGE_BACKLIGHT_LM3530)       /*  lx_kr backlight lm3530  */
extern void lm3530_lcd_backlight_set_level(int level);
#endif   /*  lx_kr backlight patch - insert #endif  */

#if defined(CONFIG_FB_MSM_MIPI_DSI_LGIT)
static int mipi_lgit_backlight_level(int level, int max, int min)
{
	lm3533_lcd_backlight_set_level(level);
	return 0;
}

static struct msm_panel_common_pdata mipi_lgit_pdata = {
	.backlight_level = mipi_lgit_backlight_level,
};

static struct platform_device mipi_dsi_lgit_panel_device = {
	.name = "mipi_lgit",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgit_pdata,
	}
};
#elif defined(CONFIG_FB_MSM_MIPI_DSI_HITACHI)
static int mipi_hitachi_backlight_level(int level, int max, int min)
{
#if defined (CONFIG_LGE_BACKLIGHT_LM3533)   /*  lx_kr backlight patch - insert #if  */
	lm3533_lcd_backlight_set_level(level);
#elif defined (CONFIG_LGE_BACKLIGHT_LM3530)       /*  lx_kr backlight lm3530  */
	lm3530_lcd_backlight_set_level(level);
#endif
	return 0;
}

#if defined(CONFIG_FB_MSM_MIPI_HITACHI_DX11D_VIDEO_HD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_HITACHI_DX11D_VIDEO_QHD_PT_PANEL)   /*  lx_kr panel patch */
/* HITACHI 4.5" HD panel */
/*  START :  lx_kr panel address 0x00 -> 0xC8  */
static char set_address_mode[2] = {0x36, 0xC8};
/*  END :  lx_kr panel address 0x00 -> 0xC8  */
static char set_pixel_format[2] = {0x3A, 0x70};

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};

static char macp_off[2] = {0xB0, 0x04};
static char macp_on[2] = {0xB0, 0x03};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
#define CABC_POWERON_OFFSET 4 /* offset from lcd display on cmds */

#define CABC_OFF 0
#define CABC_ON 1

#define CABC_10 1
#define CABC_20 2
#define CABC_30 3
#define CABC_40 4
#define CABC_50 5

#define CABC_DEFUALT CABC_10

#if defined (CONFIG_LGE_BACKLIGHT_CABC_DEBUG)
static int hitachi_cabc_index = CABC_DEFUALT;
#endif

static char backlight_ctrl1[2][6] = {

	/* off */
	{
		0xB8, 0x00, 0x1A, 0x1A,
		0x02, 0x40
	},
	/* on */
	{
		0xB8, 0x01, 0x1A, 0x1A,
		0x02, 0x40
	},
};

static char backlight_ctrl2[6][8] = {
	/* off */
	{
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	},
	/* 10% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},

	/* 20% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},

	/* 30% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},

	/* 40% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},
	/* 50% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	}
};

static char backlight_ctrl3[6][25] = {
	/* off */
	{
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00
	},
	/* 10% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x0A, 0x6C, 0x0A, 0xAC,
		0x0A, 0x0C, 0x0A, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0xD9, 0xE4,
		0xEE, 0xF7, 0xFF, 0x9F,
		0x00
	},
	/* 20% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x0B, 0x6C, 0x0B, 0xAC,
		0x0B, 0x0C, 0x0B, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0xB3, 0xC9,
		0xDC, 0xEE, 0xFF, 0x9F,
		0x00
	},
	/* 30% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x0D, 0x6C, 0x0D, 0xAC,
		0x0D, 0x0C, 0x0D, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0x8C, 0xAA,
		0xC7, 0xE3, 0xFF, 0x9F,
		0x00
	},
	/* 40% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x13, 0xAC, 0x13, 0x6C,
		0x13, 0x0C, 0x13, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0x67, 0x89,
		0xAF, 0xD6, 0xFF, 0x9F,
		0x00
	},
	/* 50% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x14, 0xAC, 0x14, 0x6C,
		0x14, 0x0C, 0x14, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0x37, 0x5A,
		0x87, 0xBD, 0xFF, 0x9F,
		0x00
	}
};
#endif

#if LCD_GAMMA
static char gamma_setting_a[23] = {
	0xC8, 0x00, 0x04, 0x03,
       0x08, 0xA9, 0x07, 0x09,
       0x01, 0x00, 0x04, 0x04,
       0x00, 0x04, 0x03, 0x08,
       0xA9, 0x07, 0x09, 0x01,
       0x00, 0x04, 0x04
};  /*  lx_kr LCD_GAMMA  */
static char gamma_setting_b[23] = {
	0xC9, 0x00, 0x04, 0x03,
       0x08, 0xA9, 0x07, 0x09,
       0x01, 0x00, 0x04, 0x04,
       0x00, 0x04, 0x03, 0x08,
       0xA9, 0x07, 0x09, 0x01,
       0x00, 0x04, 0x04
};  /*  lx_kr LCD_GAMMA  */
static char gamma_setting_c[29] = {
	0xCA, 0x00, 0x04, 0x03,
       0x08, 0xA9, 0x07, 0x09,
       0x01, 0x00, 0x04, 0x04,
       0x00, 0x04, 0x03, 0x08,
       0xA9, 0x07, 0x09, 0x01,
       0x00, 0x04, 0x04
};  /*  lx_kr LCD_GAMMA  */
#endif

static struct dsi_cmd_desc hitachi_power_on_set[] = {
	/* Display initial set */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_address_mode),
		set_address_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_pixel_format),
		set_pixel_format},

	/* Exit sleep mode */
    /*  START :  lx_kr exit_sleep 100 -> 130  */
	{DTYPE_DCS_WRITE, 1, 0, 0, 130, sizeof(exit_sleep), exit_sleep},
    /*  END :  lx_kr exit_sleep 100 -> 130  */

	/* Manufacturer command protect off */
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_off), macp_off},
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	/* Content adaptive backlight control */
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(backlight_ctrl1[0]),
		backlight_ctrl1[CABC_ON]},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(backlight_ctrl2[0]),
		backlight_ctrl2[CABC_DEFUALT]},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(backlight_ctrl3[0]),
		backlight_ctrl3[CABC_DEFUALT]},
#endif
#if LCD_GAMMA
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_a),
		gamma_setting_a},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_b),
		gamma_setting_b},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_c),
		gamma_setting_c},
#endif
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_on), macp_on},

	/* Display on */
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc hitachi_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
    /*  START :  lx_kr enter_sleep 0 -> 70   */
	{DTYPE_DCS_WRITE, 1, 0, 0, 70, sizeof(enter_sleep), enter_sleep}
    /*  END :  lx_kr enter_sleep 0 -> 70  */
};

#if defined (CONFIG_LGE_BACKLIGHT_CABC) &&\
		defined (CONFIG_LGE_BACKLIGHT_CABC_DEBUG)
void set_hitachi_cabc(int cabc_index)
{
	switch(cabc_index) {
	case 0: /* off */
	case 1: /* 10% */
	case 2: /* 20% */
	case 3: /* 30% */
	case 4: /* 40% */
	case 5: /* 50% */
		if (cabc_index == 0) { /* CABC OFF */
			hitachi_power_on_set[CABC_POWERON_OFFSET].payload =
						backlight_ctrl1[CABC_OFF];
		} else { /* CABC ON */
			hitachi_power_on_set[CABC_POWERON_OFFSET].payload =
						backlight_ctrl1[CABC_ON];
			hitachi_power_on_set[CABC_POWERON_OFFSET+1].payload =
						backlight_ctrl2[cabc_index];
			hitachi_power_on_set[CABC_POWERON_OFFSET+2].payload =
						backlight_ctrl3[cabc_index];
		}
		hitachi_cabc_index = cabc_index;
		break;
	default:
		printk("out of range cabc_index %d", cabc_index);
	}
	return;
}
EXPORT_SYMBOL(set_hitachi_cabc);

int get_hitachi_cabc(void)
{
	return hitachi_cabc_index;
}
EXPORT_SYMBOL(get_hitachi_cabc);

#endif
#elif defined(CONFIG_FB_MSM_MIPI_HITACHI_DX12D_VIDEO_HD_PT_PANEL)
static char panel_setting[3] = {0xB2, 0x00, 0xC7};
static char drive_setting[2] = {0xB3, 0x02};

static char display_ctrl1[8] = {0xB5, 0x01, 0x00, 0x22, 0x00, 0x20, 0x22, 0x00};
static char display_ctrl2[9] = {0xB6, 0x10, 0x01, 0x81, 0x01, 0x01, 0x00, 0x00,
	0x00};
static char display_ctrl3[8] = {0xB7, 0x46, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x00};
static char display_ctrl4[6] = {0xB8, 0x38, 0xF8, 0x2C, 0x66, 0xCC};

static char osc_setting[4] = {0xC0, 0x01, 0x05, 0x10};

static char power_ctrl2[3] = {0xC2, 0x07, 0x00};
static char power_ctrl3[6] = {0xC3, 0x20, 0x02, 0x00, 0x08, 0x0A};
static char power_ctrl4[7] = {0xC4, 0x01, 0x02, 0x00, 0x00, 0x11, 0x8A};
static char power_ctrl5[6] = {0xC5, 0x03, 0x33, 0x03, 0x00, 0x03};
static char power_ctrl6[3] = {0xC6, 0x22, 0x00};
/* Must chceck the P4 and P5 */
static char power_ctrl7[6] = {0xC7, 0x04, 0x96, 0x16, 0x00, 0x00};
static char power_ctrl8[3] = {0xC8, 0x43, 0x60};
static char power_ctrl9[4] = {0xC9, 0x00, 0x30, 0x10};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static char display_brightness[2] = {0x51, 0xFF};
static char write_ctrl_display[2] = {0x53, 0x24};
static char write_cabc[2] = {0x55, 0x02};
/* For CABL(Content Adaptive Backlight Control)*/
static char backlight_ctrl[5] = {0xCA, 0x82, 0xA7, 0x21, 0x11};
#endif
static char vref_gen[4] = {0xCB, 0x0B, 0x15, 0x34};

static char p_gamma_r_setting[10] = {0xD0, 0x00, 0x01, 0x41, 0x52, 0x00, 0x00,
	0x60, 0x04, 0x03};
static char n_gamma_r_setting[10] = {0xD1, 0x00, 0x01, 0x41, 0x52, 0x00, 0x00,
	0x60, 0x04, 0x03};
static char p_gamma_g_setting[10] = {0xD2, 0x00, 0x01, 0x41, 0x52, 0x00, 0x00,
	0x60, 0x04, 0x03};
static char n_gamma_g_setting[10] = {0xD3, 0x00, 0x01, 0x41, 0x52, 0x00, 0x00,
	0x60, 0x04, 0x03};
static char p_gamma_b_setting[10] = {0xD4, 0x00, 0x01, 0x41, 0x52, 0x00, 0x00,
	0x60, 0x04, 0x03};
static char n_gamma_b_setting[10] = {0xD5, 0x00, 0x01, 0x41, 0x52, 0x00, 0x00,
	0x60, 0x04, 0x03};

static char mipi_dsi_config[6] = {0xE0, 0x43, 0x80, 0x80, 0x00, 0x00};
static char discharge_mode[2] = {0xE7, 0x00};

static char exit_sleep[2] =  {0x11, 0x00};
static char display_on[2] =  {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};
static char deep_standby[2] = {0xC1, 0x01};

/* initialize device */
static struct dsi_cmd_desc hitachi_power_on_set[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting), panel_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(drive_setting), drive_setting},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl1), display_ctrl1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl2), display_ctrl2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl3), display_ctrl3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl4), display_ctrl4},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 5, sizeof(osc_setting), osc_setting},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl2), power_ctrl2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl3), power_ctrl3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl4), power_ctrl4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl5), power_ctrl5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl6), power_ctrl6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl7), power_ctrl7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl8), power_ctrl8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl9), power_ctrl9},

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	{DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(display_brightness),
		display_brightness},
	{DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(write_ctrl_display),
		write_ctrl_display},
	{DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(write_cabc), write_cabc},
	{DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(backlight_ctrl),
		backlight_ctrl},
#endif
	{DTYPE_GEN_LWRITE, 1, 0, 0, 100, sizeof(vref_gen), vref_gen},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_r_setting),
		p_gamma_r_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_r_setting),
		n_gamma_r_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_g_setting),
		p_gamma_g_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_g_setting),
		n_gamma_g_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_b_setting),
		p_gamma_b_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_b_setting),
		n_gamma_b_setting},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_dsi_config),
		mipi_dsi_config},
	{DTYPE_GEN_WRITE1, 1, 0, 0, 0, sizeof(discharge_mode), discharge_mode},

	{DTYPE_DCS_WRITE,  1, 0, 0, 150, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE,  1, 0, 0, 100, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc hitachi_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 100, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 100, sizeof(enter_sleep), enter_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0,  10, sizeof(deep_standby), deep_standby},
};
#endif /*#elif defined (CONFIG_FB_MSM_MIPI_HITACHI_DX12D_VIDEO_HD_PT_PANEL)*/

#ifdef CONFIG_LGE_LCD_TUNING
struct tuning_buff{
	char buf[TUNING_REGNUM][TUNING_REGSIZE];
	int size;
	int idx;
};

char set_buff[TUNING_REGNUM][TUNING_REGSIZE];

static int tuning_read_regset(unsigned long tmp)
{
	struct tuning_buff *rbuf = (struct tuning_buff *)tmp;
	int i;
	int size = ARRAY_SIZE(hitachi_power_on_set);

	printk(KERN_INFO "read_file\n");

	for (i = 0; i < size; i++) {
		if (copy_to_user(rbuf->buf[i], hitachi_power_on_set[i].payload,
					hitachi_power_on_set[i].dlen)) {
			printk(KERN_ERR "read_file : error of copy_to_user_buff\n");
			return -EFAULT;
		}
	/*printk("rbuf->buf: %x %x\n", rbuf->buf[i][0], rbuf->buf[i][1]);*/
	}

	if (put_user(size, &(rbuf->size))) {
		printk(KERN_ERR "read_file : error of copy_to_user_buffsize\n");
		return -EFAULT;
	}

	return 0;
}

static int tuning_write_regset(unsigned long tmp)
{
	char *buff;
	struct tuning_buff *wbuf = (struct tuning_buff *)tmp;
	int i = wbuf->idx;

	printk(KERN_INFO "write file\n");

	buff = kmalloc(TUNING_BUFSIZE, GFP_KERNEL);
	if (copy_from_user(buff, wbuf->buf, wbuf->size)) {
		printk(KERN_ERR "write_file : error of copy_from_user\n");
		return -EFAULT;
	}

	memset(set_buff[i], 0x00, TUNING_REGSIZE);
	memcpy(set_buff[i], buff, wbuf->size);
	hitachi_power_on_set[i+4].payload = set_buff[i];
	hitachi_power_on_set[i+4].dlen = wbuf->size;
	kfree(buff);
	return 0;
}

static struct msm_panel_common_pdata lge_lcd_tunning_pdata = {
	.read_regset = tuning_read_regset,
	.write_regset = tuning_write_regset,
};

static struct platform_device lcd_misc_device = {
	.name   = "lcd_misc_msm",
	.id = 0,
	.dev = {
		.platform_data = &lge_lcd_tunning_pdata,
	}
};
#endif

static struct msm_panel_common_pdata mipi_hitachi_pdata = {
	.backlight_level = mipi_hitachi_backlight_level,
#if (defined(CONFIG_FB_MSM_MIPI_HITACHI_DX11D_VIDEO_HD_PT_PANEL) ||\
	defined(CONFIG_FB_MSM_MIPI_HITACHI_DX12D_VIDEO_HD_PT_PANEL)) ||\
    defined(CONFIG_FB_MSM_MIPI_HITACHI_DX11D_VIDEO_QHD_PT_PANEL)      /*  lx_kr panel patch */
	.power_on_set = hitachi_power_on_set,
	.power_off_set = hitachi_power_off_set,
	.power_on_set_size = ARRAY_SIZE(hitachi_power_on_set),
	.power_off_set_size = ARRAY_SIZE(hitachi_power_off_set),
#endif
#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
	.max_backlight_level = 0x71,
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
	.max_backlight_level = 0xFF,
#endif
};

static struct platform_device mipi_dsi_hitachi_panel_device = {
	.name = "mipi_hitachi",
	.id = 0,
	.dev = {
		.platform_data = &mipi_hitachi_pdata,
	}
};
#elif defined (CONFIG_FB_MSM_MIPI_DSI_LGD)
static int mipi_lgd_backlight_level(int level, int max, int min)
{
#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
	lm3530_lcd_backlight_set_level(level);
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
	lm3533_lcd_backlight_set_level(level);
#endif
	return 0;
}
/* LGD 4.3" qHD panel */
/* 2012-07-11 LCD init changed */
static char Set_EXTC   [ 4] = {0xB9, 0xFF, 0x83, 0x89};
static char Set_MIPI   [ 3] = {0xBA, 0x01, 0x92};
static char Set_POWER  [ 3] = {0xDE, 0x05, 0x58};
static char Set_POWER2 [20] = {0xB1, 0x00, 0x00, 0x04, 0xE8, 0x9A, 0x10, 0x11, 0x92, 0x0F, 0x2B, 0x33, 0x22, 0x22, 0x41, 0x00, 0x3A, 0xF7, 0x20, 0x00};
static char Set_DISPLAY[ 8] = {0xB2, 0x00, 0x00, 0x78, 0x05, 0x03, 0x00, 0xF0};
static char Set_VCOM   [ 3] = {0xB6, 0x00, 0xBC};
static char Set_CLK    [ 3] = {0xCB, 0x07, 0x07};
static char Set_PANEL  [ 2] = {0xCC, 0x0A};
static char Set_CYC    [24] = {0xB4, 0x00, 0x06, 0x00, 0x32, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x0A, 0x40, 0x02, 0x37, 0x0A, 0x40, 0x14, 0x50, 0x55, 0x0A};
static char Set_OTP    [ 5] = {0xBB, 0x00, 0x00, 0xff, 0x80};
static char Set_GPIO   [30] = {0xD5, 0x00, 0x00, 0x4C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x60, 0x00, 0x99, 0x88, 0x88, 0x88, 0x88, 0x01, 0x88, 0x67, 0x88, 0x45, 0x88, 0x23, 0x01, 0x23, 0x88, 0x88, 0x88, 0x88, 0x88};
static char Set_DGC_LUT[128]= {0xC1, 0x01, 0x00, 0x07, 0x1c, 0x2a, 0x34, 0x3f, 0x49, 0x52, 0x5b, 0x63, 0x6a, 0x71, 0x78, 0x7f, 0x85, 0x8b, 0x91, 0x97, 0x9d, 
                                     0xA3, 0xA8, 0xAF, 0xB5, 0xBE, 0xC6, 0xC9, 0xCD, 0xD7, 0xE1, 0xE6, 0xF1, 0xF7, 0xFF, 0x34, 0xCB, 0x4F, 0xD9, 0x54, 0xC8, 
									 0x3C, 0x4A, 0xC0, 0x00, 0x07, 0x1C, 0x2A, 0x34, 0x3F, 0x49, 0x52, 0x5B, 0x63, 0x6A, 0x71, 0x78, 0x7F, 0x85, 0x8B, 0x91, 
									 0x97, 0x9D, 0xA3, 0xA8, 0xAF, 0xB5, 0xBE, 0xC6, 0xC9, 0xCD, 0xD7, 0xE1, 0xE6, 0xF1, 0xF7, 0xFF, 0x34, 0xCB, 0x4F, 0xD9, 
									 0x54, 0xC8, 0x3C, 0x4A, 0xC0, 0x00, 0x07, 0x1C, 0x2A, 0x34, 0x3F, 0x49, 0x52, 0x5B, 0x63, 0x6A, 0x71, 0x78, 0x7F, 0x85, 
									 0x8B, 0x91, 0x97, 0x9D, 0xA3, 0xA8, 0xAF, 0xA5, 0xBE, 0xC6, 0xC9, 0xCD, 0xD7, 0xE1, 0xE6, 0xF1, 0xF7, 0xFF, 0x34, 0xCB, 
									 0x4F, 0xD9, 0x54, 0xC8, 0x3C, 0x4A, 0xC0};
static char Set_GAMMA  [35] = {0xE0, 0x05, 0x11, 0x14, 0x37, 0x3F, 0x3F, 0x20, 0x4F, 0x08, 0x0E, 0x0D, 0x12, 0x14, 0x12, 0x14, 0x1D, 0x1C, 0x05, 0x11, 0x14, 
                                     0x37, 0x3F, 0x3F, 0x20, 0x4F, 0x08, 0x0E, 0x0D, 0x12, 0x14, 0x12, 0x14, 0x1D, 0x1C};
static char exit_sleep [2]  = {0x11, 0x00};
static char display_on [2]  = {0x29, 0x00};
static char enter_sleep[2]  = {0x10, 0x00};
static char display_off[2]  = {0x28, 0x00};

static struct dsi_cmd_desc lgd_power_on_set[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_EXTC), Set_EXTC},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_MIPI), Set_MIPI},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_POWER), Set_POWER},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_POWER2), Set_POWER2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_DISPLAY), Set_DISPLAY},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_VCOM), Set_VCOM},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_CLK), Set_CLK},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_PANEL), Set_PANEL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_CYC), Set_CYC},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_OTP), Set_OTP},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_GPIO), Set_GPIO},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_DGC_LUT), Set_DGC_LUT},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_GAMMA), Set_GAMMA},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc lgd_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(enter_sleep), enter_sleep}
};

static struct msm_panel_common_pdata mipi_lgd_pdata = {
	.backlight_level = mipi_lgd_backlight_level,
#if defined (CONFIG_FB_MSM_MIPI_LGD_VIDEO_QHD_PT)
	.power_on_set = lgd_power_on_set,
	.power_off_set = lgd_power_off_set,
	.power_on_set_size = ARRAY_SIZE(lgd_power_on_set),
	.power_off_set_size = ARRAY_SIZE(lgd_power_off_set),
#endif
#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
	.max_backlight_level = 0x71,
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
	.max_backlight_level = 0xFF,
#endif
};

static struct platform_device mipi_dsi_lgd_panel_device = {
	.name = "mipi_lgd",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgd_pdata,
	}
};

#elif defined (CONFIG_FB_MSM_MIPI_DSI_R61529)
static int mipi_r61529_backlight_level(int level, int max, int min)
{
#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
	lm3530_lcd_backlight_set_level(level);
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
	lm3533_lcd_backlight_set_level(level);
#endif
	return 0;
}

#if (defined(CONFIG_FB_MSM_MIPI_R61529_VIDEO_HVGA_PT_PANEL) || \
	defined(CONFIG_FB_MSM_MIPI_R61529_CMD_HVGA_PT_PANEL))
static char config_mcap[2] = {0xb0, 0x04};

static char config_set_tear_on[2] = {0x35, 0x00};

static char config_set_tear_scanline[3] = {
	0x44, 0x00, 0x00
};

static char config_set_addr[2] = {0x36, 0x08};

static char config_pixel_format[2] = {0x3a, 0x77};

static char config_column_addr[5] = {
	0x2a, 0x00, 0x00, 0x01, 0x3f
};

static char config_page_addr[5] = {
	0x2b, 0x00, 0x00, 0x01, 0xdf
};

static char config_frame_mem_if[5] = {
	0xb3, 0x02, 0x00, 0x00, 0x00
};

static char config_panel_drv[9] = {
	0xc0, 0x01, 0xdf, 0x40,
	0x10, 0x00, 0x01, 0x00, 0x33
};

static char config_display_timing[6] = {
	0xc1, 0x07, 0x27, 0x08,
	0x08, 0x10
};

static char config_src_gate_timing[5] = {
	0xc4, 0x77, 0x00, 0x03,	0x01
};

static char config_dpi_polarity[2] = { 0xc6, 0x00 };

static char config_gamma_a[25] = {
	0xc8, 0x02, 0x0b, 0x16,
	0x20, 0x2d, 0x47, 0x38,
	0x25, 0x1c, 0x14, 0x0b,
	0x00, 0x02, 0x0b, 0x16,
	0x20, 0x2d, 0x47, 0x38,
	0x25, 0x1c, 0x14, 0x0b, 0x00
};

static char config_gamma_b[25] = {
	0xc9, 0x02, 0x0b, 0x16,
	0x20, 0x2d, 0x47, 0x38,
	0x25, 0x1c, 0x14, 0x0b,
	0x00, 0x02, 0x0b, 0x16,
	0x20, 0x2d, 0x47, 0x38,
	0x25, 0x1c, 0x14, 0x0b, 0x00
};

static char config_gamma_c[25] = {
	0xca, 0x02, 0x0b, 0x16,
	0x20, 0x2d, 0x47, 0x38,
	0x25, 0x1c, 0x14, 0x0b,
	0x00, 0x02, 0x0b, 0x16,
	0x20, 0x2d, 0x47, 0x38,
	0x25, 0x1c, 0x14, 0x0b,	0x00
};

static char config_power_chg_pump[17] = {
	0xd0, 0xA9, 0x06, 0x08,
	0x20, 0x31, 0x04, 0x01,
	0x00, 0x08, 0x01, 0x00,
	0x06, 0x01, 0x00, 0x00,	0x20
};

static char config_vcom[5] = {
	0xd1, 0x02, 0x22, 0x22,	0x33
};

static char config_backlight_ctrl1[21] = {
	0xb8, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,	0x00
};

static char config_backlight_ctrl2[5] = {
	0xb9, 0x00, 0x00, 0x00, 0x00
};

/*
static char config_backlight_ctrl3[3] = {
	0xba, 0x00, 0x00
};
*/

static char config_nvm_access[5] = {
	0xe0, 0x00, 0x00, 0x00, 0x00
};

static char config_ddb_write[7] = {
	0xe1, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00
};

static char config_nvm_load[2] = {0xe2, 0x80};
static char config_write_memory[2] = {0x2c, 0x00};

/* sleep_mode_on */
static char sleep_display_off[1] = {0x28};
static char sleep_mode_on[1] = {0x10};
static char sleep_mcap[2] = {0xb0, 0x00};
static char sleep_low_power_mode[2] = {0xb1, 0x01};

static struct dsi_cmd_desc r61529_power_on_cmds[] = {
	{DTYPE_GEN_WRITE2, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_mcap), config_mcap},
	{DTYPE_DCS_WRITE1, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_set_tear_on), config_set_tear_on},
	{DTYPE_DCS_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_set_tear_scanline), config_set_tear_scanline},
	{DTYPE_DCS_WRITE1, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_set_addr), config_set_addr},
	{DTYPE_DCS_WRITE1, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_pixel_format), config_pixel_format},
	{DTYPE_DCS_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_column_addr), config_column_addr},
	{DTYPE_DCS_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_page_addr), config_page_addr},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_frame_mem_if), config_frame_mem_if},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_panel_drv), config_panel_drv},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_display_timing), config_display_timing},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_src_gate_timing), config_src_gate_timing},
	{DTYPE_GEN_WRITE2, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_dpi_polarity), config_dpi_polarity},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_gamma_a), config_gamma_a},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_gamma_b), config_gamma_b},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_gamma_c), config_gamma_c},

	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_power_chg_pump), config_power_chg_pump},

	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_vcom), config_vcom},

	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_backlight_ctrl1), config_backlight_ctrl1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_backlight_ctrl2), config_backlight_ctrl2},
	/*
	{DTYPE_GEN_READ1, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_backlight_ctrl3), config_backlight_ctrl3},
	*/

	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_nvm_access), config_nvm_access},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_ddb_write), config_ddb_write},
	{DTYPE_GEN_WRITE2, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_nvm_load), config_nvm_load},
	{DTYPE_DCS_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(config_write_memory), config_write_memory}
};

static struct dsi_cmd_desc r61529_disp_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 40,
		sizeof(sleep_display_off), sleep_display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 100,
		sizeof(sleep_mode_on), sleep_mode_on},
	{DTYPE_GEN_LWRITE, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(sleep_mcap), sleep_mcap},
	{DTYPE_GEN_WRITE2, 1, 0, 0, R61529_CMD_DELAY,
		sizeof(sleep_low_power_mode), sleep_low_power_mode}
};

static struct msm_panel_common_pdata mipi_r61529_pdata = {
	.backlight_level = mipi_r61529_backlight_level,
#if (defined(CONFIG_FB_MSM_MIPI_R61529_VIDEO_HVGA_PT_PANEL) || \
	defined(CONFIG_FB_MSM_MIPI_R61529_CMD_HVGA_PT_PANEL))
	.power_on_set = r61529_power_on_cmds,
	.power_off_set = r61529_disp_off_cmds,
	.power_on_set_size = ARRAY_SIZE(r61529_power_on_cmds),
	.power_off_set_size = ARRAY_SIZE(r61529_disp_off_cmds),
#endif
#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
	.max_backlight_level = 0x71,
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
	.max_backlight_level = 0xFF,
#endif

};

static struct platform_device mipi_dsi_r61529_panel_device = {
	.name = "mipi_r61529",
	.id = 0,
	.dev = {
		.platform_data = &mipi_r61529_pdata,
	}
};
#endif /*#ifdef CONFIG_FB_MSM_MIPI_R61529_VIDEO_HVGA_PT_PANEL*/
#endif /* #ifdef CONFIG_FB_MSM_MIPI_LGIT */
/*#endif *//* #ifdef CONFIG_LGE_BACKLIGHT_LM3533*/  /*  lx_kr backlight patch - delete #endif  */

#ifdef CONFIG_LGE_KCAL
extern int set_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_kcal_display(void);
extern int get_kcal_values(int *kcal_r, int *kcal_g, int *kcal_b);

static struct kcal_platform_data kcal_pdata = {
	.set_values = set_kcal_values,
	.get_values = get_kcal_values,
	.refresh_display = refresh_kcal_display
};

static struct platform_device kcal_platrom_device = {
	.name   = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static struct platform_device wfd_panel_device = {
	.name = "wfd_panel",
	.id = 0,
	.dev.platform_data = NULL,
};

static struct platform_device wfd_device = {
	.name          = "msm_wfd",
	.id            = -1,
};
#endif

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 566092800 * 2,
		.ib = 707616000 * 2,
	},
};

static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
};
#endif

static struct gpiomux_setting mdp_vsync_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdp_vsync_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8960_mdp_vsync_configs[] __initdata = {
	{
		.gpio = MDP_VSYNC_GPIO,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdp_vsync_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdp_vsync_suspend_cfg,
		},
	}
};

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL

static int hdmi_enable_5v(int on)
{
	/* TBD: PM8921 regulator instead of 8901 */
	static struct regulator *reg_8921_hdmi_mvs;	/* HDMI_5V */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8921_hdmi_mvs) {
		reg_8921_hdmi_mvs = regulator_get(&hdmi_msm_device.dev,
					"hdmi_mvs");
		if (IS_ERR(reg_8921_hdmi_mvs)) {
			pr_err("'%s' regulator not found, rc=%ld\n",
				"hdmi_mvs", IS_ERR(reg_8921_hdmi_mvs));
			reg_8921_hdmi_mvs = NULL;
			return -ENODEV;
		}
	}

	if (on) {
		rc = regulator_enable(reg_8921_hdmi_mvs);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
			return rc;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8921_hdmi_mvs);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
}

static int hdmi_core_power(int on, int show)
{
	static struct regulator *reg_8921_l23, *reg_8921_s4;
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	/* TBD: PM8921 regulator instead of 8901 */
	if (!reg_8921_l23) {
		reg_8921_l23 = regulator_get(&hdmi_msm_device.dev, "hdmi_avdd");
		if (IS_ERR(reg_8921_l23)) {
			pr_err("could not get reg_8921_l23, rc = %ld\n",
				PTR_ERR(reg_8921_l23));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_8921_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage failed for 8921_l23, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	if (!reg_8921_s4) {
		reg_8921_s4 = regulator_get(&hdmi_msm_device.dev, "hdmi_vcc");
		if (IS_ERR(reg_8921_s4)) {
			pr_err("could not get reg_8921_s4, rc = %ld\n",
				PTR_ERR(reg_8921_s4));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_8921_s4, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage failed for 8921_s4, rc=%d\n", rc);
			return -EINVAL;
		}
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_8921_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_8921_l23);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_avdd", rc);
			return rc;
		}
		rc = regulator_enable(reg_8921_s4);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_vcc", rc);
			return rc;
		}
		rc = gpio_request(100, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", 100, rc);
			goto error1;
		}
		rc = gpio_request(101, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", 101, rc);
			goto error2;
		}
		rc = gpio_request(102, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", 102, rc);
			goto error3;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(100);
		gpio_free(101);
		gpio_free(102);

		rc = regulator_disable(reg_8921_l23);
		if (rc) {
			pr_err("disable reg_8921_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_8921_s4);
		if (rc) {
			pr_err("disable reg_8921_s4 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_8921_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error3:
	gpio_free(101);
error2:
	gpio_free(100);
error1:
	regulator_disable(reg_8921_l23);
	regulator_disable(reg_8921_s4);
	return rc;

}

static int hdmi_cec_power(int on)
{
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(99, "HDMI_CEC_VAR");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_CEC_VAR", 99, rc);
			goto error;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(99);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
error:
	return rc;
#else
	return 0;
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT	*/
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_LGE_HIDDEN_RESET
int lge_get_fb_phys_info(unsigned long *start, unsigned long *size)
{
	if (!start || !size)
		return -1;

	*start = (unsigned long)msm_fb_resources[0].start;
	*size = (unsigned long)(LCD_RESOLUTION_X * LCD_RESOLUTION_Y * 4);

	return 0;
}

void *lge_get_hreset_fb_phys_addr(void)
{
       return (void *)0x88B00000;
}
#endif

static void __init msm_fb_add_devices(void)
{

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

	if (machine_is_msm8x60_rumi3()) {
		msm_fb_register_device("mdp", NULL);
		mipi_dsi_pdata.target_type = 1;
	} else
		msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#ifdef CONFIG_MSM_BUS_SCALING
	msm_fb_register_device("dtv", &dtv_pdata);
#endif
}

void __init msm8960_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

	size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));

}

/**
 * Set MDP clocks to high frequency to avoid DSI underflow
 * when using high resolution 1200x1920 WUXGA panels
 */
static void set_mdp_clocks_for_wuxga(void)
{
	int i;

	mdp_ui_vectors[0].ab = 2000000000;
	mdp_ui_vectors[0].ib = 2000000000;
	mdp_vga_vectors[0].ab = 2000000000;
	mdp_vga_vectors[0].ib = 2000000000;
	mdp_720p_vectors[0].ab = 2000000000;
	mdp_720p_vectors[0].ib = 2000000000;
	mdp_1080p_vectors[0].ab = 2000000000;
	mdp_1080p_vectors[0].ib = 2000000000;

	mdp_pdata.mdp_core_clk_rate = 200000000;

	for (i = 0; i < ARRAY_SIZE(mdp_core_clk_rate_table); i++)
		mdp_core_clk_rate_table[i] = 200000000;

	if (hdmi_is_primary) {
		dtv_bus_def_vectors[0].ab = 2000000000;
		dtv_bus_def_vectors[0].ib = 2000000000;
	}
}

void __init msm8960_set_display_params(char *prim_panel, char *ext_panel)
{
	int disable_splash = 0;
	if (strnlen(prim_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.prim_panel_name, prim_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.prim_panel_name %s\n",
			msm_fb_pdata.prim_panel_name);

		if (strncmp((char *)msm_fb_pdata.prim_panel_name,
			MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
			strnlen(MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
			/* Disable splash for panels other than Toshiba WSVGA */
			disable_splash = 1;
		}

		if (!strncmp((char *)msm_fb_pdata.prim_panel_name,
			HDMI_PANEL_NAME, strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
			pr_debug("HDMI is the primary display by"
				" boot parameter\n");
			hdmi_is_primary = 1;
			set_mdp_clocks_for_wuxga();
		}
		if (!strncmp((char *)msm_fb_pdata.prim_panel_name,
				MIPI_VIDEO_TOSHIBA_WUXGA_PANEL_NAME,
				strnlen(MIPI_VIDEO_TOSHIBA_WUXGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN))) {
			set_mdp_clocks_for_wuxga();
		}
	}
	if (strnlen(ext_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.ext_panel_name, ext_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.ext_panel_name %s\n",
			msm_fb_pdata.ext_panel_name);
	}

	if (disable_splash)
		mdp_pdata.cont_splash_enabled = 0;
}

#ifdef CONFIG_I2C

#if defined(CONFIG_LGE_BACKLIGHT_LM3533)
#define LM3533_BACKLIGHT_ADDRESS 0x36

struct backlight_platform_data {
   void (*platform_init)(void);
   int gpio;
   unsigned int mode;
   int max_current;
   int init_on_boot;
   int min_brightness;
   int max_brightness;
   int default_brightness;
   int factory_brightness;
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
#define PWM_SIMPLE_EN 0xA0
#endif

static struct backlight_platform_data lm3533_data = {
	.gpio = PM8921_GPIO_PM_TO_SYS(24),
	.max_current = 0x17,
	.min_brightness = 0x05,
	.max_brightness = 0xFF,
	.default_brightness = 0x9C,
	.factory_brightness = 0x78,
};

static struct i2c_board_info msm_i2c_backlight_info[] = {
	{
		I2C_BOARD_INFO("lm3533", LM3533_BACKLIGHT_ADDRESS),
		.platform_data = &lm3533_data,
	}
};

static struct i2c_registry d1l_i2c_backlight_device __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8960_GSBI2_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
};

#elif defined (CONFIG_LGE_BACKLIGHT_LM3530)    /*  lx_kr backlight lm3530  */
#define LM3530_BACKLIGHT_ADDRESS 0x38

struct backlight_platform_data {
   void (*platform_init)(void);
   int gpio;
   unsigned int mode;
   int max_current;
   int init_on_boot;
   int min_brightness;
   int max_brightness;
   int default_brightness;
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
#define PWM_SIMPLE_EN 0xA0
#endif

static struct backlight_platform_data lm3530_data = {
	.gpio = PM8921_GPIO_PM_TO_SYS(24),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.max_current = 0x15 | PWM_SIMPLE_EN,
#else
	.max_current = 0x15,
#endif
	/* max_current table - exponential mapping
	0x01 = 5 mA full-scale current
	0x05 = 8.5 A full-scale current
	0x09= 12 mA full-scale current
	0x0D = 15.5 mA full-scale current
	0x11 = 19 mA full-scale current
	0x15 = 22.5 mA full-scale current
	0x19 = 26 mA full-scale current
	0x1D= 29.5 mA full-scale current

	If want to use CABC then set 7th and 5th bit.
	*/

	.min_brightness = 0x46,
	.max_brightness = 0x7D,
    .default_brightness = 0x51,
};

static struct i2c_board_info msm_i2c_backlight_info[] = {
	{
		I2C_BOARD_INFO("lm3530", LM3530_BACKLIGHT_ADDRESS),
		.platform_data = &lm3530_data,
	}
};

static struct i2c_registry lx_i2c_backlight_device __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8960_GSBI2_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
};
#endif /* CONFIG_LGE_BACKLIGHT_LM3530 */       /*  lx_kr backlight lm3530  */

#endif /* CONFIG_I2C */

static int __init panel_gpiomux_init(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc == -EPERM) {
		pr_info("%s : msm_gpiomux_init is already initialized\n",
				__func__);
	} else if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}

	msm_gpiomux_install(msm8960_mdp_vsync_configs,
			ARRAY_SIZE(msm8960_mdp_vsync_configs));

	return 0;
}

static struct platform_device *lx_panel_devices[] __initdata = {
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	&hdmi_msm_device,
#endif
#if defined(CONFIG_FB_MSM_MIPI_DSI_LGIT)
	&mipi_dsi_lgit_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_DSI_HITACHI)
	&mipi_dsi_hitachi_panel_device,
#ifdef CONFIG_LGE_LCD_TUNING
	&lcd_misc_device,
#endif
#elif defined(CONFIG_FB_MSM_MIPI_DSI_R61529)
	&mipi_dsi_r61529_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_DSI_LGD)
	&mipi_dsi_lgd_panel_device,
#endif
#ifdef CONFIG_LGE_KCAL
	&kcal_platrom_device,
#endif
};

void __init lge_add_lcd_devices(void)
{
	panel_gpiomux_init();

	fb_register_client(&msm_fb_event_notifier);

	platform_add_devices(lx_panel_devices, ARRAY_SIZE(lx_panel_devices));
#if defined(CONFIG_LGE_BACKLIGHT_LM3533)
	printk(KERN_INFO "%s lm3533 init\n",__func__);
	lge_add_msm_i2c_device(&lx_i2c_backlight_device);
#elif defined(CONFIG_LGE_BACKLIGHT_LM3530)    /*  lx_kr backlight lm3530  */
	printk(KERN_INFO "%s lm3530 init\n",__func__);
	lge_add_msm_i2c_device(&lx_i2c_backlight_device);
#endif
	msm_fb_add_devices();
	platform_device_register(&msm_fb_device);
}

