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

/* QCT Original */
#ifndef __ARCH_ARM_MACH_MSM_BOARD_MSM8960_H
#define __ARCH_ARM_MACH_MSM_BOARD_MSM8960_H

#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <mach/irqs.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_memtypes.h>

/* Motor Vibration Min TimeOut */
#define MIN_TIMEOUT_MS 60

/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_MPP_BASE)
#define PM8921_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)

extern struct pm8xxx_regulator_platform_data
	msm_pm8921_regulator_pdata[] __devinitdata;

extern int msm_pm8921_regulator_pdata_len __devinitdata;

#define GPIO_VREG_ID_EXT_5V		0
#define GPIO_VREG_ID_EXT_L2		1
#define GPIO_VREG_ID_EXT_3P3V		2
#define GPIO_VREG_ID_EXT_OTG_SW		3

extern struct gpio_regulator_platform_data
	msm_gpio_regulator_pdata[] __devinitdata;

extern struct regulator_init_data msm_saw_regulator_pdata_s5;
extern struct regulator_init_data msm_saw_regulator_pdata_s6;

extern struct rpm_regulator_platform_data msm_rpm_regulator_pdata __devinitdata;

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
enum {
	GPIO_EXPANDER_IRQ_BASE = (PM8921_IRQ_BASE + PM8921_NR_IRQS),
	GPIO_EXPANDER_GPIO_BASE = (PM8921_MPP_BASE + PM8921_NR_MPPS),
	/* CAM Expander */
	GPIO_CAM_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE,
	GPIO_CAM_GP_STROBE_READY = GPIO_CAM_EXPANDER_BASE,
	GPIO_CAM_GP_AFBUSY,
	GPIO_CAM_GP_STROBE_CE,
	GPIO_CAM_GP_CAM1MP_XCLR,
	GPIO_CAM_GP_CAMIF_RESET_N,
	GPIO_CAM_GP_XMT_FLASH_INT,
	GPIO_CAM_GP_LED_EN1,
	GPIO_CAM_GP_LED_EN2,
	GPIO_LIQUID_EXPANDER_BASE = GPIO_CAM_EXPANDER_BASE + 8,
};
#endif

enum {
	SX150X_CAM,
	SX150X_LIQUID,
};

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
#endif

extern struct sx150x_platform_data msm8960_sx150x_data[];
extern struct msm_camera_board_info msm8960_camera_board_info;
#if !defined(CONFIG_MACH_MSM8960_L1v)
extern unsigned char hdmi_is_primary;
#endif
void msm8960_init_cam(void);
void msm8960_init_fb(void);
void msm8960_init_pmic(void);
void msm8960_init_mmc(void);
int msm8960_init_gpiomux(void);
void msm8960_allocate_fb_region(void);
void msm8960_set_display_params(char *prim_panel, char *ext_panel);
void msm8960_pm8921_gpio_mpp_init(void);
void msm8960_mdp_writeback(struct memtype_reserve *reserve_table);
uint32_t msm_rpm_get_swfi_latency(void);
#define PLATFORM_IS_CHARM25() \
	(machine_is_msm8960_cdp() && \
		(socinfo_get_platform_subtype() == 1) \
	)
#define MSM_8960_GSBI4_QUP_I2C_BUS_ID 4
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3
#define MSM_8960_GSBI10_QUP_I2C_BUS_ID 10

/* LGE Specific */
#ifndef _BOARD_D1LA_H_
#define _BOARD_D1LA_H_
#include <linux/i2c/melfas_ts.h>
#include <linux/i2c/atmel_ts.h>

#define PM8921_MPP_IRQ_BASE             (PM8921_IRQ_BASE + NR_GPIO_IRQS)
/* board specific macro of define */

/* LGE_CHANGE_S
* 2012-10-31, yuntaek.kim@lge.com
* Indicator Led CP Control Code
*/
#if defined(CONFIG_MACH_MSM8960_L1v)
#define	PM8921_KB_LED_MAX_CURRENT	4	/*indicator LED Mode */
#else
#define	PM8921_KB_LED_MAX_CURRENT	2	/* indicator LED I = 2mA */
#endif
/* LGE_CHANGE_E
* 2012-10-31, yuntaek.kim@lge.com
* Indicator Led CP Control Code
*/
#define	PM8921_LC_LED_MAX_CURRENT	8	/* key backight I = 8mA */
#define PM8XXX_LED_PWM_PERIOD		1000
#define PM8XXX_LED_PWM_DUTY_MS		20

/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

#define USB_SERIAL_NUMBER						"LGE_ANDROID_D1LA_DEV"
#define TS_X_MIN								0
#define TS_X_MAX								540
#define TS_Y_MIN								0
#define TS_Y_MAX								960
#define LCD_RESOLUTION_X						556
#define LCD_RESOLUTION_Y						960
#define MSM_FB_WIDTH_MM							53
#define MSM_FB_HEIGHT_MM						95

/* CONFIG_LGE_AUDIO
 * Add devide amp parameters
 * 2011-11-30, leia.shin@lge.com
 */
#define AGC_COMPRESIION_RATE		0
#define AGC_OUTPUT_LIMITER_DISABLE	1
#define AGC_FIXED_GAIN			12

#ifdef CONFIG_LGE_AUDIO_TPA2028D
	/* Add the I2C driver for Audio Amp, ehgrace.kim@lge.cim, 06/13/2011 */
#define MSM_8960_GSBI9_QUP_I2C_BUS_ID 9
#endif

#define LGE_KEY_MAP \
	KEY(0, 0, KEY_VOLUMEUP),\
	KEY(0, 1, KEY_VOLUMEDOWN),\
	KEY(0, 2, KEY_HOMEPAGE)

/*
 * board specific GPIO definition
 */

#define LGE_PM8921_GPIO_INITDATA \
	PM8XXX_GPIO_DISABLE(6),                       /* Disable unused */ \
	PM8XXX_GPIO_DISABLE(7),                       /* Disable NFC */ \
	PM8XXX_GPIO_INPUT(16, PM_GPIO_PULL_UP_30),    /* SD_CARD_WP */ \
	PM8XXX_GPIO_INPUT(18, PM_GPIO_PULL_UP_30),    /* Cradle detection gpio */ \
	PM8XXX_GPIO_DISABLE(22),                      /* Disable NFC */ \
	PM8XXX_GPIO_OUTPUT(24, 0),                    /* LCD_BL_PM_EN */ \
	PM8XXX_GPIO_INPUT(26, PM_GPIO_PULL_DN),    /* SD_CARD_DET_N */ \
	PM8XXX_GPIO_OUTPUT(43, 1)                    /* DISP_RESET_N */ /*2012-1226 skip LCD init for L1v*/ 

#ifdef CONFIG_BATTERY_MAX17043
#define MAX17043_FUELGAUGE_GPIO_OLD_IRQ			40
#define MAX17043_FUELGAUGE_GPIO_IRQ				37
#define MAX17043_FUELGAUGE_I2C_ADDR				0x36
#endif

#ifdef CONFIG_MACH_LGE
/* Battery ID GPIOs*/
#define BATT_ID_GPIO							138
#define BATT_ID_PULLUP_GPIO						14
#endif

/* LCD GPIOs */
#define LCD_VCI_EN_GPIO							50

/* TOUCH GPIOS */
#define TS_GPIO_I2C_SDA							16
#define TS_GPIO_I2C_SCL							17
#define TS_GPIO_IRQ								11

#define ATMEL_TS_I2C_SLAVE_ADDR					0x4A
#define MELFAS_TS_I2C_SLAVE_ADDR 				0x48

#if defined(CONFIG_LGE_TOUCH_SYNAPTICS_325)
#define MSM_8960_TS_PWR                     	46
#else
#define MSM_8960_TS_PWR							52
#endif

#define MSM_8960_TS_MAKER_ID                	68

#define TOUCH_FW_VERSION			9	/* default Rev.B */

/* Vibrator GPIOs */
#ifdef CONFIG_ANDROID_VIBRATOR
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

#ifdef CONFIG_BU52031NVX
/* Cradle GPIOs */
#define GPIO_CARKIT_DETECT                      PM8921_GPIO_PM_TO_SYS(18)
#endif

/* MHL GPIOs */
#define MHL_PWR_EN								43
#define MHL_RESET_N 							6
#define MHL_INT_N								7
#define MHL_CSDA								8
#define MHL_CSCL                                9

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
#define GPIO_CAM_VCM_EN         (2)
#define GPIO_CAM_I2C_SDA        (20)
#define GPIO_CAM_I2C_SCL        (21)
#define GPIO_CAM1_RST_N         (107)
#define GPIO_CAM2_RST_N         (76)
#define GPIO_CAM_FLASH_EN       (1)
#define GPIO_CAM_FLASH_I2C_SDA  (36)
#define GPIO_CAM_FLASH_I2C_SCL  (37)

//LGE_CHANGE_S L1_SERIES (MS870, VS870) jongkwon.chae@lge.com
#define I2C_SLAVE_ADDR_S5K4E1       (0x20)
#define I2C_SLAVE_ADDR_S5K4E1_ACT   (0x18)
#define I2C_SLAVE_ADDR_IMX119       (0x6E)
#define I2C_SLAVE_ADDR_LM3559       (0x53)

#ifdef CONFIG_S5K4E1
#define CAM1_VAF_MINUV          2800000
#define CAM1_VAF_MAXUV          2800000
#define CAM1_VDIG_MINUV         1800000
#define CAM1_VDIG_MAXUV         1800000
#define CAM1_VANA_MINUV         2800000
#define CAM1_VANA_MAXUV         2850000
#define CAM_CSI_VDD_MINUV       1200000
#define CAM_CSI_VDD_MAXUV       1200000

#define CAM1_VAF_LOAD_UA        300000
#define CAM1_VDIG_LOAD_UA       100000
#define CAM1_VANA_LOAD_UA       80000
#define CAM_CSI_LOAD_UA         20000
#endif
//LGE_CHANGE_E L1_SERIES (MS870, VS870)

//LGE_CHANGE_S L1_SERIES (MS870, VS870) jongkwon.chae@lge.com
#ifdef CONFIG_IMX119
/* LGE_CHANGE
 * Seperate MT9V113 and IMX119.
 * 2012-02-21 yousung.kang@lge.com
 */
#define CAM2_VIO_MINUV          1800000
#define CAM2_VIO_MAXUV          1800000

#define CAM2_VDIG_MINUV         1200000
#define CAM2_VDIG_MAXUV         1200000

#define CAM2_VANA_MINUV         2800000
#define CAM2_VANA_MAXUV         2850000

#define CAM2_VDIG_LOAD_UA       105000
#define CAM2_VANA_LOAD_UA       85600
#endif
//LGE_CHANGE_E L1_SERIES (MS870, VS870)

#ifdef CONFIG_LGE_PM
/* LGE_CHANGE
 * Classified the ADC value for cable detection
 * 2011-12-05, kidong0420.kim@lge.com
 */ 
#define ADC_CHANGE_REV	HW_REV_MAX
#define IBAT_CURRENT 	    1200

/* L0, Change the USB_ID ADC table.
*  Ref resistor is changed to 200K after Rev.B.
*  Rev.A is not supported anymore.
*  2012-02-22, junsin.park@lge.com
*/

/* Ref resistance value = 200K*/
#define ADC_NO_INIT_CABLE2   0
#define ADC_CABLE_MHL_1K2    15000
#define ADC_CABLE_U_28P7K2   250000
#define ADC_CABLE_28P7K2     255000	//temporarily setting
#define ADC_CABLE_56K2       550000
#define ADC_CABLE_100K2      640000
#define ADC_CABLE_130K2      800000
#define ADC_CABLE_180K2      860000
#define ADC_CABLE_200K2      910000
#define ADC_CABLE_220K2      1000000
#define ADC_CABLE_270K2      1080000
#define ADC_CABLE_330K2      1150000
#define ADC_CABLE_620K2      1400000
#define ADC_CABLE_910K2      1700000
#define ADC_CABLE_NONE2      1800000

 /* Ref resistance value = 665K*/
#define ADC_NO_INIT_CABLE   0
#define ADC_CABLE_MHL_1K    5000
#define ADC_CABLE_U_28P7K   5500/* This value is obsolete */
#define ADC_CABLE_28P7K     6000/* min value of 56K is so low because of factory cable issue */
#define ADC_CABLE_56K       200000
#define ADC_CABLE_100K      239000
#define ADC_CABLE_130K      340000
#define ADC_CABLE_180K      399000
#define ADC_CABLE_200K      431000
#define ADC_CABLE_220K      484000
#define ADC_CABLE_270K      570000
#define ADC_CABLE_330K      650000
#define ADC_CABLE_620K      939000
#define ADC_CABLE_910K      1140000
#define ADC_CABLE_NONE      1800000

#define C_NO_INIT_TA_MA     0
#define C_MHL_1K_TA_MA      500
#define C_U_28P7K_TA_MA     500
#define C_28P7K_TA_MA       500
#define C_56K_TA_MA         1500 /* it will be changed in future */
#define C_100K_TA_MA        500
#define C_130K_TA_MA        1500
#define C_180K_TA_MA        500
#define C_200K_TA_MA        500
#define C_220K_TA_MA        1100
#define C_270K_TA_MA        1100
#define C_330K_TA_MA        500
#define C_620K_TA_MA        500
#define C_910K_TA_MA        1500  //[ORG]500
#define C_NONE_TA_MA        500

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

