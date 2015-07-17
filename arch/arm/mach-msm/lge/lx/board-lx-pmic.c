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
#ifdef CONFIG_MACH_LGE
#include "board-lx.h"
#include <mach/board_lge.h>
#include <linux/i2c.h>
#ifdef CONFIG_BATTERY_MAX17043
#include <linux/max17043_fuelgauge.h>
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#endif
#endif

#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
#include <linux/mfd/pm8xxx/direct_qcoin.h>
#include "../../../../../drivers/staging/android/timed_output.h"
#endif

#ifdef CONFIG_LGE_PM
#include <linux/gpio.h>
#endif

#include CONFIG_BOARD_HEADER_FILE

struct pm8xxx_gpio_init {
	unsigned			gpio;
	struct pm_gpio			config;
};

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
#ifdef CONFIG_MACH_LGE
	LGE_PM8921_GPIO_INITDATA,
#endif
	PM8XXX_GPIO_OUTPUT_VIN(6, 1, PM_GPIO_VIN_VPH),	 /* MHL power EN_N */
	PM8XXX_GPIO_DISABLE(7),				 /* Disable NFC */
	PM8XXX_GPIO_INPUT(16,	    PM_GPIO_PULL_UP_30), /* SD_CARD_WP */
    /* External regulator shared by display and touchscreen on LiQUID */
#if !defined(CONFIG_MACH_LGE)
	PM8XXX_GPIO_OUTPUT(17,	    0),			 /* DISP 3.3 V Boost */
	PM8XXX_GPIO_OUTPUT(18,	0),	/* TABLA SPKR_LEFT_EN=off */
#endif
	PM8XXX_GPIO_OUTPUT(19,	0),	/* TABLA SPKR_RIGHT_EN=off */
	PM8XXX_GPIO_DISABLE(22),			 /* Disable NFC */

	PM8XXX_GPIO_INPUT(26,	    PM_GPIO_PULL_UP_30), /* SD_CARD_DET_N */
	PM8XXX_GPIO_OUTPUT(43, 1),                       /* DISP_RESET_N */

#ifdef CONFIG_SWITCH_FSA8008
	/* add by ehgrace.kim@lge.com for headset */
	PM8XXX_GPIO_OUTPUT(31, 0),                    /* EAR_MIC_EN_35D */
	PM8XXX_GPIO_INPUT(32, PM_GPIO_PULL_NO),       /* EARPOL_DETECT_35D */
#endif

#ifdef CONFIG_LGE_WIRELESS_CHARGER
	PM8XXX_GPIO_OUTPUT(42, 0),                    /* CHG_STAT */
	PM8XXX_GPIO_INPUT(25, PM_GPIO_PULL_NO),//PM_GPIO_PULL_UP_30),    /* ACTIVE_N */
#else
	PM8XXX_GPIO_OUTPUT(42, 0),                      /* USB 5V reg enable */
	PM8XXX_GPIO_OUTPUT_FUNC(25, 0, PM_GPIO_FUNC_2),	 /* TN_CLK */
#endif
	/* TABLA CODEC RESET */
	PM8XXX_GPIO_OUTPUT_STRENGTH(34, 1, PM_GPIO_STRENGTH_MED)
};

/* 120511 mansu.lee@lge.com N/C GPIOs setting for sleep current */
#if defined(CONFIG_LGE_PM)
static int pm8921_nc_gpios[] __initdata = {
	LGE_PM8921_NC_GPIO_INITDATA,
};
#endif
/* 120511 mansu.lee@lge.com */

/* Initial PM8921 MPP configurations */
static struct pm8xxx_mpp_init pm8921_mpps[] __initdata = {
	/* External 5V regulator enable; shared by HDMI and USB_OTG switches. */
	PM8XXX_MPP_INIT(7, D_INPUT, PM8921_MPP_DIG_LEVEL_VPH, DIN_TO_INT),
	PM8XXX_MPP_INIT(PM8XXX_AMUX_MPP_8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8,
								DOUT_CTRL_LOW),

#ifdef CONFIG_SWITCH_FSA8008
	/* add by ehgrace.kim@lge.com for headset */
	PM8XXX_MPP_INIT(4, D_INPUT, PM8921_MPP_DIG_LEVEL_S4, DIN_TO_INT),
	PM8XXX_MPP_INIT(9, D_INPUT, PM8921_MPP_DIG_LEVEL_S4, DIN_TO_INT),
#endif
};

void __init msm8960_pm8921_gpio_mpp_init(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(pm8921_gpios); i++) {
		rc = pm8xxx_gpio_config(pm8921_gpios[i].gpio,
					&pm8921_gpios[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pm8921_mpps); i++) {
		rc = pm8xxx_mpp_config(pm8921_mpps[i].mpp,
					&pm8921_mpps[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_mpp_config: rc=%d\n", __func__, rc);
			break;
		}
	}
/* 120511 mansu.lee@lge.com N/C GPIOs setting for sleep current */
#if defined(CONFIG_LGE_PM)
		for (i = 0; i < ARRAY_SIZE(pm8921_nc_gpios); i++) {
			gpio_tlmm_config(GPIO_CFG(pm8921_nc_gpios[i], 0,
				GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		}
#endif
/* 120511 mansu.lee@lge.com */
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
#ifdef CONFIG_LGE_PM
	{"pcb_rev", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"cdma_therm", ADC_MPP_1_AMUX8, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_XOTHERM},
	{"usb_id", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
#endif
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
	.rtc_write_enable       = false,
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8xxx_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us	= 15625,
	.wakeup			= 1,
};

/* Rotate lock key is not available so use F1 */
#define KEY_ROTATE_LOCK KEY_F1

static const unsigned int keymap_liquid[] = {
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_VOLUMEDOWN),
	KEY(1, 3, KEY_ROTATE_LOCK),
	KEY(1, 4, KEY_HOME),
};

static struct matrix_keymap_data keymap_data_liquid = {
	.keymap_size    = ARRAY_SIZE(keymap_liquid),
	.keymap         = keymap_liquid,
};

static struct pm8xxx_keypad_platform_data keypad_data_liquid = {
	.input_name             = "keypad_8960_liquid",
	.input_phys_device      = "keypad_8960/input0",
	.num_rows               = 2,
	.num_cols               = 5,
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &keymap_data_liquid,
};

#ifdef CONFIG_MACH_LGE
static const unsigned int keymap[] = { LGE_KEY_MAP };
#else
static const unsigned int keymap[] = {
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_VOLUMEDOWN),
	KEY(0, 2, KEY_CAMERA_SNAPSHOT),
	KEY(0, 3, KEY_CAMERA_FOCUS),
};
#endif

static struct matrix_keymap_data keymap_data = {
	.keymap_size    = ARRAY_SIZE(keymap),
	.keymap         = keymap,
};

static struct pm8xxx_keypad_platform_data keypad_data = {
	.input_name             = "keypad-8960",
	.input_phys_device      = "keypad-8960/input0",
#ifdef CONFIG_MACH_LGE
	.num_rows               = 5,
	.num_cols               = 5,
#else
	.num_rows               = 1,
	.num_cols               = 5,
#endif
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &keymap_data,
};

static const unsigned int keymap_sim[] = {
	KEY(0, 0, KEY_7),
	KEY(0, 1, KEY_DOWN),
	KEY(0, 2, KEY_UP),
	KEY(0, 3, KEY_RIGHT),
	KEY(0, 4, KEY_ENTER),
	KEY(0, 5, KEY_L),
	KEY(0, 6, KEY_BACK),
	KEY(0, 7, KEY_M),

	KEY(1, 0, KEY_LEFT),
	KEY(1, 1, KEY_SEND),
	KEY(1, 2, KEY_1),
	KEY(1, 3, KEY_4),
	KEY(1, 4, KEY_CLEAR),
	KEY(1, 5, KEY_MSDOS),
	KEY(1, 6, KEY_SPACE),
	KEY(1, 7, KEY_COMMA),

	KEY(2, 0, KEY_6),
	KEY(2, 1, KEY_5),
	KEY(2, 2, KEY_8),
	KEY(2, 3, KEY_3),
	KEY(2, 4, KEY_NUMERIC_STAR),
	KEY(2, 5, KEY_UP),
	KEY(2, 6, KEY_DOWN),
	KEY(2, 7, KEY_LEFTSHIFT),

	KEY(3, 0, KEY_9),
	KEY(3, 1, KEY_NUMERIC_POUND),
	KEY(3, 2, KEY_0),
	KEY(3, 3, KEY_2),
	KEY(3, 4, KEY_SLEEP),
	KEY(3, 5, KEY_F1),
	KEY(3, 6, KEY_F2),
	KEY(3, 7, KEY_F3),

	KEY(4, 0, KEY_BACK),
	KEY(4, 1, KEY_HOME),
	KEY(4, 2, KEY_MENU),
	KEY(4, 3, KEY_VOLUMEUP),
	KEY(4, 4, KEY_VOLUMEDOWN),
	KEY(4, 5, KEY_F4),
	KEY(4, 6, KEY_F5),
	KEY(4, 7, KEY_F6),

	KEY(5, 0, KEY_R),
	KEY(5, 1, KEY_T),
	KEY(5, 2, KEY_Y),
	KEY(5, 3, KEY_LEFTALT),
	KEY(5, 4, KEY_KPENTER),
	KEY(5, 5, KEY_Q),
	KEY(5, 6, KEY_W),
	KEY(5, 7, KEY_E),

	KEY(6, 0, KEY_F),
	KEY(6, 1, KEY_G),
	KEY(6, 2, KEY_H),
	KEY(6, 3, KEY_CAPSLOCK),
	KEY(6, 4, KEY_PAGEUP),
	KEY(6, 5, KEY_A),
	KEY(6, 6, KEY_S),
	KEY(6, 7, KEY_D),

	KEY(7, 0, KEY_V),
	KEY(7, 1, KEY_B),
	KEY(7, 2, KEY_N),
	KEY(7, 3, KEY_MENU),
	KEY(7, 4, KEY_PAGEDOWN),
	KEY(7, 5, KEY_Z),
	KEY(7, 6, KEY_X),
	KEY(7, 7, KEY_C),

	KEY(8, 0, KEY_P),
	KEY(8, 1, KEY_J),
	KEY(8, 2, KEY_K),
	KEY(8, 3, KEY_INSERT),
	KEY(8, 4, KEY_LINEFEED),
	KEY(8, 5, KEY_U),
	KEY(8, 6, KEY_I),
	KEY(8, 7, KEY_O),

	KEY(9, 0, KEY_4),
	KEY(9, 1, KEY_5),
	KEY(9, 2, KEY_6),
	KEY(9, 3, KEY_7),
	KEY(9, 4, KEY_8),
	KEY(9, 5, KEY_1),
	KEY(9, 6, KEY_2),
	KEY(9, 7, KEY_3),

	KEY(10, 0, KEY_F7),
	KEY(10, 1, KEY_F8),
	KEY(10, 2, KEY_F9),
	KEY(10, 3, KEY_F10),
	KEY(10, 4, KEY_FN),
	KEY(10, 5, KEY_9),
	KEY(10, 6, KEY_0),
	KEY(10, 7, KEY_DOT),

	KEY(11, 0, KEY_LEFTCTRL),
	KEY(11, 1, KEY_F11),
	KEY(11, 2, KEY_ENTER),
	KEY(11, 3, KEY_SEARCH),
	KEY(11, 4, KEY_DELETE),
	KEY(11, 5, KEY_RIGHT),
	KEY(11, 6, KEY_LEFT),
	KEY(11, 7, KEY_RIGHTSHIFT),
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_VOLUMEDOWN),
	KEY(0, 2, KEY_CAMERA_SNAPSHOT),
	KEY(0, 3, KEY_CAMERA_FOCUS),
};

static struct matrix_keymap_data keymap_data_sim = {
	.keymap_size    = ARRAY_SIZE(keymap_sim),
	.keymap         = keymap_sim,
};

static struct pm8xxx_keypad_platform_data keypad_data_sim = {
	.input_name             = "keypad_8960",
	.input_phys_device      = "keypad_8960/input0",
	.num_rows               = 12,
	.num_cols               = 8,
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &keymap_data_sim,
};

static int pm8921_therm_mitigation[] = {
	1100,
	700,
	600,
	325,
};

/* LGE_UPDATE_S, 4.35 High Cell Voltage, roy.park@lge.com, 2011/08/17 -->[ */
#ifdef CONFIG_LGE_PM_435V_BATT
#define MAX_VOLTAGE_MV		4350
#else
#define MAX_VOLTAGE_MV		4200
#endif

#define CHG_TERM_MA            100

static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
#ifdef CONFIG_LGE_PM_435V_BATT
	.safety_time			= 480,
	.update_time			= 60000,
	.max_voltage			= MAX_VOLTAGE_MV,
	.min_voltage			= 3500,
#else
	.safety_time			= 180,
	.update_time			= 1,
	.max_voltage			= MAX_VOLTAGE_MV,
	.min_voltage			= 3200,
#endif
#ifndef CONFIG_LGE_PM
	.uvd_thresh_voltage	= 4050,
#endif /* QCT ORIGIN */
	.term_current			= CHG_TERM_MA,
#if defined(CONFIG_MACH_MSM8960_L0)
	.resume_voltage_delta	= 80,		/* resume voltage 4.28V = 4.36V(vddmax) - 0.08V(delta) */
	.term_current			= 80,
#else
	.resume_voltage_delta	= 70,		/* resume voltage 4.28V = 4.35V(vdd_max) -0.07V(delta) */
	.term_current			= 100,
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	.cool_temp			= 0,	/* from 10, */
	.warm_temp			= 0,	/* from 40, */
	.cold_thr			= 1,	/* 80% */
	.hot_thr			= 0,	/* 25% */
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
#ifdef CONFIG_LGE_PM_435V_BATT
	.cool_temp			= 0,	/* from 10 */
	.warm_temp			= 45,	/* from 40 */
#else
	.cool_temp			= -10, /* from 10 */
	.warm_temp			= 45,  /* from 40 */
#endif
#endif
	.temp_check_period		= 1,
#ifdef CONFIG_LGE_PM_435V_BATT
	.max_bat_chg_current	= IBAT_CURRENT,
#else
	.max_bat_chg_current	= 1000, /* 70% of eoc. 1250mA from 400 */
#endif
	.cool_bat_chg_current	= 350,
	.warm_bat_chg_current	= 350,
	.cool_bat_voltage		= 4100,
	.warm_bat_voltage		= 4100,
	.thermal_mitigation		= pm8921_therm_mitigation,
	.thermal_levels			= ARRAY_SIZE(pm8921_therm_mitigation),
#ifndef CONFIG_LGE_PM_435V_BATT
	.rconn_mohm		= 18,
#endif
#ifdef CONFIG_MACH_LGE
	.batt_id_gpio		= BATT_ID_GPIO,			/* No. of msm gpio for battery id */
	.batt_id_pu_gpio	= BATT_ID_PULLUP_GPIO,	/* No. of msm gpio for battery id pull up */
#endif
#ifdef CONFIG_LGE_PM_435V_BATT
	.weak_voltage		= 3200,
	.weak_current		= 325,
#endif
};

static struct pm8xxx_misc_platform_data pm8xxx_misc_pdata = {
	.priority		= 0,
};

static struct pm8921_bms_platform_data pm8921_bms_pdata __devinitdata = {
	.battery_type	= BATT_UNKNOWN,
	.r_sense		= 10,
	.v_cutoff                       = 3400,
	.max_voltage_uv		= MAX_VOLTAGE_MV * 1000,
#if defined(CONFIG_MACH_MSM8960_L1A) || defined(CONFIG_MACH_MSM8960_L1E_EVE_GB)
	.rconn_mohm		= 65,
#elif defined(CONFIG_MACH_MSM8960_L0)
	.rconn_mohm		= 40,
#else /* QCT ORG */
	.rconn_mohm		= 18,
#endif
	.shutdown_soc_valid_limit       = 20,
	.adjust_soc_low_threshold       = 25,
	.chg_term_ua                    = CHG_TERM_MA * 1000,
};

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

#ifdef CONFIG_MACH_LGE
static struct led_info pm8921_led_info[] = {
#if defined (CONFIG_MACH_MSM8960_L1A) || defined (CONFIG_MACH_MSM8960_L1E_EVE_GB)
	[0] = {
		.name			= "button-backlight",
	},
#else
	[0] = {
		.name			= "pwr-led",
	},
	[1] = {
		.name			= "button-backlight",
	},
#endif
};
#else
static struct led_info pm8921_led_info[] = {
	[0] = {
		.name			= "led:battery_charging",
		.default_trigger	= "battery-charging",
	},
	[1] = {
		.name			= "led:battery_full",
		.default_trigger	= "battery-full",
	},
};
#endif

static struct led_platform_data pm8921_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8921_led_info),
	.leds = pm8921_led_info,
};

#ifdef CONFIG_MACH_LGE
static struct pm8xxx_led_config pm8921_led_configs[] = {
#if defined (CONFIG_MACH_MSM8960_L1A) || defined (CONFIG_MACH_MSM8960_L1E_EVE_GB)
	[0] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = LGE_PM8921_KB_LED_MAX_CURRENT,
	},
#else
	[0] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = LGE_PM8921_LC_LED_MAX_CURRENT,
	},
	[1] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = LGE_PM8921_KB_LED_MAX_CURRENT,
	},
#endif
};
#else
static int pm8921_led0_pwm_duty_pcts[56] = {
		1, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		40, 44, 46, 52, 56, 60, 64, 68, 72, 76,
		80, 84, 88, 92, 96, 100, 100, 100, 98, 95,
		92, 88, 84, 82, 78, 74, 70, 66, 62, 58,
		58, 54, 50, 48, 42, 38, 34, 30, 26, 22,
		14, 10, 6, 4, 1
};

/*
 * Note: There is a bug in LPG module that results in incorrect
 * behavior of pattern when LUT index 0 is used. So effectively
 * there are 63 usable LUT entries.
 */
static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = 1,
};

static struct pm8xxx_led_config pm8921_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_duty_cycles,
	},
	[1] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
	},
};
#endif

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
#if defined (CONFIG_MACH_MSM8960_D1LA)
/* START: mansu.lee@lge.com 2012-01-16 Implement Quickstart for Test Mode and SOC Accurency */
static struct max17043_ocv_to_soc_data cal_data[] = {
	{3400,	 0}, {3660,	 2}, {3711,	 15}, {3757,	 19},
	{3770,	26}, {3778,	31}, {3795, 	 36}, {3821,	 45},
	{3855,	54}, {3910,	60}, {3971,	 67}, {4023,	 74},
	{4080,	80}, {4190,	91}, {4310,	100}, {-1,	 -1},
};
/* END: mansu.lee@lge.com 2012-01-16 */
static struct max17043_platform_data max17043_pdata = {
	.starting_rcomp	=	0x44,/* Default Value for BL-53QH Battery */
	.temp_co_hot	=	-125,
	.temp_co_cold	=	-3425,
	.soc_cal_data	=	cal_data,
};
#elif defined (CONFIG_MACH_MSM8960_D1LV)
/* START: mansu.lee@lge.com 2012-01-16 Implement Quickstart for Test Mode and SOC Accurency */
static struct max17043_ocv_to_soc_data cal_data[] = {
	{3400,	 0}, {3691,	13}, {3732,	 15}, {3740,	 18},
	{3743,	23}, {3757,	23}, {3760,	 27}, {3762,	 31},
	{3806,	39}, {3866,	54}, {3973,	 66}, {4106,	 81},
	{4197,	91}, {4307,	100}, {-1,	-1},

};
/* END: mansu.lee@lge.com 2012-01-16 */
static struct max17043_platform_data max17043_pdata = {
	.starting_rcomp	=	0x44,
	.temp_co_hot	=	0,
	.temp_co_cold	=	-3925,
	.soc_cal_data	=	cal_data,
};
#endif
/* END: hiro.kwon@lge.com 2011-12-22 */

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

static void __init lge_add_i2c_pm_subsystem_devices(void)
{
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
	/* Run the array and install devices as appropriate */
	i2c_register_board_info(d1l_i2c_pm_subsystem.bus,
				d1l_i2c_pm_subsystem.info,
				d1l_i2c_pm_subsystem.len);
}

#endif // CONFIG_BATTERY_MAX17043
#endif // CONFIG_LGE_PM

/* common function */
void __init lge_add_pm_devices(void)
{
	printk(KERN_INFO "Unified Kernel for LGE_PM\n");
#ifdef CONFIG_BATTERY_MAX17043
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
#elif defined(CONFIG_MACH_MSM8960_L1A)
	if (lge_get_board_revno() == HW_REV_A) {
		pm8921_led_configs[0].id = PM8XXX_ID_LED_KB_LIGHT;
	} else if (lge_get_board_revno() <= HW_REV_1_0) {
		pm8921_led_configs[0].id = PM8XXX_ID_LED_1;
	} else {
		pm8921_led_configs[0].id = PM8XXX_ID_LED_0;
	}
#elif defined(CONFIG_MACH_MSM8960_L1E_EVE_GB)/* edward1.kim@lge.com add l1e feature */
	pm8921_led_configs[0].id = PM8XXX_ID_LED_0;
#endif
}
void __init msm8960_init_pmic(void)
{
	pmic_reset_irq = PM8921_IRQ_BASE + PM8921_RESOUT_IRQ;
	msm8960_device_ssbi_pmic.dev.platform_data =
				&msm8960_ssbi_pm8921_pdata;
	pm8921_platform_data.num_regulators = msm_pm8921_regulator_pdata_len;

	/* Simulator supports a QWERTY keypad */
	if (machine_is_msm8960_sim())
		pm8921_platform_data.keypad_pdata = &keypad_data_sim;

	if (machine_is_msm8960_liquid()) {
		pm8921_platform_data.keypad_pdata = &keypad_data_liquid;
		pm8921_platform_data.leds_pdata = &pm8xxx_leds_pdata_liquid;
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
