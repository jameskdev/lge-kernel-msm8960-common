/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/mfd/wcd9xxx/wcd9310_registers.h>

#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include "wcd9310.h"

enum tabla_bandgap_type {
	TABLA_BANDGAP_OFF = 0,
	TABLA_BANDGAP_AUDIO_MODE,
	TABLA_BANDGAP_MBHC_MODE,
};

struct tabla_priv {
	struct snd_soc_codec *codec;
	u32 ref_cnt;
	u32 adc_count;
	u32 rx_count;
	u32 cfilt1_cnt;
	u32 cfilt2_cnt;
	u32 cfilt3_cnt;

	enum tabla_bandgap_type bandgap_type;
	bool clock_active;
	bool config_mode_active;
	bool mbhc_polling_active;
	int buttons_pressed;

	struct tabla_mbhc_calibration *calibration;

	struct snd_soc_jack *headset_jack;
	struct snd_soc_jack *button_jack;

	struct tabla_pdata *pdata;
	u32 anc_slot;
};


void tabla_codec_enable_micbias2(struct snd_soc_codec *codec)
{
#if 1 
	pr_info("%s\n", __func__ );

	snd_soc_write(codec, TABLA_A_MICB_CFILT_2_CTL, 0x80);

	snd_soc_write(codec, TABLA_A_MICB_2_CTL, 0xb6);

	snd_soc_write(codec, TABLA_A_LDO_H_MODE_1, 0xCD);

	snd_soc_write(codec, TABLA_A_MICB_CFILT_2_VAL, 0x68);
#else
#if 0 
	snd_soc_update_bits(codec, TABLA_A_LDO_H_MODE_1, 0x0F, 0x0D);
	snd_soc_update_bits(codec, TABLA_A_TX_COM_BIAS, 0xE0, 0xE0);
	snd_soc_write(codec, TABLA_A_MICB_CFILT_2_VAL, 0x68);

	snd_soc_update_bits(codec, TABLA_A_MICB_2_CTL, 0x60, 0x20);
	snd_soc_update_bits(codec, TABLA_A_MICB_2_CTL, 0x0E, 0x0A);
	snd_soc_update_bits(codec, TABLA_A_MICB_CFILT_2_CTL, 0x80, 0x80);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

#else
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
//	short bias_value;

	tabla->mbhc_polling_active = true;

	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x05, 0x01);

	snd_soc_update_bits(codec, TABLA_A_TX_COM_BIAS, 0xE0, 0xE0);

	snd_soc_write(codec, TABLA_A_MICB_CFILT_2_CTL, 0x00);

	snd_soc_update_bits(codec, TABLA_A_MICB_2_CTL, 0x1F, 0x16);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x84);

	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80, 0x80);
	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x1F, 0x1C);
	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_TEST_CTL, 0x40, 0x40);

	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80, 0x00);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x00);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x6, 0x6);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

	snd_soc_update_bits(codec, TABLA_A_MICB_2_MBHC, 0x10, 0x10);
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x13, 0x01);

	//tabla_codec_calibrate_hs_polling(codec);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B10_CTL, 0xFF);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B9_CTL, 0x00);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B4_CTL, 0x08);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B3_CTL, 0xEE);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B2_CTL, 0xFC);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B1_CTL, 0xCE);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B1_CTL, 3);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B2_CTL, 9);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B3_CTL, 30);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B6_CTL, 120);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_TIMER_B1_CTL, 0x78, 0x58);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_B2_CTL, 11);

	//bias_value = tabla_codec_measure_micbias_voltage(codec, 0);
	snd_soc_write(codec, TABLA_A_MICB_CFILT_2_CTL, 0x40);
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x13, 0x00);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE);

	//tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_OFF);
	snd_soc_write(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x00);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x2, 0x0);
#endif	
#endif
}
EXPORT_SYMBOL_GPL(tabla_codec_enable_micbias2);

