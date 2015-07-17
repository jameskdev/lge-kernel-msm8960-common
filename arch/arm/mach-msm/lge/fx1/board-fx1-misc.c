/*
  * Copyright (C) 2011 LGE, Inc.
  *
  * Author: Sungwoo Cho <sungwoo.cho@lge.com>
  *
  * This software is licensed under the terms of the GNU General
  * License version 2, as published by the Free Software Foundation,
  * may be copied, distributed, and modified under those terms.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
  * GNU General Public License for more details.
  */

#include <linux/types.h>
#include <linux/err.h>
#include <mach/msm_iomap.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <mach/board_lge.h>

#include "devices.h"

#include <linux/android_vibrator.h>

/*
 * include LGE board specific header file
 */
#include "board-fx1.h"
#include CONFIG_BOARD_HEADER_FILE

/* #include <linux/regulator/gpio-regulator.h> */
#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/i2c.h>

#ifdef CONFIG_BU52031NVX
#include <linux/mfd/pm8xxx/cradle.h>
#endif

#include <linux/regulator/consumer.h>
#ifdef CONFIG_SII8334_MHL_TX
#include <linux/platform_data/mhl_device.h>
#endif

/* gpio and clock control for vibrator */

#define REG_WRITEL(value, reg)		writel(value, (MSM_CLK_CTL_BASE+reg))
#define REG_READL(reg)			readl((MSM_CLK_CTL_BASE+reg))

#define GPn_MD_REG(n)                           (0x2D00+32*(n))
#define GPn_NS_REG(n)                           (0x2D24+32*(n))

/* When use SM100 with GP_CLK
  170Hz motor : 22.4KHz - M=1, N=214 ,
  230Hz motor : 29.4KHZ - M=1, N=163 ,
  */
#ifdef CONFIG_ANDROID_VIBRATOR

static struct gpiomux_setting vibrator_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

/* gpio 4 */
static struct gpiomux_setting vibrator_active_cfg_gpio4 = {
	.func = GPIOMUX_FUNC_3, /*vfe_camif_timer3:1, cam_mclk1:2, gp_clk_1a:3 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config gpio4_vibrator_configs[] = {
	{
		.gpio = 4,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_active_cfg_gpio4,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
};

/* gpio 3 */
static struct gpiomux_setting vibrator_active_cfg_gpio3 = {
	.func = GPIOMUX_FUNC_2, /*vfe_camif_timer2:1, gp_clk_0a:2 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config gpio3_vibrator_configs[] = {
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_active_cfg_gpio3,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
};

static int gpio_vibrator_en = 0;
static int gpio_vibrator_pwm = 0;
static int gp_clk_id = 0;

static int vibrator_gpio_init(void)
{
	gpio_vibrator_en = GPIO_LIN_MOTOR_EN;
	gpio_vibrator_pwm = GPIO_LIN_MOTOR_PWM;

	return 0;
}

static int vibrator_power_set(int enable)
{
	INFO_MSG("enable=%d\n", enable);
	if (enable)
		gpio_direction_output(GPIO_LIN_MOTOR_PWR, 1);
	else
		gpio_direction_output(GPIO_LIN_MOTOR_PWR, 0);
	return 0;
}

static int vibrator_pwm_set(int enable, int amp, int n_value)
{
	/* TODO: set clk for amp */
	uint M_VAL = GP_CLK_M_DEFAULT;
	uint D_VAL = GP_CLK_D_MAX;
	uint D_INV = 0;                 /* QCT support invert bit for msm8960 */
	uint clk_id = gp_clk_id;

	INFO_MSG("amp=%d, n_value=%d\n", amp, n_value);

	if (enable) {
		D_VAL = ((GP_CLK_D_MAX * amp) >> 7);
		if (D_VAL > GP_CLK_D_HALF) {
			if (D_VAL == GP_CLK_D_MAX) {      /* Max duty is 99% */
				D_VAL = 2;
			} else {
				D_VAL = GP_CLK_D_MAX - D_VAL;
			}
			D_INV = 1;
		}

		REG_WRITEL(
			(((M_VAL & 0xffU) << 16U) + /* M_VAL[23:16] */
			((~(D_VAL << 1)) & 0xffU)),  /* D_VAL[7:0] */
			GPn_MD_REG(clk_id));

		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(1U << 11U) +  /* CLK_ROOT_ENA[11]  : Enable(1) */
			((D_INV & 0x01U) << 10U) +  /* CLK_INV[10]       : Disable(0) */
			(1U << 9U) +   /* CLK_BRANCH_ENA[9] : Enable(1) */
			(1U << 8U) +   /* NMCNTR_EN[8]      : Enable(1) */
			(0U << 7U) +   /* MNCNTR_RST[7]     : Not Active(0) */
			(2U << 5U) +   /* MNCNTR_MODE[6:5]  : Dual-edge mode(2) */
			(3U << 3U) +   /* PRE_DIV_SEL[4:3]  : Div-4 (3) */
			(5U << 0U)),   /* SRC_SEL[2:0]      : CXO (5)  */
			GPn_NS_REG(clk_id));
		INFO_MSG("GPIO_LIN_MOTOR_PWM is enable with M=%d N=%d D=%d\n", M_VAL, n_value, D_VAL);
	} else {
		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(0U << 11U) +  /* CLK_ROOT_ENA[11]  : Disable(0) */
			(0U << 10U) +  /* CLK_INV[10]	    : Disable(0) */
			(0U << 9U) +	 /* CLK_BRANCH_ENA[9] : Disable(0) */
			(0U << 8U) +   /* NMCNTR_EN[8]      : Disable(0) */
			(0U << 7U) +   /* MNCNTR_RST[7]     : Not Active(0) */
			(2U << 5U) +   /* MNCNTR_MODE[6:5]  : Dual-edge mode(2) */
			(3U << 3U) +   /* PRE_DIV_SEL[4:3]  : Div-4 (3) */
			(5U << 0U)),   /* SRC_SEL[2:0]      : CXO (5)  */
			GPn_NS_REG(clk_id));
		INFO_MSG("GPIO_LIN_MOTOR_PWM is disalbe \n");
	}

	return 0;
}

static int vibrator_ic_enable_set(int enable)
{
	int gpio_lin_motor_en = 0;

	INFO_MSG("enable=%d\n", enable);

	gpio_lin_motor_en = gpio_vibrator_en;

	if (enable)
		gpio_direction_output(gpio_lin_motor_en, 1);
	else
		gpio_direction_output(gpio_lin_motor_en, 0);

	return 0;
}

#if defined(CONFIG_MACH_MSM8960_FX1SK)
static struct regulator *vreg_ldo10;
static int vreg_set_ldo10(int on) // temp for Rev.A
{
	int rc = -EINVAL;

      if(!vreg_ldo10) {
        	vreg_ldo10 = regulator_get(NULL, "8921_l10");   /* LDO 10 */


        	if (IS_ERR(vreg_ldo10)) {
        		pr_err("%s: regulator get of 8921_l10 failed (%ld) \n"
        			, __func__, PTR_ERR(vreg_ldo10));
        		rc = PTR_ERR(vreg_ldo10);
        		return rc;
        	}

        	rc = regulator_set_voltage(vreg_ldo10,
        			3000000, 3000000); //3.0v
        }

	if(on) {
			rc = regulator_enable(vreg_ldo10);
	} else {
			rc = regulator_disable(vreg_ldo10);
	}

  	return rc;
}
#endif /*CONFIG_MACH_MSM8960_FX1SK*/

static int vibrator_init(void)
{
	int rc;
	int gpio_motor_en = 0;
	int gpio_motor_pwm = 0;

	gpio_motor_en = gpio_vibrator_en;
	gpio_motor_pwm = gpio_vibrator_pwm;

	/* GPIO function setting */
	if (gpio_motor_pwm == GPIO_LIN_MOTOR_PWM)
		msm_gpiomux_install(gpio3_vibrator_configs, ARRAY_SIZE(gpio3_vibrator_configs));
	else
		msm_gpiomux_install(gpio4_vibrator_configs, ARRAY_SIZE(gpio4_vibrator_configs));

	/* GPIO setting for PWR Motor EN in msm8960 */
	rc = gpio_request(GPIO_LIN_MOTOR_PWR, "lin_motor_pwr_en");
	if (rc) {
		ERR_MSG("GPIO_LIN_MOTOR_PWR %d request failed\n", GPIO_LIN_MOTOR_PWR);
		return 0;
	}

	/* GPIO setting for Motor EN in pmic8921 */
	rc = gpio_request(gpio_motor_en, "lin_motor_en");
	if (rc) {
		ERR_MSG("GPIO_LIN_MOTOR_EN %d request failed\n", gpio_motor_en);
		return 0;
	}

	/* gpio init */
	rc = gpio_request(gpio_motor_pwm, "lin_motor_pwm");
	if (unlikely(rc < 0))
		ERR_MSG("not able to get gpio\n");

	vibrator_ic_enable_set(0);
	vibrator_pwm_set(0, 0, GP_CLK_N_DEFAULT);
	vibrator_power_set(0);

#if defined(CONFIG_MACH_MSM8960_FX1SK)
	DEBUG_MSG("HW Revision = %d\n", lge_get_board_revno());
	if(lge_get_board_revno() == HW_REV_A) {
        vreg_set_ldo10(1);
		DEBUG_MSG("[Rev.A] PMIC Motor Power Enable...\n");
	}
#endif /*CONFIG_MACH_MSM8960_FX1SK*/

	return 0;
}



static struct android_vibrator_platform_data vibrator_data = {
	.enable_status = 0,
	.amp = MOTOR_AMP,
	.vibe_n_value = GP_CLK_N_DEFAULT,
	.power_set = vibrator_power_set,
	.pwm_set = vibrator_pwm_set,
	.ic_enable_set = vibrator_ic_enable_set,
	.vibrator_init = vibrator_init,
};

static struct platform_device android_vibrator_device = {
	.name   = "android-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &vibrator_data,
	},
};

static struct platform_device *misc_devices[] __initdata = {

	&android_vibrator_device,

};
#endif /* CONFIG_ANDROID_VIBRATOR */

#ifdef CONFIG_BU52031NVX
static struct pm8xxx_cradle_platform_data cradle_data = {
#if defined(CONFIG_BU52031NVX_POUCHDETECT)
	.pouch_detect_pin = GPIO_POUCH_DETECT,
	.pouch_irq = PM8921_GPIO_IRQ(PM8921_IRQ_BASE, 18),
#endif
#if defined(CONFIG_BU52031NVX_CARKITDETECT)
	.carkit_detect_pin = GPIO_CARKIT_DETECT,
	.carkit_irq = PM8921_GPIO_IRQ(PM8921_IRQ_BASE, 17),
	.irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
#endif
};

static struct platform_device cradle_device = {
	.name   = PM8XXX_CRADLE_DEV_NAME,
	.id = -1,
	.dev = {
		.platform_data = &cradle_data,
	},
};
#endif

#ifdef CONFIG_SII8334_MHL_TX

static int mhl_gpio_init(void);
static int mhl_power_onoff(bool, bool);

static struct mhl_platform_data mhl_pdata = {
	.power = mhl_power_onoff,
};


static struct platform_device mhl_sii833x_device = {
	.name = "sii_8334",
	.id = 0,
	.dev = {
		.platform_data = &mhl_pdata,
	}
};

static struct regulator *vreg_l18_mhl;

static int mhl_gpio_init(void)
{
	int rc;
	pr_info("%s : start!\n", __func__);

	rc = gpio_request(MHL_INT_N, "mhl_int_n");
	if (rc < 0) {
		pr_err("failed to request mhl_int_n gpio\n");
		goto error1;
	}
	gpio_export(MHL_INT_N, 1);

	rc = gpio_request(MHL_RESET_N, "mhl_reset_n");
	if (rc < 0) {
		pr_err("failed to request mhl_reset_n gpio\n");
		goto error2;
	}
	rc = gpio_direction_output(MHL_RESET_N, 0);
	if (rc < 0) {
		pr_err("failed to request mhl_reset_n gpio\n");
		goto error3;
	}
error3:
	gpio_free(MHL_RESET_N);
error2:
	gpio_free(MHL_INT_N);
error1:

	return rc;
}

static int mhl_power_onoff(bool on, bool pm_ctrl)
{
	static bool power_state=0;
	int rc = 0;

	if (pm_ctrl)
		return -EPERM;

	pr_info("%s: %s \n", __func__, on ? "on" : "off");

	if (power_state == on) {
		pr_info("sii_power_state is already %s \n",
				power_state ? "on" : "off");
		return rc;
	}
	power_state = on;

	if (!vreg_l18_mhl)
		vreg_l18_mhl = regulator_get(&mhl_sii833x_device.dev, (char *)"mhl_l18");

	if (IS_ERR(vreg_l18_mhl)) {
		rc = PTR_ERR(vreg_l18_mhl);
		pr_err("%s: vreg_l18_mhl get failed (%d)\n", __func__, rc);
		return rc;
	}

	if (on) {
		rc = regulator_set_optimum_mode(vreg_l18_mhl, 100000);
		if (rc < 0) {
			pr_err("%s : set optimum mode 100000,\
				vreg_l18_mhl failed (%d)\n",
				__func__, rc);
			return -EINVAL;
		}
		rc = regulator_set_voltage(vreg_l18_mhl, 1200000, 1200000);
		if (rc < 0) {
			pr_err("%s : set voltage 1200000,\
				vreg_l18_mhl failed (%d)\n",
				__func__, rc);
			return -EINVAL;
		}
		if (!rc)
			rc = regulator_enable(vreg_l18_mhl);

		if (rc) {
			pr_err("%s : vreg_l18_mhl enable failed (%d)\n",
							__func__, rc);
			return rc;
		}

		rc = gpio_request(MHL_RESET_N, "mhl_reset_n");
		if (rc < 0) {
			pr_err("failed to request mhl_reset_n gpio\n");
			return rc;
		}

		gpio_direction_output(MHL_RESET_N, 0);
		msleep(100);
		gpio_direction_output(MHL_RESET_N, 1);
		}
	else {
		rc = regulator_set_optimum_mode(vreg_l18_mhl, 100);

		if (rc < 0) {
			pr_err("%s : set optimum mode 100,\
				vreg_l18_mhl failed (%d)\n",
				__func__, rc);
			return -EINVAL;
		}
		rc = regulator_disable(vreg_l18_mhl);
		if (rc) {
			pr_err("%s : vreg_l18_mhl disable failed (%d)\n",
							__func__, rc);
			return rc;
		}
		gpio_direction_output(MHL_RESET_N, 0);
		gpio_free(MHL_RESET_N);
	}

	return rc;
}

static struct i2c_board_info i2c_mhl_info[] = {
	{
		I2C_BOARD_INFO("Sil-833x", 0x72 >> 1), /* 0x39 */
		.irq = MSM_GPIO_TO_INT(MHL_INT_N),
		.platform_data = &mhl_pdata,
	},
	{
		I2C_BOARD_INFO("Sil-833x", 0x7A >> 1), /* 0x3D */
	},
	{
		I2C_BOARD_INFO("Sil-833x", 0x92 >> 1), /* 0x49 */
	},
	{
		I2C_BOARD_INFO("Sil-833x", 0x9A >> 1), /* 0x4D */
	},
	{
		I2C_BOARD_INFO("Sil-833x", 0xC8 >> 1), /*  0x64 */
	},
};

static struct i2c_registry i2c_mhl_devices __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8960_GSBI1_QUP_I2C_BUS_ID,
	i2c_mhl_info,
	ARRAY_SIZE(i2c_mhl_info),
};

static void __init lge_add_i2c_mhl_device(void)
{
	i2c_register_board_info(i2c_mhl_devices.bus,
	i2c_mhl_devices.info,
	i2c_mhl_devices.len);
}

#endif /* CONFIG_SII8334_MHL_TX */

/* jjm_rgb */
#ifdef CONFIG_LEDS_LP5521
#include <linux/leds-lp5521.h>

#define I2C_SLAVE_ADDR_LP5521 0x32

#define GPIO_FUEL_I2C_SDA 24
#define GPIO_FUEL_I2C_SCL 25
#define GPIO_RGB_EN 34
#define GPIO_RGB_INT 46
#define GPIO_RGB_TRIG 99
#define PM8921_GPIO_RGB_CLK 43
#define LP5521_CONFIGS     (LP5521_PWM_HF | LP5521_PWRSAVE_EN | LP5521_CP_MODE_AUTO | LP5521_CLK_SRC_EXT)

#if 1 /* jjm_pattern */
/*  1. Power On Animation */
static u8 mode1_red[] = {0xE0, 0x0C, 0x40, 0x00, 0x0C, 0x2F, 0x06, 0x28, 0x05, 0x2D, 0x06, 0x2A, 0x06, 0x25, 0x03, 0xDC, 0x02, 0xFA, 0x48, 0x00, 0x03, 0x54, 0x44, 0x01, 0x23, 0x06, 0x31, 0x84, 0x06, 0xA8, 0x0C, 0xAF};
static u8 mode1_green[] = {0xE0, 0x80, 0x40, 0x00, 0x52, 0x00, 0x0B, 0x15, 0x05, 0x2D, 0x03, 0x48, 0x03, 0x4B, 0x09, 0x1B, 0x02, 0x63, 0x19, 0x89, 0x03, 0xCA, 0x04, 0xC1, 0x05, 0xB2, 0x06, 0xA6, 0x12, 0x8D, 0x52, 0x00};
static u8 mode1_blue[] = {0xE0, 0x80, 0x40, 0x00, 0x12, 0xFE, 0x40, 0xC0, 0x0A, 0x18, 0x06, 0xA6, 0x06, 0xAA, 0x03, 0xCF, 0x04, 0xB6, 0x52, 0x00};

/* 2. LCD On : Not Use */
static u8 mode2_red[] = {0x40, 0xff, 0x4d, 0x00, 0x0a, 0xff, 0x0a, 0xfe, 0xc0, 0x00};
static u8 mode2_green[] = {0x40, 0xff, 0x4d, 0x00, 0x0a, 0xff, 0x0a, 0xfe, 0xc0, 0x00};
static u8 mode2_blue[] = {0x40, 0xff, 0x4d, 0x00, 0x0a, 0xff, 0x0a, 0xfe, 0xc0, 0x00};

/* 3. Charging 0~99% */
static u8 mode3_red[] = {0x40, 0x0D, 0x44, 0x0C, 0x24, 0x32, 0x24, 0x32, 0x66, 0x00, 0x24, 0xB2, 0x24, 0xB2, 0x44, 0x8C};

/* 4. Charging 100% */
static u8 mode4_green[] = {0x40, 0x80};

/* 5. Charging 16~99% : Not Use */
static u8 mode5_red[] = {0x40, 0x19, 0x27, 0x19, 0xe0, 0x04, 0x0c, 0x65, 0xe0, 0x04, 0x0c, 0x65, 0xe0, 0x04, 0x0c, 0xe5, 0xe0, 0x04, 0x0c, 0xe5, 0xe0, 0x04, 0x29, 0x98, 0xe0, 0x04, 0x5a, 0x00};
static u8 mode5_green[] = {0x40, 0x0c, 0x43, 0x0b, 0xe0, 0x80, 0x19, 0x30, 0xe0, 0x80, 0x19, 0x30, 0xe0, 0x80, 0x19, 0xb0, 0xe0, 0x80, 0x19, 0xb0, 0xe0, 0x80, 0x43, 0x8b, 0xe0, 0x80, 0x5a, 0x00};

/* 6. Power Off */
static u8 mode6_red[] = {0xE0, 0x0C, 0x40, 0x00, 0x0C, 0x2F, 0x06, 0x28, 0x05, 0x2D, 0x06, 0x2A, 0x06, 0x25, 0x03, 0xDC, 0x02, 0xFA, 0x48, 0x00, 0x03, 0x54, 0x44, 0x01, 0x23, 0x06, 0x31, 0x84, 0x06, 0xA8, 0x0C, 0xAF};
static u8 mode6_green[] = {0xE0, 0x80, 0x40, 0x00, 0x52, 0x00, 0x0B, 0x15, 0x05, 0x2D, 0x03, 0x48, 0x03, 0x4B, 0x09, 0x1B, 0x02, 0x63, 0x19, 0x89, 0x03, 0xCA, 0x04, 0xC1, 0x05, 0xB2, 0x06, 0xA6, 0x12, 0x8D, 0x52, 0x00};
static u8 mode6_blue[] = {0xE0, 0x80, 0x40, 0x00, 0x12, 0xFE, 0x40, 0xC0, 0x0A, 0x18, 0x06, 0xA6, 0x06, 0xAA, 0x03, 0xCF, 0x04, 0xB6, 0x52, 0x00,};

/* 7. Missed Notification */
static u8 mode7_green[] = {0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xd4, 0x02, 0xd4, 0x02, 0xd4, 0x48, 0x00, 0x40, 0xff, 0x02, 0xd4, 0x02, 0xd4, 0x02, 0xd4, 0x25, 0xfe};

/* 8. Favorite Missed Notification */
static u8 mode8_red[] = {0x40, 0x00, 0x10, 0xFE, 0x40, 0xFF, 0x4F, 0x00, 0x05, 0xE5, 0xE0, 0x0C, 0x05, 0xE5, 0xE0, 0x0C, 0x40, 0xFF, 0x4F, 0x00, 0x04, 0xFF, 0xE0, 0x0C, 0x04, 0xFE, 0xE0, 0x0C, 0x1A, 0xFE,};
static u8 mode8_green[] = {0x40, 0x00, 0x10, 0xFE, 0x40, 0x51, 0x4F, 0x00, 0x0E, 0xA0, 0xE0, 0x80, 0x0F, 0x9F, 0xE0, 0x80, 0x40, 0x51, 0x4F, 0x00, 0x0B, 0xA8, 0xE0, 0x80, 0x0C, 0xA7, 0xE0, 0x80, 0x1A, 0xFE,};
static u8 mode8_blue[] = {0x40, 0x00, 0x10, 0xFE, 0x40, 0xFF, 0x08, 0xBA, 0x06, 0xCD, 0xE0, 0x80, 0x06, 0xCD, 0xE0, 0x80, 0x40, 0x8B, 0x09, 0x38, 0x05, 0xE1, 0xE0, 0x80, 0x05, 0xE1, 0xE0, 0x80, 0x1A, 0xFE,};

/*[pattern_id : 15, power off Charging100, brightness 50%]*/
#if 1 /*pattern 1 and off time 5sec*/
static u8 mode4_green_50[]={0x40, 0x00, 0x3f, 0x19, 0x23, 0x33, 0x24, 0x32, 0x66, 0x00, 0x24, 0xb2, 0x23, 0xb3, 0x3f, 0x99, 0x7f, 0x00, 0xa2, 0x98};
#else /*pattern 1 and off time 10sec*/
static u8 mode4_green_50[]={0x40, 0x00, 0x3f, 0x19, 0x23, 0x33, 0x24, 0x32, 0x66, 0x00, 0x24, 0xb2, 0x23, 0xb3, 0x3f, 0x99, 0x7f, 0x00, 0xa5, 0x18};
#endif

/*[pattern_id : 16, power off Charging0_99, brightness 50%]*/
static u8 mode3_red_50[]={0x40, 0x0d, 0x44, 0x0c, 0x23, 0x33, 0x24, 0x32, 0x66, 0x00, 0x24, 0xb2, 0x23, 0xb3, 0x44, 0x8c};

/*[pattern_id : 17, MissedNoti(pink)]*/
static u8 mode9_red[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xff, 0xe2, 0x00, 0x02, 0xfe, 0xe2, 0x00, 0x48, 0x00, 0x40, 0xff, 0x02, 0xff, 0xe2, 0x00, 0x02, 0xfe, 0xe2, 0x00, 0x25, 0xfe};
static u8 mode9_green[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0x66, 0x06, 0xb2, 0xe2, 0x00, 0x06, 0xb2, 0xe2, 0x00, 0x48, 0x00, 0x40, 0x66, 0x06, 0xb2, 0xe2, 0x00, 0x06, 0xb2, 0xe2, 0x00, 0x25, 0xfe};
static u8 mode9_blue[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0x73, 0x04, 0xd5, 0xe0, 0x06, 0x0b, 0x9c, 0xe0, 0x06, 0x48, 0x00, 0x40, 0x73, 0x04, 0xd5, 0xe0, 0x06, 0x0b, 0x9c, 0xe0, 0x06, 0x25, 0xfe};

/*[pattern_id : 18, MissedNoti(blue)]*/
static u8 mode10_red[]={};
static u8 mode10_green[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0x7a, 0x05, 0xbc, 0xe0, 0x08, 0x05, 0xbc, 0xe0, 0x08, 0x48, 0x00, 0x40, 0x7a, 0x05, 0xbc, 0xe0, 0x08, 0x05, 0xbc, 0xe0, 0x08, 0x25, 0xfe};
static u8 mode10_blue[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xfe, 0xe1, 0x00, 0x48, 0x00, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xfe, 0xe1, 0x00, 0x25, 0xfe};

/*[pattern_id : 19, MissedNoti(orange)]*/
static u8 mode11_red[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xfe, 0xe1, 0x00, 0x48, 0x00, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xfe, 0xe1, 0x00, 0x25, 0xfe};
static u8 mode11_green[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0x66, 0x06, 0xb2, 0xe0, 0x02, 0x06, 0xb2, 0xe0, 0x02, 0x48, 0x00, 0x40, 0x66, 0x06, 0xb2, 0xe0, 0x02, 0x06, 0xb2, 0xe0, 0x02, 0x25, 0xfe};
static u8 mode11_blue[]={};

/*[pattern_id : 20, MissedNoti(yellow)]*/
static u8 mode12_red[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xfe, 0xe1, 0x00, 0x48, 0x00, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xfe, 0xe1, 0x00, 0x25, 0xfe};
static u8 mode12_green[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xe6, 0x02, 0xff, 0x3d, 0x00, 0xe0, 0x02, 0x02, 0xff, 0xe0, 0x02, 0x48, 0x00, 0x40, 0xe6, 0x02, 0xff, 0x3d, 0x00, 0xe0, 0x02, 0x02, 0xff, 0xe0, 0x02, 0x25, 0xfe};
static u8 mode12_blue[]={};

/*[pattern_id : 29, MissedNoti(turquoise)]*/
static u8 mode13_red[]={};
static u8 mode13_green[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xfe, 0xe2, 0x00, 0x02, 0xfe, 0xe2, 0x00, 0x48, 0x00, 0x40, 0xff, 0x02, 0xff, 0xe2, 0x00, 0x02, 0xfe, 0xe2, 0x00, 0x25, 0xfe};
static u8 mode13_blue[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0x7a, 0x06, 0xb2, 0xe0, 0x06, 0x06, 0xb2, 0xe0, 0x06, 0x48, 0x00, 0x40, 0x7a, 0x06, 0xb2, 0xe0, 0x06, 0x06, 0xb2, 0xe0, 0x06, 0x25, 0xfe};


/*[pattern_id : 30, MissedNoti(purple)]*/
static u8 mode14_red[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xb3, 0x06, 0xb2, 0xe1, 0x00, 0x02, 0xff, 0xe1, 0x00, 0x48, 0x00, 0x40, 0xb3, 0x06, 0xb2, 0xe1, 0x00, 0x02, 0xff, 0xe1, 0x00, 0x25, 0xfe};
static u8 mode14_green[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0x19, 0x06, 0xb2, 0xe0, 0x0a, 0x02, 0xff, 0xe0, 0x0a, 0x48, 0x00, 0x40, 0x19, 0x06, 0xb2, 0xe0, 0x0a, 0x02, 0xff, 0xe0, 0x0a, 0x25, 0xfe};
static u8 mode14_blue[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xff, 0xe1, 0x00, 0x48, 0x00, 0x40, 0xff, 0x02, 0xff, 0xe1, 0x00, 0x02, 0xff, 0xe1, 0x00, 0x25, 0xfe};


/*[pattern_id : 31, MissedNoti(red)]*/
static u8 mode15_red[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xd4, 0x02, 0xd4, 0x02, 0xd4, 0x48, 0x00, 0x40, 0xff, 0x02, 0xd4, 0x02, 0xd4, 0x02, 0xd4, 0x25, 0xfe};
static u8 mode15_green[]={};
static u8 mode15_blue[]={};

/*[pattern_id : 32, MissedNoti(lime)]*/
static u8 mode16_red[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0x99, 0x06, 0xb2, 0xe0, 0x04, 0x02, 0xff, 0xe0, 0x04, 0x48, 0x00, 0x40, 0x99, 0x06, 0xb2, 0xe0, 0x04, 0x02, 0xff, 0xe0, 0x04, 0x25, 0xfe};
static u8 mode16_green[]={0x40, 0x00, 0x10, 0xfe, 0x40, 0xff, 0x02, 0xff, 0xe0, 0x80, 0x02, 0xff, 0xe0, 0x80, 0x48, 0x00, 0x40, 0xff, 0x02, 0xff, 0xe0, 0x80, 0x02, 0xff, 0xe0, 0x80, 0x25, 0xfe};
static u8 mode16_blue[]={};

/* [pattern_id : 37 ID_URGENT_CALL_MISSED_NOTI */
static u8 mode18_red[]={0x10, 0xFE, 0x40, 0xFF, 0x46, 0x00, 0x01, 0xFE, 0x01, 0xFF, 0x44, 0x00, 0xA1, 0x01, 0xE0, 0x04, 0x6E, 0x00, 0xE0, 0x04};
static u8 mode18_green[]={0x10, 0xFE, 0x51, 0x00, 0x40, 0x7F, 0x46, 0x00, 0x01, 0xFE, 0x01, 0xFF, 0x54, 0x00, 0xE0, 0x80, 0x6E, 0x00, 0xE0, 0x80};
static u8 mode18_blue[]={};

static struct lp5521_led_pattern board_led_patterns[] = {
	{
		.r = mode1_red,
		.g = mode1_green,
		.b = mode1_blue,
		.size_r = ARRAY_SIZE(mode1_red),
		.size_g = ARRAY_SIZE(mode1_green),
		.size_b = ARRAY_SIZE(mode1_blue),
	},
	{
		.r = mode2_red,
		.g = mode2_green,
		.b = mode2_blue,
		.size_r = ARRAY_SIZE(mode2_red),
		.size_g = ARRAY_SIZE(mode2_green),
		.size_b = ARRAY_SIZE(mode2_blue),
	},
	{
		.r = mode3_red,
		//.g = mode3_green,
		//.b = mode3_blue,
		.size_r = ARRAY_SIZE(mode3_red),
		//.size_g = ARRAY_SIZE(mode3_green),
		//.size_b = ARRAY_SIZE(mode3_blue),
	},
	{
		//.r = mode4_red,
		.g = mode4_green,
		//.b = mode4_blue,
		//.size_r = ARRAY_SIZE(mode4_red),
		.size_g = ARRAY_SIZE(mode4_green),
		//.size_b = ARRAY_SIZE(mode4_blue),
	},
	{
		.r = mode5_red,
		.g = mode5_green,
		//.b = mode5_blue,
		.size_r = ARRAY_SIZE(mode5_red),
		.size_g = ARRAY_SIZE(mode5_green),
		//.size_b = ARRAY_SIZE(mode5_blue),
	},
	{
		.r = mode6_red,
		.g = mode6_green,
		.b = mode6_blue,
		.size_r = ARRAY_SIZE(mode6_red),
		.size_g = ARRAY_SIZE(mode6_green),
		.size_b = ARRAY_SIZE(mode6_blue),
	},
	{
		//.r = mode7_red,
		.g = mode7_green,
		//.b = mode7_blue,
		//.size_r = ARRAY_SIZE(mode7_red),
		.size_g = ARRAY_SIZE(mode7_green),
		//.size_b = ARRAY_SIZE(mode7_blue),
	},
	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_ALARM = 8 */
	},
	{
		/* ID_CALL_01 = 9 */
	},
	{
		/* ID_CALL_02 = 10 */
	},
	{
		/* ID_CALL_03 = 11 */
	},
	{
		/* ID_VOLUME_UP = 12 */
	},
	{
		/* ID_VOLUME_DOWN = 13 */
	},
	{
		/* ID_FAVORITE_MISSED_NOTI = 14 */
		.r = mode8_red,
		.g = mode8_green,
		.b = mode8_blue,
		.size_r = ARRAY_SIZE(mode8_red),
		.size_g = ARRAY_SIZE(mode8_green),
		.size_b = ARRAY_SIZE(mode8_blue),
	},
	{
		/* CHARGING_100_FOR_ATT = 15 (use chargerlogo, only AT&T) */
		.g = mode4_green_50,
		.size_g = ARRAY_SIZE(mode4_green_50),
	},
	{
		/* CHARGING_FOR_ATT = 16 (use chargerlogo, only AT&T) */
		.r = mode3_red_50,
		.size_r = ARRAY_SIZE(mode3_red_50),
	},
	{
		/* ID_MISSED_NOTI_PINK = 17 */
		.r = mode9_red,
		.g = mode9_green,
		.b = mode9_blue,
		.size_r = ARRAY_SIZE(mode9_red),
		.size_g = ARRAY_SIZE(mode9_green),
		.size_b = ARRAY_SIZE(mode9_blue),
	},
	{
		/* ID_MISSED_NOTI_BLUE = 18 */
		.r = mode10_red,
		.g = mode10_green,
		.b = mode10_blue,
		.size_r = ARRAY_SIZE(mode10_red),
		.size_g = ARRAY_SIZE(mode10_green),
		.size_b = ARRAY_SIZE(mode10_blue),
	},
	{
		/* ID_MISSED_NOTI_ORANGE = 19 */
		.r = mode11_red,
		.g = mode11_green,
		.b = mode11_blue,
		.size_r = ARRAY_SIZE(mode11_red),
		.size_g = ARRAY_SIZE(mode11_green),
		.size_b = ARRAY_SIZE(mode11_blue),
	},
	{
		/* ID_MISSED_NOTI_YELLOW = 20 */
		.r = mode12_red,
		.g = mode12_green,
		.b = mode12_blue,
		.size_r = ARRAY_SIZE(mode12_red),
		.size_g = ARRAY_SIZE(mode12_green),
		.size_b = ARRAY_SIZE(mode12_blue),
	},
	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_INCALL_PINK = 21 */
	},
	{
		/* ID_INCALL_BLUE = 22 */
	},
	{
		/* ID_INCALL_ORANGE = 23 */
	},
	{
		/* ID_INCALL_YELLOW = 24 */
	},
	{
		/* ID_INCALL_TURQUOISE = 25 */
	},
	{
		/* ID_INCALL_PURPLE = 26 */
	},
	{
		/* ID_INCALL_RED = 27 */
	},
	{
		/* ID_INCALL_LIME = 28 */
	},
	{
		/* ID_MISSED_NOTI_TURQUOISE = 29 */
		.r = mode13_red,
		.g = mode13_green,
		.b = mode13_blue,
		.size_r = ARRAY_SIZE(mode13_red),
		.size_g = ARRAY_SIZE(mode13_green),
		.size_b = ARRAY_SIZE(mode13_blue),
	},
	{
		/* ID_MISSED_NOTI_PURPLE = 30 */
		.r = mode14_red,
		.g = mode14_green,
		.b = mode14_blue,
		.size_r = ARRAY_SIZE(mode14_red),
		.size_g = ARRAY_SIZE(mode14_green),
		.size_b = ARRAY_SIZE(mode14_blue),
	},
	{
		/* ID_MISSED_NOTI_RED = 31 */
		.r = mode15_red,
		.g = mode15_green,
		.b = mode15_blue,
		.size_r = ARRAY_SIZE(mode15_red),
		.size_g = ARRAY_SIZE(mode15_green),
		.size_b = ARRAY_SIZE(mode15_blue),
	},
	{
		/* ID_MISSED_NOTI_LIME = 32 */
		.r = mode16_red,
		.g = mode16_green,
		.b = mode16_blue,
		.size_r = ARRAY_SIZE(mode16_red),
		.size_g = ARRAY_SIZE(mode16_green),
		.size_b = ARRAY_SIZE(mode16_blue),
	},
	{
		/* ID_NONE = 33 */
	},
	{
		/* ID_NONE = 34 */
	},
	{
		/* ID_INCALL = 35 */
	},
	{
		/* ID_NONE = 36 */
	},
	{
		/* ID_URGENT_CALL_MISSED_NOTI = 37 */
		.r = mode18_red,
		.g = mode18_green,
		.b = mode18_blue,
		.size_r = ARRAY_SIZE(mode18_red),
		.size_g = ARRAY_SIZE(mode18_green),
		.size_b = ARRAY_SIZE(mode18_blue),
	},
};
#endif



static struct lp5521_led_config lp5521_rgb_led_config[] = {
       [0] = {
	       .name = "red",
	       .chan_nr = 0,
	       .led_current = 255,
	       .max_current = 255,
       },
       [1] = {
	       .name = "green",
	       .chan_nr = 1,
	       .led_current = 255,
	       .max_current = 255,
       },
       [2] = {
	       .name = "blue",
	       .chan_nr = 2,
	       .led_current = 255,
	       .max_current = 255,
       }
};

static int lp5521_setup_resources(void)
{
	/* setup HW resources */
	int rc = 0;
	struct pm_gpio param = {
		.direction = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value = 0,
		.pull = PM_GPIO_PULL_NO,
		.vin_sel = PM_GPIO_VIN_S4,
		.out_strength = PM_GPIO_STRENGTH_HIGH,
		.function = PM_GPIO_FUNC_1,
		.inv_int_pol = 0,
	};

	gpio_tlmm_config(GPIO_CFG(GPIO_RGB_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_RGB_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	
	#if !defined (CONFIG_MACH_MSM8960_FX1E) /* jjm_fm_rgb */
	rc = gpio_request(PM8921_GPIO_PM_TO_SYS(PM8921_GPIO_RGB_CLK), "FM_RCLK");
	if (rc)
		pr_err("%s: Error requesting GPIO %d, ret %d\n", __func__, PM8921_GPIO_RGB_CLK, rc);
	#endif

	rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(PM8921_GPIO_RGB_CLK), &param);
	if (rc)
		pr_err("%s: Failed to configure gpio %d, ret %d\n", __func__, PM8921_GPIO_RGB_CLK, rc);

	return rc;
}

static void lp5521_release_resources(void)
{
	/* Release HW resources */
	// TBD
}

static void lp5521_enable(bool state)
{
	/* Control of chip enable signal */
	if(state)
	{
		gpio_tlmm_config(GPIO_CFG(GPIO_RGB_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(GPIO_RGB_EN, 1);
	}
	else
	{
		gpio_tlmm_config(GPIO_CFG(GPIO_RGB_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(GPIO_RGB_EN, 0);
	}
}

static struct lp5521_platform_data lp5521_rgb_led_data = {
	.led_config = &lp5521_rgb_led_config[0],
	.num_channels = 3,
	.clock_mode = LP5521_CLOCK_EXT,
	.setup_resources = lp5521_setup_resources,
	.release_resources = lp5521_release_resources,
	.update_config = LP5521_CONFIGS,
	#if 1 /* jjm_pattern */
	.patterns = board_led_patterns,
	.num_patterns = ARRAY_SIZE(board_led_patterns),
	#endif
	.enable = lp5521_enable,
	.label = "lp5521",
};

static struct i2c_board_info rgb_led_i2c_info[] = {
	{
		I2C_BOARD_INFO("lp5521", I2C_SLAVE_ADDR_LP5521),
		.platform_data = &lp5521_rgb_led_data,
		.irq = MSM_GPIO_TO_INT(GPIO_RGB_INT),
	}
};

static struct i2c_registry rgb_led __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8960_GSBI5_QUP_I2C_BUS_ID,
		rgb_led_i2c_info,
		ARRAY_SIZE(rgb_led_i2c_info),
};
#endif

void __init lge_add_misc_devices(void)
{

#ifdef CONFIG_ANDROID_VIBRATOR

	INFO_MSG("%s\n", __func__);

	vibrator_gpio_init();

	platform_add_devices(misc_devices, ARRAY_SIZE(misc_devices));
#endif


#ifdef CONFIG_SII8334_MHL_TX
       mhl_gpio_init();
       platform_device_register(&mhl_sii833x_device);
       /* force power on - never power down */
       mhl_power_onoff(1, 0);
       lge_add_i2c_mhl_device();
#endif

#ifdef CONFIG_BU52031NVX
	platform_device_register(&cradle_device);
#endif

/* jjm_rgb */
#ifdef CONFIG_LEDS_LP5521
	i2c_register_board_info(rgb_led.bus, rgb_led.info, rgb_led.len);
#endif
}


