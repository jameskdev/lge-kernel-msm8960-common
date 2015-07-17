/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_lgd.h"

#include <mach/gpio.h>
#include <mach/board_lge.h>

#if defined(CONFIG_MACH_MSM8960_FX1) || defined(CONFIG_MACH_MSM8960_L1v)
#include <linux/module.h>
#endif
#if !defined(CONFIG_MACH_MSM8960_L1v) /*LCD reset moved to mipi_LGD_on*/
#include <linux/mfd/pm8xxx/pm8921.h>
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#endif

#ifdef CONFIG_MACH_MSM8960_L1v 
#define LGIT_IEF_SWITCH

#ifdef LGIT_IEF_SWITCH
struct msm_fb_data_type *local_mfd0 = NULL;
static int is_ief_on = 1;
#endif
#endif

static struct msm_panel_common_pdata *mipi_lgd_pdata;

static struct dsi_buf lgd_tx_buf;
static struct dsi_buf lgd_rx_buf;
/*
static char Set_EXTC   [ 4] = {0xB9, 0xFF, 0x83, 0x89};
static char Set_MIPI   [ 3] = {0xba, 0x01, 0x92};
static char Set_POWER  [ 3] = {0xde, 0x05, 0x58};
static char Set_POWER2 [20] = {0xb1, 0x00, 0x00, 0x04, 0xe8, 0x50, 0x10, 0x11, 0xb2, 0x0f, 0x2b, 0x33, 0x1a, 0x1a, 0x41, 0x00, 0x3a, 0xf7, 0x20, 0x80};
static char Set_DISPLAY[ 6] = {0xb2, 0x00, 0x00, 0x78, 0x05, 0x03};
static char Set_CYC    [24] = {0xb4, 0x80, 0x06, 0x00, 0x32, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x00, 0xc0, 0x02, 0x37, 0x00, 0xc0, 0x28, 0xc2, 0x96, 0x14};
static char Set_GPIO   [30] = {0xd5, 0x00, 0x00, 0x4c, 0x00, 0x01, 0x00, 0x00, 0x00, 0x60, 0x00, 0x99, 0x88, 0x88, 0x88, 0x88, 0x01, 0x88, 0x67, 0x88, 0x45, 0x88, 0x23, 0x01, 0x23, 0x88, 0x88, 0x88, 0x88, 0x88};
static char Set_PANEL  [ 2] = {0xcc, 0x0a};
static char exit_sleep [2] = {0x11, 0x00};
static char display_on [2] = {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};
static struct dsi_cmd_desc lgd_power_on_set2[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_EXTC), Set_EXTC},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_MIPI), Set_MIPI},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_POWER), Set_POWER},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_POWER2), Set_POWER2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_DISPLAY), Set_DISPLAY},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_CYC), Set_CYC},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_GPIO), Set_GPIO},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(Set_PANEL), Set_PANEL},


	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
 

	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_on), display_on},
};
static struct dsi_cmd_desc lgd_power_off_set2[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 100, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 100, sizeof(enter_sleep), enter_sleep},
};
*/

/* [LGE_CHANGE] Remove unnecessary codes.
 * Remove mutex_lock, mutex_unlock and mipi_dsi_op_mode_config
 * in mipi_lgd_lcd_on and mipi_lgd_lcd_off.
 * minjong.gong@lge.com, 2011-07-19
 */

#ifdef LGIT_IEF_SWITCH
extern int mipi_lgd_lcd_ief_off(void)
{

#ifdef LGIT_IEF_SWITCH
	if(local_mfd0->panel_power_on && is_ief_on) {
		printk("IEF_OFF Starts with Camera\n");
		mutex_lock(&local_mfd0->dma->ov_mutex);		
		
		mipi_dsi_cmds_tx(&lgd_tx_buf, 
											mipi_lgd_pdata->power_off_set_ief,
											mipi_lgd_pdata->power_off_set_ief_size);	
        
		printk("%s, %d\n", __func__,is_ief_on);
		
		mutex_unlock(&local_mfd0->dma->ov_mutex);
		printk("IEF_OFF Ends with Camera\n");

	}
	is_ief_on = 0;
#endif
	return 0;
}
EXPORT_SYMBOL(mipi_lgd_lcd_ief_off);

extern int mipi_lgd_lcd_ief_on(void)
{
#ifdef LGIT_IEF_SWITCH

	if(local_mfd0->panel_power_on && !is_ief_on) {
	
		printk("IEF_ON Starts with Camera\n");
		mutex_lock(&local_mfd0->dma->ov_mutex);		

		mipi_dsi_cmds_tx(&lgd_tx_buf,
											mipi_lgd_pdata->power_on_set_ief,
											mipi_lgd_pdata->power_on_set_ief_size);
		
		printk("%s, %d\n", __func__,is_ief_on);
		
		mutex_unlock(&local_mfd0->dma->ov_mutex);
		printk("IEF_ON Ends with Camera\n");
	}
	is_ief_on = 1;
#endif
	return 0;
}
EXPORT_SYMBOL(mipi_lgd_lcd_ief_on);
#endif

static int mipi_lgd_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	#if !defined(CONFIG_MACH_MSM8960_L1v) /*LCD reset moved to mipi_LGD_on*/
	static int gpio43 = PM8921_GPIO_PM_TO_SYS(43);
		gpio_set_value(gpio43, 1);
		mdelay(50);
		gpio_set_value(gpio43, 0);
		mdelay(10);
		gpio_set_value(gpio43, 1);
		mdelay(10);	
	#endif
	
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	#ifdef LGIT_IEF_SWITCH
		if(local_mfd0 == NULL)
			local_mfd0 = mfd;
	#endif

        /*2012-1226 skip LCD init for L1v*/
   	if(system_state == SYSTEM_BOOTING) {
		
		return 0;
	}
	else{
	printk(KERN_INFO "%s: mipi lgd lcd on started \n", __func__);
	mipi_dsi_cmds_tx(&lgd_tx_buf, mipi_lgd_pdata->power_on_set,
			mipi_lgd_pdata->power_on_set_size);
	}

	return 0;
}

static int mipi_lgd_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	printk(KERN_INFO "%s: mipi lgd lcd off started \n", __func__);
	mipi_dsi_cmds_tx(&lgd_tx_buf,
			mipi_lgd_pdata->power_off_set,
			mipi_lgd_pdata->power_off_set_size);
	return 0;
}

static void mipi_lgd_set_backlight_board(struct msm_fb_data_type *mfd)
{
	int level;

	level = (int)mfd->bl_level;
	mipi_lgd_pdata->backlight_level(level, 0, 0);
}

static int mipi_lgd_lcd_probe(struct platform_device *pdev)
{
	struct msm_fb_panel_data *pdata;
	if (pdev->id == 0) {
		mipi_lgd_pdata = pdev->dev.platform_data;
		return 0;
	}

	printk(KERN_INFO "%s: mipi lgd lcd probe start\n", __func__);
	pdata = pdev->dev.platform_data;
	if (!pdata)
		return 0;

#if defined(CONFIG_MACH_MSM8960_FX1) || defined(CONFIG_MACH_MSM8960_L1v)
	if(is_factory_cable() == 0)
		pdata->panel_info.bl_max = mipi_lgd_pdata->max_backlight_level;
	else
		pdata->panel_info.bl_max = 40;
#else
		pdata->panel_info.bl_max = mipi_lgd_pdata->max_backlight_level;
#endif
	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_lgd_lcd_probe,
	.driver = {
		.name   = "mipi_lgd",
	},
};

static struct msm_fb_panel_data lgd_panel_data = {
	.on		= mipi_lgd_lcd_on,
	.off		= mipi_lgd_lcd_off,
	.set_backlight = mipi_lgd_set_backlight_board,
};

static int ch_used[3];

int mipi_lgd_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_lgd", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	lgd_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &lgd_panel_data,
		sizeof(lgd_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int __init mipi_lgd_lcd_init(void)
{
	mipi_dsi_buf_alloc(&lgd_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&lgd_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}

module_init(mipi_lgd_lcd_init);
