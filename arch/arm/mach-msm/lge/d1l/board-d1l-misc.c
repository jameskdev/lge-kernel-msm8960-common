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
#include CONFIG_BOARD_HEADER_FILE

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

#ifdef CONFIG_ANDROID_VIBRATOR
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
#endif /* CONFIG_ANDROID_VIBRATOR */

static struct platform_device *misc_devices[] __initdata = {
#ifdef CONFIG_ANDROID_VIBRATOR
	&android_vibrator_device,
#endif
};

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
#if defined(CONFIG_MACH_MSM8960_LGPS3)
	rc = gpio_request(MHL_PWR_EN, "mhl_pwr_en");
	if (rc < 0) {
		pr_err("failed to request mhl_pwr_en gpio\n");
		goto error4;
	}
	rc = gpio_direction_output(MHL_PWR_EN, 0);
	if (rc < 0) {
		pr_err("failed to request mhl_pwr_en gpio\n");
		goto error4;
	}
error4:
	gpio_free(MHL_PWR_EN);
#elif defined(CONFIG_MACH_MSM8960_D1LV)
	if (lge_get_board_revno() < HW_REV_B) {
		rc = gpio_request(MHL_PWR_EN, "mhl_pwr_en");
		if (rc < 0) {
			pr_err("failed to request mhl_pwr_en gpio\n");
			goto error4;
		}
		rc = gpio_direction_output(MHL_PWR_EN, 0);
		if (rc < 0) {
			pr_err("failed to request mhl_pwr_en gpio\n");
			goto error4;
		}
		error4:
			gpio_free(MHL_PWR_EN);
	}
#endif
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

#if defined(CONFIG_MACH_MSM8960_D1LV)
	if (pm_ctrl)
		return -EPERM;
#elif defined(CONFIG_MACH_MSM8960_D1LA)
	if (pm_ctrl && lge_get_board_revno() >= HW_REV_D)
		return -EPERM;
#endif

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
#if defined(CONFIG_MACH_MSM8960_LGPS3)
		gpio_set_value(MHL_PWR_EN, 1);
		msleep(100);
#elif defined(CONFIG_MACH_MSM8960_D1LV)
		if (lge_get_board_revno() < HW_REV_B) {
			gpio_set_value(MHL_PWR_EN, 1);
			msleep(100);
		}
#endif
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
#if defined(CONFIG_MACH_MSM8960_LGPS3)
		gpio_set_value(MHL_PWR_EN, 0);
		msleep(100);
#elif defined(CONFIG_MACH_MSM8960_D1LV)
		if (lge_get_board_revno() < HW_REV_B) {
			gpio_set_value(MHL_PWR_EN, 0);
			msleep(100);
		}
#endif
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

void __init lge_add_misc_devices(void)
{
	int vib_flag = 0;

	INFO_MSG("%s\n", __func__);

	vibrator_gpio_init();

#ifdef CONFIG_ANDROID_VIBRATOR
	if (vib_flag == 0) {
		platform_add_devices(misc_devices, ARRAY_SIZE(misc_devices));
	}
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

}


