/*
 *  drivers/media/radio/si470x/radio-si470x.h
 *
 *  Driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
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
#define DRIVER_NAME "radio-si470x"

#include <linux/i2c.h>

/* kernel includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <asm/unaligned.h>
#include <linux/kfifo.h>        /* lock free circular buffer    */
#include <linux/uaccess.h>      /* copy to/from user            */
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#define SI470X_ADDRESS (0x20>>1)
#define GPIO_SI470X_RESET_PIN		97
#define GPIO_SI470X_EN_PIN		52
#define GPIO_SI470X_AUD_CODEC_LDO_EN 47 /* Audience VDD Codec */
#define SEEKTH_VAL 0x08
struct si470x_fmradio_platform_data {
	int (*power)(int);
};
enum radio_state_t {
	FM_OFF,
	FM_RECV,
};
typedef enum {USA, EUROPE, JAPAN} country_enum; // Could be expanded
#define STD_BUF_SIZE               (128)
#define SRCH_DIR_UP                 (0)
#define SRCH_DIR_DOWN               (1)
enum v4l2_cid_private_si470x_t {
	V4L2_CID_PRIVATE_TAVARUA_SRCHMODE = (V4L2_CID_PRIVATE_BASE + 1),
	V4L2_CID_PRIVATE_TAVARUA_SCANDWELL,
	V4L2_CID_PRIVATE_TAVARUA_SRCHON,
	V4L2_CID_PRIVATE_TAVARUA_STATE,        // 4
	V4L2_CID_PRIVATE_TAVARUA_TRANSMIT_MODE,
	V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK,
	V4L2_CID_PRIVATE_TAVARUA_REGION,
	V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH,        //  8
	V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY,
	V4L2_CID_PRIVATE_TAVARUA_SRCH_PI,
	V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT,
	V4L2_CID_PRIVATE_TAVARUA_EMPHASIS,          // 12
	V4L2_CID_PRIVATE_TAVARUA_RDS_STD,
	V4L2_CID_PRIVATE_TAVARUA_SPACING,
	V4L2_CID_PRIVATE_TAVARUA_RDSON,            // 15
	V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC,   
	V4L2_CID_PRIVATE_TAVARUA_LP_MODE,
	V4L2_CID_PRIVATE_TAVARUA_ANTENNA,           // 18
	V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF,
	V4L2_CID_PRIVATE_TAVARUA_PSALL,
	V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT,   // 21
	V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME,
	V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT,
	V4L2_CID_PRIVATE_TAVARUA_IOVERC,               //  24
	V4L2_CID_PRIVATE_TAVARUA_INTDET,
	V4L2_CID_PRIVATE_TAVARUA_MPX_DCC,
	V4L2_CID_PRIVATE_TAVARUA_AF_JUMP,              //  27
	V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA,
	V4L2_CID_PRIVATE_TAVARUA_HLSI,
	V4L2_CID_PRIVATE_SOFT_MUTE,/* 0x800001E*/         //  30
	V4L2_CID_PRIVATE_RIVA_ACCS_ADDR,
	V4L2_CID_PRIVATE_RIVA_ACCS_LEN,                 //32
	V4L2_CID_PRIVATE_RIVA_PEEK,
	V4L2_CID_PRIVATE_RIVA_POKE,
	V4L2_CID_PRIVATE_SSBI_ACCS_ADDR,         //  35
	V4L2_CID_PRIVATE_SSBI_PEEK,
	V4L2_CID_PRIVATE_SSBI_POKE,
	V4L2_CID_PRIVATE_TX_TONE,               //  38
	V4L2_CID_PRIVATE_RDS_GRP_COUNTERS,
	V4L2_CID_PRIVATE_SET_NOTCH_FILTER,/* 0x8000028 */
	V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH,/* 0x8000029 */
	V4L2_CID_PRIVATE_TAVARUA_DO_CALIBRATION,/* 0x800002A : IRIS */
	V4L2_CID_PRIVATE_TAVARUA_SRCH_ALGORITHM,/* 0x800002B */
	V4L2_CID_PRIVATE_IRIS_GET_SINR, /* 0x800002C : IRIS */
	V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD, /* 0x800002D */
	V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD, /* 0x800002E */
	V4L2_CID_PRIVATE_SINR_THRESHOLD,  /* 0x800002F : IRIS */
	V4L2_CID_PRIVATE_SINR_SAMPLES,  /* 0x8000030 : IRIS */
	V4L2_CID_SI470X_AUDIO_VOLUME = (V4L2_CID_PRIVATE_BASE + 96),/* 0x8000060 : SI4709 */
	V4L2_CID_SI470X_AUDIO_MUTE = (V4L2_CID_PRIVATE_BASE + 97),/* 0x8000061 : SI4709 */	
};


/**************************************************************************
 * Register Definitions
 **************************************************************************/
#define RADIO_REGISTER_SIZE	2	/* 16 register bit width */
#define RADIO_REGISTER_NUM	16	/* DEVICEID   ... RDSD */
#define RDS_REGISTER_NUM	6	/* STATUSRSSI ... RDSD */

#define DEVICEID		0	/* Device ID */
#define DEVICEID_PN		0xf000	/* bits 15..12: Part Number */
#define DEVICEID_MFGID		0x0fff	/* bits 11..00: Manufacturer ID */

#define CHIPID			1	/* Chip ID */
#define CHIPID_REV		0xfc00	/* bits 15..10: Chip Version */
#define CHIPID_DEV		0x0200	/* bits 09..09: Device */
#define CHIPID_FIRMWARE		0x01ff	/* bits 08..00: Firmware Version */

#define POWERCFG		2	/* Power Configuration */
#define POWERCFG_DSMUTE		0x8000	/* bits 15..15: Softmute Disable */
#define POWERCFG_DMUTE		0x4000	/* bits 14..14: Mute Disable */
#define POWERCFG_MONO		0x2000	/* bits 13..13: Mono Select */
#define POWERCFG_RDSM		0x0800	/* bits 11..11: RDS Mode (Si4701 only) */
#define POWERCFG_SKMODE		0x0400	/* bits 10..10: Seek Mode */
#define POWERCFG_SEEKUP		0x0200	/* bits 09..09: Seek Direction */
#define POWERCFG_SEEK		0x0100	/* bits 08..08: Seek */
#define POWERCFG_DISABLE	0x0040	/* bits 06..06: Powerup Disable */
#define POWERCFG_ENABLE		0x0001	/* bits 00..00: Powerup Enable */

#define CHANNEL			3	/* Channel */
#define CHANNEL_TUNE		0x8000	/* bits 15..15: Tune */
#define CHANNEL_CHAN		0x03ff	/* bits 09..00: Channel Select */

#define SYSCONFIG1		4	/* System Configuration 1 */
#define SYSCONFIG1_RDSIEN	0x8000	/* bits 15..15: RDS Interrupt Enable (Si4701 only) */
#define SYSCONFIG1_STCIEN	0x4000	/* bits 14..14: Seek/Tune Complete Interrupt Enable */
#define SYSCONFIG1_RDS		0x1000	/* bits 12..12: RDS Enable (Si4701 only) */
#define SYSCONFIG1_DE		0x0800	/* bits 11..11: De-emphasis (0=75us 1=50us) */
#define SYSCONFIG1_AGCD		0x0400	/* bits 10..10: AGC Disable */
#define SYSCONFIG1_BLNDADJ	0x00c0	/* bits 07..06: Stereo/Mono Blend Level Adjustment */
#define SYSCONFIG1_GPIO3	0x0030	/* bits 05..04: General Purpose I/O 3 */
#define SYSCONFIG1_GPIO2	0x000c	/* bits 03..02: General Purpose I/O 2 */
#define SYSCONFIG1_GPIO1	0x0003	/* bits 01..00: General Purpose I/O 1 */

#define SYSCONFIG2		5	/* System Configuration 2 */
#define SYSCONFIG2_SEEKTH	0xff00	/* bits 15..08: RSSI Seek Threshold */
#define SYSCONFIG2_BAND		0x0080	/* bits 07..06: Band Select */
#define SYSCONFIG2_SPACE	0x0030	/* bits 05..04: Channel Spacing */
#define SYSCONFIG2_VOLUME	0x000f	/* bits 03..00: Volume */

#define SYSCONFIG3		6	/* System Configuration 3 */
#define SYSCONFIG3_SMUTER	0xc000	/* bits 15..14: Softmute Attack/Recover Rate */
#define SYSCONFIG3_SMUTEA	0x3000	/* bits 13..12: Softmute Attenuation */
#define SYSCONFIG3_SKSNR	0x00f0	/* bits 07..04: Seek SNR Threshold */
#define SYSCONFIG3_SKCNT	0x000f	/* bits 03..00: Seek FM Impulse Detection Threshold */

#define TEST1			7	/* Test 1 */
#define TEST1_AHIZEN		0x4000	/* bits 14..14: Audio High-Z Enable */

#define TEST2			8	/* Test 2 */
/* TEST2 only contains reserved bits */

#define BOOTCONFIG		9	/* Boot Configuration */
/* BOOTCONFIG only contains reserved bits */

#define STATUSRSSI		10	/* Status RSSI */
#define STATUSRSSI_RDSR		0x8000	/* bits 15..15: RDS Ready (Si4701 only) */
#define STATUSRSSI_STC		0x4000	/* bits 14..14: Seek/Tune Complete */
#define STATUSRSSI_SF		0x2000	/* bits 13..13: Seek Fail/Band Limit */
#define STATUSRSSI_AFCRL	0x1000	/* bits 12..12: AFC Rail */
#define STATUSRSSI_RDSS		0x0800	/* bits 11..11: RDS Synchronized (Si4701 only) */
#define STATUSRSSI_BLERA	0x0600	/* bits 10..09: RDS Block A Errors (Si4701 only) */
#define STATUSRSSI_ST		0x0100	/* bits 08..08: Stereo Indicator */
#define STATUSRSSI_RSSI		0x00ff	/* bits 07..00: RSSI (Received Signal Strength Indicator) */

#define READCHAN		11	/* Read Channel */
#define READCHAN_BLERB		0xc000	/* bits 15..14: RDS Block D Errors (Si4701 only) */
#define READCHAN_BLERC		0x3000	/* bits 13..12: RDS Block C Errors (Si4701 only) */
#define READCHAN_BLERD		0x0c00	/* bits 11..10: RDS Block B Errors (Si4701 only) */
#define READCHAN_READCHAN	0x03ff	/* bits 09..00: Read Channel */

#define RDS_TYPE_0A     ( 0 * 2 + 0)
#define RDS_TYPE_0B     ( 0 * 2 + 1)
#define RDS_TYPE_1A     ( 1 * 2 + 0)
#define RDS_TYPE_1B     ( 1 * 2 + 1)
#define RDS_TYPE_2A     ( 2 * 2 + 0)
#define RDS_TYPE_2B     ( 2 * 2 + 1)
#define RDS_TYPE_3A     ( 3 * 2 + 0)
#define RDS_TYPE_3B     ( 3 * 2 + 1)
#define RDS_TYPE_4A     ( 4 * 2 + 0)
#define RDS_TYPE_4B     ( 4 * 2 + 1)
#define RDS_TYPE_5A     ( 5 * 2 + 0)
#define RDS_TYPE_5B     ( 5 * 2 + 1)
#define RDS_TYPE_6A     ( 6 * 2 + 0)
#define RDS_TYPE_6B     ( 6 * 2 + 1)
#define RDS_TYPE_7A     ( 7 * 2 + 0)
#define RDS_TYPE_7B     ( 7 * 2 + 1)
#define RDS_TYPE_8A     ( 8 * 2 + 0)
#define RDS_TYPE_8B     ( 8 * 2 + 1)
#define RDS_TYPE_9A     ( 9 * 2 + 0)
#define RDS_TYPE_9B     ( 9 * 2 + 1)
#define RDS_TYPE_10A    (10 * 2 + 0)
#define RDS_TYPE_10B    (10 * 2 + 1)
#define RDS_TYPE_11A    (11 * 2 + 0)
#define RDS_TYPE_11B    (11 * 2 + 1)
#define RDS_TYPE_12A    (12 * 2 + 0)
#define RDS_TYPE_12B    (12 * 2 + 1)
#define RDS_TYPE_13A    (13 * 2 + 0)
#define RDS_TYPE_13B    (13 * 2 + 1)
#define RDS_TYPE_14A    (14 * 2 + 0)
#define RDS_TYPE_14B    (14 * 2 + 1)
#define RDS_TYPE_15A    (15 * 2 + 0)
#define RDS_TYPE_15B    (15 * 2 + 1)

#define BLOCK_A 6
#define BLOCK_B 4
#define BLOCK_C 2
#define BLOCK_D 0
#define CORRECTED_NONE          0
#define CORRECTED_ONE_TO_TWO    1
#define CORRECTED_THREE_TO_FIVE 2
#define UNCORRECTABLE           3
#define ERRORS_CORRECTED(data,block) ((data>>block)&0x03)
#define RDSA			12	/* RDSA */
#define RDSA_RDSA		0xffff	/* bits 15..00: RDS Block A Data (Si4701 only) */

#define RDSB			13	/* RDSB */
#define RDSB_RDSB		0xffff	/* bits 15..00: RDS Block B Data (Si4701 only) */

#define RDSC			14	/* RDSC */
#define RDSC_RDSC		0xffff	/* bits 15..00: RDS Block C Data (Si4701 only) */

#define RDSD			15	/* RDSD */
#define RDSD_RDSD		0xffff	/* bits 15..00: RDS Block D Data (Si4701 only) */


#define SI470X_DELAY 10

/**************************************************************************
 * General Driver Definitions
 **************************************************************************/
enum si470x_buf_t {
	SI470X_BUF_SRCH_LIST,
	SI470X_BUF_EVENTS,
	SI470X_BUF_RT_RDS,
	SI470X_BUF_PS_RDS,
	SI470X_BUF_RAW_RDS,
	SI470X_BUF_AF_LIST,
	SI470X_BUF_PEEK,
	SI470X_BUF_SSBI_PEEK,
	SI470X_BUF_RDS_CNTRS,
	SI470X_BUF_RD_DEFAULT,
	SI470X_BUF_CAL_DATA,
	SI470X_BUF_MAX
};
enum si470x_evt_t {
	SI470X_EVT_RADIO_READY,
	SI470X_EVT_TUNE_SUCC,
	SI470X_EVT_SEEK_COMPLETE,
	SI470X_EVT_SCAN_NEXT,
	SI470X_EVT_NEW_RAW_RDS,
	SI470X_EVT_NEW_RT_RDS,    // 5
	SI470X_EVT_NEW_PS_RDS,
	SI470X_EVT_ERROR,
	SI470X_EVT_BELOW_TH,
	SI470X_EVT_ABOVE_TH,     //  9
	SI470X_EVT_STEREO,
	SI470X_EVT_MONO,
	SI470X_EVT_RDS_AVAIL,
	SI470X_EVT_RDS_NOT_AVAIL,  // 13
	SI470X_EVT_NEW_SRCH_LIST,
	SI470X_EVT_NEW_AF_LIST,
	SI470X_EVT_TXRDSDAT,
	SI470X_EVT_TXRDSDONE,
	SI470X_EVT_RADIO_DISABLED
};

/*
 * si470x_device - private data
 */
struct si470x_device {
	struct video_device *videodev;

	/* driver management */
	unsigned int users;

	/* Silabs internal registers (0..15) */
	unsigned short registers[RADIO_REGISTER_NUM];

	/* RDS receive buffer */
	wait_queue_head_t read_queue;
	struct mutex lock;		/* buffer locking */
	unsigned char *buffer;		/* size is always multiple of three */
	unsigned int buf_size;
	unsigned int rd_index;
	unsigned int wr_index;

	struct kfifo data_buf[SI470X_BUF_MAX];
	spinlock_t buf_lock[SI470X_BUF_MAX];
	unsigned char event_value[20];
	unsigned char event_number;
	unsigned char event_lock;
	struct completion completion;
	bool stci_enabled;		/* Seek/Tune Complete Interrupt */
	wait_queue_head_t event_queue;
	struct workqueue_struct *wqueue;
	struct delayed_work work;
	unsigned char seek_onoff;
	unsigned char mute;	/*mute = 1, unmute = 0*/
	unsigned int volume;	/*volume = 0...10 */

#if defined(CONFIG_USB_SI470X) || defined(CONFIG_USB_SI470X_MODULE)
	/* reference to USB and video device */
	struct usb_device *usbdev;
	struct usb_interface *intf;

	/* Interrupt endpoint handling */
	char *int_in_buffer;
	struct usb_endpoint_descriptor *int_in_endpoint;
	struct urb *int_in_urb;
	int int_in_running;

	/* scratch page */
	unsigned char software_version;
	unsigned char hardware_version;

	/* driver management */
	unsigned char disconnected;
#endif

//#if defined(CONFIG_I2C_SI470X) || defined(CONFIG_I2C_SI470X_MODULE)
	struct i2c_client *client;
//#endif
};



/**************************************************************************
 * Firmware Versions
 **************************************************************************/

#define RADIO_FW_VERSION	15



/**************************************************************************
 * Frequency Multiplicator
 **************************************************************************/

/*
 * The frequency is set in units of 62.5 Hz when using V4L2_TUNER_CAP_LOW,
 * 62.5 kHz otherwise.
 * The tuner is able to have a channel spacing of 50, 100 or 200 kHz.
 * tuner->capability is therefore set to V4L2_TUNER_CAP_LOW
 * The FREQ_MUL is then: 1 MHz / 62.5 Hz = 16000
 */
#define FREQ_MUL (1000000 / 62.5)



/**************************************************************************
 * Common Functions
 **************************************************************************/
extern struct video_device si470x_viddev_template;
int si470x_get_register(struct si470x_device *radio, int regnr);
int si470x_set_register(struct si470x_device *radio, int regnr);
int si470x_disconnect_check(struct si470x_device *radio);
int si470x_set_freq(struct si470x_device *radio, unsigned int freq);
int si470x_get_freq(struct si470x_device *radio, unsigned int *freq);
int si470x_start(struct si470x_device *radio);
int si470x_stop(struct si470x_device *radio);
void si470x_test(struct si470x_device *radio);
int si470x_set_seek(struct si470x_device *radio,	unsigned int wrap_around, unsigned int seek_upward);
int si470x_mute(struct si470x_device *radio, u8 mute);
int si470x_fops_open(struct file *file);
int si470x_fops_release(struct file *file);
int si470x_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability);
int fmradio_power(int on);
void si470x_q_event(struct si470x_device *radio,enum si470x_evt_t event);
irqreturn_t si470x_i2c_interrupt(int irq, void *dev_id);
void updateRds(struct si470x_device *radio);
void initRdsVars(void);
void initRdsBuffers(void);
int si470x_set_chan(struct si470x_device *radio, unsigned short chan);
