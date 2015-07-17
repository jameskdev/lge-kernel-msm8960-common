/* linux/i2c/touch_synaptics_rmi4_i2c.h
 *
 * Copyright (C) 2011 LGE, Inc.
 *
 * Author: hyesung.shin@lge.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __TOUCH_SYNAPTICS_RMI4_I2C_H
#define __TOUCH_SYNAPTICS_RMI4_I2C_H
#include <linux/earlysuspend.h>
#if 0
#ifdef CONFIG_TS_INFO_CLASS
#include <linux/list.h>
#include <linux/device.h>
#endif
#endif
#define DCM_MAX_NUM_OF_BUTTON			3
#define DCM_MAX_NUM_OF_FINGER			10

#define SYNAPTICS_TS_NAME "synaptics-ts"
#define SYNAPTICS_KEYTOUCH_NAME "so340010"
#define SYNAPTICS_SUPPORT_FW_UPGRADE

#define TS_SNTS_GET_FINGER_STATE_0(finger_status_reg) \
	(finger_status_reg&0x03)
#define TS_SNTS_GET_FINGER_STATE_1(finger_status_reg) \
	((finger_status_reg&0x0C)>>2)
#define TS_SNTS_GET_FINGER_STATE_2(finger_status_reg) \
	((finger_status_reg&0x30)>>4)
#define TS_SNTS_GET_FINGER_STATE_3(finger_status_reg) \
	((finger_status_reg&0xC0)>>6)
#define TS_SNTS_GET_FINGER_STATE_4(finger_status_reg) \
	(finger_status_reg&0x03)

#define TS_SNTS_GET_X_POSITION(high_reg, low_reg) \
					((int)(high_reg*0x10) + (int)(low_reg&0x0F))
#define TS_SNTS_GET_Y_POSITION(high_reg, low_reg) \
					((int)(high_reg*0x10) + (int)((low_reg&0xF0)/0x10))

#define TS_SNTS_HAS_PINCH(gesture_reg) \
					((gesture_reg&0x40)>>6)
#define TS_SNTS_HAS_FLICK(gesture_reg) \
					((gesture_reg&0x10)>>4)
#define TS_SNTS_HAS_DOUBLE_TAP(gesture_reg) \
					((gesture_reg&0x04)>>2)

#define TS_SNTS_GET_REPORT_RATE(device_control_reg) \
					((device_control_reg&0x40)>>6)

#define TS_SNTS_GET_SLEEP_MODE(device_control_reg) \
					(device_control_reg&0x07)


/* Voltage and Current ratings */
#define SYNAPTICS_T1320_VTG_MAX_UV            3600000
#define SYNAPTICS_T1320_VTG_MIN_UV            2500000
#define SYNAPTICS_T1320_CURR_UA               20000 /* 3.6V 16Hz  -> 9mA */

#define SYNAPTICS_I2C_VTG_MAX_UV              1800000
#define SYNAPTICS_I2C_VTG_MIN_UV              1800000
#define SYNAPTICS_I2C_CURR_UA                 9630


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                    REGISTER ADDR & SETTING VALUE                        */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
#define SYNAPTICS_FLASH_CONTROL_REG				0x12
#define SYNAPTICS_DATA_BASE_REG					0x13
#define SYNAPTICS_INT_STATUS_REG				0x14

#define SYNAPTICS_CONTROL_REG					0x5C
#define SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE	0x5D
#define SYNAPTICS_REDUCE_MODE_REG				0x63
#define SYNAPTICS_DELTA_X_THRES_REG				0x65
#define SYNAPTICS_DELTA_Y_THRES_REG				0x66

#define SYNAPTICS_RMI_QUERY_BASE_REG			0xE3
#define SYNAPTICS_RMI_CMD_BASE_REG				0xE4
#define SYNAPTICS_FLASH_QUERY_BASE_REG			0xE9
#define SYNAPTICS_FLASH_DATA_BASE_REG			0xEC

#define SYNAPTICS_INT_FLASH						(1<<0)
#define SYNAPTICS_INT_STATUS					(1<<1)
#define SYNAPTICS_INT_ABS0						(1<<2)

#define SYNAPTICS_CONTROL_SLEEP					0x01
#define SYNAPTICS_CONTROL_NOSLEEP				0x04
#define SYNAPTICS_CONTROL_CONFIGURED			0x80

#ifdef SYNAPTICS_SUPPORT_FW_UPGRADE
#define SYNAPTICS_FLASH_CMD_FW_CRC				0x01
#define SYNAPTICS_FLASH_CMD_FW_WRITE			0x02
#define SYNAPTICS_FLASH_CMD_ERASEALL			0x03
#define SYNAPTICS_FLASH_CMD_CONFIG_READ			0x05
#define SYNAPTICS_FLASH_CMD_CONFIG_WRITE		0x06
#define SYNAPTICS_FLASH_CMD_CONFIG_ERASE		0x07
#define SYNAPTICS_FLASH_CMD_ENABLE				0x0F
#define SYNAPTICS_FLASH_NORMAL_RESULT			0x80
#endif /* SYNAPTICS_SUPPORT_FW_UPGRADE */

#define SYNAPTICS_TS_SENSITYVITY_REG		0x9B
#define SYNAPTICS_TS_SENSITYVITY_VALUE		0x00

#define FEATURE_LGE_TOUCH_GRIP_SUPPRESSION

#define FEATURE_LGE_TOUCH_GHOST_FINGER_IMPROVE

#define SYNAPTICS_TM1576_PRODUCT_ID				"TM1576"
#define SYNAPTICS_TM1576_RESOLUTION_X			1036
#define SYNAPTICS_TM1576_RESOLUTION_Y			1976
#define SYNAPTICS_TM1576_LCD_ACTIVE_AREA		1728
#define SYNAPTICS_TM1576_BUTTON_ACTIVE_AREA		1828

#define SYNAPTICS_TM1702_PRODUCT_ID				"TM1702"
#define SYNAPTICS_TM1702_RESOLUTION_X			1036
#define SYNAPTICS_TM1702_RESOLUTION_Y			1896
#define SYNAPTICS_TM1702_LCD_ACTIVE_AREA		1728
#define SYNAPTICS_TM1702_BUTTON_ACTIVE_AREA		1805

#define SYNAPTICS_TM1743_PRODUCT_ID				"TM1743"
#define SYNAPTICS_TM1743_RESOLUTION_X			1123
#define SYNAPTICS_TM1743_RESOLUTION_Y			1872
#define SYNAPTICS_TM1743_LCD_ACTIVE_AREA		1872
#define SYNAPTICS_TM1743_BUTTON_ACTIVE_AREA		1805


#define SYNAPTICS_S3200_PRODUCT_ID				"TM-02145-001"
#define SYNAPTICS_S3200_RESOLUTION_X			959/*479*/
#define SYNAPTICS_S3200_RESOLUTION_Y			1599/*799*/
#define DEVICE_STATUS_REG				(ts->common_dsc.data_base)			/* Device Status */
#define DEVICE_STATUS_REG_UNCONFIGURED	0x80
#define DEVICE_FAILURE_MASK				0x03

#define SYNAPTICS_F54_TEST
#define GHOST_FINGER_SOLUTION
#define JITTER_FILTER_SOLUTION

#ifdef GHOST_FINGER_SOLUTION
#define MAX_GHOST_CHECK_COUNT			3
#endif

#define MAX_RETRY_COUNT					3

struct synaptics_regulator {
	const char *name;
	u32     min_uV;
	u32     max_uV;
	u32     load_uA;
};

struct synaptics_ts_platform_data {
	bool use_irq;
	unsigned long irqflags;
	unsigned short i2c_sda_gpio;
	unsigned short i2c_scl_gpio;
	unsigned short i2c_int_gpio;
	unsigned short i2c_ldo_gpio;
	int (*ts_power)(int on, bool log_on);
	unsigned long report_period;			/* ns */
	unsigned char num_of_finger;
	unsigned char num_of_button;
	unsigned short button[DCM_MAX_NUM_OF_BUTTON];
	int x_max;
	int y_max;
	unsigned char fw_ver;
	unsigned int palm_threshold;
	unsigned int delta_pos_threshold;
    bool ts_power_flag;
	int jitter_curr_ratio;
	int jitter_filter_enable;
	int accuracy_filter_enable;
};

typedef struct {
	u8 query_base;
	u8 command_base;
	u8 control_base;
	u8 data_base;
	u8 int_source_count;
	u8 id;
} ts_function_descriptor;

struct synaptics_ts_timestamp {
	u64 start;
	u64 end;
	u64 result_t;
	unsigned long rem;
	atomic_t ready;
};

#ifdef GHOST_FINGER_SOLUTION
struct ghost_finger_ctrl {
	volatile u8 stage;
	int count;
	int min_count;
	int max_count;
	int ghost_check_count;
};

enum {
	KEYGUARD_RESERVED = 0 ,
	KEYGUARD_ENABLE,
};

enum {
	GHOST_STAGE_CLEAR = 0,
	GHOST_STAGE_1 = 1,
	GHOST_STAGE_2 = 2,
	GHOST_STAGE_3 = 4,
};
#endif

struct t_data {
	u8 touch_status;
	u16 id;
	u16 x_position;
	u16 y_position;
	u16 width_major;
	u16 pressure;
};

struct touch_data {
	u8 total_num;
	u8 prev_total_num;
	u8 state;
	struct t_data curr_data[DCM_MAX_NUM_OF_FINGER];
	struct t_data prev_data[DCM_MAX_NUM_OF_FINGER];
};

#ifdef JITTER_FILTER_SOLUTION
struct jitter_history_data {
	u16 x;
	u16 y;
	u16 pressure;
	int delta_x;
	int delta_y;
};

struct jitter_filter_info {
	int id_mask;
	int adjust_margin;
	struct jitter_history_data his_data[10];
};

struct accuracy_history_data {
	u16 x;
	u16 y;
	u16 pressure;
	int count;
	int mod_x;
	int mod_y;
	int axis_x;
	int axis_y;
	int prev_total_num;
};

struct accuracy_filter_info {
	int ignore_pressure_gap;
	int touch_max_count;
	int delta_max;
	int max_pressure;
	int direction_count;
	int time_to_max_pressure;
	u16 finish_filter;
	struct accuracy_history_data his_data;
};
#endif

/* Device data structure */
struct synaptics_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct synaptics_ts_platform_data *pdata;
	struct touch_data ts_data;
	bool is_downloading;		/* avoid power off during F/W upgrade */
	bool is_suspended;			/* avoid power off during F/W upgrade */
	unsigned int button_width;
	char button_prestate[DCM_MAX_NUM_OF_BUTTON];
	char finger_prestate[DCM_MAX_NUM_OF_FINGER];
	bool ic_init;
	bool is_probed;
	bool melt_mode;					/* for Ghost finger defense - melt mode status */
	unsigned int idle_lock_distance;		/* for Ghost finger defense - lock screen drag distance */
	atomic_t interrupt_handled;
	ts_function_descriptor common_dsc;
	ts_function_descriptor finger_dsc;
	ts_function_descriptor button_dsc;
	bool area_detect_type_button;
	unsigned char int_status_reg_asb0_bit;
	unsigned char int_status_reg_button_bit;
	struct hrtimer timer;
	struct work_struct fw_work;
	struct work_struct t_work;
	struct work_struct reset_work;
	struct early_suspend early_suspend;
	struct synaptics_ts_timestamp int_delay;
	struct delayed_work init_delayed_work;
	char fw_rev;
  unsigned char fw_ver;
	char manufcturer_id;
	char product_id[11];
	char fw_path[256];
	int use_irq;
#ifdef CONFIG_TS_INFO_CLASS
	uint32_t flags;
#endif
#ifdef GHOST_FINGER_SOLUTION
	/*struct baseline_ctrl baseline;*/
	struct ghost_finger_ctrl gf_ctrl;
#endif
#ifdef JITTER_FILTER_SOLUTION
	struct jitter_filter_info jitter_filter;
	struct accuracy_filter_info accuracy_filter;
#endif
	u8 ic_init_err_cnt;
};

struct key_touch_platform_data {
	int (*tk_power)(int on, bool log_on);
	int irq;
	int scl;
	int sda;
    int ldo;
	unsigned char *keycode;
	int keycodemax;
	int gpio_int;
    bool tk_power_flag;
};

/* Debug Mask setting
 * SYNAPTICS_RMI4_I2C Debug mask value
 * usage: echo [debug_mask] > /sys/module/touch_synaptics_rmi4_i2c/parameters/debug_mask
 * All			: 8191 (0x1FFF)
 * No msg		: 32
 * default		: 0
 */
enum {
	ERR_INFO		= 0,
	PROBE_INFO		= 1U << 0,  /* 1 */
	FINGER_POSITION		= 1U << 1,  /* 2 */
	FINGER_POSITION_ONETIME = 1U << 2,  /* 4 */
	FW_VERSION		= 1U << 3,  /* 8 */
	FW_UPGRADE		= 1U << 4,  /* 16 */
	JITTER_INFO			= 1U << 5,
	GHOST_FINGER_INFO	= 1U << 6,
	ACCURACY_FILTER_INFO = 1U << 7,

#if 0
	FUNC_TRACE		= 1U << 0,	/* 1 */
	INT_STATUS		= 1U << 1,	/* 2 */
	FINGER_STATUS		= 1U << 2,	/* 4 */
	FINGER_POSITION		= 1U << 3,	/* 8 */
	FINGER_REG		= 1U << 4,	/* 16 */
	BUTTON_STATUS		= 1U << 5,	/* 32 */
	BUTTON_REG		= 1U << 6,	/* 64 */
	INT_INTERVAL		= 1U << 7,	/* 128 */
	INT_ISR_DELAY		= 1U << 8,	/* 256 */
	FINGER_HANDLE_TIME	= 1U << 9,	/* 512 */
	BUTTON_HANDLE_TIME	= 1U << 10,	/* 1024 */
	UPGRADE_DELAY		= 1U << 11,	/* 2048 */
	FW_UPGRADE		= 1U << 12,	/* 4096 */
#endif
};

static int synaptics_debug_mask = 4;/* = 0;*/

module_param_named(
	debug_mask, synaptics_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);


#define SYNAPTICS_DEBUG_MSG(mask, level, message, ...) \
	do { \
		if (!(mask) || ((mask) & synaptics_debug_mask)) {\
			printk(level message, ## __VA_ARGS__); \
		} \
	} while (0)


enum download_type {
	dl_internal,
	dl_external,
};

#define DESCRIPTION_TABLE_START	0xe9

/* Test define for f/w upgrade sanity */

#if 0
#define TEST_BOOTING_TIME_FW_FORCE_UPGRADE
#define TEST_WRONG_CHIPSET_FW_FORCE_UPGRADE
#endif
#endif
