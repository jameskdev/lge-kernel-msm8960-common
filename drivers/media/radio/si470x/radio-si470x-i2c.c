/*
 * drivers/media/radio/si470x/radio-si470x-i2c.c
 * 
 * I2C driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 * Copyright (c) 2009 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/* driver definitions */
#define DRIVER_AUTHOR "Joonyoung Shim <jy0922.shim@samsung.com>";
#define DRIVER_KERNEL_VERSION KERNEL_VERSION(1, 0, 1)
#define DRIVER_CARD "Silicon Labs Si470x FM Radio Receiver"
#define DRIVER_DESC "I2C radio driver for Si470x FM Radio Receivers"
#define DRIVER_VERSION "1.0.1"

/* kernel includes */
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include "radio-si470x.h"
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <mach/msm_xo.h>

#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include "../../../arch/arm/mach-msm/devices.h"
#define FM_RCLK (PM8921_GPIO_PM_TO_SYS(43))
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)

/* I2C Device ID List */
static const struct i2c_device_id si470x_i2c_id[] = {
	/* Generic Entry */
	{ "si470x", 0 },
	/* Terminating entry */
	{ }
};
MODULE_DEVICE_TABLE(i2c, si470x_i2c_id);



/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Radio Nr */
static int radio_nr = -1;
module_param(radio_nr, int, 0444);
MODULE_PARM_DESC(radio_nr, "Radio Nr");

/* RDS buffer blocks */
static unsigned int rds_buf = 100;
module_param(rds_buf, uint, 0444);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");

/* RDS maximum block errors */
static unsigned short max_rds_errors = 1;
/* 0 means   0  errors requiring correction */
/* 1 means 1-2  errors requiring correction (used by original USBRadio.exe) */
/* 2 means 3-5  errors requiring correction */
/* 3 means   6+ errors or errors in checkword, correction not possible */
module_param(max_rds_errors, ushort, 0644);
MODULE_PARM_DESC(max_rds_errors, "RDS maximum block errors: *1*");


static struct workqueue_struct *local_si470x_workqueue;

/**************************************************************************
 * I2C Definitions
 **************************************************************************/

/* Write starts with the upper byte of register 0x02 */
#define WRITE_REG_NUM		8
#define WRITE_INDEX(i)		(i + 0x02)

/* Read starts with the upper byte of register 0x0a */
#define READ_REG_NUM		RADIO_REGISTER_NUM
#define READ_INDEX(i)		((i + RADIO_REGISTER_NUM - 0x0a) % READ_REG_NUM)

struct si4708_data {
	struct i2c_client *client;
	struct si4708_fmradio_platform_data *pdata;	
};





/**************************************************************************
 * General Driver Functions - REGISTERs
 **************************************************************************/

/*
 * si470x_get_register - read register
 */
int si470x_get_register(struct si470x_device *radio, int regnr)
{
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
			(void *)buf },
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	radio->registers[regnr] = __be16_to_cpu(buf[READ_INDEX(regnr)]);

	return 0;
}


/*
 * si470x_set_register - write register
 */
int si470x_set_register(struct si470x_device *radio, int regnr)
{
	int i;
	u16 buf[WRITE_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, 0, sizeof(u16) * WRITE_REG_NUM,
			(void *)buf },
	};

	for (i = 0; i < WRITE_REG_NUM; i++){
		buf[i] = __cpu_to_be16(radio->registers[WRITE_INDEX(i)]);
//		printk(KERN_INFO "si470x_set_register buf[i]=%x, i= %d\n",buf[i],i );	
}
	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1){
		for (i = 0; i < WRITE_REG_NUM; i++){
		}
		return -EIO;
}
	return 0;
}



/**************************************************************************
 * General Driver Functions - ENTIRE REGISTERS
 **************************************************************************/

/*
 * si470x_get_all_registers - read entire registers
 */
int si470x_get_all_registers(struct si470x_device *radio)
{
	int i;
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
			(void *)buf },
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	for (i = 0; i < READ_REG_NUM; i++)
		radio->registers[i] = __be16_to_cpu(buf[READ_INDEX(i)]);

	return 0;
}



/**************************************************************************
 * General Driver Functions - DISCONNECT_CHECK
 **************************************************************************/

/*
 * si470x_disconnect_check - check whether radio disconnects
 */
int si470x_disconnect_check(struct si470x_device *radio)
{
	return 0;
}

static void si470x_interrupt_handler(struct work_struct *work)
{
	struct si470x_device *radio = container_of(work,
					struct si470x_device, work.work);
	unsigned char regnr;
	//unsigned char blocknum;
	//unsigned short bler; /* rds block errors */
	//unsigned short rds;
	//unsigned char tmpbuf[3];
	int retval = 0;

	si470x_get_all_registers(radio);
	printk("si470x_i2c_interrupt start POWER CFG %x \n", radio->registers[POWERCFG]);
	
	/* check Seek/Tune Complete */
	retval = si470x_get_register(radio, STATUSRSSI);
	if (retval < 0){
		printk("si470x_i2c_interrupt debug 1 retval = %x",retval);
		goto end;
	}
	
	if (radio->registers[STATUSRSSI] & STATUSRSSI_STC)
	{
		printk(KERN_INFO "check STATUSRSSI_STC bit\n");	

		if(radio->registers[POWERCFG] & POWERCFG_SEEK)
		{
			printk(KERN_INFO "check POWERCFG_SEEK bit\n");	

			//msleep(1000);
			complete(&radio->completion);
			//si470x_q_event(radio,SI470X_EVT_TUNE_SUCC);
			
			//si470x_q_event(radio,SI470X_EVT_ABOVE_TH);
			//si470x_q_event(radio,SI470X_EVT_STEREO);
			//si470x_q_event(radio,SI470X_EVT_RDS_NOT_AVAIL);
			if(radio->registers[STATUSRSSI] & STATUSRSSI_SF)
			{
				printk(KERN_INFO "SI470X_EVT_SEEK_COMPLETE\n");	
				//msleep(1000);
				//20120910 hanna.oh FM Radio scan complete bug fix
				//si470x_q_event(radio,SI470X_EVT_TUNE_SUCC);
				
				//si470x_q_event(radio,SI470X_EVT_ABOVE_TH);
				//si470x_q_event(radio,SI470X_EVT_STEREO);
				//si470x_q_event(radio,SI470X_EVT_RDS_NOT_AVAIL);
				
				if(radio->seek_onoff == 1)	
					si470x_q_event(radio,SI470X_EVT_SEEK_COMPLETE);

				radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
				retval = si470x_set_register(radio, POWERCFG);

				//msleep(10);
				
				do {
					retval = si470x_get_register(radio, STATUSRSSI);
					if (retval < 0){
						goto end;
						printk("si470x_i2c_interrupt debug 1 retval = %x",retval);
						}
				} while ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) != 0);
				
				if(radio->seek_onoff == 0)
				{
					radio->registers[POWERCFG] |= POWERCFG_SEEK;
					retval = si470x_set_register(radio, POWERCFG);
				}

				radio->seek_onoff = 0;
			}
			else
			{
				printk(KERN_INFO "SI470X_EVT_SCAN_NEXT\n");	

				//si470x_q_event(radio,SI470X_EVT_SCAN_NEXT);
				radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
				retval = si470x_set_register(radio, POWERCFG);

				//msleep(10);
				
				do {
					retval = si470x_get_register(radio, STATUSRSSI);
					if (retval < 0){
						goto end;
						printk("si470x_i2c_interrupt debug 1 retval = %x",retval);
						}
				} while ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) != 0);

				msleep(10);
				if(radio->seek_onoff == 1)
				{
					
					si470x_q_event(radio,SI470X_EVT_TUNE_SUCC);
				
					si470x_q_event(radio,SI470X_EVT_ABOVE_TH);
					si470x_q_event(radio,SI470X_EVT_STEREO);
					si470x_q_event(radio,SI470X_EVT_RDS_NOT_AVAIL);

					si470x_q_event(radio,SI470X_EVT_SCAN_NEXT);

					msleep(1000);
					radio->registers[POWERCFG] |= POWERCFG_SEEK;
					retval = si470x_set_register(radio, POWERCFG);
				}
				else
				{
					printk("si470x_i2c_interrupt SCAN Stop\n");
					si470x_q_event(radio,SI470X_EVT_SEEK_COMPLETE);
					
					si470x_q_event(radio,SI470X_EVT_TUNE_SUCC);
				
					si470x_q_event(radio,SI470X_EVT_ABOVE_TH);
					si470x_q_event(radio,SI470X_EVT_STEREO);
					si470x_q_event(radio,SI470X_EVT_RDS_NOT_AVAIL);

				}

				msleep(10);
			}

		}
		else
		{
			complete(&radio->completion);
			si470x_q_event(radio,SI470X_EVT_TUNE_SUCC);

		}
		
		}

#if 0
	si470x_get_register(radio,SYSCONFIG2);
	if(SEEKTH_VAL>(radio->registers[SYSCONFIG2] & SYSCONFIG2_SEEKTH))
		si470x_q_event(radio,SI470X_EVT_BELOW_TH);
	else
		si470x_q_event(radio,SI470X_EVT_ABOVE_TH);

	si470x_q_event(radio,SI470X_EVT_STEREO);
	si470x_q_event(radio,SI470X_EVT_RDS_NOT_AVAIL);
#endif

	/* safety checks */
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
	{
		printk(KERN_INFO "SYSCONFIG1_RDS OFF\n");	

		goto end;
	}
	/* Update RDS registers */
	for (regnr = 1; regnr < RDS_REGISTER_NUM; regnr++) {
		retval = si470x_get_register(radio, STATUSRSSI + regnr);
		if (retval < 0){
			goto end;
			}
	}

	/* get rds blocks */
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_RDSR) == 0)
	{
		/* No RDS group ready, better luck next time */
		printk(KERN_INFO "No RDS group ready, better luck next time\n");	
		initRdsBuffers();
		goto end;
	}
#if 0
	for (blocknum = 0; blocknum < 4; blocknum++) {
		switch (blocknum) {
		default:
			bler = (radio->registers[STATUSRSSI] &
					STATUSRSSI_BLERA) >> 9;
			rds = radio->registers[RDSA];
			break;
		case 1:
			bler = (radio->registers[READCHAN] &
					READCHAN_BLERB) >> 14;
			rds = radio->registers[RDSB];
			break;
		case 2:
			bler = (radio->registers[READCHAN] &
					READCHAN_BLERC) >> 12;
			rds = radio->registers[RDSC];
			break;
		case 3:
			bler = (radio->registers[READCHAN] &
					READCHAN_BLERD) >> 10;
			rds = radio->registers[RDSD];
			break;
		};

		/* Fill the V4L2 RDS buffer */
		put_unaligned_le16(rds, &tmpbuf);
		tmpbuf[2] = blocknum;		/* offset name */
		tmpbuf[2] |= blocknum << 3;	/* received offset */
		if (bler > max_rds_errors)
			tmpbuf[2] |= 0x80;	/* uncorrectable errors */
		else if (bler > 0)
			tmpbuf[2] |= 0x40;	/* corrected error(s) */

		/* copy RDS block to internal buffer */
		memcpy(&radio->buffer[radio->wr_index], &tmpbuf, 3);
		radio->wr_index += 3;

		/* wrap write pointer */
		if (radio->wr_index >= radio->buf_size)
			radio->wr_index = 0;

		/* check for overflow */
		if (radio->wr_index == radio->rd_index) {
			/* increment and wrap read pointer */
			radio->rd_index += 3;
			if (radio->rd_index >= radio->buf_size)
				radio->rd_index = 0;
		}
	}

	if (radio->wr_index != radio->rd_index)
		wake_up_interruptible(&radio->read_queue);
#else
	updateRds(radio);
	wake_up_interruptible(&radio->read_queue);
#endif
end:
	printk("si470x_i2c_interrupt End\n");
	return; //IRQ_HANDLED;
}

/*
 * si470x_i2c_interrupt - interrupt handler
 */

static irqreturn_t si470x_irq_handler(int irq, void *dev_id)
{
	struct si470x_device*radio = dev_id;
	
	printk(KERN_INFO "in si470x_irq_handler\n");	
	queue_delayed_work(local_si470x_workqueue, &radio->work,
				msecs_to_jiffies(SI470X_DELAY));
	return IRQ_HANDLED;
}

/**************************************************************************
 * File Operations Interface
 **************************************************************************/

/*
 * si470x_fops_open - file open
 */
int si470x_fops_open(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
//	int i=0;
	unsigned char version_warning = 0;
	static unsigned char first_init = 1;

	printk(KERN_INFO "si470x_fops_open\n");	
	mutex_lock(&radio->lock);
	radio->users++;

	if (radio->users == 1) {
		/* start radio */
		fmradio_power(1);
		//msleep(10);

//-------------------------------------------------------
			/* power up : need 110ms */
			radio->registers[POWERCFG] = POWERCFG_ENABLE;
			if (si470x_set_register(radio, POWERCFG) < 0) {
				retval = -EIO;
				goto err_video;
			}
			msleep(110);
		
			/* get device and chip versions */
			if (si470x_get_all_registers(radio) < 0) {
				retval = -EIO;
				goto err_video;
			}
		
			dev_info(&radio->videodev->dev, "DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
					radio->registers[DEVICEID], radio->registers[CHIPID]);
			if ((radio->registers[CHIPID] & CHIPID_FIRMWARE) < RADIO_FW_VERSION) {
				dev_warn(&radio->videodev->dev,
					"This driver is known to work with "
					"firmware version %hu,\n", RADIO_FW_VERSION);
				dev_warn(&radio->videodev->dev,
					"but the device has firmware version %hu.\n",
					radio->registers[CHIPID] & CHIPID_FIRMWARE);
				version_warning = 1;
			}
		
			/* give out version warning */
			if (version_warning == 1) {
				dev_warn(&radio->videodev->dev,
					"If you have some trouble using this driver,\n");
				dev_warn(&radio->videodev->dev,
					"please report to V4L ML at "
					"linux-media@vger.kernel.org\n");
			}

			retval = si470x_start(radio);
			//msleep(50);
			if (retval < 0)
				goto done;
		
			/* rds buffer allocation */
			radio->buf_size = rds_buf * 3;
			radio->buffer = kmalloc(radio->buf_size, GFP_KERNEL);
			if (!radio->buffer) {
				retval = -EIO;
				goto err_video;
			}
		
			/* rds buffer configuration */
			radio->wr_index = 0;
			radio->rd_index = 0;
			init_waitqueue_head(&radio->read_queue);
		
			/* mark Seek/Tune Complete Interrupt enabled */
			radio->seek_onoff = 0;
			radio->mute = 0;
			radio->stci_enabled = true;
			init_completion(&radio->completion);
			/* initialize wait queue for event read */
			init_waitqueue_head(&radio->event_queue);

			if(first_init == 1)
			{
				retval = gpio_request(GPIO_SI470X_EN_PIN, "si470x_int");
				if (retval < 0) {
					dev_warn(&radio->client->dev,"Failed to configure GPIO_SI470X_RESET_PIN gpio_request\n");
					goto err_all;
				}
			
				retval = gpio_direction_input(GPIO_SI470X_EN_PIN);
				if (retval< 0) {
					dev_warn(&radio->client->dev,"Failed to configure GPIO_SI470X_RESET_PIN gpio_direction_input\n");
					goto err_all;
				} 
				
				radio->client->irq = gpio_to_irq(GPIO_SI470X_EN_PIN);
			
				dev_warn(&radio->client->dev,"client->irq	= GPIO_SI470X_EN_PIN\n");
				if (radio->client->irq < 0) {
					dev_warn(&radio->client->dev,"Failed to get interrupt number\n");
					retval = radio->client->irq;
					goto err_all;
				}
				
				retval = request_threaded_irq(radio->client->irq, NULL,si470x_irq_handler,
								IRQF_TRIGGER_FALLING,DRIVER_NAME, radio);
				if (retval) {
					dev_err(&radio->client->dev, "Failed to register interrupt\n");
					goto err_all;
				}

				INIT_DELAYED_WORK(&radio->work, si470x_interrupt_handler);
				first_init = 0;
			}
		/* set initial frequency */
//		si470x_set_freq(radio, 94.5 * FREQ_MUL); /* available in all regions */

//-------------------------------------------------------
		

			/* reset last channel */
		//retval = si470x_get_register(radio, CHANNEL);
		//retval = si470x_set_chan(radio,radio->registers[CHANNEL] & CHANNEL_CHAN);

		printk(KERN_INFO "si470x_enable RDS & STC\n");	
		/* enable RDS / STC interrupt */

		goto done;
	}
	//err_rds:
	//	kfree(radio->buffer);
err_video:
	video_device_release(radio->videodev);
	//radio->users--;
	return retval;
//	err_radio:
//		kfree(radio);
//	err_initial:
//		return retval;
err_all:
	free_irq(radio->client->irq, radio);

done:
	printk(KERN_INFO "si470x_fops open radio->lock%x\n",(int)&radio->lock);
	printk(KERN_INFO "si470x_fops open return %x\n",(int)retval);	

	mutex_unlock(&radio->lock);
	return retval;
	
}


/*
 * si470x_fops_release - file release
 */
int si470x_fops_release(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	printk(KERN_INFO "si470x_fops_release\n");	
	/* safety check */
	if (!radio)
		return -ENODEV;

	mutex_lock(&radio->lock);
	radio->users--;
	if (radio->users == 0)
		/* stop radio */
		retval = si470x_stop(radio);
	fmradio_power(0);
	mutex_unlock(&radio->lock);

	return retval;
}



/**************************************************************************
 * Video4Linux Interface
 **************************************************************************/

/*
 * si470x_vidioc_querycap - query device capabilities
 */
int si470x_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	printk(KERN_INFO "si470x_vidioc_querycap\n");	
	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	capability->version = DRIVER_KERNEL_VERSION;
	capability->capabilities = V4L2_CAP_HW_FREQ_SEEK |
		V4L2_CAP_TUNER | V4L2_CAP_RADIO;

	return 0;
}

/**************************************************************************
 * I2C Interface
 **************************************************************************/
int fmradio_power(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_lvs1, *vreg_ldo10;
	struct pm_gpio param = {
	.direction      = PM_GPIO_DIR_OUT,
	.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
	.output_value   = 0,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel	= PM_GPIO_VIN_S4,
	.out_strength   = PM_GPIO_STRENGTH_HIGH,
	.function       = PM_GPIO_FUNC_1,
	.inv_int_pol      = 0,

	};

	printk(KERN_INFO "fmradio_power \n");

//--------------------------------VA, Vd POWER --------------------
		if (on) {
//-------------------------------- RST pin LOW --------------------
			rc = gpio_request(GPIO_SI470X_RESET_PIN, "si470x_reset");
			if (rc < 0) {
				printk(KERN_INFO "%s: Failed to gpio_request si470x_reset\n", __func__);
			}else{
				printk(KERN_INFO "%s: Sucess to gpio_request si470x_reset\n", __func__);

			}
			
			rc = gpio_direction_output(GPIO_SI470X_RESET_PIN, 0);
			if (rc < 0) {
				printk(KERN_INFO "%s: Failed to gpio_direction_output si470X_reset\n", __func__);
			}else{
				printk(KERN_INFO "%s: Sucess to gpio_direction_output si470X_reset\n", __func__);
			}
//-----------------------------------------------------------------
			if (lge_get_board_revno() == HW_REV_A)
			{
				rc = gpio_request(GPIO_SI470X_AUD_CODEC_LDO_EN, "si470x_enable");
				if (rc < 0) {
					printk(KERN_INFO "%s: Failed to gpio_request si470x_enable\n", __func__);
				}else{
					printk(KERN_INFO "%s: Sucess to gpio_request si470x_enable\n", __func__);
				}
				
				rc = gpio_direction_output(GPIO_SI470X_AUD_CODEC_LDO_EN, 1);
				if (rc < 0) {
					printk(KERN_INFO "%s: Failed to gpio_direction_output si470x_enable\n", __func__);
				}else{
					printk(KERN_INFO "%s: Sucess to gpio_direction_output si470x_enable\n", __func__);
				}

				msleep(10);		


				//-----------------------------------------------------------------
				//-------------------------------- Vio Power--------------------
				vreg_lvs1 = regulator_get(NULL, "8921_lvs1");	/* +V1P8_LVS1_AUD_CLK */
				if (IS_ERR(vreg_lvs1)) {
					printk(KERN_INFO "%s() regulator get of 8921_lvs1 failed(%ld)\n", __func__, PTR_ERR(vreg_lvs1));
					rc = PTR_ERR(vreg_lvs1);
					return rc;
				}
				else{ printk(KERN_INFO "%s() regulator get of 8921_lvs1 SUCESS\n", __func__);
				}
				
				rc = regulator_set_voltage(vreg_lvs1,
							1800000, 1800000); // 1.8V
				if (rc)
					pr_err("%s: fmradio_power regulator_set_voltage failed rc =%d\n", __func__, rc);
				else
					pr_err("%s: fmradio_power regulator_set_voltage succes rc =%d\n", __func__, rc);

				rc = regulator_enable(vreg_lvs1);
				if (rc)
					pr_err("%s: fmradio_power regulator_enable failed rc =%d\n", __func__, rc);
				else
					pr_err("%s: fmradio_power regulator_enable succes rc =%d\n", __func__, rc); 	
				//-----------------------------------------------------------------
				msleep(10);

			}
			else		//20120925 hanna.oh@lge.com Changing FM Radio power and Clk
			{
				//-----------------------------------------------------------------
				//-------------------------------- Vio Power--------------------
				vreg_ldo10 = regulator_get(NULL, "8921_l10");	/* +V2P7_L10_FM */
				if (IS_ERR(vreg_ldo10)) {
					printk(KERN_INFO "%s() regulator get of vreg_ldo10 failed(%ld)\n", __func__, PTR_ERR(vreg_lvs1));
					rc = PTR_ERR(vreg_ldo10);
					return rc;
				}
				else{ printk(KERN_INFO "%s() regulator get of vreg_ldo10 SUCESS\n", __func__);
				}
				
				rc = regulator_set_voltage(vreg_ldo10,
							2700000, 2700000); // 1.8V
				if (rc)
					pr_err("%s: fmradio_power regulator_set_voltage failed rc =%d\n", __func__, rc);
				else
					pr_err("%s: fmradio_power regulator_set_voltage succes rc =%d\n", __func__, rc);

				rc = regulator_enable(vreg_ldo10);
				if (rc)
					pr_err("%s: fmradio_power regulator_enable failed rc =%d\n", __func__, rc);
				else
					pr_err("%s: fmradio_power regulator_enable succes rc =%d\n", __func__, rc); 	
				//-----------------------------------------------------------------
				msleep(10);
			}
			rc = gpio_direction_output(GPIO_SI470X_RESET_PIN, 1);
			if (rc < 0) {
				printk(KERN_INFO "%s: Failed to gpio_direction_output si470X_reset\n", __func__);
			}else{
				printk(KERN_INFO "%s: Succes to gpio_direction_output si470X_reset\n", __func__);
			}
	//-----------------------------------------------------------------
			msleep(10);

			rc = gpio_request(FM_RCLK, "FM_RCLK");
			if (rc)
				pr_err("%s: Error requesting GPIO %d\n", __func__, FM_RCLK);
			rc = pm8xxx_gpio_config(FM_RCLK, &param);
			if (rc)
				pr_err("%s: Failed to configure gpio %d\n", __func__, FM_RCLK);

		} 
		else { // off case
			//vreg_lvs1 = regulator_get(NULL, "8921_lvs1");	/* +V1P8_LVS1_AUD_CLK */
			//rc = regulator_disable(vreg_lvs1);
			//msleep(10);
			if (lge_get_board_revno() == HW_REV_A)
			{
				rc = gpio_direction_output(GPIO_SI470X_AUD_CODEC_LDO_EN, 0);
				if (rc < 0) {
					printk(KERN_INFO "%s: Failed to gpio_direction_output si470x_enable\n", __func__);
				}else{
					printk(KERN_INFO "%s: Success to gpio_direction_output si470x_enable\n", __func__);
				}
			}
			else	//20120925 hanna.oh@lge.com Changing FM Radio power and Clk
			{
				rc = regulator_disable(vreg_ldo10);
			}
		}

	printk(KERN_INFO "%s() on is %d rc is %d\n", __func__, on, rc);
	return rc;
}


#if 0
static int si470x_sleep(void)
{
	int rc;
	printk(KERN_INFO "si470x_sleep\n");
	rc = gpio_direction_output(GPIO_SI470X_RESET_PIN, 0);
	if (rc < 0) {
		printk(KERN_INFO "si470x_sleep: Failed to gpio_direction_output si470X_reset\n");
	}
	return rc;
}
#endif


/*
 * si470x_i2c_probe - probe for the device
 */
static int __devinit si470x_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct si470x_device *radio;
	int retval = 0;
	unsigned char version_warning = 0;	//20120919 EU version sleep patch
	printk("si470x_i2c_probe \n");

	/* private data allocation and initialization */
	radio = kzalloc(sizeof(struct si470x_device), GFP_KERNEL);
	if (!radio) {
		retval = -ENOMEM;
		goto err_initial;
	}

	radio->users = 0;
	radio->client = client;
	radio->volume = 5;
	mutex_init(&radio->lock);

	/* video device allocation and initialization */
	radio->videodev = video_device_alloc();
	if (!radio->videodev) {
		retval = -ENOMEM;
		goto err_radio;
	}
	memcpy(radio->videodev, &si470x_viddev_template,
			sizeof(si470x_viddev_template));
	video_set_drvdata(radio->videodev, radio);
//20120919 EU version sleep patch [START]
		/* start radio */
		fmradio_power(1);
		//msleep(10);

//-------------------------------------------------------
			/* power up : need 110ms */
			radio->registers[POWERCFG] = POWERCFG_ENABLE;
			if (si470x_set_register(radio, POWERCFG) < 0) {
				retval = -EIO;
				goto err_radio;
			}
			msleep(110);
		
			/* get device and chip versions */
			if (si470x_get_all_registers(radio) < 0) {
				retval = -EIO;
				goto err_radio;
			}
		
			dev_info(&radio->videodev->dev, "DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
					radio->registers[DEVICEID], radio->registers[CHIPID]);
			if ((radio->registers[CHIPID] & CHIPID_FIRMWARE) < RADIO_FW_VERSION) {
				dev_warn(&radio->videodev->dev,
					"This driver is known to work with "
					"firmware version %hu,\n", RADIO_FW_VERSION);
				dev_warn(&radio->videodev->dev,
					"but the device has firmware version %hu.\n",
					radio->registers[CHIPID] & CHIPID_FIRMWARE);
				version_warning = 1;
			}
		
			/* give out version warning */
			if (version_warning == 1) {
				dev_warn(&radio->videodev->dev,
					"If you have some trouble using this driver,\n");
				dev_warn(&radio->videodev->dev,
					"please report to V4L ML at "
					"linux-media@vger.kernel.org\n");
			}
//20120919 EU version sleep patch [END]
#if 0
	/* power up : need 110ms */
	radio->registers[POWERCFG] = POWERCFG_ENABLE;
	if (si470x_set_register(radio, POWERCFG) < 0) {
		retval = -EIO;
		goto err_video;
	}
	msleep(110);

	/* get device and chip versions */
	if (si470x_get_all_registers(radio) < 0) {
		retval = -EIO;
		goto err_video;
	}

printk("radio->registers[SYSCONFIG1]= %x",radio->registers[SYSCONFIG1]);
printk("radio->registers[SYSCONFIG2]= %x",radio->registers[SYSCONFIG1]);


	dev_info(&client->dev, "DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
			radio->registers[DEVICEID], radio->registers[CHIPID]);
	if ((radio->registers[CHIPID] & CHIPID_FIRMWARE) < RADIO_FW_VERSION) {
		dev_warn(&client->dev,
			"This driver is known to work with "
			"firmware version %hu,\n", RADIO_FW_VERSION);
		dev_warn(&client->dev,
			"but the device has firmware version %hu.\n",
			radio->registers[CHIPID] & CHIPID_FIRMWARE);
		version_warning = 1;
	}

	/* give out version warning */
	if (version_warning == 1) {
		dev_warn(&client->dev,
			"If you have some trouble using this driver,\n");
		dev_warn(&client->dev,
			"please report to V4L ML at "
			"linux-media@vger.kernel.org\n");
	}

#if 0
	radio->registers[POWERCFG]|=radio->registers[POWERCFG]|(POWERCFG_DMUTE);
	retval = si470x_set_register(radio, POWERCFG);
	radio->registers[SYSCONFIG2]|=SYSCONFIG2_VOLUME;
	retval = si470x_set_register(radio, SYSCONFIG2);
#endif
	/* rds buffer allocation */
	radio->buf_size = rds_buf * 3;
	radio->buffer = kmalloc(radio->buf_size, GFP_KERNEL);
	if (!radio->buffer) {
		retval = -EIO;
		goto err_video;
	}

	/* rds buffer configuration */
	radio->wr_index = 0;
	radio->rd_index = 0;
	init_waitqueue_head(&radio->read_queue);

	initRdsVars();
	/* mark Seek/Tune Complete Interrupt enabled */
	radio->stci_enabled = true;
//	radio->stci_enabled = false;
	init_completion(&radio->completion);
	/* initialize wait queue for event read */
	init_waitqueue_head(&radio->event_queue);

	retval = gpio_request(GPIO_SI470X_EN_PIN, "si470x_int");
	if (retval < 0) {
		dev_warn(&client->dev,"Failed to configure GPIO_SI470X_RESET_PIN gpio_request\n");
		goto err_all;
	}

	retval = gpio_direction_input(GPIO_SI470X_EN_PIN);
	if (retval< 0) {
		dev_warn(&client->dev,"Failed to configure GPIO_SI470X_RESET_PIN gpio_direction_input\n");
		goto err_all;
	} 
/* set initial frequency */
	client->irq = gpio_to_irq(GPIO_SI470X_EN_PIN);

	dev_warn(&client->dev,"client->irq  = GPIO_SI470X_RESET_PIN\n");
	if (client->irq < 0) {
		dev_warn(&client->dev,"Failed to get interrupt number\n");
		retval = client->irq;
		goto err_all;
	}
	retval = request_threaded_irq(client->irq, NULL,si470x_i2c_interrupt,
					IRQF_TRIGGER_FALLING,DRIVER_NAME, radio);
	if (retval) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_all;
	}
//	retval = request_threaded_irq(client->irq, NULL, si470x_i2c_interrupt,
//			IRQF_TRIGGER_FALLING, DRIVER_NAME, radio);
//	if (retval) {
//		dev_err(&client->dev, "Failed to register interrupt\n");
//		goto err_rds;
//	}

#endif
	/* register video device */
	retval = video_register_device(radio->videodev, VFL_TYPE_RADIO,
			radio_nr);
	if (retval) {
		dev_warn(&client->dev, "Could not register video device\n");
		goto err_all;
	}
	i2c_set_clientdata(client, radio);

#if 0
	if (si470x_get_all_registers(radio) < 0) {
		retval = -EIO;
		goto err_video;
	}
	printk("LSW LOG \n\n");
	for(i=0;i<8;i++){
		printk("Register ID = %x value = %x\n",i, radio->registers[i]);
	}
#endif
		fmradio_power(0);	//20120919 EU version sleep patch
	printk("si470x_i2c_probe retval %x",retval);
	return retval;
	
err_all:
	free_irq(client->irq, radio);
//err_rds:
//	kfree(radio->buffer);
//err_video:
//	video_device_release(radio->videodev);
err_radio:
	kfree(radio);
err_initial:
	return retval;
}

/*
 * si470x_i2c_remove - remove the device
 */
static __devexit int si470x_i2c_remove(struct i2c_client *client)
{
	struct si470x_device *radio = i2c_get_clientdata(client);
	printk("si470x_i2c_remove \n");

	free_irq(client->irq, radio);
	video_unregister_device(radio->videodev);
	kfree(radio);

	gpio_free(FM_RCLK);
	gpio_free(GPIO_SI470X_RESET_PIN);
	gpio_free(GPIO_SI470X_AUD_CODEC_LDO_EN);


	return 0;
}

#ifdef CONFIG_PM // FX1 NOT USED
/*111
 * si470x_i2c_suspend - suspend the device
 */
static int si470x_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct si470x_device *radio = i2c_get_clientdata(client);

	printk("si470x_i2c_suspend \n");
	/* power down */
	radio->registers[POWERCFG] |= POWERCFG_DISABLE;
	if (si470x_set_register(radio, POWERCFG) < 0)
		return -EIO;

	return 0;
}


/*
 * si470x_i2c_resume - resume the device
 */
static int si470x_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct si470x_device *radio = i2c_get_clientdata(client);
	printk("si470x_i2c_resume \n");

	/* power up : need 110ms */
	radio->registers[POWERCFG] |= POWERCFG_ENABLE;
	if (si470x_set_register(radio, POWERCFG) < 0)
		return -EIO;
	msleep(110);

	return 0;
}

static SIMPLE_DEV_PM_OPS(si470x_i2c_pm, si470x_i2c_suspend, si470x_i2c_resume);
#endif


/*
 * si470x_i2c_driver - i2c driver interface
 */
static struct i2c_driver si470x_i2c_driver = {
	.driver = {
		.name		= "si470x",
		.owner		= THIS_MODULE,
#ifdef CONFIG_PM
		.pm		= &si470x_i2c_pm,
#endif
	},
	.probe			= si470x_i2c_probe,
	.remove			= __devexit_p(si470x_i2c_remove),
	.id_table		= si470x_i2c_id,
};


/**************************************************************************
 * Module Interface
 **************************************************************************/

/*
 * si470x_i2c_init - module init
 */
static int __init si470x_i2c_init(void)
{
	printk(KERN_INFO DRIVER_DESC ", Version " DRIVER_VERSION "\n");
	printk(" Version si470x_i2c_init \n");
	local_si470x_workqueue = create_workqueue("si470x") ;
	if(!local_si470x_workqueue)
	{
		printk(" local_si470x_workqueue init ERROR \n");

		return -ENOMEM;
	}
	return i2c_add_driver(&si470x_i2c_driver);
}


/*
 * si470x_i2c_exit - module exit
 */
static void __exit si470x_i2c_exit(void)
{
	if(local_si470x_workqueue)
		destroy_workqueue(local_si470x_workqueue);
	local_si470x_workqueue = NULL;
	i2c_del_driver(&si470x_i2c_driver);
}


module_init(si470x_i2c_init);
module_exit(si470x_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
