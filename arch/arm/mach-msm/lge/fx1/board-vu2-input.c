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

/* keypad */
//#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include CONFIG_BOARD_HEADER_FILE

/* i2c */
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>


/* touch screen device */
#include <linux/input/lge_touch_core_f200.h>
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3

static int maker_id_val;


static struct touch_device_caps touch_caps = {
	.button_support 			= 1,
	.y_button_boundary			= 0,
	.button_margin				= 0,
	.number_of_button 			= 2,	// F200 model
	.button_name 				= {KEY_BACK,KEY_MENU},
	.is_width_supported 		= 1,
	.is_pressure_supported 		= 1,
	.is_id_supported			= 1,
	.max_width 					= 15,
	.max_pressure 				= 0xFF,
	.max_id						= 10,
	.lcd_x						= 768,
	.lcd_y						= 1024,
	.x_max					= 1535,
	.y_max					= 2047,		
};

static struct touch_operation_role touch_role = {
	.operation_mode 		= INTERRUPT_MODE,
	.report_mode			= CONTINUOUS_REPORT_MODE,
	.key_type			= TOUCH_HARD_KEY,
	.delta_pos_threshold 	= 0,
	.orientation 			= 0,
	.report_period			= 10000000,
	.booting_delay 		= 100,
	.reset_delay			= 20,
	.suspend_pwr			= POWER_OFF,
	.jitter_filter_enable		= 1,
	.jitter_curr_ratio		= 26,
	.accuracy_filter_enable  = 1,
	.irqflags 				= IRQF_TRIGGER_FALLING,
	.ta_debouncing_count	= 2,
	.ghost_detection_enable	=1,
};

int synaptics_t1320_power_on(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_touch_3v;
	static struct regulator *vreg_touch_1p8;

	printk("[TOUCH]%s power %s\n",(maker_id_val ? "ATMEL" : "SYNAPTICS"), (on ? "ON" : "OFF"));


	vreg_touch_3v = regulator_get(NULL, "8921_l17");		/* +3V0_TOUCH */
	if (IS_ERR(vreg_touch_3v)) 
	{
		pr_err("%s: regulator get of 8921_l29 failed (%ld)\n", __func__, PTR_ERR(vreg_touch_3v));
		rc = PTR_ERR(vreg_touch_3v);
		return rc;
	}

	rc = regulator_set_voltage(vreg_touch_3v, 3000000, 3000000);


	gpio_request(TS_GPIO_IRQ , "touch_int");
	gpio_request(TS_GPIO_I2C_SDA , "touch_sda");
	gpio_request(TS_GPIO_I2C_SCL , "touch_scl");

	if(!vreg_touch_1p8)
	{
		vreg_touch_1p8 = regulator_get(NULL, "8921_l29");	/* +1V8_TOUCH_VIO */
		if (IS_ERR(vreg_touch_1p8)) 
		{
			pr_err("%s: regulator get of vreg_touch_1p8 failed (%ld)\n", __func__, PTR_ERR(vreg_touch_1p8));
			rc = PTR_ERR(vreg_touch_1p8);
			return rc;
		}
	}

	rc = regulator_set_voltage(vreg_touch_1p8, 1800000, 1800000);

	if(on)
	{
		rc = gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_direction_input(TS_GPIO_IRQ);

		gpio_direction_output(TS_GPIO_I2C_SDA, 1);
		gpio_direction_output(TS_GPIO_I2C_SCL, 1);

		gpio_set_value(MSM_8960_TS_TUCH_RESET, 1);

		rc = regulator_enable(vreg_touch_3v);
		rc = regulator_enable(vreg_touch_1p8);

	}
	else
	{
		rc = gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_OUTPUT,  GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_direction_output(TS_GPIO_IRQ, 0);
		gpio_direction_output(TS_GPIO_I2C_SDA, 0);


		rc = regulator_disable(vreg_touch_1p8);
		rc = regulator_disable(vreg_touch_3v);
		
		gpio_set_value(MSM_8960_TS_TUCH_RESET, 0);
	}

	gpio_free(TS_GPIO_IRQ);
	gpio_free(TS_GPIO_I2C_SDA);
	gpio_free(TS_GPIO_I2C_SCL);


	return rc;
}

static struct touch_power_module touch_pwr = {
	.use_regulator	= 0,
	.vdd			= "8921_l17",
	.vdd_voltage	= 3000000,
	.vio			= "8921_l29",
	.vio_voltage	= 1800000,
	.power			= synaptics_t1320_power_on,
};

static struct touch_platform_data  lge_synaptics_ts_data = {
	.int_pin	= TS_GPIO_IRQ,
	.reset_pin	= 0,
	.maker		= "Synaptics",
	.caps		= &touch_caps,
	.role		= &touch_role,
	.pwr		= &touch_pwr,
};



static struct i2c_board_info msm_i2c_synaptics_ts_info[] = {
       {
               I2C_BOARD_INFO(LGE_TOUCH_SYNAPTICS_NAME, LGE_TOUCH_SYNATICS_I2C_SLAVE_ADDR),
               .platform_data = &lge_synaptics_ts_data,
               .irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
       }
};


static struct i2c_registry lge_vu2_touch_i2c_devices[] __initdata = {
	     {
		 	I2C_SURF | I2C_FFA | I2C_FLUID,
			MSM_8960_GSBI3_QUP_I2C_BUS_ID,
			msm_i2c_synaptics_ts_info,
			ARRAY_SIZE(msm_i2c_synaptics_ts_info)
		},

};

void touch_init(void)
{
	int rc;
	maker_id_val = 0;
	
	/* gpio init */
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(MSM_8960_TS_MAKER_ID, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(MSM_8960_TS_TUCH_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);


	rc = gpio_request(MSM_8960_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	rc = gpio_request(MSM_8960_TS_TUCH_RESET, "TOUCH_PANEL_RESET");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/*  maker_id */
	gpio_direction_input(MSM_8960_TS_MAKER_ID);

	gpio_direction_output(MSM_8960_TS_TUCH_RESET, 0);

	/* read gpio value */
	maker_id_val = gpio_get_value(MSM_8960_TS_MAKER_ID);


	printk("Touch maker ID is : %s\n", (maker_id_val ? "ATMEL" : "SYNAPTICS"));

}

static void __init touch_panel(int bus_num)
{
	/* touch init */
	touch_init();

	i2c_register_board_info(lge_vu2_touch_i2c_devices[maker_id_val].bus,
		lge_vu2_touch_i2c_devices[maker_id_val].info,
		lge_vu2_touch_i2c_devices[maker_id_val].len);
	
	/*
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
	*/

}

/* common function */
void __init lge_add_input_devices(void)
{
    lge_add_gpio_i2c_device(touch_panel);
}
