/* drivers/input/touchscreen/synaptics_s3200_ts.c
 *
 * Copyright (C) 2011 LG Electironics, Inc.
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
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>
#include <mach/board_lge.h>
#include <mach/lge/board_l_dcm.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

#include <linux/i2c/synaptics_i2c_rmi4.h>
#ifdef SYNAPTICS_F54_TEST
#include "synaptics_f54.h"
#endif
#include <linux/workqueue.h>
#ifdef CONFIG_TOUCHSCREEN_S3200_SYNAPTICS_TS
/*include "TM2145-001-S001-DS4.h"*/
#include "SynaImage.h"
#endif

#ifdef CONFIG_TS_INFO_CLASS
#include "ts_class.h"
#endif

#define SYNAPTICS_TOUCH_DEBUG 1
#define TS_READ_REGS_LEN      100
#if SYNAPTICS_TOUCH_DEBUG
#define ts_printk(args...)  printk(args)
#else
#define ts_printk(args...)
#endif

/*==========================================================================*/
/*             DEFINITIONS AND DECLARATIONS FOR MODULE						*/
/* This section contains definitions for constants, macros, types, variables*/
/*and other items needed by this module.									*/
/*==========================================================================*/

struct workqueue_struct *synaptics_wq;
atomic_t ts_irq_flag;
extern atomic_t tk_irq_flag;
int fw_upgrade_status;
int checked_fw_upgrade;

static int suspend_flag;
static void synaptics_ts_early_suspend(struct early_suspend *h);
static void synaptics_ts_late_resume(struct early_suspend *h);


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                         DATA DEFINITIONS                                */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
struct so340010_device {
	struct i2c_client		*client;		/* i2c client for adapter */
	struct input_dev		*input_dev;		/* input device for android */
	struct work_struct		key_work;		/* work for touch bh */
	spinlock_t				lock;			/* protect resources */
	int	irq;
	int gpio_irq;
	struct early_suspend 	earlysuspend;
	struct delayed_work		lock_delayed_work;
	struct delayed_work		init_delayed_work;
	struct hrtimer			timer;
	u8 ic_init_err_cnt;
};
typedef struct {
	unsigned char device_status_reg;						/* 0x13 */
	unsigned char interrupt_status_reg;					/* 0x14 */
	unsigned char finger_state_reg[3];					/* 0x15~0x17 */

	unsigned char fingers_data[DCM_MAX_NUM_OF_FINGER][5];	/*0x18 ~ 0x49*/
	/* 5 data per 1 finger, support 10 fingers data
	fingers_data[x][0] : xth finger's X high position
	fingers_data[x][1] : xth finger's Y high position
	fingers_data[x][2] : xth finger's XY low position
	fingers_data[x][3] : xth finger's XY width
	fingers_data[x][4] : xth finger's Z (pressure)

	Etc...
	unsigned char gesture_flag0;							0x4A
	unsigned char gesture_flag1;							0x4B
	unsigned char pinch_motion_X_flick_distance;			0x4C
	unsigned char rotation_motion_Y_flick_distance;		0x4D
	unsigned char finger_separation_flick_time;			0x4E */
} ts_sensor_data;


static ts_sensor_data ts_reg_data;

static uint16_t SYNAPTICS_PANEL_MAX_X;
static uint16_t SYNAPTICS_PANEL_MAX_Y;
unsigned char  touch_fw_version; /* = 0;*/

int dl_mode;
int first_event;

#ifdef CONFIG_TS_INFO_CLASS
char classdev_version[5];
struct ts_info_classdev cdev;
extern int ts_info_classdev_register(struct device *parent,	struct ts_info_classdev *ts_info_cdev);
extern void ts_info_classdev_unregister(struct ts_info_classdev *ts_info_cdev);
#endif
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                           Local Functions                               */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


extern struct so340010_device *so340010_pdev;
extern struct workqueue_struct *touch_dev_reset_wq;
extern struct delayed_work		touch_reset_work; 	/* work for reset touch & key */

extern void so340010_keytouch_cancel(void);
extern void so340010_keytouch_lock_free(void);

extern int touch_dev_resume(void);


struct synaptics_ts_data *ts_data_reset;

static u8 ts_cnt;
static u8 ts_check; /* = 0;*/
static u8 ts_finger; /*= 0;*/
static u8 ts_i2c_ret; /* = 0;*/
/*static u8 ghost_count;*/

u8 is_fw_upgrade;

int synaptics_ts_read(struct i2c_client *client, u8 reg, int num, u8 *buf)
{
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = num,
			.buf = buf,
		},
	};

	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		if (printk_ratelimit())
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s transfer error\n", __func__);
		return -EIO;
	} else
		return 0;
}

int synaptics_ts_write(struct i2c_client *client, u8 reg, u8 * buf, int len)
{
	unsigned char send_buf[len + 1];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = len+1,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	memcpy(&send_buf[1], buf, len);

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s transfer error\n", __func__);
		return -EIO;
	} else
		return 0;
}

#ifdef GHOST_FINGER_SOLUTION
static int synaptics_ts_setPage(struct synaptics_ts_data *ts, unsigned short page)
{
	return i2c_smbus_write_byte_data(ts->client, 0xFF, page);
}

static int synaptics_ts_baseline_fix(struct synaptics_ts_data *ts)
{
	if (synaptics_ts_setPage(ts, 0x01) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
<<<<<<< HEAD

	if (i2c_smbus_write_byte_data(ts->client, 0x0B, 0x00) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
  msleep(10);
	if (i2c_smbus_write_byte_data(ts->client, 0x9D, 0x04) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
  msleep(10);
=======
	if (i2c_smbus_write_byte_data(ts->client, SYNAPTICS_F54_ANALOG_CTRL00, 0x00) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
	msleep(10);
	if (i2c_smbus_write_byte_data(ts->client, SYNAPTICS_F54_ANALOG_CMD00, 0x04) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
	msleep(10);
>>>>>>> FETCH_HEAD
	if (synaptics_ts_setPage(ts, 0x00) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
<<<<<<< HEAD

=======
>>>>>>> FETCH_HEAD
	SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "\n\n=====FastRelaxation Off=====\n\n");
	return 0;
}

static int synaptics_ts_baseline_open(struct synaptics_ts_data *ts)
{
	if (synaptics_ts_setPage(ts, 0x01) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}

<<<<<<< HEAD
	if (i2c_smbus_write_byte_data(ts->client, 0x0B, 0x04) < 0) {
=======
	if (i2c_smbus_write_byte_data(ts->client, SYNAPTICS_F54_ANALOG_CTRL00, 0x04) < 0) {
>>>>>>> FETCH_HEAD
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
  msleep(10);
<<<<<<< HEAD
	if (i2c_smbus_write_byte_data(ts->client, 0x9D, 0x04) < 0) {
=======
	if (i2c_smbus_write_byte_data(ts->client, SYNAPTICS_F54_ANALOG_CMD00, 0x04) < 0) {
>>>>>>> FETCH_HEAD
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
  msleep(10);
	if (synaptics_ts_setPage(ts, 0x00) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}

	SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "\n\n=====FastRelaxation On=====\n\n");
	return 0;
}

static int synaptics_ts_baseline_rebase(struct synaptics_ts_data *ts)
{
<<<<<<< HEAD
	if (i2c_smbus_write_byte_data(ts->client, 0xA2, 0x01) < 0) {
=======
	if (i2c_smbus_write_byte_data(ts->client, SYNAPTICS_F11_2D_CMD00, 0x01) < 0) {
>>>>>>> FETCH_HEAD
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
	/*msleep(100);*/
	SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "\n\n=====baseline_rebase=====\n\n");

	return 0;
}

#endif

void release_all_fingers(struct synaptics_ts_data *ts)
{
	first_event = 1;
	so340010_keytouch_lock_free();

	input_mt_sync(ts->input_dev);
	input_sync(ts->input_dev);

	ts->ts_data.total_num = 0;
	ts->ts_data.prev_total_num = 0;

	memset(&ts->ts_data.curr_data, 0x0, sizeof(struct t_data)*DCM_MAX_NUM_OF_FINGER);
	memset(&ts->ts_data.prev_data, 0x0, sizeof(struct t_data)*DCM_MAX_NUM_OF_FINGER);
}

int s3200_initialize(void)
{
	int ret = 0;
  /*int ret1 = 0;*/

	struct synaptics_ts_data *ts = i2c_get_clientdata(ts_data_reset->client);

	pr_info("%s:[TOUCH] S3200 INIT START !!\n", __func__);

    /* dummy read for touch screen activation */
<<<<<<< HEAD
	ret = i2c_smbus_read_i2c_block_data(ts_data_reset->client, 0x14, 1, (u8 *)&ts_reg_data);
=======
	ret = i2c_smbus_read_i2c_block_data(ts_data_reset->client, SYNAPTICS_INT_STATUS_REG, 1, (u8 *)&ts_reg_data);
>>>>>>> FETCH_HEAD
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] Probe::read_i2c_block_data failed\n");
		goto err_out_retry;
	}
	ret = i2c_smbus_write_byte_data(ts_data_reset->client, SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE, 0x00); /*interrupt disable*/
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c read fail-SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE\n", __func__);
		goto err_out_retry;
	}

	ret = i2c_smbus_write_byte_data(ts_data_reset->client, SYNAPTICS_CONTROL_REG, SYNAPTICS_CONTROL_CONFIGURED | SYNAPTICS_CONTROL_NOSLEEP); /*set configured*/
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-SYNAPTICS_CONTROL_REG\n", __func__);
		goto err_out_retry;
	}

	ret = i2c_smbus_write_byte_data(ts_data_reset->client, SYNAPTICS_DELTA_X_THRES_REG, 0x00);
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-SYNAPTICS_DELTA_X_THRES_REG\n", __func__);
		goto err_out_retry;
	}

	ret = i2c_smbus_write_byte_data(ts_data_reset->client, SYNAPTICS_DELTA_Y_THRES_REG, 0x00);
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-SYNAPTICS_DELTA_Y_THRES_REG\n", __func__);
		goto err_out_retry;
	}

	ret = i2c_smbus_write_byte_data(ts_data_reset->client, SYNAPTICS_REDUCE_MODE_REG, 0x00);
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-SYNAPTICS_REDUCE_MODE_REG\n", __func__);
		goto err_out_retry;
	}

	ret = i2c_smbus_write_byte_data(ts_data_reset->client, SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE, 0x07); /*interrupt enable*/
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE_failed\n", __func__);
		goto err_out_retry;
	}
#if 0
	if (synaptics_ts_setPage(ts_data_reset, 0x01) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}

  ret1 = i2c_smbus_read_i2c_block_data(ts_data_reset->client, 0x1A, 1, (u8 *)&ret);
  printk("===[Before][%s][0x%x]===\n", __func__, ret);

	ret = i2c_smbus_write_byte_data(ts_data_reset->client, 0x1A, 0xFF);
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::SYNAPTICS_SLOW_Relation_failed\n", __func__);
		goto err_out_retry;
	}

  ret1 = i2c_smbus_read_i2c_block_data(ts_data_reset->client, 0x1A, 1, (u8 *)&ret);
  printk("===[After][%s][0x%x]===\n", __func__, ret);

	if (synaptics_ts_setPage(ts_data_reset, 0x00) < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c write fail-\n", __func__);
	    return -1;
	}
#endif
#ifdef GHOST_FINGER_SOLUTION
	if (ts->gf_ctrl.stage & GHOST_STAGE_2) {
		ts->gf_ctrl.stage = GHOST_STAGE_2 | GHOST_STAGE_3;
		ret = synaptics_ts_baseline_fix(ts);
	    if (ret < 0) {
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,
					"[TOUCH] %s::GHOST_FINGER_SOLUTION_BASELINE_FIX_failed\n", __func__);
	      goto err_out_retry;
	    }
	} else {
		ts->gf_ctrl.stage = GHOST_STAGE_1;
		ret = synaptics_ts_baseline_open(ts);
		if (ret < 0) {
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,
					"[TOUCH] %s::GHOST_FINGER_SOLUTION_BASELINE_OPEN_failed\n", __func__);
	      goto err_out_retry;
	    }
	}

	ts->gf_ctrl.count = 0;
	ts->gf_ctrl.ghost_check_count = 0;
<<<<<<< HEAD
=======

#if 1
	ts->gf_ctrl.saved_x = -1;
	ts->gf_ctrl.saved_y = -1;
#endif

>>>>>>> FETCH_HEAD
#endif

#ifdef JITTER_FILTER_SOLUTION
	ts->jitter_filter.id_mask = 0;
	memset(ts->jitter_filter.his_data, 0, sizeof(ts->jitter_filter.his_data));
	memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));
#endif
	ts->ic_init_err_cnt = 0;
	return ret;

err_out_retry:
	ts->ic_init_err_cnt++;
	return -1;
}
#ifdef GHOST_FINGER_SOLUTION
<<<<<<< HEAD
=======


#define ghost_sub(x, y)	(x > y ? x - y : y - x)

>>>>>>> FETCH_HEAD
static int ghost_finger_solution(struct synaptics_ts_data *ts)
{
	int ret = 0;
	if (ts->gf_ctrl.stage & GHOST_STAGE_1) {
		if (ts->ts_data.total_num == 0) {
			if (ts->gf_ctrl.count < ts->gf_ctrl.min_count || ts->gf_ctrl.count >= ts->gf_ctrl.max_count) {
<<<<<<< HEAD
				ts->gf_ctrl.ghost_check_count = 0;
			} else {
				ts->gf_ctrl.ghost_check_count++;
			}
=======
#if 1
				if (ts->gf_ctrl.stage & GHOST_STAGE_2)
					ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
				else
					ts->gf_ctrl.ghost_check_count = 0;
			}

			else {
				if (ghost_sub(ts->gf_ctrl.saved_x, ts->ts_data.curr_data[0].x_position) > ts->gf_ctrl.max_moved ||
				   ghost_sub(ts->gf_ctrl.saved_y, ts->ts_data.curr_data[0].y_position) > ts->gf_ctrl.max_moved)
					ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT;
				else
					ts->gf_ctrl.ghost_check_count++;
			}
#else
					ts->gf_ctrl.ghost_check_count = 0;
				else
					ts->gf_ctrl.ghost_check_count++;
			}
#endif
>>>>>>> FETCH_HEAD
			if (ts->gf_ctrl.ghost_check_count >= MAX_GHOST_CHECK_COUNT) {
				ts->gf_ctrl.ghost_check_count = 0;
				ret = synaptics_ts_baseline_fix(ts);
				ts->gf_ctrl.stage &= ~GHOST_STAGE_1; /*gf_ctrl.stage bit[0] value 1->0*/
				if (!ts->gf_ctrl.stage) {
					SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "ghost_stage_finished. (NON-KEYGUARD)\n");
				}
			}
			ts->gf_ctrl.count = 0;
<<<<<<< HEAD
		} else if (ts->ts_data.total_num == 1) {
=======
#if 1
			ts->gf_ctrl.saved_x = -1;
			ts->gf_ctrl.saved_y = -1;
#endif

		} else if (ts->ts_data.total_num == 1) {
#if 1
			if (ts->gf_ctrl.saved_x == -1 && ts->gf_ctrl.saved_x == -1) {
				ts->gf_ctrl.saved_x = ts->ts_data.curr_data[0].x_position;
				ts->gf_ctrl.saved_y = ts->ts_data.curr_data[0].y_position;
			}
#endif
>>>>>>> FETCH_HEAD
			ts->gf_ctrl.count++;
			SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "ghost_stage_1 : int_count[%d/%d]\n", ts->gf_ctrl.count, ts->gf_ctrl.max_count);
		} else {
			ts->gf_ctrl.count = ts->gf_ctrl.max_count;
		}
	} else if (ts->gf_ctrl.stage & GHOST_STAGE_2) {
		if (ts->ts_data.total_num > 1) {
			ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
			ts->gf_ctrl.stage |= GHOST_STAGE_1;
			ret = synaptics_ts_baseline_open(ts);
			SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "ghost_stage_2 : multi_finger. return to ghost_stage_1[0x%x]\n", ts->gf_ctrl.stage);
		}
	} else if (ts->gf_ctrl.stage & GHOST_STAGE_3) {
		if (ts->ts_data.total_num == 0) {
			ret = synaptics_ts_baseline_rebase(ts);
			ts->gf_ctrl.stage = GHOST_STAGE_CLEAR;
			SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "ghost_stage_finished. (KEYGUARD)\n");
		}
	} else {
		;
	}
	return ret;
}
#endif


#ifdef JITTER_FILTER_SOLUTION
#define jitter_abs(x)		(x > 0 ? x : -x)
#define jitter_sub(x, y)	(x > y ? x - y : y - x)

static u16 check_boundary(int x, int max)
{
	if (x < 0)
		return 0;
	else if (x > max)
		return (u16)max;
	else
		return (u16)x;
}
<<<<<<< HEAD
#if 0
=======
#if 1
>>>>>>> FETCH_HEAD
static int check_direction(int x)
{
	if (x > 0)
		return 1;
	else if (x < 0)
		return -1;
	else
		return 0;
}

static int accuracy_filter_func(struct synaptics_ts_data *ts)
{
	int delta_x = 0;
	int delta_y = 0;

	/* finish the accuracy_filter*/
	if (ts->accuracy_filter.finish_filter == 1 &&
	   (ts->accuracy_filter.his_data.count > ts->accuracy_filter.touch_max_count
	    || ts->ts_data.total_num != 1
	    || ts->ts_data.curr_data[0].id != 0)) {
		ts->accuracy_filter.finish_filter = 0;
		ts->accuracy_filter.his_data.count = 0;
	}

	/* main algorithm*/
	if (ts->accuracy_filter.finish_filter) {
		delta_x = (int)ts->accuracy_filter.his_data.x - (int)ts->ts_data.curr_data[0].x_position;
		delta_y = (int)ts->accuracy_filter.his_data.y - (int)ts->ts_data.curr_data[0].y_position;
		if (delta_x || delta_y) {
			ts->accuracy_filter.his_data.axis_x += check_direction(delta_x);
			ts->accuracy_filter.his_data.axis_y += check_direction(delta_y);
			ts->accuracy_filter.his_data.count++;
		}

		if (ts->accuracy_filter.his_data.count == 1
				|| ((jitter_sub(ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure) > ts->accuracy_filter.ignore_pressure_gap
						|| ts->ts_data.curr_data[0].pressure > ts->accuracy_filter.max_pressure)
					&& !((ts->accuracy_filter.his_data.count > ts->accuracy_filter.time_to_max_pressure
							&& (jitter_abs(ts->accuracy_filter.his_data.axis_x) == ts->accuracy_filter.his_data.count
								|| jitter_abs(ts->accuracy_filter.his_data.axis_y) == ts->accuracy_filter.his_data.count))
						|| (jitter_abs(ts->accuracy_filter.his_data.axis_x) > ts->accuracy_filter.direction_count
							|| jitter_abs(ts->accuracy_filter.his_data.axis_y) > ts->accuracy_filter.direction_count)))) {
			ts->accuracy_filter.his_data.mod_x += delta_x;
			ts->accuracy_filter.his_data.mod_y += delta_y;
		}
	}

	/* if 'delta' > delta_max, remove the modify-value.*/
	if (ts->ts_data.curr_data[0].id != 0 ||
	   (ts->accuracy_filter.his_data.count != 1 &&
	    (jitter_abs(delta_x) > ts->accuracy_filter.delta_max || jitter_abs(delta_y) > ts->accuracy_filter.delta_max))) {
		ts->accuracy_filter.his_data.mod_x = 0;
		ts->accuracy_filter.his_data.mod_y = 0;
	}

	/* start the accuracy_filter*/
	if (ts->accuracy_filter.finish_filter == 0
	   && ts->accuracy_filter.his_data.count == 0
	   && ts->ts_data.total_num == 1
	   && ts->accuracy_filter.his_data.prev_total_num == 0
	   && ts->ts_data.curr_data[0].id == 0) {
		ts->accuracy_filter.finish_filter = 1;
		memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));
	}


	SYNAPTICS_DEBUG_MSG(ACCURACY_FILTER_INFO, KERN_INFO, "AccuracyFilter: <%d> pos[%4d,%4d] new[%4d,%4d] his[%4d,%4d] delta[%3d,%3d] mod[%3d,%3d] p[%d,%3d,%3d] axis[%2d,%2d] count[%2d/%2d] total_num[%d,%d] finish[%d]\n",
			ts->ts_data.curr_data[0].id, ts->ts_data.curr_data[0].x_position, ts->ts_data.curr_data[0].y_position,
			check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->x_max),
			check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->y_max),
			ts->accuracy_filter.his_data.x, ts->accuracy_filter.his_data.y,
			delta_x, delta_y,
			ts->accuracy_filter.his_data.mod_x, ts->accuracy_filter.his_data.mod_y,
			jitter_sub(ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure) > ts->accuracy_filter.ignore_pressure_gap,
			ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure,
			ts->accuracy_filter.his_data.axis_x, ts->accuracy_filter.his_data.axis_y,
			ts->accuracy_filter.his_data.count, ts->accuracy_filter.touch_max_count,
			ts->accuracy_filter.his_data.prev_total_num, ts->ts_data.total_num, ts->accuracy_filter.finish_filter);

	ts->accuracy_filter.his_data.x = ts->ts_data.curr_data[0].x_position;
	ts->accuracy_filter.his_data.y = ts->ts_data.curr_data[0].y_position;
	ts->accuracy_filter.his_data.pressure = ts->ts_data.curr_data[0].pressure;
	ts->accuracy_filter.his_data.prev_total_num = ts->ts_data.total_num;

	if (ts->ts_data.total_num) {
		ts->ts_data.curr_data[0].x_position
			= check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->x_max);
		ts->ts_data.curr_data[0].y_position
			= check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->y_max);
	}

	return 0;
}
#endif
static int jitter_filter_func(struct synaptics_ts_data *ts)
{
	int i;
	int jitter_count = 0;
	u16 new_id_mask = 0;
	u16 bit_mask = 0;
	u16 bit_id = 1;
	int curr_ratio = ts->pdata->jitter_curr_ratio;
<<<<<<< HEAD
#if 0
=======
#if 1
>>>>>>> FETCH_HEAD
	if (ts->accuracy_filter.finish_filter)
		return 0;
#endif
	for (i = 0; i < ts->ts_data.total_num; i++) {
		u16 id = ts->ts_data.curr_data[i].id;
		u16 width = ts->ts_data.curr_data[i].width_major;
		new_id_mask |= (1 << id);

		if (ts->jitter_filter.id_mask & (1 << id)) {
			int delta_x, delta_y;
			int f_jitter = curr_ratio*width;
			int adjust_x, adjust_y;

			if (ts->jitter_filter.adjust_margin > 0) {
				adjust_x = (int)ts->ts_data.curr_data[i].x_position - (int)ts->jitter_filter.his_data[id].x;
				adjust_y = (int)ts->ts_data.curr_data[i].y_position - (int)ts->jitter_filter.his_data[id].y;

				if (jitter_abs(adjust_x) > ts->jitter_filter.adjust_margin) {
					adjust_x = (int)ts->ts_data.curr_data[i].x_position + (adjust_x >> 2);
					ts->ts_data.curr_data[i].x_position = check_boundary(adjust_x, ts->pdata->x_max);
				}

				if (jitter_abs(adjust_y) > ts->jitter_filter.adjust_margin) {
					adjust_y = (int)ts->ts_data.curr_data[i].y_position + (adjust_y >> 2);
					ts->ts_data.curr_data[i].y_position = check_boundary(adjust_y, ts->pdata->y_max);
				}
			}

			ts->ts_data.curr_data[i].x_position = (ts->ts_data.curr_data[i].x_position + ts->jitter_filter.his_data[id].x) >> 1;
			ts->ts_data.curr_data[i].y_position = (ts->ts_data.curr_data[i].y_position + ts->jitter_filter.his_data[id].y) >> 1;

			delta_x = (int)ts->ts_data.curr_data[i].x_position - (int)ts->jitter_filter.his_data[id].x;
			delta_y = (int)ts->ts_data.curr_data[i].y_position - (int)ts->jitter_filter.his_data[id].y;

			ts->jitter_filter.his_data[id].delta_x = delta_x * curr_ratio
					+ ((ts->jitter_filter.his_data[id].delta_x * (128 - curr_ratio)) >> 7);
			ts->jitter_filter.his_data[id].delta_y = delta_y * curr_ratio
					+ ((ts->jitter_filter.his_data[id].delta_y * (128 - curr_ratio)) >> 7);

			SYNAPTICS_DEBUG_MSG(JITTER_INFO, KERN_INFO, "JitterFilter: <%d> pos[%d,%d] h_pos[%d,%d] delta[%d,%d] h_delta[%d,%d] j_fil[%d] p[%d,%d]\n",
					id, ts->ts_data.curr_data[i].x_position, ts->ts_data.curr_data[i].y_position,
					ts->jitter_filter.his_data[id].x, ts->jitter_filter.his_data[id].y, delta_x, delta_y,
					ts->jitter_filter.his_data[id].delta_x, ts->jitter_filter.his_data[id].delta_y, f_jitter,
					ts->ts_data.curr_data[i].pressure, ts->jitter_filter.his_data[id].pressure);

			if (jitter_abs(ts->jitter_filter.his_data[id].delta_x) <= f_jitter &&
			   jitter_abs(ts->jitter_filter.his_data[id].delta_y) <= f_jitter)
				jitter_count++;
		}
	}

	bit_mask = ts->jitter_filter.id_mask ^ new_id_mask;

	for (i = 0, bit_id = 1; i < DCM_MAX_NUM_OF_FINGER; i++) {
		if (ts->jitter_filter.id_mask & bit_id && !(new_id_mask & bit_id)) {
			SYNAPTICS_DEBUG_MSG(JITTER_INFO, KERN_INFO, "JitterFilter: released - id[%d] mask[0x%x]\n", bit_id, ts->jitter_filter.id_mask);
			memset(&ts->jitter_filter.his_data[i], 0, sizeof(struct jitter_history_data));
		}
		bit_id = bit_id << 1;
	}

	for (i = 0; i < ts->ts_data.total_num; i++) {
		u16 id = ts->ts_data.curr_data[i].id;
		ts->jitter_filter.his_data[id].pressure = ts->ts_data.curr_data[i].pressure;
	}

	if (!bit_mask && ts->ts_data.total_num && ts->ts_data.total_num == jitter_count) {
		SYNAPTICS_DEBUG_MSG(JITTER_INFO, KERN_INFO, "JitterFilter: ignored - jitter_count[%d] total_num[%d] bitmask[0x%x]\n", jitter_count, ts->ts_data.total_num, bit_mask);
		return -1;
	}

	for (i = 0; i < ts->ts_data.total_num; i++) {
		u16 id = ts->ts_data.curr_data[i].id;
		ts->jitter_filter.his_data[id].x = ts->ts_data.curr_data[i].x_position;
		ts->jitter_filter.his_data[id].y = ts->ts_data.curr_data[i].y_position;
	}

	ts->jitter_filter.id_mask = new_id_mask;

	return 0;
}
#endif
static void synaptics_ts_work_func(struct work_struct *t_work)
{
	struct synaptics_ts_data *ts = container_of(t_work, struct synaptics_ts_data, t_work);
  int ret = 0;

touch_work_retry:
	if (suspend_flag == 1) {
		return;
	}


	ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE, 0x00); /*interrupt disable*/
	if (unlikely(ret < 0)) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,	"[TOUCH] resume::i2c write fail-SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE\n");
	}
	ts_i2c_ret = synaptics_ts_read(ts->client, SYNAPTICS_DATA_BASE_REG, sizeof(ts_reg_data), (u8 *)&ts_reg_data);/*i2c_smbus_read_i2c_block_data changed  synaptics_ts_read*/
	/* i2c read fail check */
	/* interrupt pin check  */
	if (unlikely(!gpio_get_value(ts->pdata->i2c_int_gpio))) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] interrupt pin is not high. Reset Touch IC\n");
		goto err_out_retry;
	}

	/* i2c read fail check */
	/* when ts_i2c_res < 0, i2c read fail */
	if (unlikely(ts_i2c_ret)) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] i2c read fail. Reset Touch IC\n");
		goto err_out_retry;
	}

	/* ESD damage check */
	if ((ts_reg_data.device_status_reg & DEVICE_FAILURE_MASK) == DEVICE_FAILURE_MASK) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] ESD damage occured. Reset Touch IC\n");
		goto err_out_retry;
	}

	/* interrupt status check */
	if ((ts_reg_data.interrupt_status_reg & SYNAPTICS_INT_FLASH) == SYNAPTICS_INT_FLASH) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] FLASH interrupt occured. Reset Touch IC\n");
		goto err_out_retry;
	}
	ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE, 0x07);
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] resume::i2c write fail-SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE\n");
		goto err_out_retry;
  }

	/* Internal reset check */
	if ((ts_reg_data.device_status_reg & DEVICE_STATUS_REG_UNCONFIGURED) == DEVICE_STATUS_REG_UNCONFIGURED) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "Touch IC Resetted Internally. Reconfigure register setting\n");
		ret = s3200_initialize();
		if (ret < 0) {
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c s3200_initialize fail\n", __func__);
			goto err_out_retry;
		}
	}

	ts->ts_data.total_num = 0;
	for (ts_cnt = 0; ts_cnt < DCM_MAX_NUM_OF_FINGER; ts_cnt++) {
		ts_check = 1 << ((ts_cnt % 4) * 2);
		ts_finger = (u8)(ts_cnt / 4);

		if ((ts_reg_data.finger_state_reg[ts_finger] & ts_check) == ts_check) {
			ts->ts_data.curr_data[ts->ts_data.total_num].id = ts_cnt;
			ts->ts_data.curr_data[ts->ts_data.total_num].x_position = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.fingers_data[ts_cnt][0], ts_reg_data.fingers_data[ts_cnt][2]);
			ts->ts_data.curr_data[ts->ts_data.total_num].y_position = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.fingers_data[ts_cnt][1], ts_reg_data.fingers_data[ts_cnt][2]);

			if ((((ts_reg_data.fingers_data[ts_cnt][3] & 0xf0) >> 4) - (ts_reg_data.fingers_data[ts_cnt][3] & 0x0f)) > 0) {
				ts->ts_data.curr_data[ts->ts_data.total_num].width_major = (ts_reg_data.fingers_data[ts_cnt][3] & 0xf0) >> 4;
			} else {
				ts->ts_data.curr_data[ts->ts_data.total_num].width_major = ts_reg_data.fingers_data[ts_cnt][3] & 0x0f;
			}

			ts->ts_data.curr_data[ts->ts_data.total_num].pressure = ts_reg_data.fingers_data[ts_cnt][4];
			ts->ts_data.total_num++;
		}
	}

#ifdef GHOST_FINGER_SOLUTION
	if (unlikely(ts->gf_ctrl.stage)) {
		if (ghost_finger_solution(ts)) {
			SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "[%s] ghost_finger_solution was failed!!!!!\n", __func__);
			goto err_out_retry;
		}
	}
#endif

#ifdef JITTER_FILTER_SOLUTION
<<<<<<< HEAD
#if 0
=======
#if 1
>>>>>>> FETCH_HEAD
	if (likely(ts->pdata->accuracy_filter_enable)) {
		if (accuracy_filter_func(ts) < 0) {
			goto out;
		}
	}
#endif
	if (likely(ts->pdata->jitter_filter_enable)) {
		if (jitter_filter_func(ts) < 0) {
			goto out;
		}
	}
#endif

	if (ts->ts_data.total_num > 0) {
		for (ts_cnt = 0; ts_cnt < ts->ts_data.total_num; ts_cnt++) {
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->ts_data.curr_data[ts_cnt].x_position);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->ts_data.curr_data[ts_cnt].y_position);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->ts_data.curr_data[ts_cnt].width_major);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, ts->ts_data.curr_data[ts_cnt].pressure);
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, ts->ts_data.curr_data[ts_cnt].id);
			input_mt_sync(ts->input_dev);

			if (first_event) {
				so340010_keytouch_cancel(); /* for key cancel */
				SYNAPTICS_DEBUG_MSG(FINGER_POSITION_ONETIME, KERN_INFO, "[TOUCH] %d press position x:%d, y:%d\n",
						ts->ts_data.curr_data[ts_cnt].id, ts->ts_data.curr_data[0].x_position, ts->ts_data.curr_data[0].y_position);
				first_event = 0;
			} else {
				SYNAPTICS_DEBUG_MSG(FINGER_POSITION, KERN_INFO, "[TOUCH] %d press position x:%d, y:%d\n",
						ts->ts_data.curr_data[ts_cnt].id, ts->ts_data.curr_data[ts_cnt].x_position, ts->ts_data.curr_data[ts_cnt].y_position);
			}
		}

		memset(ts->ts_data.prev_data, 0x0, sizeof(struct t_data)*DCM_MAX_NUM_OF_FINGER);
		memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(struct t_data)*DCM_MAX_NUM_OF_FINGER);
	} else {
		first_event = 1;
		input_mt_sync(ts->input_dev);
		SYNAPTICS_DEBUG_MSG(FINGER_POSITION_ONETIME, KERN_INFO, "[TOUCH] release position x:%d, y:%d\n",
				ts->ts_data.prev_data[0].x_position, ts->ts_data.prev_data[0].y_position);
		so340010_keytouch_lock_free();/* for key cancel */
	}
	input_sync(ts->input_dev);
	ts->ts_data.prev_total_num = ts->ts_data.total_num;

	memset(ts->ts_data.curr_data, 0x0, sizeof(struct t_data)*DCM_MAX_NUM_OF_FINGER);


#ifdef JITTER_FILTER_SOLUTION
out:
	/*printk("[%s] jitter filter ignore!\n", __func__);*/
    if (gpio_get_value(ts->pdata->i2c_int_gpio) != 1) {
		goto touch_work_retry;
    }
#endif
	if (atomic_read(&ts_irq_flag) == 0) {
		atomic_set(&ts_irq_flag, 1);
		enable_irq(ts->client->irq);
	}

	return;

err_out_retry:
	ts->ic_init_err_cnt++;

	queue_delayed_work(touch_dev_reset_wq, &touch_reset_work, msecs_to_jiffies(100));

	return;
}

irqreturn_t synaptics_ts_irq_handler(int irq, void *dev_id)
{
	struct synaptics_ts_data *ts = (struct synaptics_ts_data *)dev_id;

	if (atomic_read(&ts_irq_flag) == 1) {
		atomic_set(&ts_irq_flag, 0);
		disable_irq_nosync(ts->client->irq);
	}
	queue_work(synaptics_wq, &ts->t_work);

	return IRQ_HANDLED;
}

#ifdef SYNAPTICS_F54_TEST
static void F54_PDTscan(struct synaptics_ts_data *ts)
{
	unsigned char address;
	unsigned char buffer[6];
	for (address = 0xe9; address > 0xd0; address = address - 6)	{
		synaptics_ts_read(ts->client, address, sizeof(buffer), buffer);

		if (!buffer[5])
			break;

		switch (buffer[5]) {
		case 0x01:
			F01_RMI_Ctrl0 = buffer[2]; /*0x5C*/
			break;
		case 0x11:
			F11_Query_Base = buffer[0]; /*0xC6*/
			F11_MaxNumberOfTx_Addr = F11_Query_Base + 2; /*0xC8*/
			F11_MaxNumberOfRx_Addr = F11_Query_Base + 3; /*0xC9*/
			break;
		case 0x54:
			F54_Query_Base = buffer[0]; /*0x9E*/
			F54_Command_Base = buffer[1]; /*0x9D*/
			F54_Control_Base = buffer[2]; /*0x0B*/
			F54_Data_Base = buffer[3]; /*0x00*/

			F54_Data_LowIndex = F54_Data_Base + 1; /*0x01*/
			F54_Data_HighIndex = F54_Data_Base + 2; /*0x02*/
			F54_Data_Buffer = F54_Data_Base + 3; /*0x03*/
			F54_PhysicalRx_Addr = F54_Control_Base + 18; /*0x1C*/
			break;
		}
	}
}

static void F54_RegSetup(struct synaptics_ts_data *ts)
{
	int i;
	numberOfRx = 0;
	numberOfTx = 0;

	synaptics_ts_setPage(ts, 0x01);
	F54_PDTscan(ts); /*scan for page 0x01*/

	synaptics_ts_setPage(ts, 0x00);
	F54_PDTscan(ts); /*scan for page 0x00*/

	/*Check Used Rx channels*/
	synaptics_ts_read(ts->client, F11_MaxNumberOfRx_Addr, sizeof(MaxNumberRx), &MaxNumberRx);
	synaptics_ts_setPage(ts, 0x01);
	F54_PhysicalTx_Addr = F54_PhysicalRx_Addr + MaxNumberRx; /*0x30*/
	synaptics_ts_read(ts->client, F54_PhysicalRx_Addr, MaxNumberRx, &RxChannelUsed[0]);

	/*Checking Used Tx channels*/
	synaptics_ts_setPage(ts, 0x00);
	synaptics_ts_read(ts->client, F11_MaxNumberOfTx_Addr, sizeof(MaxNumberTx), &MaxNumberTx);
	synaptics_ts_setPage(ts, 0x01);
	synaptics_ts_read(ts->client, F54_PhysicalTx_Addr, MaxNumberTx, &TxChannelUsed[0]);

	/*Check used number of Rx*/
	numberOfRx = 0;
	for (i = 0; i < MaxNumberRx; i++) {
		if (RxChannelUsed[i] == 0xff)
			break;
		numberOfRx++;
	}

	/*Check used number of Tx*/
	numberOfTx = 0;
	for (i = 0; i < MaxNumberTx; i++) {
		if (TxChannelUsed[i] == 0xff)
			break;
		numberOfTx++;
	}

	/* Enabling only the analog image reporting interrupt, and turn off the rest*/
	synaptics_ts_setPage(ts, 0x00);
	i2c_smbus_write_byte_data(ts->client, F01_RMI_Ctrl0+1, 0x08);

	synaptics_ts_setPage(ts, 0x01);
}

unsigned short ImageArray[CFG_F54_TXCOUNT][CFG_F54_RXCOUNT];
unsigned char ImageBuffer[CFG_F54_TXCOUNT*CFG_F54_RXCOUNT*2];

/* echo 1 > f54 */
static void F54_AutoScanReport(struct synaptics_ts_data *ts, u8 reportType)
{
	int i, j, k;
	int length;
	unsigned char command;

	if (atomic_read(&ts_irq_flag) == 1) {
		atomic_set(&ts_irq_flag, 0);
		disable_irq_nosync(ts->client->irq);
	}
	cancel_work_sync(&ts->t_work);

	if (atomic_read(&ts_irq_flag) == 1) {
		atomic_set(&ts_irq_flag, 0);
		disable_irq_nosync(ts->client->irq);
	}

	F54_RegSetup(ts); /*PDT scan for reg map adress mapping*/

	length = numberOfTx * numberOfRx * 2;

	/*Set report mode to to run the AutoScan*/
	/*ReportType(0x03) = Raw 16-bit Image, (0x02) = baseline(Normalized 16-Bit Image Report)*/
	if (reportType == 1) {
		printk("\nF54_AutoScanReport(), Baseline\n");
		i2c_smbus_write_byte_data(ts->client, F54_Data_Base, 0x03);
	} else if (reportType == 2) {
		printk("\nF54_AutoScanReport(), Delta image\n");
		i2c_smbus_write_byte_data(ts->client, F54_Data_Base, 0x02);
	}

	i2c_smbus_write_byte_data(ts->client, F54_Data_LowIndex, 0x00);
	i2c_smbus_write_byte_data(ts->client, F54_Data_HighIndex, 0x00);

	/*Set the GetReport bit to run the AutoScan*/
	i2c_smbus_write_byte_data(ts->client, F54_Command_Base, 0x01);

	/*Wait until the command is completed*/
	do {
		mdelay(1); /*wait 1ms*/
		synaptics_ts_read(ts->client, F54_Command_Base, sizeof(command), &command);
	} while (command == 0x01);

	synaptics_ts_read(ts->client, F54_Data_Buffer, length, (u8 *)&ImageBuffer[0]);

	k = 0;
	printk("\n========================================\n\n");
	for (i = 0; i < numberOfTx; i++) {
		for (j = 0; j < numberOfRx; j++) {
			printk("%d %d ", ImageBuffer[k], ImageBuffer[k+1]);
			ImageArray[i][j] = (ImageBuffer[k] | (ImageBuffer[k+1] << 8));
			k = k + 2;
		}
		printk("\n");
	}
	printk("\n========================================\n\n");

	/*enable all the interrupts*/
	synaptics_ts_setPage(ts, 0x00);
	i2c_smbus_write_byte_data(ts->client, F01_RMI_Ctrl0+1, 0x0f);

	if (atomic_read(&ts_irq_flag) == 0) {
		atomic_set(&ts_irq_flag, 1);
		enable_irq(ts->client->irq);
	}
}

static ssize_t show_F54(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	F54_AutoScanReport(ts, 1);
	F54_AutoScanReport(ts, 2);

	return sprintf(buf, "F54 print complete!\n");
}

#endif
#ifdef GHOST_FINGER_SOLUTION
static ssize_t store_keyguard_info(struct device *dev, struct device_attribute *attr,  const char *buf, size_t count)
{
	int value;
	struct i2c_client *client;
	struct synaptics_ts_data *ts;

	if (dev)
		client = to_i2c_client(dev);
	else
		return 0;
	if (client)
		ts = i2c_get_clientdata(client);
	else
		return 0;
	if (!ts)
		return 0;

	sscanf(buf, "%d", &value);
	SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "== keyguard value [%d]\n", value);

	if (value == KEYGUARD_ENABLE) {
		ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2 | GHOST_STAGE_3;
	} else if (value == KEYGUARD_RESERVED) {
		ts->gf_ctrl.stage &= ~GHOST_STAGE_2;
	} else {
		;
	}
	SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "ghost_stage = 0x%x\n", ts->gf_ctrl.stage);
	if (value == KEYGUARD_RESERVED)
		SYNAPTICS_DEBUG_MSG(GHOST_FINGER_INFO, KERN_INFO, "ghost_stage2 : cleard[0x%x]\n", ts->gf_ctrl.stage);

	return count;
}
#endif
static ssize_t
ts_desc_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	int len;

	len = snprintf(buf, PAGE_SIZE, "\n============ Touch_GPIO_Information => status =============\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===========================================================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== gpio_informaion => cat status                       ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== ts_version_show => cat fw_version                   ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== power_on        => 1) echo 1 > power_control        ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== power_off       => 2) echo 2 > power_control        ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== reset           => 3) echo 3 > power_control        ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== fw_Upgrade      => 1) echo dl_internal > fw_upgrade ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== fw_Upgrade      => 2) echo dl_external > fw_upgrade ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== int_pin_high    => 1) echo 1 > irq_control          ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== int_pin_low     => 2) echo 2 > irq_control          ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== enable on       => 3) echo 3 > irq_control          ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== disable on      => 4) echo 4 > irq_control          ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== ts_product_id   => cat product_id                   ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== irq_flag_status => cat irq_status                   ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                     ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=== ts_ESD_Detection => cat ESD_Detection                ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "===                                                      ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "============================================================\n");

	return len;
}
static ssize_t
ts_ESD_Detection(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;

	len = snprintf(buf, PAGE_SIZE, "[Synaptics ESD Dection 0x03]\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%0x", ts_reg_data.device_status_reg & DEVICE_FAILURE_MASK);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t
ts_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	int len;

	len = snprintf(buf, PAGE_SIZE, "[Synaptics s3200 Firmware Version, on IC]\n");
<<<<<<< HEAD
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, 0x0058));
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, 0x0059));
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, 0x005A));
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, 0x005B));
=======
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, SYNAPTICS_F34_FLASH_CTRL0001));
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, SYNAPTICS_F34_FLASH_CTRL0002));
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, SYNAPTICS_F34_FLASH_CTRL0003));
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", i2c_smbus_read_byte_data(ts->client, SYNAPTICS_F34_FLASH_CTRL0004));
>>>>>>> FETCH_HEAD
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "[Synaptics s3200 Firmware Version, on \"SynaImage.h\" file]\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", SynaFirmware[0xB100]);
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", SynaFirmware[0xB101]);
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", SynaFirmware[0xB102]);
	len += snprintf(buf + len, PAGE_SIZE - len, "%c", SynaFirmware[0xB103]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}


static ssize_t
ts_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "\nSynaptics Device Status\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=============================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "GPIO_INT num  is %d\n", ts->pdata->i2c_int_gpio);
	len += snprintf(buf + len, PAGE_SIZE - len, "GPIO_INT num  is %d(level=%d)\n", ts->pdata->i2c_int_gpio, gpio_get_value(ts->pdata->i2c_int_gpio));
	len += snprintf(buf + len, PAGE_SIZE - len, "GPIO_SCL num  is %d\n", ts->pdata->i2c_scl_gpio);
	len += snprintf(buf + len, PAGE_SIZE - len, "GPIO_SDA num  is %d\n", ts->pdata->i2c_sda_gpio);
	len += snprintf(buf + len, PAGE_SIZE - len, "GPIO_POWER_EN  num  is %d\n", ts->pdata->i2c_ldo_gpio);
	len += snprintf(buf + len, PAGE_SIZE - len, "IRQ is %s\n", ts->use_irq ? "enabled" : "disabled");
	len += snprintf(buf + len, PAGE_SIZE - len, "Power status  is %s\n", gpio_get_value(ts->pdata->i2c_ldo_gpio) ? "on" : "off");
	return len;
}

static ssize_t
ts_power_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	int cmd;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	switch (cmd) {
	case 1: /* touch power on */
		touch_dev_resume();
		break;
	case 2: /*touch power off */
		ts->pdata->ts_power(0, true);
		break;
	case 3:
		if (atomic_read(&ts_irq_flag) == 1) {
			atomic_set(&ts_irq_flag, 0);
			disable_irq_nosync(ts->client->irq);
		}
		cancel_work_sync(&ts->t_work);
		if (atomic_read(&ts_irq_flag) == 1) {
			atomic_set(&ts_irq_flag, 0);
			disable_irq_nosync(ts->client->irq);
		}
		release_all_fingers(ts);

		if (atomic_read(&tk_irq_flag) == 1) {
			atomic_set(&tk_irq_flag, 0);
			disable_irq_nosync(so340010_pdev->client->irq);
		}
		cancel_work_sync(&so340010_pdev->key_work);
		hrtimer_cancel(&so340010_pdev->timer);

		ts->pdata->ts_power(0, true);
		queue_delayed_work(touch_dev_reset_wq, &touch_reset_work, msecs_to_jiffies(100));
		break;
	default:
		printk(KERN_INFO "usage: echo [1|2|3] > control\n");
		printk(KERN_INFO "  - 1: key power on\n");
		printk(KERN_INFO "  - 2: key power off\n");
		printk(KERN_INFO "  - 3: key power reset\n");
		break;
	}
	return count;
}


static ssize_t
ts_irq_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	int cmd, ret;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	switch (cmd) {
	case 1: /* interrupt pin high */
		ret = gpio_direction_input(ts->pdata->i2c_int_gpio);
		if (ret < 0) {
			printk(KERN_ERR "%s: gpio input direction fail\n", __FUNCTION__);
			break;
		}
		gpio_set_value(ts->pdata->i2c_int_gpio, 1);
		printk(KERN_INFO "TS INTR GPIO pin high\n");
		break;
	case 2: /* interrupt pin LOW */
		ret = gpio_direction_input(ts->pdata->i2c_int_gpio);
		if (ret < 0) {
			printk(KERN_ERR "%s: gpio input direction fail\n", __FUNCTION__);
			break;
		}
		gpio_set_value(ts->pdata->i2c_int_gpio, 0);
		printk(KERN_INFO "TS INTR GPIO pin low\n");
		break;
	case 3:
		if (ts->use_irq == 0) {
			ts->use_irq = 1;
			if (atomic_read(&ts_irq_flag) == 0) {
				atomic_set(&ts_irq_flag, 1);
				enable_irq(ts->client->irq);
			}
		} else
			printk(KERN_INFO "Already Irq Enabled\n");
		break;
	case 4:
		if (ts->use_irq == 1) {
			ts->use_irq = 0;
			if (atomic_read(&ts_irq_flag) == 1) {
				atomic_set(&ts_irq_flag, 0);
				disable_irq_nosync(ts->client->irq);
			}
		} else
			printk(KERN_INFO "Already Irq Disabled\n");
		break;
	default:
		printk(KERN_INFO "usage: echo [1|2|3|4] > control\n");
		printk(KERN_INFO "  - 1: interrupt pin high\n");
		printk(KERN_INFO "  - 2: interrupt pin low\n");
		printk(KERN_INFO "  - 3: enable_irq\n");
		printk(KERN_INFO "  - 4: disable_irq_nosync\n");
		break;
	}
	return count;
}
static ssize_t ts_irq_flag_show(struct device *dev,  struct device_attribute *attr,  char *buf)
{

	int len;
  int ts_value;

  if (atomic_read(&ts_irq_flag) == 1)
    ts_value = 1;/*enable_irq*/
  else
    ts_value = 0;/*disable_irq*/

  len = snprintf(buf, PAGE_SIZE, "==========[result = %d]=========\n", ts_value);
  len += snprintf(buf + len, PAGE_SIZE - len, "=== result[1] = enable_irq  ===\n");
  len += snprintf(buf + len, PAGE_SIZE - len, "=== result[0] = disable_irq ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=================================\n");

	return len;
}

static ssize_t
ts_reg_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	int cmd, ret, reg_addr, length, i;
	uint8_t reg_buf[TS_READ_REGS_LEN];

	if (sscanf(buf, "%d, 0x%x, %d", &cmd, &reg_addr, &length) != 3)
		return -EINVAL;
	switch (cmd) {
	case 1:
		reg_buf[0] = reg_addr;
		ret = i2c_master_send(ts->client, reg_buf, 1);
		if (ret < 0) {
			printk(KERN_ERR "i2c master send fail\n");
			break;
		}
		ret = i2c_master_recv(ts->client, reg_buf, length);
		if (ret < 0) {
			printk(KERN_ERR "i2c master recv fail\n");
			break;
		}
		for (i = 0; i < length; i++) {
			printk(KERN_INFO "0x%x", reg_buf[i]);
		}
		printk(KERN_INFO "\n 0x%x register read done\n", reg_addr);
		break;
	case 2:
		reg_buf[0] = reg_addr;
		reg_buf[1] = length;
		ret = i2c_master_send(ts->client, reg_buf, 2);
		if (ret < 0) {
			printk(KERN_ERR "i2c master send fail\n");
			break;
		}
		printk(KERN_INFO "\n 0x%x register write done\n", reg_addr);
		break;
	default:
		printk(KERN_INFO "usage: echo [1(read)|2(write)], [reg address], [length|value] > reg_control\n");
		printk(KERN_INFO "  - Register Set or Read\n");
		break;
	}
	return count;
}


#if 0
static unsigned char synaptics_ts_check_fwver(struct i2c_client *client)
{
	unsigned char RMI_Query_BaseAddr;
	unsigned char FWVersion_Addr;

	unsigned char SynapticsFirmVersion;

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] synaptics_firmware_check [START]\n");
	RMI_Query_BaseAddr = i2c_smbus_read_byte_data(client, SYNAPTICS_RMI_QUERY_BASE_REG);
	FWVersion_Addr = RMI_Query_BaseAddr+3; /*0xAC + 3 = 0xAF(=Firmware Revision value)*/

	SynapticsFirmVersion = i2c_smbus_read_byte_data(client, FWVersion_Addr);
    printk(KERN_INFO"[touch] Touch controller Firmware Version = %x\n", SynapticsFirmVersion);

	return SynapticsFirmVersion;
}
#endif

static int atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		default:
			return val;
		}
	}
}

u8 synaptics_ts_need_upgrade(struct i2c_client *client)
{
	unsigned char ic_data[5] = {0,};
	unsigned char image_data[5] = {SynaFirmware[0xB100], SynaFirmware[0xB101], SynaFirmware[0xB102], SynaFirmware[0xB103], '\0'};

	u8 ic_version = 0;
	u8 image_version = 0;

<<<<<<< HEAD
	i2c_smbus_read_i2c_block_data(client, 0x0058, 4, (u8 *)ic_data);
=======
	i2c_smbus_read_i2c_block_data(client, SYNAPTICS_F34_FLASH_CTRL0001, 4, (u8 *)ic_data);
>>>>>>> FETCH_HEAD
	ic_version = atoi(&ic_data[1]);
	image_version = atoi(&image_data[1]);

	if (ic_data[0] == 'S') {
		if (image_data[0] == 'S') {
			if (ic_version < image_version) {
				return 1;
			} else {
				return 0;
			}
		} else {
			return 1;
		}
	} else {
		if (image_data[0] == 'S') {
			return 1;
		} else {
			if (ic_version < image_version) {
				return 1;
			} else {
<<<<<<< HEAD
				if (image_version == 10 && ic_version != 10)
					return 1;
				else
=======
#if 0
				if (image_version == 10 && ic_version != 10)
					return 1;
				else
#endif
>>>>>>> FETCH_HEAD
					return 0;
			}
		}
	}
}

static ssize_t ts_product_id(struct device *dev,  struct device_attribute *attr,  char *buf)
{
	int r;
	char product_id[10];
	uint8_t product_id_addr;

	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	r = snprintf(buf, PAGE_SIZE, "%d\n", touch_fw_version);
	product_id_addr = (i2c_smbus_read_byte_data(ts->client, SYNAPTICS_RMI_QUERY_BASE_REG)) + 11;
	i2c_smbus_read_i2c_block_data(ts->client, product_id_addr, sizeof(product_id), (u8 *)&product_id[0]);
	printk("Product ID : < %s >\n", product_id);

	return r;
}
/*static DEVICE_ATTR(fw, 0666, synaptics_ts_FW_show, NULL);*/


static unsigned long ExtractLongFromHeader(const unsigned char *SynaImage)  /* Endian agnostic*/
{
	return (unsigned long)SynaImage[0] +
		(unsigned long)SynaImage[1]*0x100 +
		(unsigned long)SynaImage[2]*0x10000 +
		(unsigned long)SynaImage[3]*0x1000000;
}

static void CalculateChecksum(uint16_t *data, uint16_t len, uint32_t *dataBlock)
{
	unsigned long temp = *data++;
	unsigned long sum1;
	unsigned long sum2;

	*dataBlock = 0xffffffff;

	sum1 = *dataBlock & 0xFFFF;
	sum2 = *dataBlock >> 16;

	while (len--) {
		sum1 += temp;
		sum2 += sum1;
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}

	*dataBlock = sum2 << 16 | sum1;
}

static void SpecialCopyEndianAgnostic(uint8_t *dest, uint16_t src)
{
	dest[0] = src%0x100;  /*Endian agnostic method*/
	dest[1] = src/0x100;
}

const unsigned char *SynaFirm;
uint8_t *binary;

static int check_ts_firmware(int type)
{
	int fd;
	int nRead;
	mm_segment_t old_fs = 0;
	struct stat fw_bin_stat;
	int bin_length;

	switch (type) {
	case dl_internal:
		SynaFirm = SynaFirmware;
		break;
	case dl_external:
		SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO,
			"[TOUCH][t-reflash] firmware is external file\n");
		old_fs = get_fs();
		set_fs(get_ds());

		fd = sys_open("/data/SYN_FIRMWARE.bin", O_RDONLY, 0);
		if (fd < 0) {
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] read data fail\n");
			goto read_fail;
		}

		nRead = sys_newstat("/data/SYN_FIRMWARE.bin", (struct stat *)&fw_bin_stat);
		if (nRead < 0) {
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] new stat fail\n");
			goto fw_mem_alloc_fail;
		}

		bin_length = fw_bin_stat.st_size;
		SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO,
			"[TOUCH] length ==> %d\n", bin_length);

		binary = kzalloc(sizeof(char) * (bin_length + 1), GFP_KERNEL);
		if (binary == NULL) {
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] binary is NULL\n");
			goto fw_mem_alloc_fail;
		}

		nRead = sys_read(fd, (char __user *)binary, bin_length);
		if (nRead != bin_length) {
			sys_close(fd);
			if (binary != NULL)
				kfree(binary);
			goto fw_mem_alloc_fail;
		}
		SynaFirm = binary;
		sys_close(fd);
		set_fs(old_fs);
		break;
	default:
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH][T-Reflash] Type is not available!!\n");
		return -1;
	}
	return 0;

fw_mem_alloc_fail:
	sys_close(fd);
read_fail:
	set_fs(old_fs);

	return -1;
}

int synaptics_ts_fw_upgrade_work(struct synaptics_ts_data *ts)
{
	int i;
	int ret;

	uint8_t FlashQueryBaseAddr, FlashDataBaseAddr;
	uint8_t RMICommandBaseAddr;

	uint8_t BootloaderIDAddr;
	uint8_t BlockSizeAddr;
	uint8_t FirmwareBlockCountAddr;
	uint8_t ConfigBlockCountAddr;

	uint8_t BlockNumAddr;
	uint8_t BlockDataStartAddr;

	/*uint8_t current_fw_ver;*/

	uint8_t bootloader_id[2];

	uint8_t temp_array[2], temp_data, flashValue, m_firmwareImgVersion;
	uint8_t checkSumCode;

	uint16_t ts_block_size, ts_config_block_count, ts_fw_block_count;
	uint16_t m_bootloadImgID;

	uint32_t ts_config_img_size;
	uint32_t ts_fw_img_size;
	uint32_t pinValue, m_fileSize, m_firmwareImgSize, m_configImgSize, m_FirmwareImgFile_checkSum;

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] synaptics_upgrade firmware [START]\n");
	fw_upgrade_status = 1;

	/*current_fw_ver = synaptics_ts_check_fwver(ts->client);*/

	if (check_ts_firmware(dl_mode) < 0) {
		return 0;
	}
	/* Address Configuration */
	FlashQueryBaseAddr = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_QUERY_BASE_REG);
	if (FlashQueryBaseAddr < 0) {
		goto err_out_retry;
	}

	BootloaderIDAddr = FlashQueryBaseAddr;
	BlockSizeAddr = FlashQueryBaseAddr + 3;
	FirmwareBlockCountAddr = FlashQueryBaseAddr + 5;
	ConfigBlockCountAddr = FlashQueryBaseAddr + 7;


	FlashDataBaseAddr = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_DATA_BASE_REG);
	if (FlashDataBaseAddr < 0) {
		goto err_out_retry;
	}

	BlockNumAddr = FlashDataBaseAddr;
	BlockDataStartAddr = FlashDataBaseAddr + 2;

	/* Get New Firmware Information from Header*/

	m_fileSize = sizeof(SynaFirm) - 1;

	checkSumCode         = ExtractLongFromHeader(&(SynaFirm[0]));
	m_bootloadImgID      = (unsigned int)SynaFirm[4] + (unsigned int)SynaFirm[5]*0x100;
	m_firmwareImgVersion = SynaFirm[7];
	m_firmwareImgSize    = ExtractLongFromHeader(&(SynaFirm[8]));
	m_configImgSize      = ExtractLongFromHeader(&(SynaFirm[12]));

	CalculateChecksum((uint16_t *)&(SynaFirm[4]), (uint16_t)(m_fileSize-4)>>1, &m_FirmwareImgFile_checkSum);

	/* Get Current Firmware Information*/
	ret = i2c_smbus_read_i2c_block_data(ts->client, BlockSizeAddr, sizeof(temp_array), (u8 *)&temp_array[0]);
	if (ret < 0) {
		goto err_out_retry;
	}
	ts_block_size = temp_array[0] + (temp_array[1] << 8);

	ret = i2c_smbus_read_i2c_block_data(ts->client, FirmwareBlockCountAddr, sizeof(temp_array), (u8 *)&temp_array[0]);
	if (ret < 0) {
		goto err_out_retry;
	}
	ts_fw_block_count = temp_array[0] + (temp_array[1] << 8);
	ts_fw_img_size = ts_block_size * ts_fw_block_count;

	ret = i2c_smbus_read_i2c_block_data(ts->client, ConfigBlockCountAddr, sizeof(temp_array), (u8 *)&temp_array[0]);
	if (ret < 0) {
		goto err_out_retry;
	}
	ts_config_block_count = temp_array[0] + (temp_array[1] << 8);
	ts_config_img_size = ts_block_size * ts_config_block_count;

	ret = i2c_smbus_read_i2c_block_data(ts->client, BootloaderIDAddr, sizeof(bootloader_id), (u8 *)&bootloader_id[0]);
	if (ret < 0) {
		goto err_out_retry;
	}

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:BootloaderID %02x %02x\n", bootloader_id[0], bootloader_id[1]);

	/* Flash Write Ready - Flash Command Enable & Erase */
	ret = synaptics_ts_write(ts->client, BlockDataStartAddr, &bootloader_id[0], sizeof(bootloader_id));
	if (ret < 0) {
		goto err_out_retry;
	}
	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:Address %02x%02x, Value %02x%02x\n", BlockDataStartAddr, BlockDataStartAddr, bootloader_id[0], bootloader_id[1]);

	do {
		flashValue = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_INT_STATUS_REG);
		if (unlikely(temp_data < 0)) {
			goto err_out_retry;
		}
	} while ((flashValue & 0x0f) != 0x00);

	ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_ENABLE);
	if (ret < 0) {
		goto err_out_retry;
	}

	do {
		pinValue = gpio_get_value(SYNAPTICS_T1320_TS_I2C_INT_GPIO); /*TOUCH_INT_N_GPIO*/
		udelay(1);
	} while (pinValue);
	do {
		flashValue = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_INT_STATUS_REG);
		if (unlikely(temp_data < 0)) {
			goto err_out_retry;
		}
	} while (flashValue != 0x80);
	flashValue = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG);
	if (flashValue < 0) {
		goto err_out_retry;
	}

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:Flash Program Enable Setup Complete\n");

	ret = synaptics_ts_write(ts->client, BlockDataStartAddr, &bootloader_id[0], sizeof(bootloader_id));
	if (ret < 0) {
		goto err_out_retry;
	}
	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:Address %02x%02x, Value %02x%02x\n", BlockDataStartAddr, BlockDataStartAddr, bootloader_id[0], bootloader_id[1]);

	if (m_firmwareImgVersion == 0 && ((unsigned int)bootloader_id[0] + (unsigned int)bootloader_id[1]*0x100) != m_bootloadImgID) {
		SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_ERR, "[TOUCH] Synaptics_UpgradeFirmware:Invalid Bootload Image\n");
	}

	ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_ERASEALL);
	if (ret < 0) {
		goto err_out_retry;
	}

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:SYNAPTICS_FLASH_CMD_ERASEALL\n");

	do {
		pinValue = gpio_get_value(SYNAPTICS_T1320_TS_I2C_INT_GPIO);
		udelay(1);
	} while (pinValue);
	do {
		flashValue = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_INT_STATUS_REG);
		if (unlikely(temp_data < 0)) {
			goto err_out_retry;
		}
	} while (flashValue != 0x80);


	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:Flash Erase Complete\n");

	/* Flash Firmware Data Write*/
	for (i = 0; i < ts_fw_block_count; ++i) {
		temp_array[0] = i & 0xff;
		temp_array[1] = (i & 0xff00) >> 8;

		/* Write Block Number */
		ret = synaptics_ts_write(ts->client, BlockNumAddr, &temp_array[0], sizeof(temp_array));
		if (ret < 0) {
			goto err_out_retry;
		}

		/* Write Data Block&SynapticsFirmware[0] */
		ret = synaptics_ts_write(ts->client, BlockDataStartAddr, (u8 *)&SynaFirm[0x100 + i*ts_block_size], ts_block_size);
		if (ret < 0) {
			goto err_out_retry;
		}

		/* Issue Write Firmware Block command*/
		ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_FW_WRITE);
		if (ret < 0) {
			goto err_out_retry;
		}
		do {
			pinValue = gpio_get_value(SYNAPTICS_T1320_TS_I2C_INT_GPIO);
			udelay(1);
		} while (pinValue);
		do {
			flashValue = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG);
			temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_INT_STATUS_REG);
			if (unlikely(temp_data < 0)) {
				goto err_out_retry;
			}
		} while (flashValue != 0x80);
	}

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:Flash Firmware Write Complete\n");

	/* Flash Firmware Config Write*/
	for (i = 0; i < ts_config_block_count; i++) {
		SpecialCopyEndianAgnostic(&temp_array[0], i);

		/* Write Configuration Block Number*/
		ret = synaptics_ts_write(ts->client, BlockNumAddr, &temp_array[0], sizeof(temp_array));
		if (ret < 0) {
			goto err_out_retry;
		}

		/* Write Data Block */
		ret = synaptics_ts_write(ts->client, BlockDataStartAddr, (u8 *)&SynaFirm[0x100 + m_firmwareImgSize + i*ts_block_size], ts_block_size);
		if (ret < 0) {
			goto err_out_retry;
		}

		/* Issue Write Configuration Block command to flash command register */
		ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_CONFIG_WRITE);
		if (ret < 0) {
			goto err_out_retry;
		}
		do {
			pinValue = gpio_get_value(SYNAPTICS_T1320_TS_I2C_INT_GPIO);   /*TOUCH_INT_N_GPIO*/
			udelay(1);
		} while (pinValue);
		do {
			flashValue = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG);
			temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_INT_STATUS_REG);
			if (unlikely(temp_data < 0)) {
				goto err_out_retry;
			}
		} while (flashValue != 0x80);
	}

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics_UpgradeFirmware:Flash Config write complete\n");

	RMICommandBaseAddr = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_RMI_CMD_BASE_REG);
	ret = i2c_smbus_write_byte_data(ts->client, RMICommandBaseAddr, 0x01);
	if (ret < 0) {
		goto err_out_retry;
	}
	mdelay(100);

	do {
		pinValue = gpio_get_value(SYNAPTICS_T1320_TS_I2C_INT_GPIO);  /*TOUCH_INT_N_GPIO*/
		udelay(1);
	} while (pinValue);
	do {
		flashValue = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_INT_STATUS_REG);
		if (unlikely(temp_data < 0)) {
			goto err_out_retry;
		}
	} while ((flashValue & 0x0f) != 0x00);

	/* Clear the attention assertion by reading the interrupt status register*/
	temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_INT_STATUS_REG);
	if (temp_data < 0) {
		goto err_out_retry;
	}

	/* Read F01 Status flash prog, ensure the 6th bit is '0'*/
	do {
		temp_data = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_DATA_BASE_REG);
		if (unlikely(temp_data < 0)) {
			goto err_out_retry;
		}
	} while ((temp_data & 0x40) != 0);

	fw_upgrade_status = 0;
	is_fw_upgrade = 1;

	SYNAPTICS_DEBUG_MSG(FW_UPGRADE, KERN_INFO, "[TOUCH] Synaptics Fw Upgrade Completed");

	return 0;

err_out_retry:
	return -1;
}

static ssize_t syn_fw_upgrade_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	int cmd;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;
	switch (cmd) {
	case dl_internal:
		dl_mode = dl_internal;
		break;
	case dl_external:
		dl_mode = dl_external;
		break;
	default:
		return -EINVAL;
	}

	ts->pdata->ts_power(1, true);
	if (atomic_read(&ts_irq_flag) == 1) {
		atomic_set(&ts_irq_flag, 0);
		disable_irq_nosync(ts->client->irq);
	}
	cancel_work_sync(&ts->t_work);
	if (atomic_read(&ts_irq_flag) == 1) {
		atomic_set(&ts_irq_flag, 0);
		disable_irq_nosync(ts->client->irq);
	}
	release_all_fingers(ts);

	if (atomic_read(&tk_irq_flag) == 1) {
		atomic_set(&tk_irq_flag, 0);
		disable_irq_nosync(so340010_pdev->client->irq);
	}
	cancel_work_sync(&so340010_pdev->key_work);
	hrtimer_cancel(&so340010_pdev->timer);

	synaptics_ts_fw_upgrade_work(ts);
	ts->pdata->ts_power(0, true);
	queue_delayed_work(touch_dev_reset_wq, &touch_reset_work, msecs_to_jiffies(100));

	return count;
}

static ssize_t jitter_solution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	int ret = 0;

	memset(&ts->jitter_filter, 0, sizeof(struct jitter_filter_info));

	ret = sscanf(buf, "%d %d",
			&ts->pdata->jitter_filter_enable,
			&ts->jitter_filter.adjust_margin);

	return count;
}

static ssize_t accuracy_solution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	int ret = 0;

	memset(&ts->jitter_filter, 0, sizeof(struct jitter_filter_info));

	ret = sscanf(buf, "%d %d %d %d %d %d %d",
			&ts->pdata->accuracy_filter_enable,
			&ts->accuracy_filter.ignore_pressure_gap,
			&ts->accuracy_filter.delta_max,
			&ts->accuracy_filter.touch_max_count,
			&ts->accuracy_filter.max_pressure,
			&ts->accuracy_filter.direction_count,
			&ts->accuracy_filter.time_to_max_pressure);

	return count;
}

static struct device_attribute dev_attr_gripsuppression[] = {
	__ATTR(ts_desc,  S_IRUGO | S_IWUSR, ts_desc_show, NULL),
	__ATTR(status,  S_IRUGO | S_IWUSR, ts_status_show, NULL),
	__ATTR(fw_version, S_IRUGO | S_IWUSR, ts_version_show, NULL),
	__ATTR(power_control, S_IRUGO | S_IWUSR, NULL, ts_power_control_store),
	__ATTR(irq_control, S_IRUGO | S_IWUSR, NULL, ts_irq_control_store),
	__ATTR(reg_control, S_IRUGO | S_IWUSR, NULL, ts_reg_control_store),
	__ATTR(product_id, S_IRUGO | S_IWUSR, ts_product_id, NULL),
	__ATTR(fw_upgrade, S_IRUGO | S_IWUSR, NULL, syn_fw_upgrade_store),
	__ATTR(irq_status, S_IRUGO | S_IWUSR, ts_irq_flag_show, NULL),
	__ATTR(ESD_Detection, S_IRUGO | S_IWUSR, ts_ESD_Detection, NULL),
#ifdef GHOST_FINGER_SOLUTION
	__ATTR(keyguard, S_IRUGO | S_IWUSR, NULL, store_keyguard_info),
#endif
#ifdef SYNAPTICS_F54_TEST
	__ATTR(f54, S_IRUGO | S_IWUSR, show_F54, NULL),
#endif
	__ATTR(jitter, S_IRUGO | S_IWUSR, NULL, jitter_solution_store),
	__ATTR(accuracy, S_IRUGO | S_IWUSR, NULL, accuracy_solution_store),
};


int synaptics_ts_check_product_id(void)
{
	char product_id[10];
	uint8_t product_id_addr;

	int ret = 0;

	/* device check*/
	product_id_addr = (i2c_smbus_read_byte_data(ts_data_reset->client, SYNAPTICS_RMI_QUERY_BASE_REG)) + 11;
	ret = i2c_smbus_read_i2c_block_data(ts_data_reset->client, product_id_addr, sizeof(product_id), (u8 *)&product_id[0]);

	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,
				"[TOUCH] Product ID - Check Fail\n");
		return -1;
	}
	SYNAPTICS_DEBUG_MSG(PROBE_INFO, KERN_INFO,
			"[TOUCH] product_id_addr: product ID : [%x]\n", product_id_addr);
	SYNAPTICS_DEBUG_MSG(PROBE_INFO, KERN_INFO,
			"[TOUCH] product_id_value: product ID : [%s]\n", product_id);

	return 0;
}

void synaptics_ts_classdev_register(void)
{
#ifdef CONFIG_TS_INFO_CLASS
	cdev.name = "touchscreen";
	/*cdev.version = ts->fw_ver;*/
<<<<<<< HEAD
	synaptics_ts_read(ts_data_reset->client, 0x0058, 4, (u8 *)classdev_version);
=======
	synaptics_ts_read(ts_data_reset->client, SYNAPTICS_F34_FLASH_CTRL0001, 4, (u8 *)classdev_version);
>>>>>>> FETCH_HEAD
	cdev.version = classdev_version;
	cdev.flags = ts_data_reset->flags;
	ts_info_classdev_register(&ts_data_reset->client->dev, &cdev);
#endif
}

/*************************************************************************************************
 * 1. Set interrupt configuration
 * 2. Disable interrupt
 * 3. Power up
 * 4. Read RMI Version
 * 5. Read Firmware version & Upgrade firmware automatically
 * 6. Read Data To Initialization Touch IC
 * 7. Set some register
 * 8. Enable interrupt
*************************************************************************************************/
static int synaptics_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct synaptics_ts_platform_data *pdata;
	struct synaptics_ts_data *ts;

	int ret = 0;
	int i;
	int one_sec = 0;
	uint16_t max_x;
	uint16_t max_y;
	uint8_t max_pressure;
	uint8_t max_width;

	fw_upgrade_status = 0;
	checked_fw_upgrade = 0;
	SynaFirm = NULL;
	binary = NULL;
	dl_mode = dl_internal;
	first_event = 1;

	suspend_flag = 0;

	atomic_set(&ts_irq_flag, 1);

	/*synaptics_fw_upgrade_wq = create_singlethread_workqueue("synaptics_fw_upgrade_wq");*/

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "synaptics_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "Can not read platform data\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	/* Device data setting */
	ts->pdata = pdata;
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts_data_reset = ts; /*for key reset*/

	one_sec = 1000000 / (ts->pdata->report_period/1000);
#ifdef GHOST_FINGER_SOLUTION
	ts->gf_ctrl.min_count = 75000 / (ts->pdata->report_period/1000);
	ts->gf_ctrl.max_count = one_sec; /*frame per second*/
<<<<<<< HEAD
=======
	ts->gf_ctrl.saved_x = -1;
	ts->gf_ctrl.saved_y = -1;
	ts->gf_ctrl.max_moved = SYNAPTICS_S3200_RESOLUTION_X / 3;

>>>>>>> FETCH_HEAD
#endif
#ifdef JITTER_FILTER_SOLUTION
	if (ts->pdata->jitter_filter_enable) {
		ts->jitter_filter.adjust_margin = 100;
	}

	if (ts->pdata->accuracy_filter_enable) {
		ts->accuracy_filter.ignore_pressure_gap = 5;
		ts->accuracy_filter.delta_max = 150;
<<<<<<< HEAD
		ts->accuracy_filter.max_pressure = 150;
=======
		ts->accuracy_filter.max_pressure = 250;
>>>>>>> FETCH_HEAD
		ts->accuracy_filter.time_to_max_pressure = one_sec / 20;
		ts->accuracy_filter.direction_count = one_sec / 4;
		ts->accuracy_filter.touch_max_count = one_sec / 2;
	}
#endif
	SYNAPTICS_DEBUG_MSG(PROBE_INFO, KERN_INFO, "     ================================================\n");
	SYNAPTICS_DEBUG_MSG(PROBE_INFO, KERN_INFO, "     =                 [Touch_Info]                 =\n");
	SYNAPTICS_DEBUG_MSG(PROBE_INFO, KERN_INFO, "     ================================================\n");

	INIT_WORK(&ts->t_work, synaptics_ts_work_func);

	for (i = 0; i < ARRAY_SIZE(dev_attr_gripsuppression); i++) {
		ret = device_create_file(&client->dev, &dev_attr_gripsuppression[i]);
		if (ret) {
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,
				"[TOUCH] Probe::grip suppression device_create_file failed\n");
			goto err_check_functionality_failed;
		}
	}

	SYNAPTICS_PANEL_MAX_X = SYNAPTICS_S3200_RESOLUTION_X;
	SYNAPTICS_PANEL_MAX_Y = SYNAPTICS_S3200_RESOLUTION_Y;

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,
			"[TOUCH] Probe::Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "synaptics_ts";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);

	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	max_x = SYNAPTICS_PANEL_MAX_X;
	max_y = SYNAPTICS_PANEL_MAX_Y;
	max_pressure = 0xff;/*0x7F;*/
	max_width = 0x0F;

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, max_width, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, max_pressure, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, DCM_MAX_NUM_OF_FINGER - 1, 0, 0);

	ts->input_dev->name = client->name;
	ts->input_dev->id.bustype = BUS_HOST;
	ts->input_dev->dev.parent = &client->dev;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;

	input_set_drvdata(ts->input_dev, ts);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,
				"[TOUCH] Probe::Unable to register %s input device\n",
				ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	SYNAPTICS_DEBUG_MSG(PROBE_INFO, KERN_INFO,
			"[TOUCH] ## irq [%d], irqflags[0x%x]\n",
			client->irq, (unsigned int)pdata->irqflags);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	ts->early_suspend.suspend = synaptics_ts_early_suspend;
	ts->early_suspend.resume = synaptics_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	SYNAPTICS_DEBUG_MSG(PROBE_INFO, KERN_INFO,
			"[TOUCH] Probe::Start touchscreen %s in %s mode\n",
			ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");
	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] Probe::err_input_dev_alloc failed\n");
err_alloc_data_failed:
	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] Probe::err_alloc_data failed_\n");
err_check_functionality_failed:
	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] Probe::err_check_functionality failed_\n");
	return ret;
}

static int synaptics_ts_remove(struct i2c_client *client)
{
	int i;
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	if (ts->use_irq)
		free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
#ifdef CONFIG_TS_INFO_CLASS
	ts_info_classdev_unregister(&cdev);
#endif
	for (i = 0; i < ARRAY_SIZE(dev_attr_gripsuppression); i++) {
		device_remove_file(&client->dev, &dev_attr_gripsuppression[i]);
	}
	kfree(ts);
	return 0;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_ts_early_suspend(struct early_suspend *h)
{
	struct synaptics_ts_data *ts;

	ts = container_of(h, struct synaptics_ts_data, early_suspend);

	if (atomic_read(&ts_irq_flag) == 1) {
		atomic_set(&ts_irq_flag, 0);
		disable_irq_nosync(ts->client->irq);
	}
	cancel_work_sync(&ts->t_work);

	release_all_fingers(ts);

	if (fw_upgrade_status == 1) {
		checked_fw_upgrade = 1;
		return;
	}

	suspend_flag = 1;

	return;
}

static void synaptics_ts_late_resume(struct early_suspend *h)
{
	if (fw_upgrade_status == 1 || checked_fw_upgrade == 1) {
		checked_fw_upgrade = 0;
		return;
	}

	suspend_flag = 0;
	touch_dev_resume();
}
#endif

static const struct i2c_device_id synaptics_ts_id[] = {
	{ SYNAPTICS_TS_NAME, 0 },
	{ },
};

static struct i2c_driver synaptics_ts_driver = {
	.probe		= synaptics_ts_probe,
	.remove		= synaptics_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= synaptics_ts_suspend,
	.resume		= synaptics_ts_resume,
#endif
	.id_table	= synaptics_ts_id,
	.driver = {
		.name	= SYNAPTICS_TS_NAME,
		.owner = THIS_MODULE,
	},
};


static int __devinit synaptics_ts_init(void)
{
	synaptics_wq = create_singlethread_workqueue("synaptics_wq");
	if (!synaptics_wq) {
		pr_err("failed to create singlethread workqueue\n");
		return -ENOMEM;
	}
	return i2c_add_driver(&synaptics_ts_driver);
}

static void __exit synaptics_ts_exit(void)
{
	i2c_del_driver(&synaptics_ts_driver);

	if (synaptics_wq)
		destroy_workqueue(synaptics_wq);
}

module_init(synaptics_ts_init);
module_exit(synaptics_ts_exit);

MODULE_DESCRIPTION("Synaptics Touchscreen Driver");
MODULE_AUTHOR("Junhee Lee <junhee7.lee@lge.com>");
MODULE_LICENSE("GPL");
