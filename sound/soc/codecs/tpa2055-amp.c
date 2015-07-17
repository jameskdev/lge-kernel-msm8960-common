/* arch/arm/mach-msm/qdsp5v2/tpa2055-amp.c
 *
 * Copyright (C) 2009 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <asm/gpio.h>
#include <asm/system.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
/* BEGIN:0009748        ehgrace.kim@lge.com     2010.10.07*/
/* MOD: modifiy the amp for sonification mode for subsystem audio calibration */
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/ioctls.h>
#include <mach/debug_mm.h>
#include "tpa2055-amp.h"


#define MODULE_NAME	"tpa2055"

#define ICODEC_HANDSET_RX	1
#define ICODEC_HEADSET_ST_RX	2
#define ICODEC_HEADSET_SPK_RX	3
#define ICODEC_SPEAKER_RX	4
#define ICODEC_POWEROFF		5
/* BEGIN:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* MOD: add the call mode */
#define ICODEC_HEADSET_PHONE_RX	6
#define ICODEC_SPEAKER_PHONE_RX	7
/* END:0010882        ehgrace.kim@lge.com     2010.11.15*/
#define	DEBUG_AMP_CTL	1
#define AMP_IOCTL_MAGIC 't'
/* BEGIN:0010385        ehgrace.kim@lge.com     2010.11.08*/
/* MOD: add the get value for hiddenmenu */
#define AMP_SET_DATA	_IOW(AMP_IOCTL_MAGIC, 0, struct amp_cal *)
#define AMP_GET_DATA	_IOW(AMP_IOCTL_MAGIC, 1, struct amp_cal *)
/* END:0010385        ehgrace.kim@lge.com     2010.11.08*/

/* BEGIN:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* MOD: add the call mode */
#define IN1_GAIN 0
#define IN2_GAIN 1
#define SPK_VOL  2
#define HP_LVOL  3
#define HP_RVOL	 4
#define SPK_LIM 5
#define HP_LIM 6

struct amp_cal_type {
	u8 in1_gain;
	u8 in2_gain;
	u8 spk_vol;
	u8 hp_lvol;
	u8 hp_rvol;
	u8 mix_in1_gain;
	u8 mix_in2_gain;
	u8 mix_spk_vol;
	u8 mix_hp_lvol;
	u8 mix_hp_rvol;
	u8 spk_lim;
	u8 hp_lim;
	u8 mix_spk_lim;
	u8 mix_hp_lim;
	u8 call_in1_gain;
	u8 call_in2_gain;
	u8 call_spk_vol;
	u8 call_hp_lvol;
	u8 call_hp_rvol;
	u8 call_spk_lim;
	u8 call_hp_lim;
	u8 call_mix_spk_lim;
	u8 call_mix_hp_lim;
};


/*BEGIN:0011017	ehgrace.kim@lge.com	2010.11.16, 2010.12.06, 2010.12.13, 2011.02.07, 2011.02.17*/
/*MOD:apply the audio calibration */
/*BEGIN:0010363	ehgrace.kim@lge.com	2010.11.01*/
/*MOD:apply the audio calibration for spkand spk&headset */
#if 0
struct amp_cal_type amp_cal_data = { IN1GAIN_0DB, IN2GAIN_0DB, SPK_VOL_M10DB, HPL_VOL_0DB, HPR_VOL_M60DB, IN1GAIN_0DB, IN2GAIN_0DB, SPK_VOL_M1DB, HPL_VOL_0DB, HPR_VOL_M60DB };
#else
//struct amp_cal_type amp_cal_data = { IN1GAIN_0DB, IN2GAIN_12DB, SPK_VOL_M9DB, HPL_VOL_M30DB, HPR_VOL_M30DB, IN1GAIN_0DB, IN2GAIN_12DB, SPK_VOL_M9DB, HPL_VOL_M30DB, HPR_VOL_M30DB };
//struct amp_cal_type amp_cal_data = { IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M10DB, HPR_VOL_M10DB, IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M10DB, HPR_VOL_M10DB };
//struct amp_cal_type amp_cal_data = { IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M13DB, HPR_VOL_M13DB, IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M21DB, HPR_VOL_M21DB };
struct amp_cal_type amp_cal_data = { 
	IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M1DB, HPR_VOL_M1DB, 
	IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M21DB, HPR_VOL_M21DB, 
	SLIMLVL_4P90V, HLIMLVL_1P15V, SLIMLVL_4P90V, HLIMLVL_1P15V,
	IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M6DB, HPR_VOL_M6DB,
	SLIMLVL_4P90V, HLIMLVL_1P15V, SLIMLVL_4P90V, HLIMLVL_1P15V};
#endif
/*END:0010363	ehgrace.kim@lge.com	2010.11.01*/
/*END:0011017	ehgrace.kim@lge.com	2010.11.16, 2010.12.06, 2010.12.13, 2011.02.07, 2011.02.17*/

/* BEGIN:0009753        ehgrace.kim@lge.com     2010.10.22*/
/* MOD: modifiy to delete the first boot noise */
bool first_boot = 1;
/* END:0009753        ehgrace.kim@lge.com     2010.10.22*/
/* END:0009748        ehgrace.kim@lge.com     2010.10.07*/

static uint32_t msm_snd_debug = 1;
module_param_named(debug_mask, msm_snd_debug, uint, 0664);


#if DEBUG_AMP_CTL
#define D(fmt, args...) printk(fmt, ##args)
#else
#define D(fmt, args...) do {} while(0)
#endif

struct amp_data {
	struct i2c_client *client;
};

static struct amp_data *_data = NULL;

int ReadI2C(char reg, char* ret)
{

	unsigned int err;
	unsigned char buf = reg;

	struct i2c_msg msg[2] = {
		{ _data->client->addr, 0, 1, &buf },
		{ _data->client->addr, I2C_M_RD, 1, ret}
	};

	if ((err = i2c_transfer(_data->client->adapter, &msg, 2)) < 0) {
		dev_err(&_data->client->dev, "i2c read error\n");
	}else{
		D(KERN_INFO "%s():i2c read ok:%x\n", __FUNCTION__, reg);
	}

	return 0;

}

int WriteI2C(char reg, char val)
{
	
	int	err;
	unsigned char    buf[2];
	struct i2c_msg	msg = { _data->client->addr, 0, 2, &buf }; 
	
	buf[0] = reg;
	buf[1] = val;

	if ((err = i2c_transfer(_data->client->adapter, &msg, 1)) < 0){
		return -EIO;
	} 
	else {
		return 0;
	}
}


int TPA2055D3_ResetToDefaults(void)
{
	//Can also use this function to switch to Bypass mode
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (BYPASS | SWS));								//~SSM
	fail |= WriteI2C(INPUT_CONTROL, (IN2GAIN_0DB | IN1GAIN_0DB | IN2_SE | IN1_SE));
	fail |= WriteI2C(LIMITER_CONTROL, (ATTACK_2P56MS | RELEASE_451MS));
	fail |= WriteI2C(SPEAKER_OUTPUT, (SLIMLVL_4P90V | SPKOUT_MUTE));					//~SPLIM_EN
	fail |= WriteI2C(HEADPHONE_OUTPUT, (HLIMLVL_1P15V | HPOUT_MUTE));					//~HPLIM_EN
	fail |= WriteI2C(SPEAKER_VOLUME, (SPK_VOL_M10DB));									//~SPK_EN
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_0DB | HP_TRACK));							//~HPL_EN
	fail |= WriteI2C(HP_RIGHT_VOLUME, HPR_VOL_0DB);										//~HPR_EN
	return(fail);
}

int TPA2055D3_PowerUpClassD_IN1(void)
{
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	fail |= WriteI2C(INPUT_CONTROL, (IN1GAIN_0DB | IN1_SE)); 							//Modify for desired IN gain
	fail |= WriteI2C(SPEAKER_VOLUME, (SPK_EN | SPK_VOL_M10DB));							//Modify for desired SP gain
	fail |= WriteI2C(SPEAKER_OUTPUT, SPKOUT_IN1);
	return(fail);
}

/* BEGIN:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* MOD: add the call mode */
/* BEGIN:0009748        ehgrace.kim@lge.com     2010.10.07*/
/* MOD: modifiy the amp for sonification mode for subsystem audio calibration */
/* BEGIN:0009753        ehgrace.kim@lge.com     2010.10.22*/
/* MOD: modifiy to delete the first boot noise */
int TPA2055D3_PowerUpClassD_IN2(void)
{
	int fail=0;

        fail |= WriteI2C(INPUT_CONTROL, (amp_cal_data.in2_gain | IN2_DIFF));    //Modify for desired IN gain 
        fail |= WriteI2C(SPEAKER_VOLUME, (SPK_EN | SPK_VOL_M60DB));
        fail |= WriteI2C(SPEAKER_OUTPUT, SPKOUT_IN2 | SPLIM_EN | amp_cal_data.spk_lim);
        fail |= WriteI2C(SPEAKER_VOLUME, (SPK_EN | amp_cal_data.spk_vol));      //Modify for desired SP gain
        printk("%s : first_boot %d\n", __func__, first_boot);
        if (first_boot == 1){
		fail |= WriteI2C(SUBSYSTEM_CONTROL, (SWS | BYPASS | SSM_EN));
                msleep(100);
                first_boot = 0;
	}
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	return(fail);
}

int TPA2055D3_PowerUpClassD_PHONE_IN2(void)
{
	int fail=0;

        fail |= WriteI2C(INPUT_CONTROL, (amp_cal_data.call_in2_gain | IN2_DIFF));    //Modify for desired IN gain 
        fail |= WriteI2C(SPEAKER_VOLUME, (SPK_EN | SPK_VOL_M60DB));
        fail |= WriteI2C(SPEAKER_OUTPUT, SPKOUT_IN2 | SPLIM_EN | amp_cal_data.call_spk_lim);
        fail |= WriteI2C(SPEAKER_VOLUME, (SPK_EN | amp_cal_data.call_spk_vol));      //Modify for desired SP gain
        printk("%s : first_boot %d\n", __func__, first_boot);
        if (first_boot == 1){
		fail |= WriteI2C(SUBSYSTEM_CONTROL, (SWS | BYPASS | SSM_EN));
                msleep(100);
                first_boot = 0;
	}
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	return(fail);
}
/* END:0009753		ehgrace.kim@lge.com 	2010.10.22*/		
/* END:0010882        ehgrace.kim@lge.com     2010.11.15*/

int TPA2055D3_PowerUpClassD_IN1IN2(void)
{
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	fail |= WriteI2C(INPUT_CONTROL, (IN1GAIN_0DB | IN2GAIN_0DB | IN1_SE | IN2_SE)); 	//Modify for desired IN gain  
	fail |= WriteI2C(SPEAKER_VOLUME, (SPK_EN | SPK_VOL_M10DB));							//Modify for desired SP gain
	fail |= WriteI2C(SPEAKER_OUTPUT, SPKOUT_IN1IN2);
	return(fail);
}

int TPA2055D3_PowerDownClassD(void)
{
	int fail=0;
	fail |= WriteI2C(SPEAKER_VOLUME, SPK_EN);
	fail |= WriteI2C(SPEAKER_OUTPUT, SPKOUT_MUTE);	
	fail |= WriteI2C(SPEAKER_VOLUME, ~SPK_EN & SPK_VOL_M60DB);
	return(fail);
}

/* BEGIN:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* MOD: add the call mode */
int TPA2055D3_PowerUpHP_IN1(void)
{
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	fail |= WriteI2C(INPUT_CONTROL, (amp_cal_data.in1_gain | IN1_SE)); 	//Modify for desired IN gain 
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_M60DB | HPL_EN | HP_TRACK));
	fail |= WriteI2C(HP_RIGHT_VOLUME, (amp_cal_data.hp_rvol | HPR_EN));
	fail |= WriteI2C(HEADPHONE_OUTPUT, HPOUT_IN1 | HPLIM_EN | amp_cal_data.hp_lim);
	fail |= WriteI2C(HP_LEFT_VOLUME, (amp_cal_data.hp_lvol | HPL_EN | HP_TRACK));	//Modify for desired HP gain
	printk("HP : gain %x, volR:%x, volL:%x\n", amp_cal_data.in1_gain, amp_cal_data.hp_rvol, amp_cal_data.hp_lvol);
	return(fail);
}

int TPA2055D3_PowerUpHP_PHONE_IN1(void)
{
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	fail |= WriteI2C(INPUT_CONTROL, (amp_cal_data.call_in1_gain | IN1_SE)); 	//Modify for desired IN gain 
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_M60DB | HPL_EN | HP_TRACK));
	fail |= WriteI2C(HP_RIGHT_VOLUME, (amp_cal_data.call_hp_rvol | HPR_EN));
	fail |= WriteI2C(HEADPHONE_OUTPUT, HPOUT_IN1 | HPLIM_EN | amp_cal_data.call_hp_lim);
	fail |= WriteI2C(HP_LEFT_VOLUME, (amp_cal_data.call_hp_lvol | HPL_EN | HP_TRACK));	//Modify for desired HP gain
	printk("HP : gain %x, volR:%x, volL:%x\n", amp_cal_data.call_in1_gain, amp_cal_data.call_hp_rvol, amp_cal_data.call_hp_lvol);
	return(fail);
}
/* END:0010882        ehgrace.kim@lge.com     2010.11.15*/

int TPA2055D3_PowerUpHP_IN2(void)
{
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	fail |= WriteI2C(INPUT_CONTROL, (IN2GAIN_0DB | IN2_SE)); 				//Modify for desired IN gain 
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_M60DB | HPL_EN | HP_TRACK));
	fail |= WriteI2C(HP_RIGHT_VOLUME, (HPR_VOL_M60DB | HPR_EN));
	fail |= WriteI2C(HEADPHONE_OUTPUT, HPOUT_IN2);
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_0DB | HPL_EN | HP_TRACK));			//Modify for desired HP gain
	return(fail);
}

int TPA2055D3_PowerUpHP_IN1IN2(void)
{
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	fail |= WriteI2C(INPUT_CONTROL, (IN1GAIN_0DB | IN2GAIN_0DB | IN1_SE | IN2_SE)); 					//Modify for desired IN gain 
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_M60DB | HPL_EN | HP_TRACK));
	fail |= WriteI2C(HP_RIGHT_VOLUME, (HPR_VOL_M60DB | HPR_EN));
	fail |= WriteI2C(HEADPHONE_OUTPUT, HPOUT_IN1IN2);
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_0DB | HPL_EN | HP_TRACK));			//Modify for desired HP gain
	return(fail);
}

/* BEGIN:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* MOD: add add the subsystem control for call mode */
int TPA2055D3_PowerUp_IN1IN2(void)
{
	int fail=0;
	fail |= WriteI2C(SUBSYSTEM_CONTROL, (~SWS & ~BYPASS & ~SSM_EN));
	fail |= WriteI2C(INPUT_CONTROL, (amp_cal_data.mix_in1_gain | IN1_SE)|(amp_cal_data.mix_in2_gain | IN2_DIFF)); 	//Modify for desired IN gain 
	fail |= WriteI2C(HP_LEFT_VOLUME, (HPL_VOL_M60DB | HPL_EN | HP_TRACK));
	fail |= WriteI2C(HP_RIGHT_VOLUME, (amp_cal_data.mix_hp_rvol | HPR_EN));
	fail |= WriteI2C(HEADPHONE_OUTPUT, HPOUT_IN1 | HPLIM_EN | amp_cal_data.mix_hp_lim);
	fail |= WriteI2C(HP_LEFT_VOLUME, (amp_cal_data.mix_hp_lvol | HPL_EN | HP_TRACK));	//Modify for desired HP gain
	fail |= WriteI2C(SPEAKER_VOLUME, (SPK_EN | amp_cal_data.mix_spk_vol));	//Modify for desired SP gain
	fail |= WriteI2C(SPEAKER_OUTPUT, SPKOUT_IN2 | SPLIM_EN | amp_cal_data.mix_spk_lim);
/* ehgrace.kim	2010-10-04
   added to delete the noise at first time after booting */
	msleep(10);	
	printk("MIX : HP - gain %x, volR %x, volL %x : SPK - gain %x, vol %x\n", amp_cal_data.mix_in1_gain, amp_cal_data.mix_hp_rvol, amp_cal_data.mix_hp_lvol, amp_cal_data.mix_in2_gain, amp_cal_data.mix_spk_vol);
	return(fail);
}
/* END:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* END:0009748        ehgrace.kim@lge.com     2010.10.07*/

int TPA2055D3_PowerDown_HP(void)
{
	int fail=0;
	fail |= WriteI2C(HP_LEFT_VOLUME, HPL_EN | HP_TRACK | HPL_VOL_M60DB);	//Turn volume down to -60dB
	fail |= WriteI2C(HP_LEFT_VOLUME, HPL_VOL_M60DB);			//Disable HPL
	fail |= WriteI2C(HP_RIGHT_VOLUME, HPR_VOL_M60DB);			//Disable HPR
	fail |= WriteI2C(HEADPHONE_OUTPUT, HPOUT_MUTE);			//Mute the input stages
	return(fail);
}

void set_amp_gain(int icodec_num)
{
        switch(icodec_num) {
                case ICODEC_HANDSET_RX:
                	TPA2055D3_ResetToDefaults();
                        printk("voc_codec %d does not use the amp\n", icodec_num);
                        break;
                case  ICODEC_HEADSET_ST_RX:
                        printk("voc_codec %d  for HEADSET_ST_RX amp\n", icodec_num);
                        TPA2055D3_PowerUpHP_IN1();
                        break;
/* BEGIN:0009748        ehgrace.kim@lge.com     2010.10.07*/
/* MOD: modifiy the amp for sonification mode for subsystem audio calibration */
                case  ICODEC_SPEAKER_RX:
                        printk("voc_codec %d for SPEAKER_RX amp\n", icodec_num);
                        TPA2055D3_PowerUpClassD_IN2();
                        break;
/* END:0009748        ehgrace.kim@lge.com     2010.10.07*/
                case  ICODEC_HEADSET_SPK_RX:
                        printk("voc_codec %d for HS_SPK_RX amp\n", icodec_num);
                        TPA2055D3_PowerUp_IN1IN2();
                        break;
/* BEGIN:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* MOD: add the call mode */
                case  ICODEC_HEADSET_PHONE_RX:
                        printk("voc_codec %d  for HEADSET_PHONE_RX amp\n", icodec_num);
                        TPA2055D3_PowerUpHP_PHONE_IN1();
                        break;
                case  ICODEC_SPEAKER_PHONE_RX:
                        printk("voc_codec %d for SPEAKER_PHONE_RX amp\n", icodec_num);
                        TPA2055D3_PowerUpClassD_PHONE_IN2();
                        break;
/* END:0010882        ehgrace.kim@lge.com     2010.11.15*/

                case  ICODEC_POWEROFF:
                        printk("voc_codec %d for AMP PWROFF\n", icodec_num);
                	TPA2055D3_PowerDownClassD();
                	TPA2055D3_PowerDown_HP();
                	break;
                	
                default :
                	printk("%s : voc_icodec %d does not support AMP\n", icodec_num);
 
        }
}
EXPORT_SYMBOL(set_amp_gain);

/* BEGIN:0010882        ehgrace.kim@lge.com     2010.11.15*/
/* MOD: add the call mode */
/* BEGIN:0009748        ehgrace.kim@lge.com     2010.10.07*/
/* ADD: modifiy the amp for sonification mode for subsystem audio calibration */
/* BEGIN:0010385        ehgrace.kim@lge.com     2010.11.08*/
/* MOD: add the get value for hiddenmenu */
static long amp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct amp_cal amp_cal;

	switch (cmd) {
	case AMP_SET_DATA:
		if (copy_from_user(&amp_cal, (void __user *) arg, sizeof(amp_cal))) {
			MM_ERR("AMP_SET_DATA : invalid pointer\n");
			rc = -EFAULT;
			break;
		}
		switch (amp_cal.dev_type) {
		case ICODEC_HEADSET_SPK_RX:
			if (amp_cal.gain_type == IN1_GAIN) amp_cal_data.mix_in1_gain = amp_cal.data;
			else if (amp_cal.gain_type == IN2_GAIN) amp_cal_data.mix_in2_gain = amp_cal.data;
			else if (amp_cal.gain_type == SPK_VOL) amp_cal_data.mix_spk_vol = amp_cal.data;
			else if (amp_cal.gain_type == HP_LVOL) amp_cal_data.mix_hp_lvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_RVOL) amp_cal_data.mix_hp_rvol = amp_cal.data;
			else if (amp_cal.gain_type == SPK_LIM) amp_cal_data.mix_spk_lim = amp_cal.data;
			else if (amp_cal.gain_type == HP_LIM) amp_cal_data.mix_hp_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for HP & SPK  %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_ST_RX:
			if (amp_cal.gain_type == IN1_GAIN) amp_cal_data.in1_gain = amp_cal.data;
			else if (amp_cal.gain_type == HP_LVOL) amp_cal_data.hp_lvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_RVOL) amp_cal_data.hp_rvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_LIM) amp_cal_data.hp_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for HP %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_SPEAKER_RX:
			if (amp_cal.gain_type == IN2_GAIN) amp_cal_data.in2_gain = amp_cal.data;
			else if (amp_cal.gain_type == SPK_VOL) amp_cal_data.spk_vol = amp_cal.data;
			else if (amp_cal.gain_type == SPK_LIM) amp_cal_data.spk_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for SPK %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_PHONE_RX:
			if (amp_cal.gain_type == IN1_GAIN) amp_cal_data.call_in1_gain = amp_cal.data;
			else if (amp_cal.gain_type == HP_LVOL) amp_cal_data.call_hp_lvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_RVOL) amp_cal_data.call_hp_rvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_LIM) amp_cal_data.call_hp_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for HP_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_SPEAKER_PHONE_RX:
			if (amp_cal.gain_type == IN2_GAIN) amp_cal_data.call_in2_gain = amp_cal.data;
			else if (amp_cal.gain_type == SPK_VOL) amp_cal_data.call_spk_vol = amp_cal.data;
			else if (amp_cal.gain_type == SPK_LIM) amp_cal_data.call_spk_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for SPK_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;

		default:
			MM_ERR("unknown dev type for setdata %d\n", amp_cal.dev_type);
			rc = -EFAULT;
 			break;
		}
		break;
	case AMP_GET_DATA:
		if (copy_from_user(&amp_cal, (void __user *) arg, sizeof(amp_cal))) {
			MM_ERR("AMP_GET_DATA : invalid pointer\n");
			rc = -EFAULT;
			break;
		}
		switch (amp_cal.dev_type) {
		case ICODEC_HEADSET_SPK_RX:
			if (amp_cal.gain_type == IN1_GAIN) amp_cal.data = amp_cal_data.mix_in1_gain;
			else if (amp_cal.gain_type == IN2_GAIN) amp_cal.data = amp_cal_data.mix_in2_gain;
			else if (amp_cal.gain_type == SPK_VOL) amp_cal.data = amp_cal_data.mix_spk_vol;
			else if (amp_cal.gain_type == HP_LVOL) amp_cal.data = amp_cal_data.mix_hp_lvol;
			else if (amp_cal.gain_type == HP_RVOL) amp_cal.data = amp_cal_data.mix_hp_rvol;
			else if (amp_cal.gain_type == SPK_LIM) amp_cal.data = amp_cal_data.mix_spk_lim;
			else if (amp_cal.gain_type == HP_LIM) amp_cal.data = amp_cal_data.mix_hp_lim;
			else {
				MM_ERR("invalid get_gain_type for HP & SPK %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_ST_RX:
			if (amp_cal.gain_type == IN1_GAIN) amp_cal.data = amp_cal_data.in1_gain;
			else if (amp_cal.gain_type == HP_LVOL) amp_cal.data = amp_cal_data.hp_lvol;
			else if (amp_cal.gain_type == HP_RVOL) amp_cal.data = amp_cal_data.hp_rvol;
			else if (amp_cal.gain_type == HP_LIM) amp_cal.data = amp_cal_data.hp_lim;
			else {
				MM_ERR("invalid get_gain_type for HP %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;	
		case ICODEC_SPEAKER_RX:
			if (amp_cal.gain_type == IN2_GAIN) amp_cal.data = amp_cal_data.in2_gain;
			else if (amp_cal.gain_type == SPK_VOL) amp_cal.data = amp_cal_data.spk_vol;
			else if (amp_cal.gain_type == SPK_LIM) amp_cal.data = amp_cal_data.spk_lim;
			else {
				MM_ERR("invalid get_gain_type for SPK %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_PHONE_RX:
			if (amp_cal.gain_type == IN1_GAIN) amp_cal.data = amp_cal_data.call_in1_gain;
			else if (amp_cal.gain_type == HP_LVOL) amp_cal.data = amp_cal_data.call_hp_lvol;
			else if (amp_cal.gain_type == HP_RVOL) amp_cal.data = amp_cal_data.call_hp_rvol;
			else if (amp_cal.gain_type == HP_LIM) amp_cal.data = amp_cal_data.call_hp_lim;
			else {
				MM_ERR("invalid get_gain_type for HP_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;	
		case ICODEC_SPEAKER_PHONE_RX:
			if (amp_cal.gain_type == IN2_GAIN) amp_cal.data = amp_cal_data.call_in2_gain;
			else if (amp_cal.gain_type == SPK_VOL) amp_cal.data = amp_cal_data.call_spk_vol;
			else if (amp_cal.gain_type == SPK_LIM) amp_cal.data = amp_cal_data.call_spk_lim;
			else {
				MM_ERR("invalid get_gain_type for SPK_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;	
		default:
			MM_ERR("unknown dev type for getdata %d\n", amp_cal.dev_type);
			rc = -EFAULT;
 			break;
		}
		MM_ERR("AMP_GET_DATA :dev %d, gain %d, data %d \n", amp_cal.dev_type, amp_cal.gain_type, amp_cal.data);
		if (copy_to_user((void __user *)arg, &amp_cal, sizeof(amp_cal))) {
			MM_ERR("AMP_GET_DATA : invalid pointer\n");
			rc = -EFAULT;
		}
		break;
	default:
		MM_ERR("unknown command\n");
		rc = -EINVAL;
		break;
	}
	return rc;
}
/* END:0010385        ehgrace.kim@lge.com     2010.11.08*/
/* END:0010882        ehgrace.kim@lge.com     2010.11.15*/

static int amp_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	return rc;
}

static int amp_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	return rc;
}

static struct file_operations tpa_fops = {
	.owner		= THIS_MODULE,
	.open		= amp_open,
	.release	= amp_release,
	.unlocked_ioctl	= amp_ioctl,
};

struct miscdevice amp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tpa_amp",
	.fops = &tpa_fops,
};
/* END:0009748        ehgrace.kim@lge.com     2010.10.07*/

static int bryce_amp_ctl_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct amp_data *data;
	struct i2c_adapter* adapter = client->adapter;
	int err;
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)){
		err = -EOPNOTSUPP;
		return err;
	}

	if (msm_snd_debug & 1)
		printk(KERN_INFO "%s()\n", __FUNCTION__);
	
	data = kzalloc(sizeof (struct amp_data), GFP_KERNEL);
	if (NULL == data) {
			return -ENOMEM;
	}
	_data = data;
	data->client = client;
	i2c_set_clientdata(client, data);
	
	if (msm_snd_debug & 1)
		printk(KERN_INFO "%s chip found\n", client->name);
	
	set_amp_gain(ICODEC_POWEROFF);

/* BEGIN:0009748        ehgrace.kim@lge.com     2010.10.07*/
/* ADD: modifiy the amp for sonification mode for subsystem audio calibration */
	err = misc_register(&amp_misc);
/* END:0009748        ehgrace.kim@lge.com     2010.10.07*/
	return 0;
}

static int bryce_amp_ctl_remove(struct i2c_client *client)
{
	struct amp_data *data = i2c_get_clientdata(client);
	kfree (data);
	
	printk(KERN_INFO "%s()\n", __FUNCTION__);
	i2c_set_clientdata(client, NULL);
	return 0;
}


static struct i2c_device_id bryce_amp_idtable[] = {
	{ "tpa2055", 1 },
};

static struct i2c_driver bryce_amp_ctl_driver = {
	.probe = bryce_amp_ctl_probe,
	.remove = bryce_amp_ctl_remove,
	.id_table = bryce_amp_idtable,
	.driver = {
		.name = MODULE_NAME,
	},
};


static int __init bryce_amp_ctl_init(void)
{
	return i2c_add_driver(&bryce_amp_ctl_driver);	
}

static void __exit bryce_amp_ctl_exit(void)
{
	return i2c_del_driver(&bryce_amp_ctl_driver);
}

module_init(bryce_amp_ctl_init);
module_exit(bryce_amp_ctl_exit);

MODULE_DESCRIPTION("Bryce Amp Control");
MODULE_AUTHOR("Kim EunHye <ehgrace.kim@lge.com>");
MODULE_LICENSE("GPL");
