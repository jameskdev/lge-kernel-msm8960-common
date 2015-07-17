/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_LGE_BOARD_L_DCM_H
#define __ASM_ARCH_MSM_LGE_BOARD_L_DCM_H

#define VENDOR_LGE			0x1004

#ifdef CONFIG_MACH_LGE
#include <linux/regulator/msm-gpio-regulator.h>
#endif
#include <linux/mfd/pm8xxx/pm8921.h>
#define LCD_RESOLUTION_X  496
#define LCD_RESOLUTION_Y  800
#define MSM_FB_WIDTH_MM   52
#define MSM_FB_HEIGHT_MM  86
/* BEGIN : jooyeong.lee@lge.com 2012-02-27 Change the charger_temp_scenario */
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
enum {
	THERM_M10,
	THERM_M5,
	THERM_42,
	THERM_45,
	THERM_55,
	THERM_57,
	THERM_60,
	THERM_65,
	THERM_LAST,
};

enum {
	DISCHG_BATT_TEMP_OVER_60,
	DISCHG_BATT_TEMP_57_60,
	DISCHG_BATT_TEMP_UNDER_57,
	CHG_BATT_TEMP_LEVEL_1, // OVER_55
	CHG_BATT_TEMP_LEVEL_2, // 46_55
	CHG_BATT_TEMP_LEVEL_3, // 42_45
	CHG_BATT_TEMP_LEVEL_4, // 0_41
	CHG_BATT_TEMP_LEVEL_5, // M10_0
	CHG_BATT_TEMP_LEVEL_6, // UNDER_M10
};

enum {
	DISCHG_BATT_NORMAL_STATE,
	DISCHG_BATT_WARNING_STATE,
	DISCHG_BATT_POWEROFF_STATE,
	CHG_BATT_NORMAL_STATE,
	CHG_BATT_DC_CURRENT_STATE,
	CHG_BATT_WARNING_STATE,
	CHG_BATT_STOP_CHARGING_STATE,
};
#endif
/* END : jooyeong.lee@lge.com 2012-02-27 */
#define TS_X_MIN          0
#define TS_X_MAX          720
#define TS_Y_MIN          0
#define TS_Y_MAX          1280

#define USB_SERIAL_NUMBER						"LGE_ANDROID_L_DCM_DEV"

/* CONFIG_LGE_AUDIO
 * Add devide amp parameters
 * 2011-11-30, leia.shin@lge.com
 */
#define AGC_COMPRESIION_RATE		0
#define AGC_OUTPUT_LIMITER_DISABLE	1
#define AGC_FIXED_GAIN			14

#ifdef CONFIG_LGE_AUDIO_TPA2028D
	/* Add the I2C driver for Audio Amp, ehgrace.kim@lge.cim, 06/13/2011 */
#define MSM_8960_GSBI9_QUP_I2C_BUS_ID 9
#endif
#define LGE_KEY_MAP \
	KEY(0, 0, KEY_VOLUMEDOWN),\
	KEY(0, 1, KEY_VOLUMEUP)

#define LGE_PM8921_GPIO_INITDATA \
	PM8XXX_GPIO_DISABLE(6),						/* Disable unused */ \
	PM8XXX_GPIO_DISABLE(7),						/* Disable NFC */ \
	PM8XXX_GPIO_INPUT(16, PM_GPIO_PULL_UP_30),	/* SD_CARD_WP */ \
	PM8XXX_GPIO_INPUT(18, PM_GPIO_PULL_UP_30),	/* Cradle detection gpio */ \
	PM8XXX_GPIO_DISABLE(22),					/* Disable NFC */ \
	PM8XXX_GPIO_OUTPUT(24, 0),					/* LCD_BL_PM_EN */ \
	PM8XXX_GPIO_INPUT(26, PM_GPIO_PULL_UP_30),	/* SD_CARD_DET_N */ \
	PM8XXX_GPIO_OUTPUT(33, 0),					/* MOTOR_EN */ \
	PM8XXX_GPIO_OUTPUT(43, 0)					/* DISP_RESET_N */

/* MAX17043 GPIOs */
#ifdef CONFIG_BATTERY_MAX17043
#define MAX17043_FUELGAUGE_GPIO_OLD_IRQ			40
#define MAX17043_FUELGAUGE_GPIO_IRQ				37
#define MAX17043_FUELGAUGE_I2C_ADDR				0x36
#endif

/* BATTERY ID GPIOs */
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
#define BATT_ID_GPIO							138
#define BATT_ID_PULLUP_GPIO						14
#endif

/* LCD GPIOs */
#define LCD_VCI_EN_GPIO							50

#define MSM_8960_TS_PWR							52
#define	MSM_8960_TS_MAKER_ID					68

/* DCM Touch GPIOS */
#define SYNAPTICS_T1320_TS_I2C_SDA				16
#define SYNAPTICS_T1320_TS_I2C_SCL				17
#define SYNAPTICS_T1320_TS_I2C_INT_GPIO			11

#define SYNAPTICS_T1320_TS_PWR					52
#define SYNAPTICS_T1320_TS_MAKER_ID				68

/* DCM_KeyTouch GPIOS  */
#define	KEY_TOUCH_GPIO_INT						39
#define	KEY_TOUCH_GPIO_I2C_SCL					17
#define	KEY_TOUCH_GPIO_I2C_SDA					16

/* Touch slave address  */
#define	SYNAPTICS_TS_I2C_SLAVE_ADDR				0x20
/* KeyTouch slave address  */
#define	SYNAPTICS_KEYTOUCH_I2C_SLAVE_ADDR		0x2c

/* Vibrator GPIOs */
#if defined(CONFIG_LGE_ISA1200)
#define GPIO_MOTOR_EN                           47
#define ISA1200_SLAVE_ADDR 						0x90
#endif

/* Vibrator GPIOs */
#ifdef CONFIG_ANDROID_VIBRATOR
#define GPIO_LIN_MOTOR_EN        41
#define GPIO_LIN_MOTOR_PWR       47
#define GPIO_LIN_MOTOR_PWM       3

#define GP_CLK_ID                0 /* gp clk 0 */
#define GP_CLK_M_DEFAULT         1
#define GP_CLK_N_DEFAULT         213
#define GP_CLK_D_MAX             GP_CLK_N_DEFAULT
#define GP_CLK_D_HALF            (GP_CLK_N_DEFAULT >> 1)

#define MOTOR_AMP                128
#endif

#ifdef CONFIG_BU52031NVX
/* Cradle GPIOs */
#define GPIO_CARKIT_DETECT                      PM8921_GPIO_PM_TO_SYS(18)
#endif

/* MHL GPIOs */
#ifdef CONFIG_SII8334_MHL_TX
#define MHL_RESET_N 							6
#define MHL_INT_N								7
#define MHL_CSDA								8
#define MHL_CSCL                                9
#endif /* CONFIG_SII8334_MHL_TX */

/* NFC GPIOSs */
#define NXP_PN544PN65N_NFC_I2C_SDA				32
#define NXP_PN544PN65N_NFC_I2C_SCL				33
#define NXP_PN544PN65N_NFC_I2C_SLAVEADDR		0x28
#define NXP_PN544PN65N_NFC_GPIO_IRQ				106
#define NXP_PN544PN65N_NFC_GPIO_VEN				58
#define NXP_PN544PN65N_NFC_GPIO_FIRM			89

/* Headset GPIOs */
#ifdef CONFIG_SWITCH_FSA8008
#define GPIO_EAR_SENSE_N						PM8921_MPP_PM_TO_SYS(4)
#define GPIO_EAR_MIC_EN							PM8921_GPIO_PM_TO_SYS(31)
#define GPIO_EARPOL_DETECT						PM8921_GPIO_PM_TO_SYS(32)
#define GPIO_EAR_KEY_INT						PM8921_MPP_PM_TO_SYS(9)
#endif

/* LED Defines */
#define	LGE_PM8921_KB_LED_MAX_CURRENT	20	/* I = 2mA */
#define	LGE_PM8921_LC_LED_MAX_CURRENT	2	/* I = 2mA */

#ifdef CONFIG_LGE_PM
/* LGE_CHANGE
 * Classified the ADC value for cable detection
 * 2012-01-03, hiro.kwon@lge.com
 */

#define ADC_CHANGE_REV	HW_REV_B
#define IBAT_CURRENT    700

/* Ref resistance value = 200K*/
#define ADC_NO_INIT_CABLE   0
#define ADC_CABLE_MHL_1K    50000
#define ADC_CABLE_U_28P7K   200000/* This value is obsolete */
#define ADC_CABLE_28P7K     300000/* min value of 56K is so low because of factory cable issue */
#define ADC_CABLE_56K       490000
#define ADC_CABLE_100K      650000
#define ADC_CABLE_130K      780000
#define ADC_CABLE_180K      875000
#define ADC_CABLE_200K	    920000
#define ADC_CABLE_220K      988000
#define ADC_CABLE_270K      1077000
#define ADC_CABLE_330K      1294000
#define ADC_CABLE_620K      1418000
#define ADC_CABLE_910K      1600000
#define ADC_CABLE_NONE      1900000

/* Ref resistance value = 200K, same with above definition */
#define ADC_NO_INIT_CABLE2   0
#define ADC_CABLE_MHL_1K2    50000
#define ADC_CABLE_U_28P7K2   200000
#define ADC_CABLE_28P7K2     300000
#define ADC_CABLE_56K2       490000
#define ADC_CABLE_100K2      650000
#define ADC_CABLE_130K2      780000
#define ADC_CABLE_180K2      875000
#define ADC_CABLE_200K2      920000
#define ADC_CABLE_220K2      988000
#define ADC_CABLE_270K2      1077000
#define ADC_CABLE_330K2      1294000
#define ADC_CABLE_620K2      1418000
#define ADC_CABLE_910K2      1600000
#define ADC_CABLE_NONE2      1900000

#define C_NO_INIT_TA_MA     0
#define C_MHL_1K_TA_MA      500
#define C_U_28P7K_TA_MA     500
#define C_28P7K_TA_MA       500
#define C_56K_TA_MA         1500 /* it will be changed in future */
#define C_100K_TA_MA        500
#define C_130K_TA_MA        1500
#define C_180K_TA_MA        700
#define C_200K_TA_MA        700
#define C_220K_TA_MA        700
#define C_270K_TA_MA        500
#define C_330K_TA_MA        500
#define C_620K_TA_MA        500
#define C_910K_TA_MA        1500//[ORG]500
#define C_NONE_TA_MA        700

#define C_NO_INIT_USB_MA    0
#define C_MHL_1K_USB_MA     500
#define C_U_28P7K_USB_MA    500
#define C_28P7K_USB_MA      500
#define C_56K_USB_MA        1500 /* it will be changed in future */
#define C_100K_USB_MA       500
#define C_130K_USB_MA       1500
#define C_180K_USB_MA       500
#define C_200K_USB_MA       500
#define C_220K_USB_MA       500
#define C_270K_USB_MA       500
#define C_330K_USB_MA       500
#define C_620K_USB_MA       500
#define C_910K_USB_MA       1500//[ORG]500
#define C_NONE_USB_MA       500
#endif
#endif /* __ASM_ARCH_MSM_LGE_BOARD_D1LV_H */
