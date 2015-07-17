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

/* LGE Specific */
#ifndef _BOARD_VU2KT_H_
#define _BOARD_VU2KT_H_

#ifdef CONFIG_MACH_LGE
#include <linux/regulator/msm-gpio-regulator.h>
#endif
#include <linux/mfd/pm8xxx/pm8921.h>

/* Motor Vibration Min TimeOut */
#define VIB_MIN_TIMEOUT_MS     25
#define VIB_MIN_STOP_MS         180
#define VIB_LEVEL_MV           2900
#define VIB_OVERDRIVE_MV        3100
#define VIB_OVERDRIVE_MS       20
/* if less than VIB_OVERDRIVE_RANGE_MS,
 * vibrator will execute haptic touch feedback effect mode */
#define VIB_OVERDRIVE_RANGE_MS  100
/* F7 motor spec is adaptive from 1500mv to 2400mv.
 * UI Lvl   HAL Lvl   KERNEL Lvl
 *      0         0          0mv
 *      1        72       1700mv
 *      2        80       1800mv
 *      3        88       1900mv
 *      4        96       2000mv
 *      5       104       2100mv
 *      6       112       2200mv
 *      7       128       2400mv
 */

#define VIB_NORAML_RANGE_MIN_MV 1000 /* for leveling with parameter from UI */
#define VIB_NORMAL_RANGE_MAX_MV 2400
/* F7 haptic touch feedback strength is fixed to 3000mv (overdrive is 3100mv) */
#define VIB_HAPTIC_RANGE_MIN_MV 3000
#define VIB_HAPTIC_RANGE_MAX_MV 3000
#define VIB_HAPTIC_DEFAULT_MV   3000
#define VIB_NORMAL_LVL_TABLE    {{  0,    0},\
                                 { 72, 1200},\
                                 { 80, 1400},\
                                 { 88, 1600},\
                                 { 96, 1800},\
                                 {104, 2000},\
                                 {112, 2200},\
                                 {128, 2400}}

/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_MPP_BASE)
#define PM8921_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)

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
	CHG_BATT_TEMP_LEVEL_4, // M4_41
	CHG_BATT_TEMP_LEVEL_5, // M10_M5
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


#include <linux/i2c/melfas_ts.h>
/*#include <linux/i2c/atmel_ts.h> */

#define PM8921_MPP_IRQ_BASE             (PM8921_IRQ_BASE + NR_GPIO_IRQS)


/* board specific macro of define */
/* It's on board-fx1-pmic.c */
#define	LGE_PM8921_KB_LED_MAX_CURRENT	30	/* I = 2mA */
#ifdef CONFIG_MACH_MSM8960_VU2
  #define	PM8921_LC_LED_MAX_CURRENT	16	/* I = 8mA */
#else
  #define	LGE_PM8921_LC_LED_MAX_CURRENT	2	/* I = 2mA */
#endif
#define PM8XXX_LED_PWM_PERIOD		3000
#define PM8XXX_LED_PWM_DUTY_MS		50
#define PM8921_TOUCH_LED_MAX_CURRENT    16

/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

#define USB_SERIAL_NUMBER						"LGE_ANDROID_FX1_DEV"
#define TS_X_MIN								0
#define TS_X_MAX								540
#define TS_Y_MIN								0
#define TS_Y_MAX								960
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_HD_PT)
#define LCD_RESOLUTION_X						768
#define LCD_RESOLUTION_Y						1024
#else
#define LCD_RESOLUTION_X						736
#define LCD_RESOLUTION_Y						1280
#endif
#define MSM_FB_WIDTH_MM							76
#define MSM_FB_HEIGHT_MM						101


/* CONFIG_LGE_AUDIO
 * Add devide amp parameters
 * 2011-11-30, leia.shin@lge.com
 */
#ifdef CONFIG_MACH_MSM8960_VU2
#define AGC_COMPRESIION_RATE		0
#define AGC_OUTPUT_LIMITER_DISABLE	1
#define AGC_FIXED_GAIN			13 // FX1SK AMP Gain change 12->13, mint.choi@lge.com, 2012-10-15
#else
#define AGC_COMPRESIION_RATE		0
#define AGC_OUTPUT_LIMITER_DISABLE	1
#define AGC_FIXED_GAIN			12
#endif

#ifdef CONFIG_SND_SOC_TPA2028D
	/* Add the I2C driver for Audio Amp, ehgrace.kim@lge.cim, 06/13/2011 */
#define MSM_8960_GSBI9_QUP_I2C_BUS_ID 9
#endif

#ifdef CONFIG_MACH_MSM8960_VU2     /* FX1SK has home key as DOME key (R1+C0) */
#define LGE_KEY_MAP \
	KEY(0, 0, KEY_VOLUMEUP),\
	KEY(0, 1, KEY_VOLUMEDOWN),\
	KEY(0, 2, KEY_HOMEPAGE),\
	KEY(1, 0, KEY_QUICK_CLIP)
#else
#ifndef CONFIG_MACH_MSM8960_FX1V
#define LGE_KEY_MAP \
	KEY(0, 0, KEY_VOLUMEDOWN),\
	KEY(0, 1, KEY_VOLUMEUP),\
	KEY(0, 2, KEY_QUICK_CLIP),\
	KEY(1, 3, KEY_HOMEPAGE)
#else
#define LGE_KEY_MAP \
	KEY(0, 0, KEY_VOLUMEDOWN),\
	KEY(0, 1, KEY_VOLUMEUP),\
	KEY(0, 2, KEY_QUICK_CLIP)
#endif
#endif
/*
 * board specific GPIO definition
 */

 /* LGE_CHANGE
  * FX1A Rev.A Adaptation
  * 20120626 kideok.kim@lge.com */
#ifdef CONFIG_MACH_MSM8960_VU2
/* SKT ONLY: TOUCH_RESET_N: GPIO #75 -> GPIO #35 */
#define LGE_PM8921_GPIO_INITDATA \
	PM8XXX_GPIO_DISABLE(4),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(5),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(6),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(7),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(8),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(10),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(11),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(12),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(13),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(14),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(15),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(16),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(17),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(18),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(21),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(22),                      /* Disable NFC    */ \
	PM8XXX_GPIO_DISABLE(23),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(25),                      /* Disable unused */ \
	PM8XXX_GPIO_INPUT(26, PM_GPIO_PULL_NO),    	  /* SD_CARD_DET_N  */ \
	PM8XXX_GPIO_DISABLE(28),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(33),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(36),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(38),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(40),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(41),                      /* Disable unused */ \
	PM8XXX_GPIO_OUTPUT(43, 0)                     /* SLEEP_CLK_FM   */
#elif defined (CONFIG_MACH_MSM8960_FX1V) /* jjm_carkit */
/* VZW ONLY : Cradle use GPIO 17, 18 */
#define LGE_PM8921_GPIO_INITDATA \
	PM8XXX_GPIO_DISABLE(4),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(5),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(6),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(7),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(8),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(10),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(11),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(12),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(13),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(14),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(15),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(16),                      /* Disable unused */ \
	PM8XXX_GPIO_INPUT(17, PM_GPIO_PULL_UP_30),    /* Cradle detection gpio */ \
	PM8XXX_GPIO_INPUT(18, PM_GPIO_PULL_UP_30),    /* Cradle detection gpio */ \
	PM8XXX_GPIO_OUTPUT(20, 0),                    /* DISP_RESET_N   */ \
	PM8XXX_GPIO_DISABLE(21),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(22),                      /* Disable NFC    */ \
	PM8XXX_GPIO_DISABLE(23),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(25),                      /* Disable unused */ \
	PM8XXX_GPIO_INPUT(26, PM_GPIO_PULL_NO),    	  /* SD_CARD_DET_N  */ \
	PM8XXX_GPIO_DISABLE(28),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(33),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(35),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(36),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(38),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(40),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(41),                      /* Disable unused */ \
	PM8XXX_GPIO_OUTPUT(43, 0)                     /* SLEEP_CLK_FM   */
#else
#define LGE_PM8921_GPIO_INITDATA \
	PM8XXX_GPIO_DISABLE(4),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(5),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(6),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(7),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(8),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(10),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(11),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(12),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(13),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(14),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(15),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(16),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(17),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(18),                      /* Disable unused */ \
	PM8XXX_GPIO_OUTPUT(20, 0),                    /* DISP_RESET_N   */ \
	PM8XXX_GPIO_DISABLE(21),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(22),                      /* Disable NFC    */ \
	PM8XXX_GPIO_DISABLE(23),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(25),                      /* Disable unused */ \
	PM8XXX_GPIO_INPUT(26, PM_GPIO_PULL_NO),    	  /* SD_CARD_DET_N  */ \
	PM8XXX_GPIO_DISABLE(28),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(33),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(35),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(36),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(38),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(40),                      /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(41),                      /* Disable unused */ \
	PM8XXX_GPIO_OUTPUT(43, 0)                     /* SLEEP_CLK_FM   */
#endif /* CONFIG_MACH_MSM8960_FX1SK */

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
/* #define LCD_VCI_EN_GPIO							50  */

/* TOUCH POWER */
#define MELFAS_VD33_MAX_UV			3000000
#define MELFAS_VD33_MIN_UV			3000000
#define MELFAS_VD33_CURR_UA			4230

/* TOUCH GPIOS */
#define TS_GPIO_I2C_SDA							16
#define TS_GPIO_I2C_SCL							17
#define TS_GPIO_IRQ								11
#define MSM_8960_TS_TUCH_RESET					35
/* mystery184.kim@lge.com touch init porting */
#define SYNAPTICS_T1320_TS_PWR                  52

#ifdef CONFIG_MACH_MSM8960_VU2
/* SKT ONLY: TOUCH_RESET_N: GPIO #75 -> GPIO #35 */
#define TS_GPIO_RESET							35
#else
#define TS_GPIO_RESET							75
#endif
/* mystery184.kim@lge.com touch init porting */

#define ATMEL_TS_I2C_SLAVE_ADDR					0x4A
#define MELFAS_TS_I2C_SLAVE_ADDR 				0x48

#define MSM_8960_TS_PWR                     	52   /* unused at FX1A */
#define MSM_8960_TS_MAKER_ID                	68

/* FM Radio */
#define GPIO_FM_EN                		52


/* Vibrator GPIOs */
#ifdef CONFIG_ANDROID_VIBRATOR
#ifdef CONFIG_MACH_MSM8960_VU2
#define GPIO_LIN_MOTOR_EN		       	41
#define GPIO_LIN_MOTOR_PWR		      			47
#define GPIO_LIN_MOTOR_PWM                      3

#define GP_CLK_ID                          	0 /* gp clk 0 */
#define GP_CLK_M_DEFAULT                        1
#define GP_CLK_N_DEFAULT                        165
#define GP_CLK_D_MAX                            GP_CLK_N_DEFAULT
#define GP_CLK_D_HALF                           (GP_CLK_N_DEFAULT >> 1)

#define MOTOR_AMP				128
#endif
#endif

#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
#define MOTOR_AMP		128
#define VIB_DRV			0x4A

#define VIB_DRV_SEL_MASK	0xf8
#define VIB_DRV_SEL_SHIFT	0x03
#define VIB_DRV_EN_MANUAL_MASK	0xfc
#define VIB_DRV_LOGIC_SHIFT	0x2
#define VIB_HIGH_MAX            30    /* for Short Impact */
#define VIB_LOW_MAX             24    /* Max level for Normal Scene */
#define VIB_LOW_MIN             20    /* Min level for Normal Scene */

#endif


#ifdef CONFIG_BU52031NVX /* jjm_carkit */
/* Cradle GPIOs */
#define GPIO_POUCH_DETECT                       PM8921_GPIO_PM_TO_SYS(18)
#define GPIO_CARKIT_DETECT                      PM8921_GPIO_PM_TO_SYS(17)
#endif

/* MHL GPIOs */
#ifdef CONFIG_SII8334_MHL_TX
#define MHL_PWR_EN								43
#define MHL_RESET_N 							6
#define MHL_INT_N								7
#define MHL_CSDA								8
#define MHL_CSCL                                9
#endif

/* Sensor GPIOs */

/* Headset GPIOs */
#define GPIO_EAR_SENSE_N						PM8921_MPP_PM_TO_SYS(4)
#define GPIO_EAR_MIC_EN							PM8921_GPIO_PM_TO_SYS(31)
#define GPIO_EARPOL_DETECT						PM8921_GPIO_PM_TO_SYS(32)
#define GPIO_EAR_KEY_INT						PM8921_MPP_PM_TO_SYS(9)

/* NFC GPIOSs */
#define NXP_PN544PN65N_NFC_I2C_SDA				36
#define NXP_PN544PN65N_NFC_I2C_SCL				37
#define NXP_PN544PN65N_NFC_I2C_SLAVEADDR		0x28
#define NXP_PN544PN65N_NFC_GPIO_IRQ				106
#define NXP_PN544PN65N_NFC_GPIO_VEN				58
#define NXP_PN544PN65N_NFC_GPIO_FIRM			89

/* Camera */
#define GPIO_CAM_MCLK0          (5)
#define GPIO_CAM_MCLK1          (4)
#define GPIO_CAM_VCM_EN         (53) 	/* P740 */
#define GPIO_CAM_I2C_SDA        (20)
#define GPIO_CAM_I2C_SCL        (21)
#define GPIO_CAM1_RST_N         (107)
#define GPIO_CAM2_RST_N         (76)
#define GPIO_CAM_FLASH_EN       (1)
#define GPIO_CAM_FLASH_I2C_SDA  (36)	/* P740 */
#define GPIO_CAM_FLASH_I2C_SCL  (37)   	/* P740 */

#define I2C_SLAVE_ADDR_IMX111       (0x0D)
#define I2C_SLAVE_ADDR_IMX111_ACT   (0x18)
#define I2C_SLAVE_ADDR_IMX119       (0x6E)
#define I2C_SLAVE_ADDR_LM3559       (0x53)

#define CAM1_VAF_MINUV          2800000
#define CAM1_VAF_MAXUV          2800000
#define CAM1_VDIG_MINUV         1200000
#define CAM1_VDIG_MAXUV         1200000
#define CAM1_VANA_MINUV         2800000
#define CAM1_VANA_MAXUV         2850000
#define CAM_CSI_VDD_MINUV       1200000
#define CAM_CSI_VDD_MAXUV       1200000

#define CAM1_VAF_LOAD_UA        300000
#define CAM1_VDIG_LOAD_UA       105000
#define CAM1_VANA_LOAD_UA       85600
#define CAM_CSI_LOAD_UA         20000

#define CAM2_VIO_MINUV          1800000
#define CAM2_VIO_MAXUV          1800000
#define CAM2_VDIG_MINUV         1200000
#define CAM2_VDIG_MAXUV         1200000
#define CAM2_VANA_MINUV         2800000
#define CAM2_VANA_MAXUV         2850000

#define CAM2_VDIG_LOAD_UA       105000
#define CAM2_VANA_LOAD_UA       85600

#ifdef CONFIG_LGE_PM
/* LGE_CHANGE
 * Classified the ADC value for cable detection
 * 2011-12-05, kidong0420.kim@lge.com
 */
#define ADC_CHANGE_REV	HW_REV_EVB1
#define IBAT_CURRENT 	800

 /* Ref resistance value = 665K*/
#define ADC_NO_INIT_CABLE   0
#define ADC_CABLE_MHL_1K    5000
#define ADC_CABLE_U_28P7K   5500/* This value is obsolete */
#define ADC_CABLE_28P7K     6000/* min value of 56K is so low because of factory cable issue */
#define ADC_CABLE_56K       200000
#define ADC_CABLE_100K      239000
#define ADC_CABLE_130K      340000
#define ADC_CABLE_180K      399000
#define ADC_CABLE_200K	431000
#define ADC_CABLE_220K      484000
#define ADC_CABLE_270K      570000
#define ADC_CABLE_330K      650000
#define ADC_CABLE_620K      939000
#define ADC_CABLE_910K      1140000
#define ADC_CABLE_NONE      1900000

/* Ref resistance value = 200K*/
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
#define ADC_CABLE_NONE2      1800000

#define C_NO_INIT_TA_MA     0
#define C_MHL_1K_TA_MA      500
#define C_U_28P7K_TA_MA     500
#define C_28P7K_TA_MA       500
#define C_56K_TA_MA         1500 /* it will be changed in future */
#define C_100K_TA_MA        500
#define C_130K_TA_MA        1500
#define C_180K_TA_MA        500
#define C_200K_TA_MA        500
#define C_220K_TA_MA        500
#define C_270K_TA_MA        500
#define C_330K_TA_MA        500
#define C_620K_TA_MA        500
#define C_910K_TA_MA        1500
#define C_NONE_TA_MA        900

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
#define C_910K_USB_MA       1500  //[ORG]500
#define C_NONE_USB_MA       500
#endif

/*
 * board specific Vreg of LDO definition
 */

#endif

