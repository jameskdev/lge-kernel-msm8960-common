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

#include "board-l_dcm.h"
#include CONFIG_BOARD_HEADER_FILE

/* i2c */
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>

#include <linux/i2c/synaptics_i2c_rmi4.h>

/* touch screen device */
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3
int ts_set_vreg(int on, bool log_en);
int tk_set_vreg(int on, bool log_en);

static struct synaptics_ts_platform_data ts_platform_data[] = {
{
	.use_irq				= 1,
	.irqflags				= IRQF_TRIGGER_LOW,/*IRQF_TRIGGER_FALLING,*/
	.i2c_sda_gpio			= SYNAPTICS_T1320_TS_I2C_SDA,
	.i2c_scl_gpio			= SYNAPTICS_T1320_TS_I2C_SCL,
	.i2c_int_gpio			= SYNAPTICS_T1320_TS_I2C_INT_GPIO,
	.i2c_ldo_gpio           = SYNAPTICS_T1320_TS_PWR,
	.ts_power				= ts_set_vreg,
	.report_period			= 10000000,
	.num_of_finger			= DCM_MAX_NUM_OF_FINGER,
	.x_max					= SYNAPTICS_S3200_RESOLUTION_X,
	.y_max					= SYNAPTICS_S3200_RESOLUTION_Y,
	.fw_ver					= 1,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
	.ts_power_flag			= 0,
	.jitter_curr_ratio		= 26,
	.jitter_filter_enable	= 1,
	.accuracy_filter_enable = 1,
	},
};

static struct i2c_board_info synaptics_panel_i2c_bdinfo[] = {
   [0] = {
			I2C_BOARD_INFO(SYNAPTICS_TS_NAME, SYNAPTICS_TS_I2C_SLAVE_ADDR),
				.platform_data = &ts_platform_data,
				.irq = MSM_GPIO_TO_INT(SYNAPTICS_T1320_TS_I2C_INT_GPIO),
		 },
};

static unsigned short key_touch_map[] = {
	KEY_RESERVED,
	KEY_HOMEPAGE,
	KEY_BACK,
	KEY_MENU,
};

static struct key_touch_platform_data tk_platform_data = {
	.tk_power	= tk_set_vreg,
	.irq   	= KEY_TOUCH_GPIO_INT,
	.scl  	= KEY_TOUCH_GPIO_I2C_SCL,
	.sda  	= KEY_TOUCH_GPIO_I2C_SDA,
	.ldo    = SYNAPTICS_T1320_TS_PWR,
	.keycode = (unsigned char *)key_touch_map,
	.keycodemax = (ARRAY_SIZE(key_touch_map) * 2),
	.tk_power_flag = 0,
};

static bool touch_dev_first_boot = false;

int ts_set_vreg(int on, bool log_en)
{
	int rc = -EINVAL;

#if 0
	if (log_en)
		printk(KERN_INFO "[Touch D] %s: power %s\n",
			__func__, on ? "On" : "Off");
#endif

	if (on) {
		if (!ts_platform_data[0].ts_power_flag && !tk_platform_data.tk_power_flag) {
			rc = gpio_direction_output(SYNAPTICS_T1320_TS_PWR, 1);
			printk("===\n[TOUCH_POWER_ON] BY [TS], gpio:[%d]===\n\n", SYNAPTICS_T1320_TS_PWR);
			mdelay(100);
		}
		ts_platform_data[0].ts_power_flag = 1;
	} else {
		ts_platform_data[0].ts_power_flag = 0;
		if (!ts_platform_data[0].ts_power_flag && !tk_platform_data.tk_power_flag) {
			rc = gpio_direction_output(SYNAPTICS_T1320_TS_PWR, 0);
			printk("===\n[TOUCH_POWER_OFF] BY [TS], gpio:[%d]===\n\n", SYNAPTICS_T1320_TS_PWR);
			mdelay(20);
		}
	}

	return rc;
}

int tk_set_vreg(int on, bool log_en)
{
	int rc = -EINVAL;
#if 0
	if (log_en)
		printk(KERN_INFO "[KEY D] %s: power %s\n",
			   __func__, on ? "On" : "Off");
#endif
	if (on) {
		if (!tk_platform_data.tk_power_flag) {

			if (!touch_dev_first_boot) {
				pr_info("%s: Touch LDO ON On first boot \n", __func__);

				gpio_tlmm_config(GPIO_CFG(16, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
				gpio_tlmm_config(GPIO_CFG(17, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
				gpio_tlmm_config(GPIO_CFG(52, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
				rc = gpio_direction_output(52, 0);
				mdelay(20);
				touch_dev_first_boot = true;
			}

			rc = gpio_direction_output(52, 1);

			gpio_tlmm_config(GPIO_CFG(16, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_tlmm_config(GPIO_CFG(17, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_tlmm_config(GPIO_CFG(52, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

			printk("===\n[TOUCH_POWER_ON] BY [TK], gpio:[%d]===\n\n", SYNAPTICS_T1320_TS_PWR);
			mdelay(45);
		} else {
			pr_info("%s: Touch LDO Aleady ON \n", __func__);
		}

		tk_platform_data.tk_power_flag = 1;
	} else {
		tk_platform_data.tk_power_flag = 0;
		if (!tk_platform_data.tk_power_flag) {

			gpio_tlmm_config(GPIO_CFG(16, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_tlmm_config(GPIO_CFG(17, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_tlmm_config(GPIO_CFG(52, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

			rc = gpio_direction_output(SYNAPTICS_T1320_TS_PWR, 0);
			printk("===\n[TOUCH_POWER_OFF BY] [TK], gpio:[%d]===\n\n", SYNAPTICS_T1320_TS_PWR);
			/*mdelay(20);*/
		} else {
			pr_info("%s: Touch LDO Aleady OFF \n", __func__);
		}
	}

	return rc;
}

static struct i2c_board_info key_touch_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO(SYNAPTICS_KEYTOUCH_NAME, SYNAPTICS_KEYTOUCH_I2C_SLAVE_ADDR),
				.type = "so340010",
				.platform_data = &tk_platform_data,
		},
};

static int maker_id_val;

static void __init touch_init(void)
{
	int rc;
	maker_id_val = 0;

	/* gpio init */
	rc = gpio_request(MSM_8960_TS_PWR, "TOUCH_PANEL_PWR");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	rc = gpio_request(MSM_8960_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/*  maker_id */
	gpio_direction_input(MSM_8960_TS_MAKER_ID);
}

static void __init touch_panel(int bus_num)
{
	/* touch init */
	touch_init();

	i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
			&synaptics_panel_i2c_bdinfo[0], 1);
}

void dcm_key_init(void)
{
#if 0
	int rc;


	/* gpio init */
	rc = gpio_request(MSM_8960_TS_PWR, "KEY_TOUCH_PWR");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);
#endif /* 0 */
}

static void __init key_touch(int bus_num)
{
    /* touch init */
    dcm_key_init();

	i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
			&key_touch_i2c_bdinfo[0], 1);
}

/* common function */
void __init lge_add_input_devices(void)
{
	lge_add_gpio_i2c_device(touch_panel);
    lge_add_gpio_i2c_device(key_touch);

}
