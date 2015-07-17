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

#include "board-lx.h"
#include CONFIG_BOARD_HEADER_FILE

/* i2c */
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/i2c/melfas_ts.h>

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
#include <linux/synaptics_i2c_rmi4.h>
#endif

/* touch screen device */
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MAXT224E) ||\
	defined(CONFIG_TOUCHSCREEN_MMS136) ||\
	defined(CONFIG_TOUCHSCREEN_MMS128)
static int ts_set_l17(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_l17 = NULL;

	if (IS_ERR_OR_NULL(vreg_l17)) {
		vreg_l17 = regulator_get(NULL, "8921_l17");	/* 3P0_L17_TOUCH */
	}
	if (IS_ERR(vreg_l17)) {
		pr_err("%s: regulator get of touch_3p0 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_l17));
		rc = PTR_ERR(vreg_l17);
		return rc;
	}
	rc = regulator_set_voltage(vreg_l17,
		MELFAS_VD33_MIN_UV, MELFAS_VD33_MAX_UV);

	if (on) {
		if (regulator_is_enabled(vreg_l17) <= 0) {
			rc = regulator_set_optimum_mode(vreg_l17, MELFAS_VD33_CURR_UA);
			rc = regulator_enable(vreg_l17);
		}
	} else {
		if (regulator_is_enabled(vreg_l17) > 0) {
			rc = regulator_set_optimum_mode(vreg_l17, 0);
			rc = regulator_disable(vreg_l17);
		}
	}
	return rc;
}

int ts_set_vreg(int on, bool log_en)
{
	int rc = -EINVAL;

	if (log_en)
		printk(KERN_INFO "[Touch D] %s: power %s\n",
			__func__, on ? "On" : "Off");

#if defined(CONFIG_USING_INNOTEK_PANEL_4_3)
	rc = ts_set_l17(on);
#else
	if (lge_get_board_revno() >= HW_REV_B) {
		rc = ts_set_l17(on);
	}
	else {
		if (on)
			rc = gpio_direction_output(MSM_8960_TS_PWR, 1);
		else
			rc = gpio_direction_output(MSM_8960_TS_PWR, 0);
	}
#endif
	msleep(30);
	return rc;
}

static struct melfas_tsi_platform_data melfas_ts_pdata = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 3,
	.button[0]				= KEY_BACK,
	.button[1]				= KEY_HOMEPAGE,
	.button[2]				= KEY_MENU,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= TOUCH_FW_VERSION,
	.palm_threshold			= 0,
	.delta_pos_threshold		= 0,
};

static struct melfas_tsi_platform_data melfas_ts_pdata_revA = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
#if !defined(CONFIG_USING_INNOTEK_PANEL_4_3)
	.gpio_ldo = MSM_8960_TS_PWR,
#endif
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay               = 400,          /* ms */
	.report_period                  = 12500000,     /* 12.5 msec */
	.num_of_finger                  = 10,
	.num_of_button                  = 4,
#if defined(CONFIG_USING_INNOTEK_PANEL_4_3)
	.button[0]                              = KEY_BACK,
	.button[1]                              = KEY_HOMEPAGE,
	.button[2]                              = KEY_MENU,
	.button[3]                              = KEY_SEARCH,
#else
	.button[0]                              = KEY_MENU,
	.button[1]                              = KEY_HOMEPAGE,
	.button[2]                              = KEY_BACK,
	.button[3]                              = KEY_SEARCH,
#endif
	.x_max                                  = TS_X_MAX,
	.y_max                                  = TS_Y_MAX,
	.fw_ver                                 = TOUCH_FW_VERSION,
	.palm_threshold			= 0,
	.delta_pos_threshold		= 0,
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
};

/* ATMEL Vic*/
/* touch-screen macros */
#elif defined(CONFIG_TOUCHSCREEN_ATMEL_MAXT224E)
static struct qt602240_platform_data atmel_ts_pdata = {
	.x_line		= 19,
	.y_line		= 11,
	.x_size		= 800,
	.y_size		= 480,
	.blen		= 32,
	.threshold	= 55,
	.voltage	= 2700000,
	.orient		= 7,
	.gpio_int	= TS_GPIO_IRQ,
	.scl		= TS_GPIO_I2C_SDA,
	.sda		= TS_GPIO_I2C_SCL,
	.power_enable	  = ts_set_vreg,
};

static struct i2c_board_info atmel_touch_panel_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("qt602240_ts", ATMEL_TS_I2C_SLAVE_ADDR),
		.platform_data = &atmel_ts_pdata,
		.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	},
};
#endif

static int maker_id_val;

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MAXT224E) || defined(CONFIG_TOUCHSCREEN_MMS136)
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
#elif defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)
int synaptics_t1320_power_on(int on, bool log_en)
{
	int rc = -EINVAL;
	static struct regulator *vreg_lvs6;

	if (log_en)
		printk(KERN_INFO "[Touch D] %s: power %s\n",
			__func__, on ? "On" : "Off");


	vreg_lvs6 = regulator_get(NULL, "8921_lvs6");	/* +1V8_TOUCH_VIO */
	if (IS_ERR(vreg_lvs6)) {
		pr_err("%s: regulator get of 8921_lvs6 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_lvs6));
		rc = PTR_ERR(vreg_lvs6);
		return rc;
	}
	rc = regulator_set_voltage(vreg_lvs6,
		SYNAPTICS_I2C_VTG_MIN_UV, SYNAPTICS_I2C_VTG_MAX_UV);

	if (on) {
		rc = gpio_direction_output(SYNAPTICS_T1320_TS_PWR, 1);
		rc = regulator_set_optimum_mode(vreg_lvs6, SYNAPTICS_I2C_CURR_UA);
		rc = regulator_enable(vreg_lvs6);
	} else {
		rc = regulator_disable(vreg_lvs6);
		rc = gpio_direction_output(SYNAPTICS_T1320_TS_PWR, 0);
	}

	return rc;
}

static struct synaptics_ts_platform_data synaptics_t1320_ts_platform_data[] = {
	{
		.use_irq        		= 1,
		.irqflags       		= IRQF_TRIGGER_FALLING,
		.i2c_sda_gpio   		= SYNAPTICS_T1320_TS_I2C_SDA,
		.i2c_scl_gpio			= SYNAPTICS_T1320_TS_I2C_SCL,
		.i2c_int_gpio			= SYNAPTICS_T1320_TS_I2C_INT_GPIO,
		.power					= synaptics_t1320_power_on,
		.ic_booting_delay		= 400,		/* ms */
		.report_period			= 12500000, 	/* 12.5 msec */
		.num_of_finger			= 10,
		.num_of_button			= 4,
		.button[0]				= KEY_MENU,
		.button[1]				= KEY_HOMEPAGE,
		.button[2]				= KEY_BACK,
		.button[3]				= KEY_SEARCH,
		.x_max					= 1110,
		.y_max					= 1910,
		.fw_ver					= 1,
		.palm_threshold			= 0,
		.delta_pos_threshold	= 0,
	},
};

static struct i2c_board_info synaptics_panel_i2c_bdinfo[] = {
   [0] = {
			I2C_BOARD_INFO("synaptics_ts", 0x20),
			.platform_data = &synaptics_t1320_ts_platform_data,
			.irq = MSM_GPIO_TO_INT(SYNAPTICS_T1320_TS_I2C_INT_GPIO),
   },
};
#endif

static void __init touch_panel(int bus_num)
{
	int rc;

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MAXT224E) ||\
	defined(CONFIG_TOUCHSCREEN_MMS136) ||\
	defined(CONFIG_TOUCHSCREEN_MMS128)

	rc = gpio_request(MSM_8960_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");

	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/*  maker_id set 0 - normal , maker_id set 1 - dummy pattern */
	gpio_direction_input(MSM_8960_TS_MAKER_ID);

        ts_set_vreg(1, false);
	if (lge_get_board_revno() >= HW_REV_B) {
		i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
			(&melfas_touch_panel_i2c_bdinfo[0]), 1);
	} else {
		i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
			(&melfas_touch_panel_i2c_bdinfo[1]), 1);
	}
#elif defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)
	i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
		&synaptics_panel_i2c_bdinfo[0], 1);
#endif
}

/* common function */
void __init lge_add_input_devices(void)
{
    lge_add_gpio_i2c_device(touch_panel);
}
