#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include "devices.h"
#include CONFIG_BOARD_HEADER_FILE

#ifdef CONFIG_SWITCH_FSA8008
#include "../../../../sound/soc/codecs/wcd9310.h" // 2012-02-06, mint.choi@lge.com. for fsa8008 tabla_codec_micbias2_ctl API
#endif
//LGE_BSP_AUDIO, hanna.oh@lge.com, 2012-09-03, si4709 FM Radio porting
#ifdef CONFIG_RADIO_SI470X
#include <linux/regulator/consumer.h>
#include "../../../../drivers/media/radio/si470x/radio-si470x.h"
#define FM_RCLK (PM8921_GPIO_PM_TO_SYS(43))
#endif

// Start LGE_BSP_AUDIO, jeremy.pi@lge.com, 2012-07-20, earSmart eS310 Voice Processor.
#if defined(CONFIG_EARSMART_310)

#if defined(CONFIG_MACH_MSM8960_VU2)
// In fx1sk_skt_rk, GPIO #10 : Audio EARSMART -> VIBRATOR
#else
#include <linux/regulator/consumer.h>
#include <sound/es310.h>
#endif

#endif /* CONFIG_EARSMART_310 */
// End LGE_BSP_AUDIO, jeremy.pi@lge.com, 2012-07-20, earSmart eS310 Voice Processor.

#ifdef CONFIG_SND_SOC_TPA2028D
#include <sound/tpa2028d.h>

/* Add the I2C driver for Audio Amp, ehgrace.kim@lge.cim, 06/13/2011 */
#define TPA2028D_ADDRESS (0xB0>>1)

#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)
#define MSM_AMP_EN (PM8921_GPIO_PM_TO_SYS(19))

#if defined(CONFIG_MACH_MSM8960_D1LA) || defined(CONFIG_MACH_MSM8960_LGPS3)
int msm8921_l29_poweron(void)
{
	int rc = 0;
	struct regulator *audio_reg;

	audio_reg = regulator_get(NULL, "8921_l29");

	if (IS_ERR(audio_reg)) {
		rc = PTR_ERR(audio_reg);
		pr_err("%s:regulator get failed rc=%d\n", __func__, rc);
		return -1;
	}
	regulator_set_voltage(audio_reg, 0, 1800000);
	rc = regulator_enable(audio_reg);
	if (rc)
		pr_err("%s: regulator_enable failed rc =%d\n", __func__, rc);

	regulator_put(audio_reg);

	return rc;
}
#endif

int amp_power(bool on)
{
    return 0;
}

int amp_enable(int on_state)
{
	int err = 0;
	static int init_status = 0;
	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	if (init_status == 0) {
		err = gpio_request(MSM_AMP_EN, "AMP_EN");
		if (err)
			pr_err("%s: Error requesting GPIO %d\n", __func__, MSM_AMP_EN);

		err = pm8xxx_gpio_config(MSM_AMP_EN, &param);
		if (err)
			pr_err("%s: Failed to configure gpio %d\n", __func__, MSM_AMP_EN);
		else
			init_status++;
	}

	switch (on_state) {
	case 0:
		err = gpio_direction_output(MSM_AMP_EN, 0);
		printk(KERN_INFO "%s: AMP_EN is set to 0\n", __func__);
		break;
	case 1:
		err = gpio_direction_output(MSM_AMP_EN, 1);
		printk(KERN_INFO "%s: AMP_EN is set to 1\n", __func__);
		break;
	case 2:
	#ifdef CONFIG_MACH_MSM8960_D1L
			return 0;
	#else
		printk(KERN_INFO "%s: amp enable bypass(%d)\n", __func__, on_state);
		err = 0;
	#endif
		break;

	default:
		pr_err("amp enable fail\n");
		err = 1;
		break;
	}
	return err;
}

static struct audio_amp_platform_data amp_platform_data =  {
	.enable = amp_enable,
	.power = amp_power,
	.agc_compression_rate = AGC_COMPRESIION_RATE,
	.agc_output_limiter_disable = AGC_OUTPUT_LIMITER_DISABLE,
	.agc_fixed_gain = AGC_FIXED_GAIN,
};

static struct i2c_board_info lge_i2c_amp_info[] = {
	{
		I2C_BOARD_INFO("tpa2028d_amp", TPA2028D_ADDRESS),
		.platform_data = &amp_platform_data,
	}
};

static struct i2c_registry d1l_i2c_audiosubsystem __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8960_GSBI9_QUP_I2C_BUS_ID,
	lge_i2c_amp_info,
	ARRAY_SIZE(lge_i2c_amp_info),
};

static void __init lge_add_i2c_audiosubsystem_devices(void)
{
	i2c_register_board_info(d1l_i2c_audiosubsystem.bus,
				d1l_i2c_audiosubsystem.info,
				d1l_i2c_audiosubsystem.len);
}
#endif
//LGE_BSP_AUDIO, hanna.oh@lge.com, 2012-09-03, si4709 FM Radio porting
#ifdef CONFIG_RADIO_SI470X
static struct si470x_fmradio_platform_data fmradio_platform_data =  {
	.power = fmradio_power,
};
static struct i2c_board_info lge_i2c_fmradio_info[] = {
	{
		I2C_BOARD_INFO("si470x", SI470X_ADDRESS),
		.platform_data = &fmradio_platform_data,
	}
};

static struct i2c_registry vu2_i2c_fmradio_revA __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8960_GSBI8_QUP_I2C_BUS_ID,
	lge_i2c_fmradio_info,
	ARRAY_SIZE(lge_i2c_fmradio_info),
};

static struct i2c_registry vu2_i2c_fmradio __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8960_GSBI1_QUP_I2C_BUS_ID,
	lge_i2c_fmradio_info,
	ARRAY_SIZE(lge_i2c_fmradio_info),
};


static void __init lge_add_i2c_fmradio_devices(void)
{
	printk(KERN_INFO "lge_add_i2c_fmradio_devices\n");
	if (lge_get_board_revno() == HW_REV_A)
	{
		i2c_register_board_info(vu2_i2c_fmradio_revA.bus,
					vu2_i2c_fmradio_revA.info,
					vu2_i2c_fmradio_revA.len);
	}
	else
	{
		i2c_register_board_info(vu2_i2c_fmradio.bus,
					vu2_i2c_fmradio.info,
					vu2_i2c_fmradio.len);	
	}
}

#endif
#ifdef CONFIG_SWITCH_FSA8008
static struct fsa8008_platform_data lge_hs_pdata = {
	.switch_name = "h2w",
	.keypad_name = "hs_detect",
	.key_code = KEY_MEDIA,
	.gpio_detect = GPIO_EAR_SENSE_N,
	.gpio_mic_en = GPIO_EAR_MIC_EN,
	.gpio_jpole  = GPIO_EARPOL_DETECT,
	.gpio_key    = GPIO_EAR_KEY_INT,
	.latency_for_detection = 75, /* 75 -> 10, 2012.03.23 donggyun.kim - spec : 4.5 ms-> 2013.02.05,  75msec revert, mint.choi */
	.set_headset_mic_bias = tabla_codec_micbias2_ctl, // 2012-02-06, mint.choi@lge.com. for fsa8008 tabla_codec_micbias2_ctl API
};

static struct platform_device lge_hsd_device = {
   .name = "fsa8008",
   .id   = -1,
   .dev = {
      .platform_data = &lge_hs_pdata,
   },
};

static int __init lge_hsd_fsa8008_init(void)
{
	printk(KERN_INFO "lge_hsd_fsa8008_init\n");
	return platform_device_register(&lge_hsd_device);
}

static void __exit lge_hsd_fsa8008_exit(void)
{
	printk(KERN_INFO "lge_hsd_fsa8008_exit\n");
	platform_device_unregister(&lge_hsd_device);
}
#endif

// Start LGE_BSP_AUDIO, jeremy.pi@lge.com, 2012-07-20, earSmart eS310 Voice Processor.
#if defined(CONFIG_EARSMART_310)

#if defined(CONFIG_MACH_MSM8960_VU2)
// In fx1sk_skt_rk, GPIO #10 : Audio EARSMART -> VIBRATOR
#else
int es310_gpio_init(void)
{
	int rc;
	printk(KERN_INFO "%s\n", __func__);

	rc = gpio_request(GPIO_ES310_AUD_CODEC_LDO_EN, "es310_aud_codec_ldo_en");
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_request es310_aud_codec_ldo_en\n", __func__);
		goto gpio_error3;
	}

	rc = gpio_direction_output(GPIO_ES310_AUD_CODEC_LDO_EN, 0);
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_direction_output es310_aud_codec_ldo_en\n", __func__);
		goto gpio_error3;
	}

	rc = gpio_request(GPIO_ES310_RESET_PIN, "es310_reset");
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_request es310_reset\n", __func__);
		goto gpio_error1;
	}

	rc = gpio_direction_output(GPIO_ES310_RESET_PIN, 0);
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_direction_output es310_reset\n", __func__);
		goto gpio_error1;
	}


	rc = gpio_request(GPIO_ES310_WAKEUP_PIN, "es310_wakeup");
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_request es310_wakeup\n", __func__);
		goto gpio_error2;
	}

	rc = gpio_direction_output(GPIO_ES310_WAKEUP_PIN, 0);
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_direction_output es310_wakeup\n", __func__);
		goto gpio_error2;
	}

	rc = gpio_request(GPIO_ES310_UART_TXD, "es310_uart_txd");
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_request es310_uart_txd\n", __func__);
		goto gpio_error2;
	}

	rc = gpio_direction_output(GPIO_ES310_UART_TXD, 0);
	if (rc < 0) {
		printk(KERN_INFO "%s: Failed to gpio_direction_output es310_uart_txd\n", __func__);
		goto gpio_error2;
	}

	gpio_direction_output(GPIO_ES310_AUD_CODEC_LDO_EN, 1);
	gpio_direction_output(GPIO_ES310_RESET_PIN, 1);
	gpio_direction_output(GPIO_ES310_WAKEUP_PIN, 1);

	gpio_error1:
		gpio_free(GPIO_ES310_RESET_PIN);
	gpio_error2:
		gpio_free(GPIO_ES310_WAKEUP_PIN);
	gpio_error3:
		gpio_free(GPIO_ES310_AUD_CODEC_LDO_EN);

	return rc;
}

static int es310_power(bool on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_ldo29;
	static bool power_status = false;
	static bool init_status = false;		

	printk(KERN_INFO "%s() on[%d] power_status[%d] init_status[%d]\n", __func__, on, power_status, init_status);

	if (init_status == false) {
		vreg_ldo29 = regulator_get(NULL, "8921_l29");	/* +V1P8_L29_AUD_IO */
		if (IS_ERR(vreg_ldo29)) {
			printk(KERN_INFO "%s() regulator get of 8921_l29 failed(%ld)\n", __func__, PTR_ERR(vreg_ldo29));

			rc = PTR_ERR(vreg_ldo29);
			return rc;
		}
		rc = regulator_set_voltage(vreg_ldo29,
					1800000, 1800000); // 1.8V
	init_status = true;
	}
	
	if (power_status != on) {
		if (on) {
			rc = regulator_enable(vreg_ldo29);
		} else {
			rc = regulator_disable(vreg_ldo29);
		}
		power_status = on;
	}

	printk(KERN_INFO "%s() on[%d] power_status[%d] init_status[%d]\n", __func__, on, power_status, init_status);
	return rc;
}

static int es310_aud_clk(bool on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_lvs1;
	static bool power_status = false;
	static bool init_status = false;		

	printk(KERN_INFO "%s() on[%d] power_status[%d] init_status[%d]\n", __func__, on, power_status, init_status);

	if (init_status == false) {
		vreg_lvs1 = regulator_get(NULL, "8921_lvs1");	/* +V1P8_LVS1_AUD_CLK */
		if (IS_ERR(vreg_lvs1)) {
			printk(KERN_INFO "%s() regulator get of 8921_lvs1 failed(%ld)\n", __func__, PTR_ERR(vreg_lvs1));

			rc = PTR_ERR(vreg_lvs1);
			return rc;
		}
		rc = regulator_set_voltage(vreg_lvs1,
					1800000, 1800000); // 1.8V
	init_status = true;
	}
	
	if (power_status != on) {
		if (on) {
			rc = regulator_enable(vreg_lvs1);
		} else {
			rc = regulator_disable(vreg_lvs1);
		}
		power_status = on;
	}

	printk(KERN_INFO "%s() on[%d] power_status[%d] init_status[%d]\n", __func__, on, power_status, init_status);
	return rc;
}

static struct es310_platform_data es310_pdata =  {
	.power = es310_power,
	.aud_clk = es310_aud_clk,
};

static struct i2c_board_info i2c_audience_es310_info[] = {
	{
		I2C_BOARD_INFO("es310", 0x3E),
		.platform_data = &es310_pdata,
	}
};

static struct i2c_registry i2c_rev_a_audience_es310 __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8960_GSBI8_QUP_I2C_BUS_ID, // Rev.A only GSBI8, must move to GSBI1
	i2c_audience_es310_info,
	ARRAY_SIZE(i2c_audience_es310_info),
};

static struct i2c_registry i2c_audience_es310 __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8960_GSBI1_QUP_I2C_BUS_ID,
	i2c_audience_es310_info,
	ARRAY_SIZE(i2c_audience_es310_info),
};

static void __init lge_add_i2c_audience_es310_devices(void)
{
	if (lge_get_board_revno() == HW_REV_A)
		i2c_register_board_info(i2c_rev_a_audience_es310.bus,
								i2c_rev_a_audience_es310.info,
								i2c_rev_a_audience_es310.len);
	else
		i2c_register_board_info(i2c_audience_es310.bus,
								i2c_audience_es310.info,
								i2c_audience_es310.len);
}
#endif

#endif /* CONFIG_EARSMART_310 */
// End LGE_BSP_AUDIO, jeremy.pi@lge.com, 2012-07-20, earSmart eS310 Voice Processor.

void __init lge_add_sound_devices(void)
{

#ifdef CONFIG_SND_SOC_TPA2028D
	lge_add_i2c_audiosubsystem_devices();
#endif
//LGE_BSP_AUDIO, hanna.oh@lge.com, 2012-09-03, si4709 FM Radio porting
#ifdef CONFIG_RADIO_SI470X
	lge_add_i2c_fmradio_devices();
#endif
#ifdef CONFIG_SWITCH_FSA8008
	lge_hsd_fsa8008_init();
#endif

// Start LGE_BSP_AUDIO, jeremy.pi@lge.com, 2012-07-20, earSmart eS310 Voice Processor.
#if defined(CONFIG_EARSMART_310)

#if defined(CONFIG_MACH_MSM8960_VU2)
// In fx1sk_skt_rk, GPIO #10 : Audio EARSMART -> VIBRATOR
#else
	es310_power(true);
	es310_aud_clk(true);
	es310_gpio_init();	
	lge_add_i2c_audience_es310_devices();
#endif

#endif /* CONFIG_EARSMART_310 */
// End LGE_BSP_AUDIO, jeremy.pi@lge.com, 2012-07-20, earSmart eS310 Voice Processor.

}
