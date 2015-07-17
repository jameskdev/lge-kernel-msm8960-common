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
#include <linux/mfd/pm8xxx/pm8921.h>
#include CONFIG_BOARD_HEADER_FILE

/* i2c */
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>

/* LGE_CHANGE_S mystery184.kim@lge.com touch initial porting */
#ifdef CONFIG_LGE_TOUCH_SYNAPTICS_325
#include <linux/input/lge_touch_core_325.h>
#endif
/* LGE_CHANGE_E mystery184.kim@lge.com touch initial porting */

/* touch screen device */
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3

/* LGE_CHANGE_S 
 * 2012-06-27
 * mystery184.kim@lge.com touch initial porting 
 */
#if defined(CONFIG_LGE_TOUCH_SYNAPTICS_325)
static int maker_id_val;
void touch_init(void)
{
	int rc;
	maker_id_val = 0;
	
	/* gpio init */
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	/* LGE_CHANGE_S 20120906 mystery184.kim@lge.com   
	 * update reset gpiopin  configration
	 * REV.A - NO_PULL or PULL_DOWN(sleep), reset pin has 10k external resistance 
	 * REV.B - Need PULL_UP or PULL_DOWN(sleep) , remove 10k resistor
	 */
	//gpio_tlmm_config(GPIO_CFG(TS_GPIO_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	/* LGE_CHANGE_E 20120906 mystery184.kim@lge.com   */
	gpio_tlmm_config(GPIO_CFG(MSM_8960_TS_PWR, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);


	rc = gpio_request(MSM_8960_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/*  maker_id */
	gpio_direction_input(MSM_8960_TS_MAKER_ID);

	/* read gpio value */
	maker_id_val = gpio_get_value(MSM_8960_TS_MAKER_ID);

	printk("Touch maker ID is : %s\n", (maker_id_val ? "ATMEL" : "SYNAPTICS"));
}

static struct touch_device_caps touch_caps = {
	.button_support 			= 1,
	.y_button_boundary			= 2560,
	.button_margin				= 10,
	#if 0
	.number_of_button 			= 4,	// 325 kyle.jeon@lge.com
	.button_name 				= {KEY_MENU,KEY_HOMEPAGE,KEY_BACK,KEY_SEARCH},
	#endif
	.number_of_button 			= 2,    // fx1 mystery184.kim@lge.com
	.button_name				= {KEY_BACK,KEY_MENU},
	.is_width_supported 		= 1,
	.is_pressure_supported 		= 1,
	.is_id_supported			= 1,
	.max_width 					= 15,
	.max_pressure 				= 0xFF,
	.max_id						= 10,
	.lcd_x						= 736,
	.lcd_y						= 1280,    
	.x_max					= 1540,
	.y_max					= 2560,		
};

static struct touch_operation_role touch_role = {
	.operation_mode 		= INTERRUPT_MODE,
	.report_mode			= CONTINUOUS_REPORT_MODE,
	.key_type			= TOUCH_HARD_KEY,
	.delta_pos_threshold 	= 0,
	.orientation 			= 0,
	.report_period			= 10000000,
	.booting_delay 		= 250,
	.reset_delay			= 20,
	.suspend_pwr			= POWER_OFF,
	.jitter_filter_enable		= 1,
	.jitter_curr_ratio		= 26,
	.accuracy_filter_enable		= 1,
	.irqflags 				= IRQF_TRIGGER_FALLING,
	.ta_debouncing_count	= 2,
	.ghost_detection_enable	= 1,
};

int synaptics_7020s_power_on(int on)
{
	int rc = -EINVAL;
	 static struct regulator *vreg_core; 

#if defined (CONFIG_MACH_MSM8960_FX1SK)
	 static struct regulator *vreg_core_l29;
#endif

	/* LGE_CHAGE_S 20120905 mystery184.kim@lge.com disable FX1 touch 1.8V VIO, it is controlled by LCD */
	// static struct regulator *vreg_io; 
	/* LGE_CHAGE_E 20120905 mystery184.kim@lge.com disable FX1 touch 1.8V VIO, it is controlled by LCD */
	printk(KERN_INFO "[Touch D] %s: power %s\n",
		__func__, on ? "On" : "Off");
	if (!vreg_core)
	vreg_core = regulator_get(NULL, "8921_l17_touch");

#if defined (CONFIG_MACH_MSM8960_FX1SK)
	if (!vreg_core_l29)
	vreg_core_l29 = regulator_get(NULL, "8921_l29_touch");
#endif

	if (IS_ERR(vreg_core)) {
		pr_err("%s: regulator get of 8921_l17 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_core));
		rc = PTR_ERR(vreg_core);
		return rc;
	}
	rc = regulator_set_voltage(vreg_core,
		3000000, 3000000); 
	if (on) {
		/* LGE_CHANGE_S 20120906 mystery184.kim@lge.com   
		 * update reset gpiopin  configration
		 * REV.A - NO_PULL or PULL_DOWN(sleep), reset pin has 10k external resistance 
		 * REV.B - Need PULL_UP or PULL_DOWN(sleep) , remove 10k resistance
		 */
		gpio_tlmm_config(GPIO_CFG(TS_GPIO_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(TS_GPIO_RESET , 1);
		/* LGE_CHANGE_E 20120906 mystery184.kim@lge.com   */
		
		rc = regulator_set_optimum_mode(vreg_core, 9630);
		rc = regulator_enable(vreg_core);
	} else {
		rc = regulator_disable(vreg_core);
		/* LGE_CHANGE_S 20120906 mystery184.kim@lge.com   
		 * update reset gpiopin  configration
		 * REV.A - NO_PULL or PULL_DOWN(sleep), reset pin has 10k external resistance 
		 * REV.B - Need PULL_UP or PULL_DOWN(sleep) , remove 10k resistance
		 */
		gpio_tlmm_config(GPIO_CFG(TS_GPIO_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(TS_GPIO_RESET , 0);		
		/* LGE_CHANGE_E 20120906 mystery184.kim@lge.com   */
	}

#if defined (CONFIG_MACH_MSM8960_FX1SK)
	if (IS_ERR(vreg_core_l29)) {
		pr_err("%s: regulator get of 8921_l29 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_core_l29));
		rc = PTR_ERR(vreg_core_l29);
		return rc;
	}
	rc = regulator_set_voltage(vreg_core_l29,
		1800000, 1800000); 
	if (on) {
		rc = regulator_set_optimum_mode(vreg_core_l29, 9630);
		rc = regulator_enable(vreg_core_l29);
	} else {
		rc = regulator_disable(vreg_core_l29);
	}
#endif

/* LGE_CHAGE_S 20120905 mystery184.kim@lge.com disable FX1 touch 1.8V VIO, it is controlled by LCD */
/*	vreg_io = regulator_get(NULL, "8921_lvs6");	
	if (IS_ERR(vreg_io)) {
		pr_err("%s: regulator get of 8921_lvs6 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_io));
		rc = PTR_ERR(vreg_io);
		return rc;
	}
	// no need to set volatage because regulator is LSV, not LDO
	//rc = regulator_set_voltage(vreg_io,
	//	1800000, 1800000); 
	if (on) {
		rc = regulator_enable(vreg_io);
	} else {
		// never turn off io regulator
		//rc = regulator_disable(vreg_io);
	}*/
/* LGE_CHAGE_E 20120905 mystery184.kim@lge.com disable FX1 touch 1.8V VIO, it is controlled by LCD */
	return rc;
}

static struct touch_power_module touch_pwr = {
	.use_regulator	= 0,
	.vdd			= "8921_l17",
	.vdd_voltage	= 3000000,
	.vio			= "8921_l29",
	.vio_voltage	= 1800000,
	.power			= synaptics_7020s_power_on,
};

static struct touch_platform_data  lge_synaptics_ts_data = {
	.int_pin	= TS_GPIO_IRQ,
	.reset_pin	= TS_GPIO_RESET,
	.maker		= "Synaptics",
	.caps		= &touch_caps,
	.role		= &touch_role,
	.pwr		= &touch_pwr,
};

static struct i2c_board_info msm_i2c_synaptics_ts_info[] = {
       [0] = {
               I2C_BOARD_INFO(LGE_TOUCH_SYNAPTICS_NAME, LGE_TOUCH_SYNATICS_I2C_SLAVE_ADDR),
               .platform_data = &lge_synaptics_ts_data,
               .irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
       },
};
#else
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

static struct melfas_tsi_platform_data melfas_ts_pdata_vzw = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.gpio_ldo = MSM_8960_TS_PWR,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 4,
	.button[0]				= KEY_BACK,
	.button[1]				= KEY_HOMEPAGE,
	.button[2]				= KEY_RECENT,
	.button[3]				= KEY_MENU,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= TOUCH_FW_VERSION,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
};

static struct i2c_board_info melfas_touch_panel_i2c_bdinfo[] = {
	[0] = {
			I2C_BOARD_INFO(MELFAS_TS_NAME, MELFAS_TS_I2C_SLAVE_ADDR),
			.platform_data = &melfas_ts_pdata_vzw,
			.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	},
};

void touch_init(void)
{
	int rc;

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(MSM_8960_TS_PWR, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	/* gpio init */
	rc = gpio_request(MSM_8960_TS_PWR, "TOUCH_PANEL_PWR");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/* power on */
	rc = ts_set_vreg(1, false);
}
#endif
/* LGE_CHANGE_E mystery184.kim@lge.com touch initial porting */

static void __init touch_panel(int bus_num)
{
/* LGE_CHANGE_S mystery184.kim@lge.com touch initial porting */
#if defined(CONFIG_LGE_TOUCH_SYNAPTICS_325)
	touch_init();

	/*i2c_register_board_info(lge_fx1_touch_i2c_devices[0].bus,
		lge_fx1_touch_i2c_devices[0].info,
		lge_fx1_touch_i2c_devices[0].len);*/
	i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
		(&msm_i2c_synaptics_ts_info[0]), 1);
#else
	touch_init();
	i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
				(&melfas_touch_panel_i2c_bdinfo[0]), 1);
#endif
/* LGE_CHANGE_E mystery184.kim@lge.com touch initial porting */
}

void __init lge_add_input_devices(void)
{
    lge_add_gpio_i2c_device(touch_panel);
}
