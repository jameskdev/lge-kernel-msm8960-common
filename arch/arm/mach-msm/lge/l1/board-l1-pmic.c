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

/*
 * include LGE board specific header file
 */
#include "board-l1.h"
#include CONFIG_BOARD_HEADER_FILE
#include <mach/board_lge.h>

#include <linux/interrupt.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/msm_ssbi.h>
#include <asm/mach-types.h>
#include <mach/msm_bus_board.h>
#include <mach/restart.h>
#include "devices.h"
#include <linux/i2c.h>
#ifdef CONFIG_BATTERY_MAX17043
#include <linux/max17043_fuelgauge.h>
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#endif

#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
#include <linux/mfd/pm8xxx/direct_qcoin.h>
#include "../../../../../drivers/staging/android/timed_output.h"
#endif

#ifdef CONFIG_LGE_WIRELESS_CHARGER_BQ24160
#include <linux/bq24160_charger.h>
#endif

struct pm8xxx_gpio_init {
	unsigned			gpio;
	struct pm_gpio			config;
};
/* LGE_CHANGE_S [dongju99.kim@lge.com] 2012-05-22 */
struct pm_gpio param2 = {
.direction = PM_GPIO_DIR_OUT,
.output_buffer = 0,
.output_value = 0,
.pull = PM_GPIO_PULL_NO,
.vin_sel = 2,
.out_strength = PM_GPIO_STRENGTH_HIGH,
.function = PM_GPIO_FUNC_1,
.inv_int_pol = 0,
};
/* LGE_CHANGE_E [dongju99.kim@lge.com] 2012-05-22 */ 

struct pm8xxx_mpp_init {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8XXX_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{ \
	.gpio	= PM8921_GPIO_PM_TO_SYS(_gpio), \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}

#define PM8XXX_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8921_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8XXX_GPIO_DISABLE(_gpio) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
			 0, 0, 0, 1)

#define PM8XXX_GPIO_OUTPUT(_gpio, _val) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_INPUT(_gpio, _pull) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_OUTPUT_FUNC(_gpio, _val, _func) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			_func, 0, 0)

#define PM8XXX_GPIO_OUTPUT_VIN(_gpio, _val, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, _vin, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_OUTPUT_STRENGTH(_gpio, _val, _out_strength) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			_out_strength, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

/* Initial PM8921 GPIO configurations */
static struct pm8xxx_gpio_init pm8921_gpios[] __initdata = {
	LGE_PM8921_GPIO_INITDATA,
#ifdef CONFIG_BATTERY_MAX17043
	PM8XXX_GPIO_INPUT(MAX17043_FUELGAUGE_GPIO_IRQ, PM_GPIO_PULL_UP_30),    /* FUEL_GAUGE_INT_N */
#endif
	PM8XXX_GPIO_OUTPUT(19,	0),	/* TABLA SPKR_RIGHT_EN=off */
#ifdef CONFIG_LGE_AUDIO
/* add by ehgrace.kim@lge.com for headset */
	PM8XXX_GPIO_OUTPUT(31, 0),                    /* EAR_MIC_EN_35D */
	PM8XXX_GPIO_INPUT(32, PM_GPIO_PULL_NO),       /* EARPOL_DETECT_35D */
#endif
#ifdef CONFIG_LGE_WIRELESS_CHARGER
	PM8XXX_GPIO_OUTPUT(42, 0),                    /* CHG_STAT */
	PM8XXX_GPIO_INPUT(25, PM_GPIO_PULL_NO),//PM_GPIO_PULL_UP_30),    /* ACTIVE_N */
#endif
	/* TABLA CODEC RESET */
	PM8XXX_GPIO_OUTPUT_STRENGTH(34, 1, PM_GPIO_STRENGTH_MED)
};

/* Initial PM8921 MPP configurations */
static struct pm8xxx_mpp_init pm8921_mpps[] __initdata = {
	/* External 5V regulator enable; shared by HDMI and USB_OTG switches. */
	PM8XXX_MPP_INIT(7, D_INPUT, PM8921_MPP_DIG_LEVEL_VPH, DIN_TO_INT),
	PM8XXX_MPP_INIT(PM8XXX_AMUX_MPP_8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8,
								DOUT_CTRL_LOW),
#ifdef CONFIG_LGE_AUDIO
/* add by ehgrace.kim@lge.com for headset */
	PM8XXX_MPP_INIT(4, D_INPUT, PM8921_MPP_DIG_LEVEL_S4, DIN_TO_INT),
	PM8XXX_MPP_INIT(9, D_INPUT, PM8921_MPP_DIG_LEVEL_S4, DIN_TO_INT),
#endif
};

void __init msm8960_pm8921_gpio_mpp_init(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(pm8921_gpios); i++) {
		/* LGE_CHANGE
		 * 2011-12-03, hyuncheol0.kim@lge.com
		 * Work-around code to support old H/W revision.
		 */
#ifdef CONFIG_LGE_PM
#ifdef CONFIG_BATTERY_MAX17043
		if (pm8921_gpios[i].gpio ==
				PM8921_GPIO_PM_TO_SYS(MAX17043_FUELGAUGE_GPIO_IRQ)) {
#if defined (CONFIG_MACH_MSM8960_D1LA)
			if (lge_get_board_revno() >= HW_REV_A &&
				lge_get_board_revno() <= HW_REV_B)
				pm8921_gpios[i].gpio =
					PM8921_GPIO_PM_TO_SYS(MAX17043_FUELGAUGE_GPIO_OLD_IRQ);
#elif defined (CONFIG_MACH_MSM8960_D1LV)
			if (lge_get_board_revno() >= HW_REV_B &&
				lge_get_board_revno() <= HW_REV_C)
				pm8921_gpios[i].gpio =
					PM8921_GPIO_PM_TO_SYS(MAX17043_FUELGAUGE_GPIO_OLD_IRQ);
#endif
		}
#endif
#endif
		rc = pm8xxx_gpio_config(pm8921_gpios[i].gpio,
					&pm8921_gpios[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}
/* LGE_CHANGE_S [dongju99.kim@lge.com] 2012-05-22 */
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(44), &param2); 
		if (rc)
			pr_err("%s: Failed to configure gpio %d\n", __func__, PM8921_GPIO_PM_TO_SYS(44));
/* LGE_CHANGE_E [dongju99.kim@lge.com] 2012-05-22 */ 

	for (i = 0; i < ARRAY_SIZE(pm8921_mpps); i++) {
		rc = pm8xxx_mpp_config(pm8921_mpps[i].mpp,
					&pm8921_mpps[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_mpp_config: rc=%d\n", __func__, rc);
			break;
		}
	}
}

static struct pm8xxx_adc_amux pm8xxx_adc_channels_data[] = {
	{"vcoin", CHANNEL_VCOIN, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vbat", CHANNEL_VBAT, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"dcin", CHANNEL_DCIN, CHAN_PATH_SCALING4, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ichg", CHANNEL_ICHG, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vph_pwr", CHANNEL_VPH_PWR, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ibat", CHANNEL_IBAT, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"batt_therm", CHANNEL_BATT_THERM, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_BATT_THERM},
	{"batt_id", CHANNEL_BATT_ID, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"usbin", CHANNEL_USBIN, CHAN_PATH_SCALING3, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pmic_therm", CHANNEL_DIE_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PMIC_THERM},
	{"625mv", CHANNEL_625MV, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"125v", CHANNEL_125V, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"chg_temp", CHANNEL_CHG_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pa_therm1", ADC_MPP_1_AMUX8, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
	{"xo_therm", CHANNEL_MUXOFF, CHAN_PATH_SCALING1, AMUX_RSV0,
		ADC_DECIMATION_TYPE2, ADC_SCALE_XOTHERM},
	{"pa_therm0", ADC_MPP_1_AMUX3, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
/* BEGIN: kidong0420.kim@lge.com 2011-11-09 Implement cable detection */
	{"pcb_rev", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"cdma_therm", ADC_MPP_1_AMUX8, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_XOTHERM},
	{"usb_id", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
/* END: kidong0420.kim@lge.com 2011-11-09 */
};

static struct pm8xxx_adc_properties pm8xxx_adc_data = {
	.adc_vdd_reference	= 1800, /* milli-voltage for this adc */
	.bitresolution		= 15,
	.bipolar                = 0,
};

static struct pm8xxx_adc_platform_data pm8xxx_adc_pdata = {
	.adc_channel            = pm8xxx_adc_channels_data,
	.adc_num_board_channel  = ARRAY_SIZE(pm8xxx_adc_channels_data),
	.adc_prop               = &pm8xxx_adc_data,
	.adc_mpp_base		= PM8921_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_irq_platform_data pm8xxx_irq_pdata __devinitdata = {
	.irq_base		= PM8921_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(104),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8xxx_gpio_pdata __devinitdata = {
	.gpio_base	= PM8921_GPIO_PM_TO_SYS(1),
};

static struct pm8xxx_mpp_platform_data pm8xxx_mpp_pdata __devinitdata = {
	.mpp_base	= PM8921_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_rtc_platform_data pm8xxx_rtc_pdata __devinitdata = {
	.rtc_write_enable       = true,
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8xxx_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us	= 15625,
	.wakeup			= 1,
};

static const unsigned int keymap[] = { LGE_KEY_MAP };

static struct matrix_keymap_data keymap_data = {
	.keymap_size    = ARRAY_SIZE(keymap),
	.keymap         = keymap,
};

static struct pm8xxx_keypad_platform_data keypad_data = {
	.input_name             = "keypad-8960",
	.input_phys_device      = "keypad-8960/input0",
#if defined(CONFIG_MACH_MSM8960_L2S)
	.num_rows		= 10,
	.num_cols		= 6,
#else
	.num_rows               = 5,
	.num_cols               = 5,
#endif /* CONFIG_MACH_MSM8960_L2S */
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &keymap_data,
};

static int pm8921_therm_mitigation[] = {
	1100,
	700,
	600,
	325,
};

/* LGE_UPDATE_S, 4.35 High Cell Voltage, roy.park@lge.com, 2011/08/17 -->[ */
#if defined(CONFIG_LGE_PM_435V_BATT)
#define MAX_VOLTAGE_MV		4350
static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
	.safety_time			= 480,
	.update_time			= 60000,
	.max_voltage			= MAX_VOLTAGE_MV,
	.min_voltage			= 3400,
	.resume_voltage_delta	= 70,
	.term_current			= 100,
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	.cool_temp			= 0,	/* from 10, */
	.warm_temp			= 0,	/* from 40, */
/* BEGIN : jooyeong.lee@lge.com 2012-02-27 Change the charger_temp_scenario */
	.temp_level_1		= 55,
	.temp_level_2		= 45,
	.temp_level_3		= 42,
	.temp_level_4		= -5,
	.temp_level_5		= -10,
/* END : jooyeong.lee@lge.com 2012-02-27 */
	/* LGE_CHANGE
 	* add the xo_thermal mitigation way 
	* 2012-04-10, hiro.kwon@lge.com
	*/
	.thermal_mitigation_method = IUSB_REDUCE_METHOD,
	/* 2012-04-10, hiro.kwon@lge.com */	
#else
	.cool_temp			= 0,	/* from 10, */
	.warm_temp			= 45,	/* from 40, */
#endif
	.temp_check_period		= 1,
#ifdef CONFIG_LGE_PM	
	.max_bat_chg_current	= IBAT_CURRENT,
#else
	.max_bat_chg_current	= 800,
#endif
	.cool_bat_chg_current	= 350,
	.warm_bat_chg_current	= 350,
	.cool_bat_voltage		= 4100,
	.warm_bat_voltage		= 4100,
	.thermal_mitigation		= pm8921_therm_mitigation,
	.thermal_levels			= ARRAY_SIZE(pm8921_therm_mitigation),
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	.cold_thr			= 1,	/* 80% */
	.hot_thr			= 0,	/* 25% */
#endif
#ifdef CONFIG_MACH_LGE
	.batt_id_gpio		= BATT_ID_GPIO,			/* No. of msm gpio for battery id */
	.batt_id_pu_gpio	= BATT_ID_PULLUP_GPIO,	/* No. of msm gpio for battery id pull up */
#endif
	.weak_voltage		= 3200,
	.weak_current		= 325,
};
#else
#define MAX_VOLTAGE_MV		4200
static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
	.safety_time			= 480,
	.update_time			= 60000,
	.max_voltage			= MAX_VOLTAGE_MV,
	.min_voltage			= 3200,
	.resume_voltage_delta	= 100,
	.term_current			= CHG_TERM_MA,
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	.cool_temp			= 0,
	.warm_temp			= 0,
/* BEGIN : jooyeong.lee@lge.com 2012-02-27 Change the charger_temp_scenario */
	.temp_level_1		= 55,
	.temp_level_2		= 45,
	.temp_level_3		= 42,
	.temp_level_4		= -5,
	.temp_level_5		= -10,
/* END : jooyeong.lee@lge.com 2012-02-27 */
	/* LGE_CHANGE
 	* add the xo_thermal mitigation way 
	* 2012-04-10, hiro.kwon@lge.com
	*/
	.thermal_mitigation_method = IUSB_REDUCE_METHOD,
	/* 2012-04-10, hiro.kwon@lge.com */	
#else
	.cool_temp		= 10,	/* from 10 */
	.warm_temp		= 40,	/* from 40 */
#endif
	.temp_check_period		= 1,
#ifdef CONFIG_LGE_PM	
	.max_bat_chg_current	= IBAT_CURRENT,
#else
	.max_bat_chg_current	= 800,
#endif

	.cool_bat_chg_current	= 350,
	.warm_bat_chg_current	= 350,
	.cool_bat_voltage		= 4100,
	.warm_bat_voltage		= 4100,
	.thermal_mitigation		= pm8921_therm_mitigation,
	.thermal_levels			= ARRAY_SIZE(pm8921_therm_mitigation),
	.rconn_mohm		= 18,
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	.cold_thr			= 1,	/* 80% */
	.hot_thr			= 0,	/* 25% */
#endif
#ifdef CONFIG_MACH_LGE
	.batt_id_gpio		= BATT_ID_GPIO,			/* No. of msm gpio for battery id */
	.batt_id_pu_gpio	= BATT_ID_PULLUP_GPIO,	/* No. of msm gpio for battery id pull up */
#endif
};
#endif/* LGE_UPDATE_E, 4.35 High Cell Voltage,  <--] */

#define CHG_TERM_MA            100
static struct pm8xxx_misc_platform_data pm8xxx_misc_pdata = {
	.priority		= 0,
};

static struct pm8921_bms_platform_data pm8921_bms_pdata __devinitdata = {
	.battery_type                   = BATT_UNKNOWN,
	.r_sense                        = 10,
	.v_cutoff                       = 3400,
	.max_voltage_uv                 = MAX_VOLTAGE_MV * 1000,
	.rconn_mohm                     = 18,
	.shutdown_soc_valid_limit       = 20,
	.adjust_soc_low_threshold       = 25,
	.chg_term_ua                    = CHG_TERM_MA * 1000,
};

/* QCT Original - NOT USED */
#if !defined(CONFIG_MACH_LGE)
#define	PM8921_LC_LED_MAX_CURRENT	4	/* I = 4mA */
#define	PM8921_LC_LED_LOW_CURRENT	1	/* I = 1mA */
#define PM8XXX_LED_PWM_PERIOD		1000
#define PM8XXX_LED_PWM_DUTY_MS		20
/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

static struct led_info pm8921_led_info_liquid[] = {
	{
		.name		= "led:red",
		.flags		= PM8XXX_ID_LED_0,
		.default_trigger	= "battery-charging",
	},
	{
		.name		= "led:green",
		.flags		= PM8XXX_ID_LED_0,
		.default_trigger	= "battery-full",
	},
	{
		.name		= "led:blue",
		.flags		= PM8XXX_ID_LED_2,
		.default_trigger	= "notification",
	},
};

static struct pm8xxx_led_config pm8921_led_configs_liquid[] = {
	[0] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
	[1] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_LOW_CURRENT,
	},
	[2] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
};

static struct led_platform_data pm8xxx_leds_core_liquid = {
	.num_leds = ARRAY_SIZE(pm8921_led_info_liquid),
	.leds = pm8921_led_info_liquid,
};

static struct pm8xxx_led_platform_data pm8xxx_leds_pdata_liquid = {
	.led_core = &pm8xxx_leds_core_liquid,
	.configs = pm8921_led_configs_liquid,
	.num_configs = ARRAY_SIZE(pm8921_led_configs_liquid),
};
#endif /* QCT Original */

#if defined(CONFIG_MACH_MSM8960_L2S)
static struct led_info pm8921_led_info[] = {
	[0] = {
		.name			= "keyboard-backlight",
		.default_trigger	= "ac-online",
	},
#ifdef CONFIG_LEDS_PM8XXX_RGB
	[1] = {
		.name			= "red",
		.default_trigger	= "ac-online",
	},
	[2] = {
		.name			= "green",
		.default_trigger	= "ac-online",
	},
	[3] = {
		.name			= "button-backlight",
		.default_trigger	= "ac-online",
	},
#endif
};
#else
static struct led_info pm8921_led_info[] = {
#if defined (CONFIG_MACH_MSM8960_L1A)||defined(CONFIG_MACH_MSM8960_L1T)	// added L1T  20120320_sunwoo.lee
	[0] = {
		.name			= "button-backlight",
	},
#else
	[0] = {
		.name			= "button-backlight",
	},
#ifdef CONFIG_LEDS_PM8XXX_RGB
#ifdef CONFIG_MACH_MSM8960_L1v
	[1] = {
			.name			= "pwr-led",
		},

#else
	[1] = {
		.name			= "red",
		.default_trigger	= "ac-online",
	},
	[2] = {
		.name			= "green",
		.default_trigger	= "ac-online",
	},
	[3] = {
		.name			= "blue",
		.default_trigger	= "ac-online",
	},
#endif	
#endif
#endif
};
#endif /* CONFIG_MACH_MSM8960_L2S */

static struct led_platform_data pm8921_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8921_led_info),
	.leds = pm8921_led_info,
};

#if defined(CONFIG_MACH_MSM8960_L2S)
static struct pm8xxx_led_config pm8921_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_LED_KB_LIGHT,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_KB_LED_MAX_CURRENT,
	},
#ifdef CONFIG_LEDS_PM8XXX_RGB
	[1] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
	[2] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
	[3] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_TOUCH_LED_MAX_CURRENT,
	},
#endif
};
#else
static struct pm8xxx_led_config pm8921_led_configs[] = {
#if defined (CONFIG_MACH_MSM8960_L1A)||defined(CONFIG_MACH_MSM8960_L1T)	// added L1T  20120320_sunwoo.lee
	[0] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_KB_LED_MAX_CURRENT,
	},
#else
	[0] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
#ifdef CONFIG_LEDS_PM8XXX_RGB
#ifdef CONFIG_MACH_MSM8960_L1v
	[1] = {
			.id = PM8XXX_ID_LED_1,
			.mode = PM8XXX_LED_MODE_MANUAL,
			.max_current = PM8921_KB_LED_MAX_CURRENT,
		},
#else
	[1] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
	[2] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
	[3] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
#endif	
#endif
#endif
};
#endif /* CONFIG_MACH_MSM8960_L2S */

static struct pm8xxx_led_platform_data pm8xxx_leds_pdata = {
		.led_core = &pm8921_led_core_pdata,
		.configs = pm8921_led_configs,
		.num_configs = ARRAY_SIZE(pm8921_led_configs),
};

static struct pm8xxx_ccadc_platform_data pm8xxx_ccadc_pdata = {
	.r_sense		= 10,
	.calib_delay_ms		= 600000,
};


/**
 * PM8XXX_PWM_DTEST_CHANNEL_NONE shall be used when no LPG
 * channel should be in DTEST mode.
 */

#define PM8XXX_PWM_DTEST_CHANNEL_NONE   (-1)

static struct pm8xxx_pwm_platform_data pm8xxx_pwm_pdata = {
	.dtest_channel	= PM8XXX_PWM_DTEST_CHANNEL_NONE,
};

#ifdef CONFIG_PMIC8XXX_VIBRATOR
static struct pm8xxx_vibrator_platform_data pm8xxx_vibrator_pdata = {
	.initial_vibrate_ms = 500,
/*change max timeout from 15 sec to 30 sec.
 * Max vib time is 25 sec on the All Auto Test. (5 sec is just buffer)
 * 2012-03-07, donggyun.kim@lge.com,
 */
	.max_timeout_ms = 30000,
	.min_timeout_ms = MIN_TIMEOUT_MS,
	.level_mV = 3100,
};
#endif

#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
static int qcoin_power_set(struct device *dev, int level)
{
	int rc;
	u8 val;
	printk("[%s] Motor Level is %d\n", __func__, level);
	if (level > 0) {
		rc = pm8xxx_readb(dev, VIB_DRV, &val);
		if (rc < 0) {
			printk("[%s] Vibrator read error on pmic\n", __func__);
			return rc;
		}
		val |= ((level << VIB_DRV_SEL_SHIFT) & VIB_DRV_SEL_MASK);
		rc = pm8xxx_writeb(dev, VIB_DRV, val);
		if (rc < 0) {
			printk("[%s] Vibrator write error on pmic\n", __func__);
			return rc;
		}
	} else {
		pm8xxx_readb(dev, VIB_DRV, &val);
		val &= ~VIB_DRV_SEL_MASK;
		rc = pm8xxx_writeb(dev, VIB_DRV, val);
		if (rc < 0) {
			printk("[%s] Vibrator write error on pmic\n", __func__);
			return rc;
		}
	}
	return 0;
}

static struct direct_qcoin_platform_data pm8xxx_qcoin_pdata = {
	.enable_status = 0,
	.amp = MOTOR_AMP,
	.power_set = qcoin_power_set,
	.high_max = VIB_HIGH_MAX,
	.low_max = VIB_LOW_MAX,
	.low_min = VIB_LOW_MIN,
};
#endif /* CONFIG_LGE_DIRECT_QCOIN_VIBRATOR */

static struct pm8921_platform_data pm8921_platform_data __devinitdata = {
	.irq_pdata		= &pm8xxx_irq_pdata,
	.gpio_pdata		= &pm8xxx_gpio_pdata,
	.mpp_pdata		= &pm8xxx_mpp_pdata,
	.rtc_pdata              = &pm8xxx_rtc_pdata,
	.pwrkey_pdata		= &pm8xxx_pwrkey_pdata,
	.keypad_pdata		= &keypad_data,
	.misc_pdata		= &pm8xxx_misc_pdata,
	.regulator_pdatas	= msm_pm8921_regulator_pdata,
	.charger_pdata		= &pm8921_chg_pdata,
	.bms_pdata		= &pm8921_bms_pdata,
	.adc_pdata		= &pm8xxx_adc_pdata,
	.leds_pdata		= &pm8xxx_leds_pdata,
	.ccadc_pdata		= &pm8xxx_ccadc_pdata,
	.pwm_pdata		= &pm8xxx_pwm_pdata,
#ifdef CONFIG_PMIC8XXX_VIBRATOR
	.vibrator_pdata 	= &pm8xxx_vibrator_pdata,
#endif
#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
	.pm8xxx_qcoin_pdata     = &pm8xxx_qcoin_pdata,
#endif
};

static struct msm_ssbi_platform_data msm8960_ssbi_pm8921_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name			= "pm8921-core",
		.platform_data		= &pm8921_platform_data,
	},
};



#ifdef CONFIG_LGE_PM

#ifdef CONFIG_BATTERY_MAX17043

/* BEGIN: hiro.kwon@lge.com 2011-12-22 RCOMP update when the temperature of the cell changes */
#if defined (CONFIG_MACH_MSM8960_D1LA)|| defined(CONFIG_MACH_MSM8960_L1A)||defined(CONFIG_MACH_MSM8960_L1T)	// added L1T  20120320_sunwoo.lee
static struct max17043_platform_data max17043_pdata = {
	.starting_rcomp	=	0x60,
	.temp_co_hot	=	-125,
	.temp_co_cold	=	-3425,
};
#elif defined (CONFIG_MACH_MSM8960_D1LV)
static struct max17043_platform_data max17043_pdata = {
	.starting_rcomp	=	0x4D,
	.temp_co_hot	=	-55,
	.temp_co_cold	=	-535,
};
#elif defined(CONFIG_MACH_MSM8960_L1m)
/* START: dukyong.kim@lge.com 2012-01-16 Implement Quickstart for Test Mode and SOC Accurency */
static struct max17043_ocv_to_soc_data cal_data[] = {
	{3470,	 0}, {3610,	5}, {3660,	 5}, {3668,	 11},
	{3715,	16}, {3725,	24}, {3772,	 25}, {3781,	 31},
	{3794,	37}, {3853,	49}, {3915,	 59}, {4019,	 70},
	{4110,	81}, {4219,	92}, {4306,	 100}, {-1,	-1},

};
/* END: dukyong.kim@lge.com 2012-01-16 */
static struct max17043_platform_data max17043_pdata = {
	.starting_rcomp	=	0x44,
	.temp_co_hot	=	0,
	.temp_co_cold	=	-3925,
	.soc_cal_data	=	cal_data,
};
#elif defined(CONFIG_MACH_MSM8960_L1v)
static struct max17043_platform_data max17043_pdata = {
        .starting_rcomp[0] = 0x63,
        .temp_co_hot[0]    = -6,
        .temp_co_cold[0]   =  -74,
        .starting_rcomp[1] = 0x58,
        .temp_co_hot[1]    = -8,
        .temp_co_cold[1]   =  -5325,
};
#else
static struct max17043_platform_data max17043_pdata = {
	.starting_rcomp	=	0x4D,
	.temp_co_hot	=	-55,
	.temp_co_cold	=	-535,
};

#endif
/* END: hiro.kwon@lge.com 2011-12-22 */

#endif // CONFIG_BATTERY_MAX17043

#ifdef CONFIG_LGE_WIRELESS_CHARGER_BQ24160

static struct bq24160_platform_data bq24160_data = {
	.tmr_rst = 0b1,
	.supply_sel = 0b1,

	.reset = 0b0,			/* reset all reg to default values : 0 => no effect, 1 => reset all */
	.iusblimit = 0b011,		/* usb current limit : 100 => USB3.0 host 900mA current limit */
	.enstat = 0b1,			/* enable STAT : 0 => disable, 1 => enable */
	.te = 0b1,				/* enable charger termination : 0 => disable, 1 => enable */
	.ce = 0b0,				/* charger enable : 0 => enable, 1 => disable */
	.hz_mode = 0b0,			/* high impedance mode : 0 => no high impedance, 1 => high impedance */

	.vbatt_reg = 0b101011,	/* battery regulation voltage : 101011 => 4.36V*/
	.inlimit_in = 0b0,		/* input limit for IN input : 0 => 1.5A, 1 => 2.5A */
	.dpdm_en = 0b0,			/* D+/D- detention : 0 => normal state, 1 => force D+/D- detection */

	.chgcrnt = 0b00001,		/* charge current : 00010 => 700mA */
	.termcrnt = 0b011,		/* termination current sense : 011 => 200mA*/

	.minsys_stat = 0b0,		/* minimum system voltage mode : 0 => not active, 1 => active */
	.dpm_stat = 0b1,		/* Vin-DPM mode : 0 => not active, 1 => active */
	.vindpm_usb = 0b101,	/* usb input Vin-dpm voltage : 000 => 4.2V */
	.vindpm_in = 0b101,		/* IN input Vin-dpm voltage */

	.tmr2x_en = 0b1,		/* timer slowed by 2x */
	.safety_tmr = 0b01,		/* safety timer : 01 => 6hrs*/
	.ts_en = 0b0,			/* ts function enable */
	.ts_fault = 0b00,		/* ts fault mode */

//	.valid_n_gpio = PM8921_MPP_PM_TO_SYS(WIRELESS_CHG_MPP_11),
//	.chg_detection_config =  NULL,//bq24160_detection_setup,
};

static struct i2c_board_info msm_i2c_gsbi5_info[] __initdata= {
#ifdef CONFIG_BATTERY_MAX17043
	{
		I2C_BOARD_INFO("max17043", MAX17043_FUELGAUGE_I2C_ADDR),
		.irq = PM8921_GPIO_PM_TO_SYS(MAX17043_FUELGAUGE_GPIO_IRQ),
		/* BEGIN: hiro.kwon@lge.com 2011-12-22 RCOMP update when the temperature of the cell changes */
		.platform_data = (void *)&max17043_pdata,
		/* END: hiro.kwon@lge.com 2011-12-22 */
	},
#endif
	{
		I2C_BOARD_INFO("bq24160", 0x6B),
		.irq = PM8921_IRQ_BASE + PM8921_CABLE_IRQ,
		.platform_data = (void*)&bq24160_data,
	}
};

static struct i2c_registry d1l_i2c_pm_subsystem __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8960_GSBI5_QUP_I2C_BUS_ID,
		msm_i2c_gsbi5_info,
		ARRAY_SIZE(msm_i2c_gsbi5_info),
};
#else // CONFIG_LGE_WIRELESS_CHARGER_BQ24160

#ifdef CONFIG_BATTERY_MAX17043
static struct i2c_board_info max17043_i2c_info[] = {
	{
		I2C_BOARD_INFO("max17043", MAX17043_FUELGAUGE_I2C_ADDR),
		.irq = PM8921_GPIO_PM_TO_SYS(MAX17043_FUELGAUGE_GPIO_IRQ),
		/* BEGIN: hiro.kwon@lge.com 2011-12-22 RCOMP update when the temperature of the cell changes */
		.platform_data = (void *)&max17043_pdata,
		/* END: hiro.kwon@lge.com 2011-12-22 */
	}
};

static struct i2c_registry d1l_i2c_pm_subsystem __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8960_GSBI5_QUP_I2C_BUS_ID,
		max17043_i2c_info,
		ARRAY_SIZE(max17043_i2c_info),
};
#endif //CONFIG_BATTERY_MAX17043

#endif

#if defined(CONFIG_BATTERY_MAX17043) || defined(CONFIG_LGE_WIRELESS_CHARGER_BQ24160)
static void __init lge_add_i2c_pm_subsystem_devices(void)
{
#ifdef CONFIG_BATTERY_MAX17043

	/* LGE_CHANGE
	 * 2011-12-03, hyuncheol0.kim@lge.com
	 * Work-around code to support old H/W revision.
	 */
#if defined (CONFIG_MACH_MSM8960_D1LA)
	if (lge_get_board_revno() >= HW_REV_A &&
		lge_get_board_revno() <= HW_REV_B)
		max17043_i2c_info[0].irq =
			PM8921_GPIO_PM_TO_SYS(MAX17043_FUELGAUGE_GPIO_OLD_IRQ);
#elif defined (CONFIG_MACH_MSM8960_D1LV)
	if (lge_get_board_revno() >= HW_REV_B &&
		lge_get_board_revno() <= HW_REV_C)
		max17043_i2c_info[0].irq =
			PM8921_GPIO_PM_TO_SYS(MAX17043_FUELGAUGE_GPIO_OLD_IRQ);
#endif

	/* LGE_CHANGE
	 * 2012-02-23, hyoungjin.kim@lge.com
	 * Work-around code to support old H/W revision.
	 */
#if defined (CONFIG_MACH_MSM8960_L1A)
	if (lge_get_board_revno() == HW_REV_A) {
		pm8921_led_configs[0].id = PM8XXX_ID_LED_KB_LIGHT;
	} else if (lge_get_board_revno() >= HW_REV_B) {
		pm8921_led_configs[0].id = PM8XXX_ID_LED_1;
	}
#endif

#endif //CONFIG_BATTERY_MAX17043

	/* Run the array and install devices as appropriate */
	i2c_register_board_info(d1l_i2c_pm_subsystem.bus,
				d1l_i2c_pm_subsystem.info,
				d1l_i2c_pm_subsystem.len);
}
#endif //CONFIG_BATTERY_MAX17043 CONFIG_LGE_WIRELESS_CHARGER_BQ24160

#endif // CONFIG_LGE_PM

/* common function */
void __init lge_add_pm_devices(void)
{
	printk(KERN_INFO "Unified Kernel for LGE_PM\n");
#if defined(CONFIG_BATTERY_MAX17043) || defined(CONFIG_LGE_WIRELESS_CHARGER_BQ24160)
	lge_add_i2c_pm_subsystem_devices();
#else
	;
#endif
#if defined(CONFIG_MACH_MSM8960_L0)
	if (lge_get_board_revno() >= HW_REV_D) {
		pm8921_led_configs[0].id = PM8XXX_ID_LED_0;
	} else {
		pm8921_led_configs[0].id = PM8XXX_ID_LED_2;
	}
#endif
#if defined(CONFIG_MACH_MSM8960_L1m)
	pm8921_led_configs[0].id = PM8XXX_ID_LED_0;
#endif

	/* LGE_CHANGE
	 * 2012-09-06, yuntaek.kim@lge.com
	 * Work-around code to support H/W revision.
	 */
#if defined(CONFIG_MACH_MSM8960_L1v)
	if (lge_get_board_revno() >= HW_REV_C){
		pm8921_led_configs[0].id = PM8XXX_ID_LED_1;
		pm8921_led_configs[1].id = PM8XXX_ID_LED_0;
	}
	else{
		pm8921_led_configs[0].id = PM8XXX_ID_LED_0;
	}
#endif
}

void __init msm8960_init_pmic(void)
{
	pmic_reset_irq = PM8921_IRQ_BASE + PM8921_RESOUT_IRQ;
	msm8960_device_ssbi_pmic.dev.platform_data =
				&msm8960_ssbi_pm8921_pdata;
	pm8921_platform_data.num_regulators = msm_pm8921_regulator_pdata_len;
	if (machine_is_msm8960_liquid()) {
		pm8921_platform_data.bms_pdata->battery_type = BATT_DESAY;
	} else if (machine_is_msm8960_mtp()) {
		pm8921_platform_data.bms_pdata->battery_type = BATT_PALLADIUM;
	} else if (machine_is_msm8960_cdp()) {
#ifndef CONFIG_LGE_PM
		pm8921_chg_pdata.has_dc_supply = true;
#else
		;
#endif
	}

	if (machine_is_msm8960_fluid())
		pm8921_bms_pdata.rconn_mohm = 20;
}
