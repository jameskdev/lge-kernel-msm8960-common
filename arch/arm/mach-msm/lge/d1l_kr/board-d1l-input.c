/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#include <mach/vreg.h>
#include <mach/rpc_server_handset.h>
#include <mach/board.h>
#include <mach/board_lge.h>

#include "board-d1l.h"
#include CONFIG_BOARD_HEADER_FILE

/* i2c */
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/i2c/melfas_ts.h>


/* touch screen device */
int ts_set_vreg(int on, bool log_en)
{
	int rc = -EINVAL;

	if (log_en)
		printk(KERN_INFO "[Touch D] %s: power %s\n",
			__func__, on ? "On" : "Off");

	if (on)
		rc = gpio_direction_output(MSM_8960_TS_PWR, 1);
	else
		rc = gpio_direction_output(MSM_8960_TS_PWR, 0);

	msleep(30);
	return rc;
}

static struct melfas_tsi_platform_data melfas_ts_pdata = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.gpio_ldo = MSM_8960_TS_PWR,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 4,
	.button[0]				= KEY_MENU,
	.button[1]				= KEY_HOMEPAGE,
	.button[2]				= KEY_BACK,
	.button[3]				= KEY_SEARCH,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= 1,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
};

static struct melfas_tsi_platform_data melfas_ts_pdata_revA = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.gpio_ldo = MSM_8960_TS_PWR,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 2,
	.button[0]				= KEY_MENU,
	.button[1]				= KEY_BACK,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= 3,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
};

static struct melfas_tsi_platform_data melfas_ts_pdata_revB = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.gpio_ldo = MSM_8960_TS_PWR,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 2,
	.button[0]				= KEY_BACK,
	.button[1]				= KEY_MENU,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= 0xc7,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
};

static struct i2c_board_info melfas_touch_panel_i2c_bdinfo[] = {

	[0] = {
			I2C_BOARD_INFO(MELFAS_TS_NAME, MELFAS_TS_I2C_SLAVE_ADDR),
			.platform_data = &melfas_ts_pdata,
			.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	},
	[1] = {
			I2C_BOARD_INFO(MELFAS_TS_NAME, MELFAS_TS_I2C_SLAVE_ADDR),
			.platform_data = &melfas_ts_pdata_revA,
			.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	},
	[2] = {
			I2C_BOARD_INFO(MELFAS_TS_NAME, MELFAS_TS_I2C_SLAVE_ADDR),
			.platform_data = &melfas_ts_pdata_revB,
			.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	},
};

static int maker_id_val;

void touch_init(void)
{
	int rc;
	maker_id_val = 0;

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(MSM_8960_TS_PWR, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	/* gpio init */
	rc = gpio_request(MSM_8960_TS_PWR, "TOUCH_PANEL_PWR");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	rc = gpio_request(MSM_8960_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/*  maker_id */
	gpio_direction_input(MSM_8960_TS_MAKER_ID);

	/* power on */
	rc = ts_set_vreg(1, false);

	/* read gpio value */
	maker_id_val = gpio_get_value(MSM_8960_TS_MAKER_ID);
}

static void __init touch_panel(int bus_num)
{
    /* touch init */
	touch_init();

	if(lge_get_board_revno() < HW_REV_A) {
		i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
						(&melfas_touch_panel_i2c_bdinfo[0]), 1);
	} else if (lge_get_board_revno() == HW_REV_A) {
		i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
						(&melfas_touch_panel_i2c_bdinfo[1]), 1);
	} else if (lge_get_board_revno() > HW_REV_A) {
		i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
						(&melfas_touch_panel_i2c_bdinfo[2]), 1);
	}

}

/* common function */
void __init lge_add_input_devices(void)
{
    lge_add_gpio_i2c_device(touch_panel);
}
