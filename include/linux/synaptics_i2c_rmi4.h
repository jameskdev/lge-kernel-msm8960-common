/* lge/include/touch_synaptics_rmi4_i2c.h
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

#define MAX_NUM_OF_BUTTON			4
#define MAX_NUM_OF_FINGER			10

/* Voltage and Current ratings */
#define SYNAPTICS_T1320_VTG_MAX_UV            3600000
#define SYNAPTICS_T1320_VTG_MIN_UV            2500000
#define SYNAPTICS_T1320_CURR_UA               20000 /* 3.6V 16Hz  -> 9mA */

#define SYNAPTICS_I2C_VTG_MAX_UV              1800000
#define SYNAPTICS_I2C_VTG_MIN_UV              1800000
#define SYNAPTICS_I2C_CURR_UA                 9630

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
	int (*power)(int on, bool log_on);
	unsigned short ic_booting_delay;		/* ms */
	unsigned long report_period;			/* ns */
	unsigned char num_of_finger;
	unsigned char num_of_button;
	unsigned short button[MAX_NUM_OF_BUTTON];
	int x_max;
	int y_max;
	unsigned char fw_ver;
	unsigned int palm_threshold;
	unsigned int delta_pos_threshold;
};

typedef struct {
	u8 query_base;
	u8 command_base;
	u8 control_base;
	u8 data_base;
	u8 int_source_count;
	u8 id;
} ts_function_descriptor;

typedef struct {
	unsigned int pos_x[MAX_NUM_OF_FINGER+1];
	unsigned int pos_y[MAX_NUM_OF_FINGER+1];
	unsigned char pressure[MAX_NUM_OF_FINGER+1];
} ts_finger_data;

struct synaptics_ts_timestamp {
	u64 start;
	u64 end;
	u64 result_t;
	unsigned long rem;
	atomic_t ready;
};

/* Debug Mask setting */
#define SYNAPTICS_RMI4_I2C_DEBUG_PRINT   (1)
#define SYNAPTICS_RMI4_I2C_ERROR_PRINT   (1)
#define SYNAPTICS_RMI4_I2C_INFO_PRINT   (1)

#if defined(SYNAPTICS_RMI4_I2C_INFO_PRINT)
#define SYNAPTICS_INFO_MSG(fmt, args...) \
			printk(KERN_INFO "[Touch] " fmt, ##args);
#else
#define SYNAPTICS_INFO_MSG(fmt, args...)     do {} while (0);
#endif

#if defined(SYNAPTICS_RMI4_I2C_DEBUG_PRINT)
#define SYNAPTICS_DEBUG_MSG(fmt, args...) \
			printk(KERN_INFO "[Touch D] [%s %d] " \
				fmt, __FUNCTION__, __LINE__, ##args);
#else
#define SYNAPTICS_DEBUG_MSG(fmt, args...)     do {} while (0);
#endif

#if defined(SYNAPTICS_RMI4_I2C_ERROR_PRINT)
#define SYNAPTICS_ERR_MSG(fmt, args...) \
			printk(KERN_ERR "[Touch E] [%s %d] " \
				fmt, __FUNCTION__, __LINE__, ##args);
#else
#define SYNAPTICS_ERR_MSG(fmt, args...)     do {} while (0);
#endif

/* SYNAPTICS_RMI4_I2C Debug mask value
 * usage: echo [debug_mask] > /sys/module/touch_synaptics_rmi4_i2c/parameters/debug_mask
 * All			: 8191 (0x1FFF)
 * No msg		: 32
 * default		: 0
 */
enum {
	SYNAPTICS_RMI4_I2C_DEBUG_NONE				= 0,
	SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE			= 1U << 0,	/* 1 */
	SYNAPTICS_RMI4_I2C_DEBUG_INT_STATUS			= 1U << 1,	/* 2 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS		= 1U << 2,	/* 4 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_POSITION	= 1U << 3,	/* 8 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_REG			= 1U << 4,	/* 16 */
	SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_STATUS		= 1U << 5,	/* 32 */
	SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_REG			= 1U << 6,	/* 64 */
	SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL		= 1U << 7,	/* 128 */
	SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY		= 1U << 8,	/* 256 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME	= 1U << 9,	/* 512 */
	SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME	= 1U << 10,	/* 1024 */
	SYNAPTICS_RMI4_I2C_DEBUG_UPGRADE_DELAY		= 1U << 11,	/* 2048 */
	SYNAPTICS_DEBUG_FW_UPGRADE					= 1U << 12,	/* 4096 */
};

#define DESCRIPTION_TABLE_START	0xe9

/* Test define for f/w upgrade sanity */

#if 0
#define TEST_BOOTING_TIME_FW_FORCE_UPGRADE
#define TEST_WRONG_CHIPSET_FW_FORCE_UPGRADE
#endif
#endif
