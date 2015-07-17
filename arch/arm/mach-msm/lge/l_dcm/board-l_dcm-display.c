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
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-l_dcm.h"
#include CONFIG_BOARD_HEADER_FILE

#include <linux/fb.h>
#include "../../../../drivers/video/msm/msm_fb.h"
#include "../../../../drivers/video/msm/msm_fb_def.h"
#include "../../../../drivers/video/msm/mipi_dsi.h"

#include <mach/board_lge.h>

#ifdef CONFIG_LGE_KCAL
#ifdef CONFIG_LGE_QC_LCDC_LUT
extern int set_qlut_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_qlut_display(void);
#else
#error only kcal by Qucalcomm LUT is supported now!!!
#endif
#endif

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((roundup(LCD_RESOLUTION_X, 32) * roundup(LCD_RESOLUTION_Y, 32) * 4), 4096) * 3)
			/* 4 bpp x 3 pages */
#else
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((roundup(LCD_RESOLUTION_X, 32) * roundup(LCD_RESOLUTION_Y, 32) * 4), 4096) * 2)
			/* 4 bpp x 2 pages */
#endif

/* Note: must be multiple of 4096 */
#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE, 4096)

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE \
		roundup((roundup(LCD_RESOLUTION_X, 32) * roundup(LCD_RESOLUTION_Y, 32) * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE \
		roundup((roundup(1920, 32) * roundup(1080, 32) * 3 * 2), 4096)
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
static unsigned char hdmi_is_primary = 1;
#else
static unsigned char hdmi_is_primary;
#endif /* CONFIG_FB_MSM_HDMI_AS_PRIMARY */

#define TUNING_BUFSIZE 4096
#define TUNING_REGSIZE 40
#define TUNING_REGNUM 10

#define	LCD_GAMMA	0

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
#endif /* CONFIG_FB_MSM_MIPI_PANEL_DETECT */
#endif /* CONFIG_MACH_LGE */

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
#endif /* CONFIG_MACH_LGE */
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

#ifndef CONFIG_MACH_LGE
static void mipi_dsi_panel_pwm_cfg(void)
{
	int rc;
	static int mipi_dsi_panel_gpio_configured;
	static struct pm_gpio pwm_enable = {
		.direction        = PM_GPIO_DIR_OUT,
		.output_buffer    = PM_GPIO_OUT_BUF_CMOS,
		.output_value     = 1,
		.pull             = PM_GPIO_PULL_NO,
		.vin_sel          = PM_GPIO_VIN_VPH,
		.out_strength     = PM_GPIO_STRENGTH_HIGH,
		.function         = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol      = 0,
		.disable_pin      = 0,
	};
	static struct pm_gpio pwm_mode = {
		.direction        = PM_GPIO_DIR_OUT,
		.output_buffer    = PM_GPIO_OUT_BUF_CMOS,
		.output_value     = 0,
		.pull             = PM_GPIO_PULL_NO,
		.vin_sel          = PM_GPIO_VIN_S4,
		.out_strength     = PM_GPIO_STRENGTH_HIGH,
		.function         = PM_GPIO_FUNC_2,
		.inv_int_pol      = 0,
		.disable_pin      = 0,
	};

	if (mipi_dsi_panel_gpio_configured == 0) {
		/* pm8xxx: gpio-21, Backlight Enable */
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(21),
					&pwm_enable);
		if (rc != 0)
			pr_err("%s: pwm_enabled failed\n", __func__);

		/* pm8xxx: gpio-24, Bl: Off, PWM mode */
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(24),
					&pwm_mode);
		if (rc != 0)
			pr_err("%s: pwm_mode failed\n", __func__);

		mipi_dsi_panel_gpio_configured++;
	}
}
#endif /* CONFIG_MACH_LGE */

static bool dsi_power_on;

#ifndef CONFIG_MACH_LGE
/**
 * LiQUID panel on/off
 *
 * @param on
 *
 * @return int
 */
static int mipi_dsi_liquid_panel_power(int on)
{
	static struct regulator *reg_l2, *reg_ext_3p3v;
	static int gpio21, gpio24, gpio43;
	int rc;

	mipi_dsi_panel_pwm_cfg();
	pr_debug("%s: on=%d\n", __func__, on);

	gpio21 = PM8921_GPIO_PM_TO_SYS(21); /* disp power enable_n */
	gpio43 = PM8921_GPIO_PM_TO_SYS(43); /* Displays Enable (rst_n)*/
	gpio24 = PM8921_GPIO_PM_TO_SYS(24); /* Backlight PWM */

	if (!dsi_power_on) {

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		reg_ext_3p3v = regulator_get(&msm_mipi_dsi1_device.dev,
			"vdd_lvds_3p3v");
		if (IS_ERR(reg_ext_3p3v)) {
			pr_err("could not get reg_ext_3p3v, rc = %ld\n",
			       PTR_ERR(reg_ext_3p3v));
		    return -ENODEV;
		}

		rc = gpio_request(gpio21, "disp_pwr_en_n");
		if (rc) {
			pr_err("request gpio 21 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = gpio_request(gpio43, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 43 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = gpio_request(gpio24, "disp_backlight_pwm");
		if (rc) {
			pr_err("request gpio 24 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		dsi_power_on = true;
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_enable(reg_ext_3p3v);
		if (rc) {
			pr_err("enable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}

		/* set reset pin before power enable */
		gpio_set_value_cansleep(gpio43, 0); /* disp disable (resx=0) */

		gpio_set_value_cansleep(gpio21, 0); /* disp power enable_n */
		msleep(20);
		gpio_set_value_cansleep(gpio43, 1); /* disp enable */
		msleep(20);
		gpio_set_value_cansleep(gpio43, 0); /* disp enable */
		msleep(20);
		gpio_set_value_cansleep(gpio43, 1); /* disp enable */
		msleep(20);
	} else {
		gpio_set_value_cansleep(gpio43, 0);
		gpio_set_value_cansleep(gpio21, 1);

		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_ext_3p3v);
		if (rc) {
			pr_err("disable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}

	return 0;
}

static int mipi_dsi_cdp_panel_power(int on)
{
	static struct regulator *reg_l8, *reg_l23, *reg_l2;
	static int gpio43;
	int rc;

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdc");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8921_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}
		reg_l23 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vddio");
		if (IS_ERR(reg_l23)) {
			pr_err("could not get 8921_l23, rc = %ld\n",
				PTR_ERR(reg_l23));
			return -ENODEV;
		}
		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l8, 2800000, 3000000);
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_voltage(reg_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage l23 failed, rc=%d\n", rc);
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
		rc = regulator_set_optimum_mode(reg_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
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
		rc = regulator_enable(reg_l23);
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		gpio_set_value_cansleep(gpio43, 1);
	} else {
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l8);
		if (rc) {
			pr_err("disable reg_l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l23);
		if (rc) {
			pr_err("disable reg_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		gpio_set_value_cansleep(gpio43, 0);
	}
	return 0;
}
#endif /* CONFIG_MACH_LGE */

static char mipi_dsi_splash_is_enabled(void);

#ifndef CONFIG_MACH_LGE
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
#else
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_l8, *reg_l2, *reg_lvs6;
	static int gpio43 = PM8921_GPIO_PM_TO_SYS(43);
	int rc;

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

		rc = regulator_set_voltage(reg_l8, 2800000, 2800000);
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		/* VREG_2P8_LCD_VCI enable - kyunghoo.ryu@lge.com */
		rc = gpio_request(LCD_VCI_EN_GPIO, "LCD_VCI_EN_GPIO");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"LCD_VCI_EN_GPIO", LCD_VCI_EN_GPIO, rc);		
		}
		
		gpio_tlmm_config(GPIO_CFG(LCD_VCI_EN_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),	GPIO_CFG_ENABLE);

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

		rc = gpio_direction_output(LCD_VCI_EN_GPIO, 1);
		mdelay(1);

		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
	} else {
		
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

		/* LCD Reset LOW */
		gpio_direction_output(gpio43, 0);

		/* LCD VCI EN LOW */
		rc = gpio_direction_output(LCD_VCI_EN_GPIO, 0);
		
		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
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
#endif /* CONFIG_MACH_LGE */

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
	.mdp_max_clk = 200000000,
#ifdef CONFIG_MSM_BUS_SCALING
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
#endif
	.mdp_rev = MDP_REV_42,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
#ifdef CONFIG_MACH_LGE
	.cont_splash_enabled = 0x00,
#else
	.cont_splash_enabled = 0x01,
#endif
	.mdp_iommu_split_domain = 0,
};

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

#ifdef CONFIG_BACKLIGHT_LM3533
extern void lm3533_lcd_backlight_set_level(int level);
#endif

#ifdef CONFIG_FB_MSM_MIPI_DSI_LGIT
static int mipi_lgit_backlight_level(int level, int max, int min)
{
#ifdef CONFIG_BACKLIGHT_LM3533
	lm3533_lcd_backlight_set_level(level);
#endif /* CONFIG_BACKLIGHT_LM3533 */
	return 0;
}
#endif /* CONFIG_FB_MSM_MIPI_DSI_LGIT */

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WVGA_INVERSE_PT_PANEL)
static char video_switch[] = {0x01, 0x47};
#endif

/* LG-4572B only for Rev.A and Rev.B */
static char hrx_to_old					[ 2] = {0x03, 0x00};
static char inversion_off_old			[ 2] = {0x20, 0x00};
static char tear_on_old					[ 2] = {0x35, 0x00};
static char set_address_mode_old		[ 2] = {0x36, 0x02};			/* Flip Horizontal Only (cause Tearing problem) - Kyunghoo.ryu@lge.com */
static char if_pixel_format_old			[ 2] = {0x3A, 0x77};
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WVGA_INVERSE_PT_PANEL)
static char rgb_interface_setting_old	[  ] = {0xB1, 0x06, 0x43, 0x0A};
#endif
static char page_address_set_old		[ 5] = {0x2B, 0x00, 0x00, 0x03, 0x1F};
static char panel_char_setting_old		[ 3] = {0xB2, 0x00, 0xC8};
static char panel_drive_setting_old		[ 2] = {0xB3, 0x00};
static char display_mode_ctrl_old		[ 2] = {0xB4, 0x04};
static char display_ctrl1_old			[ 6] = {0xB5, 0x42, 0x10, 0x10, 0x00, 0x20};
static char display_ctrl2_old			[ 7] = {0xB6, 0x0B, 0x0F, 0x02, 0x40, 0x10, 0xE8};
#if defined(CONFIG_FB_MSM_MIPI_LGIT_CMD_WVGA_INVERSE_PT_PANEL)
static char display_ctrl3_old			[ 6] = {0xB7, 0x48, 0x06, 0x2E, 0x00, 0x00};
#endif

static char osc_setting_old				[ 3] = {0xC0, 0x01, 0x15};

static char power_ctrl3_old				[ 6] = {0xC3, 0x07, 0x03, 0x04, 0x04, 0x04};
static char power_ctrl4_old				[ 7] = {0xC4, 0x12, 0x24, 0x18, 0x18, 0x05, 0x49};
static char power_ctrl5_old				[ 2] = {0xC5, 0x69};
static char power_ctrl6_old				[ 3] = {0xC6, 0x41, 0x63};

static char exit_sleep_old				[ 2] = {0x11, 0x00};
static char display_on_old				[ 2] = {0x29, 0x00};
static char enter_sleep_old				[ 2] = {0x10, 0x00};
static char display_off_old				[ 2] = {0x28, 0x00};
static char deep_standby_old			[ 2] = {0xC1, 0x01};

/* LGE_CHANGE_S LG-4573B H/W Rev.C or upper revision, jamin.koo@lge.com, 2011.02.27 */
static char hrx_to						[ 2] = {0x03, 0x00};
static char inversion_off				[ 1] = {0x20};
static char set_address_mode			[ 2] = {0x36, 0x02};         /* Flip Horizontal Only (cause Tearing problem) - Kyunghoo.ryu@lge.com */
static char if_pixel_format				[ 2] = {0x3A, 0x70};

static char rgb_interface_setting		[ 4] = {0xB1, 0x06, 0x43, 0x0A};
static char panel_char_setting			[ 3] = {0xB2, 0x00, 0xC8};
static char panel_drive_setting			[ 2] = {0xB3, 0x00};
static char display_mode_ctrl			[ 2] = {0xB4, 0x04};
static char display_ctrl1				[ 6] = {0xB5, 0x40, 0x10, 0x10, 0x00, 0x00};
static char display_ctrl2				[ 7] = {0xB6, 0x0B, 0x0F, 0x02, 0x40, 0x10, 0xE8};

static char osc_setting					[ 3] = {0xC0, 0x01, 0x18};

static char power_ctrl3					[ 6] = {0xC3, 0x07, 0x0A, 0x0A, 0x0A, 0x02};
static char power_ctrl4					[ 7] = {0xC4, 0x12, 0x24, 0x18, 0x18, 0x05, 0x49};
static char power_ctrl5					[ 2] = {0xC5, 0x69};
static char power_ctrl6					[ 4] = {0xC6, 0x41, 0x63, 0x03};

static char p_gamma_r_setting			[10] = {0xD0, 0x00, 0x01, 0x64, 0x25, 0x07, 0x02, 0x61, 0x13, 0x03};
static char n_gamma_r_setting			[10] = {0xD1, 0x00, 0x01, 0x64, 0x25, 0x07, 0x02, 0x61, 0x13, 0x03};
static char p_gamma_g_setting			[10] = {0xD2, 0x00, 0x01, 0x64, 0x25, 0x07, 0x02, 0x61, 0x13, 0x03};
static char n_gamma_g_setting			[10] = {0xD3, 0x00, 0x01, 0x64, 0x25, 0x07, 0x02, 0x61, 0x13, 0x03};
static char p_gamma_b_setting			[10] = {0xD4, 0x00, 0x01, 0x64, 0x25, 0x07, 0x02, 0x61, 0x13, 0x03};
static char n_gamma_b_setting			[10] = {0xD5, 0x00, 0x01, 0x64, 0x25, 0x07, 0x02, 0x61, 0x13, 0x03};

static char exit_sleep					[ 1] = {0x11};
static char display_on					[ 1] = {0x29};
static char enter_sleep					[ 1] = {0x10};
static char display_off					[ 1] = {0x28};
/* LGE_CHANGE_E LG-4573B H/W Rev.C or upper revision, jamin.koo@lge.com, 2011.02.27 */


/* LGE_CHANGE_S, Add CABC Code, jamin.koo@lge.com, 2012.03.30 */
#ifdef CONFIG_LGE_BACKLIGHT_CABC
static char cabc_51				[ 2] = {0x51,0xE6};	 /* LCD CABC CODE, Write Display Brightness */
static char cabc_53				[ 2] = {0x53,0x24};	 /* LCD CABC CODE, Write Control Display */
static char cabc_55				[ 2] = {0x55,0x01};	 /* LCD CABC CODE, Write Content Adaptive Brightness Control */
static char cabc_5e				[ 2] = {0x5E,0x33};	 /* LCD CABC CODE, Write CABC Minimum Brightness */

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
static int lgit_cabc_index = CABC_DEFAULT;

/* Write Display Brightness */
static char config_cabc_51[6][2] = {
	{0x51, 0x00},	/* off */
	{0x51, 0xE6},	/* 10%, 230 */
	{0x51, 0xCC},	/* 20%, 204 */
	{0x51, 0xB3},	/* 30%, 179 */
	{0x51, 0x99},	/* 40%, 153 */
	{0x51, 0x80}	/* 50%, 128 */
};

/* Write Control Display */
static char config_cabc_53[2][2] = {
	{0x53, 0x00},	/* off */
	{0x53, 0x24}	/* on */
};

/* Write Content Adaptive Brightness Control */
static char config_cabc_55[2][2] = {
	{0x55, 0x00},	/* off */
	{0x55, 0x01}	/* on */
};

/* Write CABC Minimum Brightness */
static char config_cabc_5e[6][2] = {
	{0x5E, 0x00},	/* off */
	{0x5E, 0x33},	/* 10% */
	{0x5E, 0x33},	/* 20% */
	{0x5E, 0x33},	/* 30% */
	{0x5E, 0x33},	/* 40% */
	{0x5E, 0x33}	/* 50% */
};
#endif /* CONFIG_LGE_BACKLIGHT_CABC DEBUG */
#endif /* CONFIG_LGE_BACKLIGHT_CABC */

/* LG-4572B only for Rev.A and Rev.B */
/* initialize device */
static struct dsi_cmd_desc lgit_power_on_set_old[] = {
#if	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WVGA_INVERSE_PT_PANEL)
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(video_switch), video_switch},
#endif
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(hrx_to_old), hrx_to_old},
	{DTYPE_DCS_WRITE,  1, 0, 0, 0, sizeof(inversion_off_old), inversion_off_old},
	{DTYPE_DCS_WRITE,  1, 0, 0, 0, sizeof(tear_on_old), tear_on_old},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_address_mode_old), set_address_mode_old},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(if_pixel_format_old),	if_pixel_format_old},
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WVGA_INVERSE_PT_PANEL)
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(rgb_interface_setting_old), rgb_interface_setting_old},
#endif
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(page_address_set_old), page_address_set_old},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(panel_char_setting_old), panel_char_setting_old},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(panel_drive_setting_old), panel_drive_setting_old},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(display_mode_ctrl_old), display_mode_ctrl_old},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl1_old), display_ctrl1_old},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl2_old), display_ctrl2_old},
#if defined(CONFIG_FB_MSM_MIPI_LGIT_CMD_WVGA_INVERSE_PT_PANEL)
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl3_old), display_ctrl3_old},
#endif
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(osc_setting_old), osc_setting_old},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl3_old), power_ctrl3_old},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl4_old), power_ctrl4_old},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(power_ctrl5_old), power_ctrl5_old},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl6_old), power_ctrl6_old},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_r_setting), p_gamma_r_setting},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_r_setting), n_gamma_r_setting},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_g_setting), p_gamma_g_setting},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_g_setting), n_gamma_g_setting},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_b_setting), p_gamma_b_setting},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_b_setting), n_gamma_b_setting},

	{DTYPE_DCS_WRITE,  1, 0, 0, 100, sizeof(exit_sleep_old), exit_sleep_old},
	{DTYPE_DCS_WRITE,  1, 0, 0, 100, sizeof(display_on_old), display_on_old},
};

static struct dsi_cmd_desc lgit_power_off_set_old[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off_old), display_off_old},
	{DTYPE_DCS_WRITE, 1, 0, 0, 60, sizeof(enter_sleep_old), enter_sleep_old},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,   sizeof(deep_standby_old), deep_standby_old},
};

/* LGE_CHANGE_S LG-4573B H/W Rev.C or upper revision, jamin.koo@lge.com, 2011.02.27 */
/* initialize device */
static struct dsi_cmd_desc lgit_power_on_set[] = {

   {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(hrx_to), hrx_to},
   {DTYPE_DCS_WRITE,  1, 0, 0, 0, sizeof(inversion_off), inversion_off},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_address_mode), set_address_mode},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(if_pixel_format),   if_pixel_format},

	/* LGE_CHANGE_S, Add CABC Code, jamin.koo@lge.com, 2012.03.30 */
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(cabc_51), cabc_51},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(cabc_53), cabc_53},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(cabc_55), cabc_55},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(cabc_5e), cabc_5e},
#endif /* CONFIG_LGE_BACKLIGHT_CABC */
	/* LGE_CHANGE_E, Add CABC Code, jamin.koo@lge.com, 2012.03.30 */

   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(rgb_interface_setting), rgb_interface_setting},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(panel_char_setting), panel_char_setting},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(panel_drive_setting), panel_drive_setting},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(display_mode_ctrl), display_mode_ctrl},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl1), display_ctrl1},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(display_ctrl2), display_ctrl2},

   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(osc_setting), osc_setting},

   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl3), power_ctrl3},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl4), power_ctrl4},
   {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(power_ctrl5), power_ctrl5},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_ctrl6), power_ctrl6},

   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_r_setting), p_gamma_r_setting},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_r_setting), n_gamma_r_setting},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_g_setting), p_gamma_g_setting},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_g_setting), n_gamma_g_setting},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_b_setting), p_gamma_b_setting},
   {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_b_setting), n_gamma_b_setting},

   {DTYPE_DCS_WRITE,  1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
   {DTYPE_DCS_WRITE,  1, 0, 0, 40, sizeof(display_on), display_on},

};

/* LGE_CHANGE_E LG-4573B H/W Rev.C or upper revision, jamin.koo@lge.com, 2011.02.27 */
static struct dsi_cmd_desc lgit_power_off_set[] = {
   {DTYPE_DCS_WRITE, 1, 0, 0, 40, sizeof(display_off), display_off},
   {DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(enter_sleep), enter_sleep},
};

#if defined (CONFIG_LGE_BACKLIGHT_CABC) && \
		defined (CONFIG_LGE_BACKLIGHT_CABC_DEBUG)
void set_lgit_cabc(int cabc_index)
{
	pr_info("%s! cabc_index: %d\n", __func__, cabc_index);
	switch(cabc_index) {
	case 0: /* CABC OFF */
		lgit_power_on_set[CABC_POWERON_OFFSET+2].payload =	config_cabc_55[CABC_OFF];
		break;
	case 1: /* 10% */
	case 2: /* 20% */
	case 3: /* 30% */
	case 4: /* 40% */
	case 5: /* 50% */
		{ /* CABC ON */
			lgit_power_on_set[CABC_POWERON_OFFSET].payload = config_cabc_51[cabc_index];
			lgit_power_on_set[CABC_POWERON_OFFSET+1].payload = config_cabc_53[CABC_ON];
			lgit_power_on_set[CABC_POWERON_OFFSET+2].payload = config_cabc_55[CABC_ON];
			lgit_power_on_set[CABC_POWERON_OFFSET+3].payload = config_cabc_5e[cabc_index];
		}
		break;
	default:
		printk("out of range cabc_index %d", cabc_index);
		return;
	}
	lgit_cabc_index = cabc_index;
	return;
}
EXPORT_SYMBOL(set_lgit_cabc);

int get_lgit_cabc(void)
{
	return lgit_cabc_index;
}
EXPORT_SYMBOL(get_lgit_cabc);
#endif /* CONFIG_LGE_BACKLIGHT_CABC && CONFIG_LGE_BACKLIGHT_CABC_DEBUG */

/* LG-4572B only for Rev.A and Rev.B */
static struct msm_panel_common_pdata mipi_lgit_pdata_old = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set = lgit_power_on_set_old,
	.power_off_set = lgit_power_off_set_old,
	.power_on_set_size = ARRAY_SIZE(lgit_power_on_set_old),
	.power_off_set_size = ARRAY_SIZE(lgit_power_off_set_old),
	.max_backlight_level = 0xFF,

#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
	.max_backlight_level = 0x71,
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
	.max_backlight_level = 0xFF,
#endif
};

/* LGE_CHANGE_S LG-4573B H/W Rev.C or upper revision, jamin.koo@lge.com, 2011.02.27 */
static struct msm_panel_common_pdata mipi_lgit_pdata = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set = lgit_power_on_set,
	.power_off_set = lgit_power_off_set,
	.power_on_set_size = ARRAY_SIZE(lgit_power_on_set),
	.power_off_set_size = ARRAY_SIZE(lgit_power_off_set),
	.max_backlight_level = 0xFF,
/* LGE_CHANGE_E LG-4573B H/W Rev.C or upper revision, jamin.koo@lge.com, 2011.02.27 */

#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
	.max_backlight_level = 0x71,
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
	.max_backlight_level = 0xFF,
#endif
};

static struct platform_device mipi_dsi_lgit_panel_device = {
	.name = "mipi_lgit",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgit_pdata,
	}
};

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
static int hdmi_gpio_config(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
	.gpio_config = hdmi_gpio_config,
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
#endif /* CONFIG_FB_MSM_WRITEBACK_MSM_PANEL */

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
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL		
	.lcdc_power_save = hdmi_panel_power,
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
};

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static int hdmi_panel_power(int on)
{
	int rc;

	pr_debug("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_debug("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
#endif /* CONFIG_MSM_BUS_SCALING */

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
		pr_debug("%s(on): success\n", __func__);
	} else {
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
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(100, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", 100, rc);
			return rc;
		}
		rc = gpio_request(101, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", 101, rc);
			goto error1;
		}
		rc = gpio_request(102, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", 102, rc);
			goto error2;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(100);
		gpio_free(101);
		gpio_free(102);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
	return 0;

error2:
	gpio_free(101);
error1:
	gpio_free(100);
	return rc;
}

static int hdmi_cec_power(int on)
{
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
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_LGE_HIDDEN_RESET
static unsigned long hreset_fb_va = 0;

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
       return (void *)0x88A00000;
}

void *lge_get_hreset_fb_va(void)
{
       return (void *)hreset_fb_va;
}

void lge_set_hreset_fb_va(unsigned long va)
{
	hreset_fb_va = va;
}
#endif

void __init msm8960_init_fb(void)
{
	platform_device_register(&msm_fb_device);

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

#ifndef CONFIG_MACH_LGE
	if (machine_is_msm8960_sim())
		platform_device_register(&mipi_dsi_simulator_panel_device);

	if (machine_is_msm8960_rumi3())
		platform_device_register(&mipi_dsi_renesas_panel_device);

	if (!machine_is_msm8960_sim() && !machine_is_msm8960_rumi3()) {
		platform_device_register(&mipi_dsi_novatek_panel_device);
		platform_device_register(&mipi_dsi_orise_panel_device);

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
		platform_device_register(&hdmi_msm_device);
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
	}

	if (machine_is_msm8960_liquid())
		platform_device_register(&mipi_dsi2lvds_bridge_device);
	else
		platform_device_register(&mipi_dsi_toshiba_panel_device);
#endif /* CONFIG_MACH_LGE */

	if (machine_is_msm8x60_rumi3()) {
		msm_fb_register_device("mdp", NULL);
		mipi_dsi_pdata.target_type = 1;
	} else
		msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#ifdef CONFIG_MSM_BUS_SCALING
	msm_fb_register_device("dtv", &dtv_pdata);
#endif /* CONFIG_MSM_BUS_SCALING */
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

#ifndef CONFIG_MACH_LGE
#ifndef CONFIG_FB_MSM_MIPI_PANEL_DETECT
/**
 * Set MDP clocks to high frequency to avoid DSI underflow
 * when using high resolution 1200x1920 WUXGA panels
 */
static void set_mdp_clocks_for_wuxga(void)
{
	mdp_ui_vectors[0].ab = 2000000000;
	mdp_ui_vectors[0].ib = 2000000000;
	mdp_vga_vectors[0].ab = 2000000000;
	mdp_vga_vectors[0].ib = 2000000000;
	mdp_720p_vectors[0].ab = 2000000000;
	mdp_720p_vectors[0].ib = 2000000000;
	mdp_1080p_vectors[0].ab = 2000000000;
	mdp_1080p_vectors[0].ib = 2000000000;

	if (hdmi_is_primary) {
		dtv_bus_def_vectors[0].ab = 2000000000;
		dtv_bus_def_vectors[0].ib = 2000000000;
	}
}
#endif /* CONFIG_FB_MSM_MIPI_PANEL_DETECT */
#endif /* QCT Original */

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
#ifndef CONFIG_MACH_LGE
#ifndef CONFIG_FB_MSM_MIPI_PANEL_DETECT
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
#endif
#endif /* QCT Original */
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
	.max_current = 0x13,
	.min_brightness = 0x05,
	.max_brightness = 0xFF,
	.default_brightness = 0x96,
	.factory_brightness = 0x64,
};

static struct i2c_board_info msm_i2c_backlight_info[] = {
	{
		I2C_BOARD_INFO("lm3533", LM3533_BACKLIGHT_ADDRESS),
		.platform_data = &lm3533_data,
	}
};

static struct i2c_registry l_dcm_i2c_backlight_device __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8960_GSBI2_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
};
#endif /* CONFIG_I2C */

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

static struct platform_device *l_dcm_panel_devices[] __initdata = {
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	&hdmi_msm_device,
#endif
#if defined(CONFIG_FB_MSM_MIPI_DSI_LGIT)
	&mipi_dsi_lgit_panel_device,
#ifdef CONFIG_LGE_LCD_TUNING
	&lcd_misc_device,
#endif /* CONFIG_LGE_LCD_TUNING */
#endif /* CONFIG_FB_MSM_MIPI_DSI_LGIT */
#ifdef CONFIG_LGE_KCAL
	&kcal_platrom_device,
#endif /* CONFIG_LGE_KCAL */
};

void __init lge_add_lcd_devices(void)
{
	panel_gpiomux_init();

	fb_register_client(&msm_fb_event_notifier);

	/* LGE_CHANGE_S, Assign command set to panel info as H/W revision, jamin.koo@lge.com, 2011.02.27 */
	if(lge_get_board_revno() < HW_REV_C)
		mipi_dsi_lgit_panel_device.dev.platform_data = &mipi_lgit_pdata_old;
	/* LGE_CHANGE_E, Assign command set to panel info as H/W revision, jamin.koo@lge.com, 2011.02.27 */
	platform_add_devices(l_dcm_panel_devices,
		ARRAY_SIZE(l_dcm_panel_devices));

	lge_add_msm_i2c_device(&l_dcm_i2c_backlight_device);
	msm8960_init_fb();
}
