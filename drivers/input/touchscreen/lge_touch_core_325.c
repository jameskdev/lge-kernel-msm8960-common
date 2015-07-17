/* Lge_touch_core.c
 *
 * Copyright (C) 2011 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
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


#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/jiffies.h>
#include <linux/sysdev.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/kernel.h>

#include <asm/atomic.h>
#include <mach/gpio.h>

#include <linux/input/lge_touch_core_325.h>
#include <mach/board_lge.h>
#if defined(CONFIG_MACH_MSM8960_FX1)
#define SYNAPTICS_TOUCH_F54

#ifdef SYNAPTICS_TOUCH_F54
#include "touch_synaptics_f54.h"
#endif
#endif

#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1m)
#include "ts_class.h"
#endif

struct lge_touch_data
{
	void*			h_touch;
	atomic_t		next_work;
	atomic_t		device_init;
	u8				work_sync_err_cnt;
	u8				ic_init_err_cnt;
	volatile int	curr_pwr_state;
	struct i2c_client 			*client;
	struct input_dev 			*input_dev;
	struct hrtimer 				timer;
	struct work_struct  		work;
	struct delayed_work			work_init;
	struct delayed_work			work_touch_lock;
	struct work_struct  		work_fw_upgrade;
	struct early_suspend		early_suspend;
	struct touch_platform_data 	*pdata;
	struct touch_data			ts_data;
	struct touch_fw_info		fw_info;
	struct fw_upgrade_info		fw_upgrade;
	struct section_info			st_info;
	struct kobject 				lge_touch_kobj;
	struct ghost_finger_ctrl	gf_ctrl;
	struct jitter_filter_info	jitter_filter;
	struct accuracy_filter_info	accuracy_filter;
};

struct touch_device_driver*	touch_device_func;
struct workqueue_struct*	touch_wq;

struct lge_touch_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct lge_touch_data *ts, char *buf);
	ssize_t (*store)(struct lge_touch_data *ts, const char *buf, size_t count);
};

#define LGE_TOUCH_ATTR(_name, _mode, _show, _store)	\
struct lge_touch_attribute lge_touch_attr_##_name = __ATTR(_name, _mode, _show, _store)

/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/lge_touch_core/parameters/debug_mask
 */
u32 touch_debug_mask = DEBUG_BASE_INFO;
module_param_named(debug_mask, touch_debug_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

extern int lge_bd_rev;

#ifdef LGE_TOUCH_TIME_DEBUG
/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/lge_touch_core/parameters/time_debug_mask
 */
u32 touch_time_debug_mask = DEBUG_TIME_PROFILE_NONE;
module_param_named(time_debug_mask, touch_time_debug_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

#define get_time_interval(a,b) a>=b ? a-b : 1000000+a-b
struct timeval t_debug[TIME_PROFILE_MAX];
#endif

#define get_time_interval(a,b) a>=b ? a-b : 1000000+a-b

static u8 probe_flag = 0;
static u8 resume_flag = 0;
static unsigned int need_to_rebase = 0;
static unsigned int ta_debouncing_count = 0;
static unsigned int button_press_count =0;
static unsigned int ts_rebase_count =0;
struct timeval t_ex_debug[TIME_EX_PROFILE_MAX];
struct t_data	 ts_prev_finger_press_data;

int trigger_baseline = 0;
int ts_pen_ctrl = 0;
int ts_charger_plug = 0;
static struct hrtimer hr_touch_trigger_timer;
#define MS_TO_NS(x)	(x * 1E6L)

#define MAX_RETRY_COUNT			3
#define MAX_GHOST_CHECK_COUNT	3

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void touch_early_suspend(struct early_suspend *h);
static void touch_late_resume(struct early_suspend *h);
#endif

#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1m)
char classdev_version[5];
struct ts_info_classdev cdev;
extern int ts_info_classdev_register(struct device *parent, struct ts_info_classdev *ts_info_cdev);
extern void ts_info_classdev_unregister(struct ts_info_classdev *ts_info_cdev);
#endif
void synaptics_ts_classdev_register(void);

/* Auto Test interface for some model */
static struct lge_touch_data *touch_test_dev = NULL;

/* disable interrupts to stabilize */
#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1)
bool block_interrupt;
#elif defined(CONFIG_TS_INFO_CLASS) 
	#if defined(CONFIG_MACH_MSM8960_FX1S) || defined(CONFIG_MACH_MSM8960_FX1SU) || defined(CONFIG_MACH_MSM8960_FX1SK) 
		bool block_interrupt;
	#endif
#endif

void Send_Touch( unsigned int x, unsigned int y)
{
	if (touch_test_dev)
	{
		/* press */
		input_report_abs(touch_test_dev->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_PRESSURE, 1);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_WIDTH_MINOR, 1);
		input_mt_sync(touch_test_dev->input_dev);
		input_sync(touch_test_dev->input_dev);

		/* release */
		input_report_abs(touch_test_dev->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_PRESSURE, 0);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_WIDTH_MINOR, 0);
		input_mt_sync(touch_test_dev->input_dev);
		input_sync(touch_test_dev->input_dev);
	}
	else
	{
		TOUCH_ERR_MSG("Touch device not found\n");
	}
}
EXPORT_SYMBOL(Send_Touch);

#if defined(CONFIG_MACH_MSM8960_L1)
static ssize_t store_send_touch_down(struct lge_touch_data *ts, const char *buf, size_t count)
{
       unsigned int x;
       unsigned int y;

       sscanf(buf, "%d %d", &x, &y);

       if (ts){
              input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
              input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
              input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 1);
              input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
              input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR, 1);
              input_mt_sync(ts->input_dev);
              input_sync(ts->input_dev);
       }
       else{
              TOUCH_ERR_MSG("Touch device not found\n");

       }

       return count;
}

static ssize_t store_send_touch_up(struct lge_touch_data *ts, const char *buf, size_t count)
{
       unsigned int x;
       unsigned int y;

       sscanf(buf, "%d %d", &x, &y);

       if (ts){
              input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
              input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
              input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
              input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
              input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR, 0);
              input_mt_sync(ts->input_dev);
              input_sync(ts->input_dev);
       }
       else{
              TOUCH_ERR_MSG("Touch device not found\n");
       }

       return count;
}
#endif


int get_touch_ts_fw_version(char *fw_ver)
{
	if (touch_test_dev)
	{
		sprintf(fw_ver, "%d.0%d", touch_test_dev->fw_info.manufacturer_id,
				touch_test_dev->fw_info.fw_rev);
		return 1;
	}
	else
	{
		return 0;
	}
}
EXPORT_SYMBOL(get_touch_ts_fw_version);


/* set_touch_handle / get_touch_handle
 *
 * Developer can save their object using 'set_touch_handle'.
 * Also, they can restore that using 'get_touch_handle'.
 */
void set_touch_handle(struct i2c_client *client, void* h_touch)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	ts->h_touch = h_touch;
}

void* get_touch_handle(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	return ts->h_touch;
}

/* touch_i2c_read / touch_i2c_write
 *
 * Developer can use these fuctions to communicate with touch_device through I2C.
 */
int touch_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf)
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
			.len = len,
			.buf = buf,
		},
	};

	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}

int touch_i2c_write(struct i2c_client *client, u8 reg, int len, u8 * buf)
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
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}

int touch_i2c_write_byte(struct i2c_client *client, u8 reg, u8 data)
{
	unsigned char send_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 2,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	send_buf[1] = (unsigned char)data;

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}

#ifdef LGE_TOUCH_TIME_DEBUG
static void time_profile_result(struct lge_touch_data *ts)
{
	if (touch_time_debug_mask & DEBUG_TIME_PROFILE_ALL) {
		if (t_debug[TIME_INT_INTERVAL].tv_sec == 0
				&& t_debug[TIME_INT_INTERVAL].tv_usec == 0) {
			t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
			t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
		} else {
			TOUCH_INFO_MSG("Interval [%6luus], Total [%6luus], IRQ -> Thread IRQ [%6luus] -> work [%6luus] -> report [%6luus]\n",
				get_time_interval(t_debug[TIME_ISR_START].tv_usec, t_debug[TIME_INT_INTERVAL].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_THREAD_ISR_START].tv_usec, t_debug[TIME_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_START].tv_usec, t_debug[TIME_THREAD_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_WORKQUEUE_START].tv_usec));

			t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
			t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
		}
	} else {
		if (touch_time_debug_mask & DEBUG_TIME_INT_INTERVAL) {
			if (t_debug[TIME_INT_INTERVAL].tv_sec == 0
					&& t_debug[TIME_INT_INTERVAL].tv_usec == 0) {
				t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
				t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
			} else {
				TOUCH_INFO_MSG("Interrupt interval: %6luus\n",
					get_time_interval(t_debug[TIME_ISR_START].tv_usec, t_debug[TIME_INT_INTERVAL].tv_usec));

				t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
				t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
			}
		}

		if (touch_time_debug_mask & DEBUG_TIME_INT_IRQ_DELAY) {
			TOUCH_INFO_MSG("IRQ -> Thread IRQ : %6luus\n",
				get_time_interval(t_debug[TIME_THREAD_ISR_START].tv_usec, t_debug[TIME_ISR_START].tv_usec));
		}

		if (touch_time_debug_mask & DEBUG_TIME_INT_THREAD_IRQ_DELAY) {
			TOUCH_INFO_MSG("Thread IRQ -> work: %6luus\n",
				get_time_interval(t_debug[TIME_WORKQUEUE_START].tv_usec, t_debug[TIME_THREAD_ISR_START].tv_usec));
		}

		if (touch_time_debug_mask & DEBUG_TIME_DATA_HANDLE) {
			TOUCH_INFO_MSG("work -> report: %6luus\n",
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_WORKQUEUE_START].tv_usec));
		}
	}

	if (!ts->ts_data.total_num) {
		memset(t_debug, 0x0, sizeof(t_debug));
	}

}
#endif

/* release_all_ts_event
 *
 * When system enters suspend-state,
 * if user press touch-panel, release them automatically.
 */
static void release_all_ts_event(struct lge_touch_data *ts)
{
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY){
		if (ts->ts_data.prev_total_num) {
			input_mt_sync(ts->input_dev);

			if (likely(touch_debug_mask & (DEBUG_ABS | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("touch finger position released\n");
		}

		if(ts->ts_data.prev_button.state == BUTTON_PRESSED) {
			input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

			if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("Touch KEY[%d] is released\n", ts->ts_data.prev_button.key_code);
		}
	}
	else if (ts->pdata->role->key_type == VIRTUAL_KEY){
		if (ts->ts_data.prev_total_num) {
			input_mt_sync(ts->input_dev);

			if (likely(touch_debug_mask & (DEBUG_ABS | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("touch finger position released\n");
		}
	}
	else if (ts->pdata->role->key_type == TOUCH_SOFT_KEY){
		if (ts->ts_data.state == ABS_PRESS) {
			input_mt_sync(ts->input_dev);

			if (likely(touch_debug_mask & (DEBUG_ABS | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("touch finger position released\n");
		} else if (ts->ts_data.state == BUTTON_PRESS) {
			input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

			if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("Touch KEY[%d] is released\n", ts->ts_data.prev_button.key_code);
		}
	}

	ts->ts_data.prev_total_num = 0;

	input_sync(ts->input_dev);
}

/* touch_power_cntl
 *
 * 1. POWER_ON
 * 2. POWER_OFF
 * 3. POWER_SLEEP
 * 4. POWER_WAKE
 */
static int touch_power_cntl(struct lge_touch_data *ts, int onoff)
{
	int ret = 0;

	if (touch_device_func->power == NULL) {
		TOUCH_INFO_MSG("There is no specific power control function\n");
		return -1;
	}

	switch (onoff) {
	case POWER_ON:
		ret = touch_device_func->power(ts->client, POWER_ON);
		if (ret < 0) {
			TOUCH_ERR_MSG("power on failed\n");
		} else
			ts->curr_pwr_state = POWER_ON;

		break;
	case POWER_OFF:
		ret = touch_device_func->power(ts->client, POWER_OFF);
		if (ret < 0) {
			TOUCH_ERR_MSG("power off failed\n");
		} else
			ts->curr_pwr_state = POWER_OFF;

		msleep(ts->pdata->role->reset_delay);

		atomic_set(&ts->device_init, 0);
		break;
	case POWER_SLEEP:
		ret = touch_device_func->power(ts->client, POWER_SLEEP);
		if (ret < 0) {
			TOUCH_ERR_MSG("power sleep failed\n");
		} else
			ts->curr_pwr_state = POWER_SLEEP;

		break;
	case POWER_WAKE:
		ret = touch_device_func->power(ts->client, POWER_WAKE);
		if (ret < 0) {
			TOUCH_ERR_MSG("power wake failed\n");
		} else
			ts->curr_pwr_state = POWER_WAKE;

		break;
	default:
		break;
	}

	if (unlikely(touch_debug_mask & DEBUG_POWER))
		if (ret >= 0)
			TOUCH_INFO_MSG("%s: power_state[%d]\n", __FUNCTION__, ts->curr_pwr_state);

	return ret;
}

static enum hrtimer_restart touch_trigger_timer_handler(struct hrtimer *timer)
{
	if (touch_test_dev->pdata->role->ghost_detection_enable) {
		if(trigger_baseline==1)
		{
			trigger_baseline = 2;
			atomic_inc(&touch_test_dev->next_work);
			queue_work(touch_wq, &touch_test_dev->work);
		}
	}
	return HRTIMER_NORESTART;
}

void trigger_baseline_state_machine(int plug_in)
{
	if(touch_test_dev->gf_ctrl.probe != 1)
		return;

#if defined(CONFIG_MACH_MSM8960_FX1)
	if (touch_test_dev && touch_test_dev->pdata->role->ghost_detection_enable) {
#else
	if (touch_test_dev->pdata->role->ghost_detection_enable) {
#endif
		if(plug_in == 0 || plug_in == 1)    // TA/USB connected or disconnected
		{
			TOUCH_INFO_MSG(" trigger_baseline_state_machine = %d \n", plug_in);
			ts_charger_plug = plug_in;

			if(touch_test_dev && atomic_read(&touch_test_dev->device_init) == 1 && trigger_baseline==0 ){
				trigger_baseline = 1;

				//hrtimer_cancel(&hr_touch_trigger_timer);
				hrtimer_start(&hr_touch_trigger_timer, ktime_set(0, MS_TO_NS(1000)), HRTIMER_MODE_REL);
			}
		}
	}
#if 0
    else if (plug_in ==1)   // MHL connected
    {
        u8 buff=0;
        ts_pen_ctrl = 1;
        touch_i2c_read(touch_test_dev->client, 0x83, 1, &buff);
        buff = buff&0xFB;
        touch_i2c_write_byte(touch_test_dev->client, 0x83, buff);
        TOUCH_INFO_MSG(" - pen disabled because of MHL \n");
    }
    else if (plug_in ==2)  // MHL disconnected
    {
        u8 buff=0;
        touch_i2c_read(touch_test_dev->client, 0x83, 1, &buff);
        buff = buff|0x04;
        touch_i2c_write_byte(touch_test_dev->client, 0x83, buff);
        TOUCH_INFO_MSG(" - pen enabled because of MHL \n");
    }
#endif
}

/* safety_reset
 *
 * 1. disable irq/timer.
 * 2. turn off the power.
 * 3. turn on the power.
 * 4. sleep (booting_delay)ms, usually 400ms(synaptics).
 * 5. enable irq/timer.
 *
 * After 'safety_reset', we should call 'touch_init'.
 */
static void safety_reset(struct lge_touch_data *ts)
{
	if (ts->pdata->role->operation_mode)
		disable_irq(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);

	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}

	release_all_ts_event(ts);

	touch_power_cntl(ts, POWER_OFF);
	touch_power_cntl(ts, POWER_ON);
	msleep(ts->pdata->role->booting_delay);

	if (ts->pdata->role->operation_mode)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	return;
}

/* touch_ic_init
 *
 * initialize the device_IC and variables.
 */
static int touch_ic_init(struct lge_touch_data *ts)
{
	int int_pin = 0;
	int next_work = 0;
	int ret = 0;

	if(unlikely(ts->ic_init_err_cnt >= MAX_RETRY_COUNT)){
		TOUCH_ERR_MSG("Init Failed: Irq-pin has some unknown problems\n");
		goto err_out_critical;
	}

	atomic_set(&ts->next_work, 0);
	atomic_set(&ts->device_init, 1);

	if (touch_device_func->init == NULL) {
		TOUCH_INFO_MSG("There is no specific IC init function\n");
		goto err_out_critical;
	}

	if (touch_device_func->init(ts->client, &ts->fw_info) < 0) {
		TOUCH_ERR_MSG("specific device initialization fail\n");
		goto err_out_retry;
	}

	/* Interrupt pin check after IC init - avoid Touch lockup */
	if(ts->pdata->role->operation_mode == INTERRUPT_MODE){
		int_pin = gpio_get_value(ts->pdata->int_pin);
		next_work = atomic_read(&ts->next_work);

		if(unlikely(int_pin != 1 && next_work <= 0)){
			TOUCH_INFO_MSG("WARN: Interrupt pin is low - next_work: %d, try_count: %d]\n",
					next_work, ts->ic_init_err_cnt);
			goto err_out_retry;
		}
	}
#if !defined(CONFIG_MACH_MSM8960_L1v) && !defined(CONFIG_MACH_MSM8960_FX1)
	if(ts->gf_ctrl.stage & GHOST_STAGE_2){
		ts->gf_ctrl.stage = GHOST_STAGE_2 | GHOST_STAGE_3;
		if(touch_device_func->ic_ctrl){
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0){
				TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
				goto err_out_retry;
			}
		}
	}
	else {
		ts->gf_ctrl.stage = GHOST_STAGE_1;
		if(touch_device_func->ic_ctrl){
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0){
				TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
				goto err_out_retry;
			}
			ts->gf_ctrl.baseline_state = BASELINE_OPEN;
		}
	}
#endif
	if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_GHOST))){
		TOUCH_INFO_MSG("%s %s(%s): ID[%d], FW rev[%s], f/u[%d]\n",
				ts->pdata->maker, ts->fw_info.product_id,
				ts->pdata->role->operation_mode?"Interrupt mode":"Polling mode",
				ts->fw_info.manufacturer_id, ts->fw_info.fw_config_id, ts->fw_upgrade.fw_force_upgrade);
		TOUCH_INFO_MSG("irq_pin[%d] next_work[%d] ghost_stage[0x%x]\n",
				int_pin, next_work, ts->gf_ctrl.stage);
	}

#if !defined(CONFIG_MACH_MSM8960_L1v) && !defined(CONFIG_MACH_MSM8960_FX1)
	ts->gf_ctrl.probe = 0;
#endif
	ts->gf_ctrl.count = 0;
	ts->gf_ctrl.ghost_check_count = 0;
	memset(&ts->ts_data, 0, sizeof(ts->ts_data));
	memset(&ts->fw_upgrade, 0, sizeof(ts->fw_upgrade));
	ts->ic_init_err_cnt = 0;

	ts->jitter_filter.id_mask = 0;
	memset(ts->jitter_filter.his_data, 0, sizeof(ts->jitter_filter.his_data));
	memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));

	ts->accuracy_filter.finish_filter = 0;

	return 0;

err_out_retry:
	ts->ic_init_err_cnt++;
	safety_reset(ts);
	ret = touch_ic_init(ts);

	//Return value is defined as -2 to handle touch_ic_init fail case
	return ret;

err_out_critical:
	ts->ic_init_err_cnt = 0;

	return -1;
}

/* ghost_finger_solution
 *
 * GHOST_STAGE_1
 * - melt_mode.
 * - If user press and release their finger in 1 sec, STAGE_1 will be cleared. --> STAGE_2
 * - If there is no key-guard, ghost_finger_solution is finished.
 *
 * GHOST_STAGE_2
 * - no_melt_mode
 * - if user use multi-finger, stage will be changed to STAGE_1
 *   (We assume that ghost-finger occured)
 * - if key-guard is unlocked, STAGE_2 is cleared. --> STAGE_3
 *
 * GHOST_STAGE_3
 * - when user release their finger, device re-scan the baseline.
 * - Then, GHOST_STAGE3 is cleared and ghost_finger_solution is finished.
 */
int ghost_finger_solution(struct lge_touch_data *ts)
{
	static int single_touch_state = 0;
#if defined(CONFIG_MACH_MSM8960_L1v)
	static int multi_touch_state = 0;
#endif
	static int single_press_point_x = 0;
	static int single_press_point_y = 0;
	int single_drag_distance_x = 0;
	int single_drag_distance_y = 0;

	if(ts->gf_ctrl.stage & GHOST_STAGE_1){
		if(ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm_touch == 0){
#if defined(CONFIG_MACH_MSM8960_FX1)
			single_touch_state = 0;
			if(ts->gf_ctrl.count < ts->gf_ctrl.min_count || ts->gf_ctrl.count >= ts->gf_ctrl.max_count){
				if(ts->gf_ctrl.stage & GHOST_STAGE_2)
					ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
				else
					ts->gf_ctrl.ghost_check_count = 0;
			}
			else {
				ts->gf_ctrl.ghost_check_count++;
			}

			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_1: ghost_check_count+[0x%x]\n", ts->gf_ctrl.ghost_check_count);

			if(ts->gf_ctrl.ghost_check_count >= MAX_GHOST_CHECK_COUNT){
				ts->gf_ctrl.ghost_check_count = 0;
				if(touch_device_func->ic_ctrl){
					if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0)
						return -1;
				}
				ts->gf_ctrl.stage &= ~GHOST_STAGE_1;
				if (unlikely(touch_debug_mask & DEBUG_GHOST))
					TOUCH_INFO_MSG("ghost_stage_1: cleared[0x%x]\n", ts->gf_ctrl.stage);
				if(!ts->gf_ctrl.stage){
					if (unlikely(touch_debug_mask & DEBUG_GHOST
							|| touch_debug_mask & DEBUG_BASE_INFO))
						TOUCH_INFO_MSG("ghost_stage_finished. (NON-KEYGUARD)\n");
				}
			}
			ts->gf_ctrl.count = 0;
#else
			single_touch_state = 0;
			multi_touch_state = 0;
#endif
		}
		else if(ts->ts_data.total_num == 1 && ts->ts_data.curr_button.state == 0 && ts->ts_data.curr_data[0].id == 0
										&& ts->ts_data.palm_touch == 0){
#if defined(CONFIG_MACH_MSM8960_FX1)
			if(ts->gf_ctrl.stage & GHOST_STAGE_2 || ts->gf_ctrl.stage & GHOST_STAGE_3){
				ts->gf_ctrl.count++;

				if (unlikely(touch_debug_mask & DEBUG_GHOST))
					TOUCH_INFO_MSG("ghost_stage_1: int_count[%d/%d]\n", ts->gf_ctrl.count, ts->gf_ctrl.max_count);
			}
			else{
#endif
				if(single_touch_state == 1){
					single_drag_distance_x = ts->ts_data.curr_data[ts->ts_data.total_num-1].x_position - single_press_point_x;
					if(single_drag_distance_x < 0)
						single_drag_distance_x = -(single_drag_distance_x);

					single_drag_distance_y = ts->ts_data.curr_data[ts->ts_data.total_num-1].y_position - single_press_point_y;
					if(single_drag_distance_y < 0)
						single_drag_distance_y = -(single_drag_distance_y);

					if((single_drag_distance_x + single_drag_distance_y) > 450){
#if defined(CONFIG_MACH_MSM8960_FX1)
						if(touch_device_func->ic_ctrl){
							if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0)
								return -1;
						}
#endif
						single_touch_state = -1;
						ts->gf_ctrl.stage &= ~GHOST_STAGE_1;
						if (unlikely(touch_debug_mask & DEBUG_GHOST))
							TOUCH_INFO_MSG("ghost_stage_1: cleared[0x%x]\n", ts->gf_ctrl.stage);
						if(!ts->gf_ctrl.stage){
							if (unlikely(touch_debug_mask & DEBUG_GHOST
									|| touch_debug_mask & DEBUG_BASE_INFO))
								TOUCH_INFO_MSG("ghost_stage_finished. (NON-KEYGUARD)\n");
						}
					}
				}
				else if(single_touch_state == 0){
#if defined(CONFIG_MACH_MSM8960_FX1)
					if(ts->gf_ctrl.baseline_state == BASELINE_OPEN){
						if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0)
							return -1;
					}
#endif
					ts->gf_ctrl.baseline_state = BASELINE_FIX;
					single_touch_state = 1;
					single_press_point_x = ts->ts_data.curr_data[ts->ts_data.total_num-1].x_position;
					single_press_point_y = ts->ts_data.curr_data[ts->ts_data.total_num-1].y_position;

					printk("[TOUCH] Single press point [%d:%d]\n", single_press_point_x, single_press_point_y);
				}
#if defined(CONFIG_MACH_MSM8960_L1v)
				else if(single_touch_state == -1) {
					single_touch_state = 0;
				}
#endif
#if defined(CONFIG_MACH_MSM8960_FX1)
			}
#endif
		}
		else{
#if defined(CONFIG_MACH_MSM8960_FX1)
			single_touch_state = -1;
			if((ts->gf_ctrl.count != ts->gf_ctrl.max_count) && ((ts->gf_ctrl.stage & GHOST_STAGE_2) == 0)){
				if(ts->gf_ctrl.baseline_state == BASELINE_FIX)
				if(touch_device_func->ic_ctrl){
					if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0)
						return -1;
					ts->gf_ctrl.baseline_state = BASELINE_OPEN;
				}
			}
			ts->gf_ctrl.count = ts->gf_ctrl.max_count;
#endif
#if defined(CONFIG_MACH_MSM8960_L1v)
			if(single_touch_state != -1)
				multi_touch_state++;

			single_touch_state = -1;
			if(multi_touch_state == 2) {
				if(touch_device_func->ic_ctrl) {
					if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0)
						return -1;
				}
			}
#endif
		}
	}
	else if(ts->gf_ctrl.stage & GHOST_STAGE_2){
		if((ts->ts_data.total_num > 1 || ((ts->ts_data.total_num == 1) && (ts->ts_data.curr_button.state==BUTTON_PRESSED)))){
			ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
#if !defined(CONFIG_MACH_MSM8960_FX1)
			ts->gf_ctrl.stage |= GHOST_STAGE_1;
#endif
#if defined(CONFIG_MACH_MSM8960_FX1)
			if(touch_device_func->ic_ctrl){
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0)
					return -1;
#if !defined(CONFIG_MACH_MSM8960_FX1)
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0)
					return -1;
#endif
			}
#endif
			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_2: multi_finger. return to ghost_stage_1[0x%x]\n", ts->gf_ctrl.stage);
		}
	}
	else if(ts->gf_ctrl.stage & GHOST_STAGE_3){
		if(ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm_touch == 0){
			if(touch_device_func->ic_ctrl){
				if (touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0)
					return -1;
			}
			ts->gf_ctrl.stage = GHOST_STAGE_CLEAR;
			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_3: cleared[0x%x]\n", ts->gf_ctrl.stage);
			if (unlikely(touch_debug_mask & DEBUG_GHOST
					|| touch_debug_mask & DEBUG_BASE_INFO))
				TOUCH_INFO_MSG("ghost_stage_finished. (KEYGUARD)\n");
		}
	}

	return 0;
}

#ifdef CONFIG_LGE_TOUCH_CHAGER_CTRL
int trigger_baseline = 0;
int ts_chg_ctrl = 0;
void trigger_baseline_state_machine(int plug_in)
{
	if(touch_test_dev && atomic_read(&touch_test_dev->device_init) == 1){
		trigger_baseline = 1;
		ts_chg_ctrl = plug_in;
	}
}
#endif

/* Jitter Filter
 *
 */
#define jitter_abs(x)		(x > 0 ? x : -x)
#define jitter_sub(x, y)	(x > y ? x - y : y - x)

static u16 check_boundary(int x, int max){
	if(x < 0)
		return 0;
	else if(x > max)
		return (u16)max;
	else
		return (u16)x;
}

static int check_direction(int x){
	if(x > 0)
		return 1;
	else if(x < 0)
		return -1;
	else
		return 0;
}

static int accuracy_filter_func(struct lge_touch_data *ts)
{
	int delta_x = 0;
	int delta_y = 0;

	// finish the accuracy_filter
	if(ts->accuracy_filter.finish_filter == 1 &&
	   (ts->accuracy_filter.his_data.count > ts->accuracy_filter.touch_max_count
	    || ts->ts_data.total_num != 1
	    || ts->ts_data.curr_data[0].id != 0)){
		ts->accuracy_filter.finish_filter = 0;
		ts->accuracy_filter.his_data.count = 0;
	}

	// main algorithm
	if (ts->accuracy_filter.finish_filter){
		delta_x = (int)ts->accuracy_filter.his_data.x - (int)ts->ts_data.curr_data[0].x_position;
		delta_y = (int)ts->accuracy_filter.his_data.y - (int)ts->ts_data.curr_data[0].y_position;
		if(delta_x || delta_y){
			ts->accuracy_filter.his_data.axis_x += check_direction(delta_x);
			ts->accuracy_filter.his_data.axis_y += check_direction(delta_y);
			ts->accuracy_filter.his_data.count++;
		}

		if(ts->accuracy_filter.his_data.count == 1
		    ||(  (jitter_sub(ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure) > ts->accuracy_filter.ignore_pressure_gap
		           || ts->ts_data.curr_data[0].pressure > ts->accuracy_filter.max_pressure)
		        && !(  (ts->accuracy_filter.his_data.count > ts->accuracy_filter.time_to_max_pressure
		                 && (jitter_abs(ts->accuracy_filter.his_data.axis_x) == ts->accuracy_filter.his_data.count
			             || jitter_abs(ts->accuracy_filter.his_data.axis_y) == ts->accuracy_filter.his_data.count))
			      ||(jitter_abs(ts->accuracy_filter.his_data.axis_x) > ts->accuracy_filter.direction_count
			         || jitter_abs(ts->accuracy_filter.his_data.axis_y) > ts->accuracy_filter.direction_count)))){
					ts->accuracy_filter.his_data.mod_x += delta_x;
					ts->accuracy_filter.his_data.mod_y += delta_y;
		}
	}

	// if 'delta' > delta_max or id != 0, remove the modify-value.
	if(ts->ts_data.curr_data[0].id != 0 ||
	   (ts->accuracy_filter.his_data.count != 1 &&
	    (jitter_abs(delta_x) > ts->accuracy_filter.delta_max || jitter_abs(delta_y) > ts->accuracy_filter.delta_max))){
		ts->accuracy_filter.his_data.mod_x = 0;
		ts->accuracy_filter.his_data.mod_y = 0;
	}

	// start the accuracy_filter
	if(ts->accuracy_filter.finish_filter == 0
	   && ts->accuracy_filter.his_data.count == 0
	   && ts->ts_data.total_num == 1
	   && ts->accuracy_filter.his_data.prev_total_num == 0
	   && ts->ts_data.curr_data[0].id == 0){
		ts->accuracy_filter.finish_filter = 1;
		memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));
	}

	if(unlikely(touch_debug_mask & DEBUG_ACCURACY)){
		TOUCH_INFO_MSG("AccuracyFilter: <%d> pos[%4d,%4d] new[%4d,%4d] his[%4d,%4d] delta[%3d,%3d] mod[%3d,%3d] p[%d,%3d,%3d] axis[%2d,%2d] count[%2d/%2d] total_num[%d,%d] finish[%d]\n",
				ts->ts_data.curr_data[0].id, ts->ts_data.curr_data[0].x_position, ts->ts_data.curr_data[0].y_position,
				check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->caps->x_max),
				check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->caps->y_max),
				ts->accuracy_filter.his_data.x, ts->accuracy_filter.his_data.y,
				delta_x, delta_y,
				ts->accuracy_filter.his_data.mod_x, ts->accuracy_filter.his_data.mod_y,
				jitter_sub(ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure) > ts->accuracy_filter.ignore_pressure_gap,
				ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure,
				ts->accuracy_filter.his_data.axis_x, ts->accuracy_filter.his_data.axis_y,
				ts->accuracy_filter.his_data.count, ts->accuracy_filter.touch_max_count,
				ts->accuracy_filter.his_data.prev_total_num, ts->ts_data.total_num, ts->accuracy_filter.finish_filter);
	}

	ts->accuracy_filter.his_data.x = ts->ts_data.curr_data[0].x_position;
	ts->accuracy_filter.his_data.y = ts->ts_data.curr_data[0].y_position;
	ts->accuracy_filter.his_data.pressure = ts->ts_data.curr_data[0].pressure;
	ts->accuracy_filter.his_data.prev_total_num = ts->ts_data.total_num;

	if(ts->ts_data.total_num){
		ts->ts_data.curr_data[0].x_position
			= check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->caps->x_max);
		ts->ts_data.curr_data[0].y_position
			= check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->caps->y_max);
	}

	return 0;
}

static int jitter_filter_func(struct lge_touch_data *ts)
{
	int i = 0;
	int jitter_count = 0;
	u16 new_id_mask = 0;
	u16 bit_mask = 0;
	u16 bit_id = 1;
	int curr_ratio = ts->pdata->role->jitter_curr_ratio;

	if(ts->accuracy_filter.finish_filter)
		return 0;

	if(ts->ts_data.total_num == 0 || ts->ts_data.total_num >= MAX_FINGER){
					memset(&ts->jitter_filter, 0, sizeof(struct jitter_filter_info));
//				   TOUCH_INFO_MSG("JitterFilter: release-all\n");
				   return 0;
	 }

	for(i=0; i<ts->ts_data.total_num; i++){
		u16 id = ts->ts_data.curr_data[i].id;
		u16 width = ts->ts_data.curr_data[i].width_major;
		new_id_mask |= (1 << id);

		if(ts->jitter_filter.id_mask & (1 << id)){
			int delta_x, delta_y;
			int f_jitter = curr_ratio*width;
			int adjust_x, adjust_y;

			if (ts->jitter_filter.ignore_pressure_gap > 0) {
				if(jitter_sub(ts->ts_data.curr_data[i].pressure, ts->jitter_filter.his_data[id].pressure) > ts->jitter_filter.ignore_pressure_gap){
					ts->jitter_filter.his_data[id].mod_x = (int)ts->jitter_filter.his_data[id].x - (int)ts->ts_data.curr_data[i].x_position;
					ts->jitter_filter.his_data[id].mod_y = (int)ts->jitter_filter.his_data[id].y - (int)ts->ts_data.curr_data[i].y_position;
					if(ts->jitter_filter.his_data[id].mod_x > ts->jitter_filter.delta_max|| ts->jitter_filter.his_data[id].mod_y > ts->jitter_filter.delta_max){
						ts->jitter_filter.his_data[id].mod_x = 0;
						ts->jitter_filter.his_data[id].mod_y = 0;
				}
			}

				ts->ts_data.curr_data[i].x_position = check_boundary((int)ts->ts_data.curr_data[i].x_position + ts->jitter_filter.his_data[id].mod_x, ts->pdata->caps->x_max);
				ts->ts_data.curr_data[i].y_position = check_boundary((int)ts->ts_data.curr_data[i].y_position + ts->jitter_filter.his_data[id].mod_y, ts->pdata->caps->y_max);
					}

			if (ts->jitter_filter.adjust_margin > 0) {
				adjust_x = (int)ts->ts_data.curr_data[i].x_position - (int)ts->jitter_filter.his_data[id].x;
				adjust_y = (int)ts->ts_data.curr_data[i].y_position - (int)ts->jitter_filter.his_data[id].y;

				if (jitter_abs(adjust_x) > ts->jitter_filter.adjust_margin) {
					adjust_x = (int)ts->ts_data.curr_data[i].x_position + (adjust_x >> 2);
					ts->ts_data.curr_data[i].x_position = check_boundary(adjust_x, ts->pdata->caps->x_max);
				}

				if (jitter_abs(adjust_y) > ts->jitter_filter.adjust_margin) {
					adjust_y = (int)ts->ts_data.curr_data[i].y_position + (adjust_y >> 2);
					ts->ts_data.curr_data[i].y_position = check_boundary(adjust_y, ts->pdata->caps->y_max);
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

			if(unlikely(touch_debug_mask & DEBUG_JITTER)){
				TOUCH_INFO_MSG("JitterFilter: <%d> pos[%d,%d] h_pos[%d,%d] delta[%d,%d] h_delta[%d,%d] j_fil[%d]\n",
						id, ts->ts_data.curr_data[i].x_position, ts->ts_data.curr_data[i].y_position,
						ts->jitter_filter.his_data[id].x, ts->jitter_filter.his_data[id].y, delta_x, delta_y,
						ts->jitter_filter.his_data[id].delta_x, ts->jitter_filter.his_data[id].delta_y, f_jitter);
			}

			if(jitter_abs(ts->jitter_filter.his_data[id].delta_x) <= f_jitter &&
			   jitter_abs(ts->jitter_filter.his_data[id].delta_y) <= f_jitter)
				jitter_count++;
		}
	}

	bit_mask = ts->jitter_filter.id_mask ^ new_id_mask;

	for(i=0, bit_id=1; i<MAX_FINGER; i++){
		if ((ts->jitter_filter.id_mask & bit_id) && !(new_id_mask & bit_id)){
			if(unlikely(touch_debug_mask & DEBUG_JITTER))
				TOUCH_INFO_MSG("JitterFilter: released - id[%d] mask[0x%x]\n", bit_id, ts->jitter_filter.id_mask);
			memset(&ts->jitter_filter.his_data[i], 0, sizeof(ts->jitter_filter.his_data[i]));
		}
		bit_id = bit_id << 1;
	}

	for(i=0; i<ts->ts_data.total_num; i++){
		u16 id = ts->ts_data.curr_data[i].id;
		ts->jitter_filter.his_data[id].pressure = ts->ts_data.curr_data[i].pressure;
		}

	if(!bit_mask && ts->ts_data.total_num && ts->ts_data.total_num == jitter_count){
		if(unlikely(touch_debug_mask & DEBUG_JITTER))
			TOUCH_INFO_MSG("JitterFilter: ignored - jitter_count[%d] total_num[%d] bitmask[0x%x]\n",
					jitter_count, ts->ts_data.total_num, bit_mask);
		return -1;
	}

	for(i=0; i<ts->ts_data.total_num; i++){
		u16 id = ts->ts_data.curr_data[i].id;
		ts->jitter_filter.his_data[id].x = ts->ts_data.curr_data[i].x_position;
		ts->jitter_filter.his_data[id].y = ts->ts_data.curr_data[i].y_position;
	}

	ts->jitter_filter.id_mask = new_id_mask;

	return 0;
}

/* touch_init_func
 *
 * In order to reduce the booting-time,
 * we used delayed_work_queue instead of msleep or mdelay.
 */
static void touch_init_func(struct work_struct *work_init)
{
	int ret = 0;
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_init), struct lge_touch_data, work_init);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	/* Specific device initialization */
	ret = touch_ic_init(ts);

//	ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2 | GHOST_STAGE_3;
/*
	if (ret == 0) {
		//ic_ctrl and fw_upgrade methods are called in the workqueue context only when touch_ic_init() is successful.
		ts->gf_ctrl.stage = GHOST_STAGE_1;
		if (touch_device_func->ic_ctrl)
			touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN);

		// Firmware Upgrade Check - use thread for booting time reduction
		if (touch_device_func->fw_upgrade)
			queue_work(touch_wq, &ts->work_fw_upgrade);
	}
*/
}

static void touch_lock_func(struct work_struct *work_touch_lock)
{
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_touch_lock), struct lge_touch_data, work_touch_lock);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	ts->ts_data.state = DO_NOT_ANYTHING;
}

/* touch_work_func_a
 *
 * HARD_TOUCH_KEY
 */
static void touch_work_func_a(struct work_struct *work)
{
	struct lge_touch_data *ts =
			container_of(work, struct lge_touch_data, work);
	u8 report_enable = 0;
	int int_pin = 0;
	int next_work = 0;
	static u8 pre_stage;
	int ret = 0;
	u8 cnt = 0;
	u8 ghost_detection = 0;
	extern u8 pressure_zero;

	if (ts->pdata->role->ghost_detection_enable) {
		if(trigger_baseline==2){
			trigger_baseline = 0;
			touch_device_func->data(ts->client, ts->ts_data.curr_data, 
							&ts->ts_data.curr_button, &ts->ts_data.total_num, &ts->ts_data.palm_touch);
			goto err_out_ghost;
		}
	}

	atomic_dec(&ts->next_work);
	ts->ts_data.total_num = 0;

	if(unlikely(ts->work_sync_err_cnt >= MAX_RETRY_COUNT)){
		TOUCH_ERR_MSG("Work Sync Failed: Irq-pin has some unknown problems\n");
		goto err_out_critical;
	}
#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_START]);
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	ret = touch_device_func->data(ts->client, ts->ts_data.curr_data,
							&ts->ts_data.curr_button, &ts->ts_data.total_num, &ts->ts_data.palm_touch);
	if(ret<0){
		if(ret == -EPERM)
			return;
		else {
			TOUCH_ERR_MSG("get data fail\n");
			goto err_out_critical;
		}

	}

	if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE))
		int_pin = gpio_get_value(ts->pdata->int_pin);

	if (ts->pdata->role->ghost_detection_enable) {
		if(probe_flag) {
			probe_flag = 0;
			do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_INT_TIME]);

			if( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_PROBE].tv_sec == 0 ) {
				if((get_time_interval(t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec,
				                   t_ex_debug[TIME_EX_PROBE].tv_usec)) <= 200000) ghost_detection = 1;
			} else if( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_PROBE].tv_sec == 1 ) {
				if( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec + 1000000
					- t_ex_debug[TIME_EX_PROBE].tv_usec <= 200000) {
					ghost_detection = 1;
				}
			}
		}

		if(resume_flag) {
			resume_flag = 0;
			do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_INT_TIME]);

			if( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_RESUME_ON].tv_sec == 0 ) {
				if((get_time_interval(t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec,
				                   t_ex_debug[TIME_EX_RESUME_ON].tv_usec)) <= 200000) ghost_detection = 1;
			} else if( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_RESUME_ON].tv_sec == 1 ) {
				if( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec + 1000000
					- t_ex_debug[TIME_EX_RESUME_ON].tv_usec <= 200000) {
					ghost_detection = 1;
				}
			}
		}

		if(pressure_zero == 1) {
			TOUCH_INFO_MSG("pressure\n");
			need_to_rebase = 1;
		}

#if defined(CONFIG_MACH_MSM8960_L1) || defined(CONFIG_MACH_MSM8960_FX1)
		if(ts->gf_ctrl.incoming_call && (ts->ts_data.total_num > 1)) {
			TOUCH_INFO_MSG("call state rebase\n");
			goto err_out_ghost;
		}
#endif

		if(ghost_detection)	{
			for (cnt = 0; cnt < MAX_FINGER; cnt++) {
				if (ts->ts_data.curr_data[cnt].status == FINGER_PRESSED) {
					if(ts->ts_data.curr_data[cnt].pressure <= 30) {
						TOUCH_INFO_MSG("first input time is 200ms\n");
						need_to_rebase = 1;
						break;
					}
				}
			}
		}

		if(need_to_rebase == 1 && ts->ts_data.total_num == 0 && ts->ts_data.palm_touch== 0) {
			TOUCH_INFO_MSG("need_to_rebase zero\n");
			goto err_out_ghost;
		}
	}

	/* Ghost finger solution */
	if (unlikely(ts->gf_ctrl.stage)){
		if(ghost_finger_solution(ts)){
			TOUCH_ERR_MSG("ghost_finger_solution was failed\n");
			goto err_out_critical;
		}
		if(pre_stage != ts->gf_ctrl.stage){
			pre_stage = ts->gf_ctrl.stage;
			printk("[TOUCH] Ghost Finger Stage : %d\n", pre_stage);
		}
	}

	/* Accuracy Solution */
	if (likely(ts->pdata->role->accuracy_filter_enable)){
		if (accuracy_filter_func(ts) < 0)
			goto out;
	}
	if (ts->pdata->role->ghost_detection_enable) {
		if (ts_charger_plug)
		{
			if( (1 < ts->ts_data.total_num) && ( ta_debouncing_count < ts->pdata->role->ta_debouncing_count))
			{
				ta_debouncing_count ++;
#if defined(CONFIG_MACH_MSM8960_FX1)
				memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));
#endif
				goto out;
			}
			else if (ts->ts_data.total_num <= 1)
				ta_debouncing_count = 0;
			else
				;
		}
	}

	/* Jitter Solution */
	if (likely(ts->pdata->role->jitter_filter_enable)){
		if (jitter_filter_func(ts) < 0)
			goto out;
	}

	/* Finger handle */
	if (ts->ts_data.state != TOUCH_ABS_LOCK) {
		if (!ts->ts_data.total_num) {
			input_mt_sync(ts->input_dev);
			report_enable = 1;

			queue_delayed_work(touch_wq, &ts->work_touch_lock, msecs_to_jiffies(200));

			if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS))) {
				if (ts->ts_data.prev_total_num) {
					for(cnt=0; cnt < MAX_FINGER; cnt++){
						if (ts->ts_data.prev_data[cnt].status == FINGER_PRESSED) {
							break;
						}
					}

					if (ts->pdata->role->ghost_detection_enable) {
						ta_debouncing_count = 0;
					}
					if(cnt < 10) {
						TOUCH_INFO_MSG("touch_release : <%d> x[%4d] y[%4d]\n",
								cnt, ts->ts_data.prev_data[cnt].x_position,
								ts->ts_data.prev_data[cnt].y_position);
					}
				}
			}

			ts->ts_data.prev_total_num = 0;

			memset(&ts->ts_data.prev_data, 0x0, sizeof(ts->ts_data.prev_data));

		} else if (ts->ts_data.total_num <= MAX_FINGER) {
			cancel_delayed_work_sync(&ts->work_touch_lock);

//			if (ts->gf_ctrl.stage == GHOST_STAGE_CLEAR || ts->gf_ctrl.stage == GHOST_STAGE_3)
			ts->ts_data.state = TOUCH_BUTTON_LOCK;

			/* key button cancel */
			if(ts->ts_data.prev_button.state == BUTTON_PRESSED) {
#if 0
#if defined(CONFIG_MACH_MSM8960_L1)
				/* touch cancel */
				if(ts->ts_data.curr_data[0].y_position > 1880) {
					TOUCH_INFO_MSG("touch_cancel : x[%4d] y[%4d], Ghost Stage %d\n", ts->ts_data.curr_data[0].x_position, ts->ts_data.curr_data[0].y_position, ts->gf_ctrl.stage);
					/* Reset finger position data */
					memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));
					ts->ts_data.state = TOUCH_ABS_LOCK;
					queue_delayed_work(touch_wq, &ts->work_touch_lock, msecs_to_jiffies(50));
					}
				else {
#endif
#endif
				input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_CANCLED);

				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is canceled\n",
							ts->ts_data.prev_button.key_code);

				memset(&ts->ts_data.prev_button, 0x0, sizeof(ts->ts_data.prev_button));
#if 0
#if defined(CONFIG_MACH_MSM8960_L1)
				}
#endif
#endif
			}

			if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS))) {
				if (ts->ts_data.prev_total_num != ts->ts_data.total_num) {
					/* Finger added or changed */
					if (ts->ts_data.prev_total_num <= ts->ts_data.total_num) {
						for(cnt=0; cnt < MAX_FINGER; cnt++){
							if (ts->ts_data.curr_data[cnt].status == FINGER_PRESSED
									&& ts->ts_data.prev_data[cnt].status == FINGER_RELEASED) {
								break;
							}
						}
						if(cnt < 10) {
							TOUCH_INFO_MSG("%d finger pressed : <%d> x[%4d] y[%4d] p[%3d]\n",
									ts->ts_data.total_num, cnt,
									ts->ts_data.curr_data[cnt].x_position,
									ts->ts_data.curr_data[cnt].y_position,
									ts->ts_data.curr_data[cnt].pressure);
						}

						if (ts->pdata->role->ghost_detection_enable && cnt < 10) {
							memcpy(&t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval));
							do_gettimeofday(&t_ex_debug[TIME_EX_CURR_PRESS_TIME]);  

#if defined(CONFIG_MACH_MSM8960_FX1)
							if ( 1== ts->ts_data.prev_total_num && 1== ts->ts_data.total_num && jitter_sub(ts_prev_finger_press_data.x_position,ts->ts_data.curr_data[cnt].x_position)<10 && jitter_sub(ts_prev_finger_press_data.y_position,ts->ts_data.curr_data[cnt].y_position)<10 
								&& (ts_prev_finger_press_data.pressure + 3 > ts->ts_data.curr_data[cnt].pressure) && (ts_prev_finger_press_data.pressure - 3 < ts->ts_data.curr_data[cnt].pressure))
#else
							if ( 1<= ts->ts_data.prev_total_num && 1<= ts->ts_data.total_num && jitter_sub(ts_prev_finger_press_data.x_position,ts->ts_data.curr_data[cnt].x_position)<10 && jitter_sub(ts_prev_finger_press_data.y_position,ts->ts_data.curr_data[cnt].y_position)<10 )
#endif
							{
							       // if time_interval between prev fingger pressed and curr finger pressed is less than 50ms, we need to rebase touch ic.
								if(((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==1) &&
									(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) < 51*1000))
									goto err_out_ghost;	
								else if(((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==0) &&
									(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) < 51*1000))
									goto err_out_ghost;
								else
									; // do not anything.

							}
							else if (ts->ts_data.prev_total_num==0 && ts->ts_data.total_num==1 && jitter_sub(ts_prev_finger_press_data.x_position,ts->ts_data.curr_data[cnt].x_position)<10 && jitter_sub(ts_prev_finger_press_data.y_position,ts->ts_data.curr_data[cnt].y_position)<10 )
							{
							       // if time_interval between prev fingger pressed and curr finger pressed is less than 50ms, we need to rebase touch ic.
								if(((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==1) &&	
									(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) < 51*1000))
									need_to_rebase = 1;
								else if(((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==0) &&
									(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) < 51*1000))
									need_to_rebase = 1;	
								else
									; // do not anything.

							}
							else if ( 5 < jitter_sub(ts->ts_data.prev_total_num,ts->ts_data.total_num) )
							{
								 need_to_rebase = 1;
							}
							else
								;

							memcpy(&ts_prev_finger_press_data, &ts->ts_data.curr_data[cnt], sizeof(ts_prev_finger_press_data));
						}

					} else {
					/* Finger subtracted */
						TOUCH_INFO_MSG("%d finger pressed\n", ts->ts_data.total_num);
					}
				}
			}

			ts->ts_data.prev_total_num = ts->ts_data.total_num;

			while(ts->ts_data.total_num--) {
				if ((cnt < MAX_FINGER) && (ts->ts_data.curr_data[cnt].status == FINGER_PRESSED)) {
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
							ts->ts_data.curr_data[ts->ts_data.total_num].x_position);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
							ts->ts_data.curr_data[ts->ts_data.total_num].y_position);
					if (ts->pdata->caps->is_pressure_supported)
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
										 ts->ts_data.curr_data[ts->ts_data.total_num].pressure);
					if (ts->pdata->caps->is_width_supported) {
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
										 ts->ts_data.curr_data[ts->ts_data.total_num].width_major);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR,
										 ts->ts_data.curr_data[ts->ts_data.total_num].width_minor);
						input_report_abs(ts->input_dev, ABS_MT_ORIENTATION,
										 ts->ts_data.curr_data[ts->ts_data.total_num].width_orientation);
					}
					if (ts->pdata->caps->is_id_supported)
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
										 ts->ts_data.curr_data[ts->ts_data.total_num].id);
					input_mt_sync(ts->input_dev);

					if (unlikely(touch_debug_mask & DEBUG_ABS))
						TOUCH_INFO_MSG("<%d> pos[%4d,%4d] w_m[%2d] w_n[%2d] w_o[%2d] p[%3d]\n",
								ts->pdata->caps->is_id_supported?
								ts->ts_data.curr_data[ts->ts_data.total_num].id : 0,
								ts->ts_data.curr_data[ts->ts_data.total_num].x_position,
								ts->ts_data.curr_data[ts->ts_data.total_num].y_position,
								ts->pdata->caps->is_width_supported?
								ts->ts_data.curr_data[ts->ts_data.total_num].width_major: 0,
								ts->pdata->caps->is_width_supported?
								ts->ts_data.curr_data[ts->ts_data.total_num].width_minor: 0,
								ts->pdata->caps->is_width_supported?
								ts->ts_data.curr_data[ts->ts_data.total_num].width_orientation: 0,
								ts->pdata->caps->is_pressure_supported ?
								ts->ts_data.curr_data[ts->ts_data.total_num].pressure : 0);
				}

				else if (cnt >= MAX_FINGER) {
					TOUCH_ERR_MSG("the number of 2D inputs has exceeded MAX_FINGER value\n");
				}
			}
			report_enable = 1;

			memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(ts->ts_data.curr_data));
		}

		/* Reset finger position data */
		memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));

		if (report_enable)
			input_sync(ts->input_dev);
	}

	/* Button handle */
	if (ts->ts_data.state != TOUCH_BUTTON_LOCK) {
		/* do not check when there is no pressed button at error case
		 * 	- if you check it, sometimes touch is locked becuase button pressed via IC error.
		 */
		if (ts->work_sync_err_cnt > 0
				&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
			/* Do nothing */
		} else {
			report_enable = 0;
#if 0
			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
				TOUCH_INFO_MSG("Cur. button -code: %d state: %d, Prev. button -code: %d state: %d\n",
						ts->ts_data.curr_button.key_code,
						ts->ts_data.curr_button.state,
						ts->ts_data.prev_button.key_code,
						ts->ts_data.prev_button.state);
#endif
			if (ts->pdata->role->ghost_detection_enable) {
				if (button_press_count ==0)
					do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME]);
				else
					do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME]);

				button_press_count++;

				if (unlikely(touch_debug_mask & DEBUG_BUTTON))
					TOUCH_INFO_MSG("button_press_count %d \n", button_press_count);

				if (6 <= button_press_count)
				{
				     if(((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec - t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec)==1) &&
						(( get_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_usec)) < 100*1000)) {
							TOUCH_INFO_MSG("need_to_rebase button zero\n");
							goto err_out_ghost;
					} else if(((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec - t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec)==0) &&
						(( get_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_usec, t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_usec)) < 100*1000)) {
							TOUCH_INFO_MSG("need_to_rebase button zero\n");
							goto err_out_ghost;
					} else
						; //do not anything
					button_press_count = 0;
				} else {
					if((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec - 
						t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec) > 1)
						button_press_count = 0;
				}
			}

			if (ts->ts_data.curr_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
				/* button pressed */
				cancel_delayed_work_sync(&ts->work_touch_lock);

				if (ts->gf_ctrl.stage == GHOST_STAGE_CLEAR || ts->gf_ctrl.stage == GHOST_STAGE_3)
					ts->ts_data.state = TOUCH_ABS_LOCK;

				queue_delayed_work(touch_wq, &ts->work_touch_lock, msecs_to_jiffies(200));

				input_report_key(ts->input_dev, ts->ts_data.curr_button.key_code, BUTTON_PRESSED);

				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is pressed\n",
							ts->ts_data.curr_button.key_code);

				memcpy(&ts->ts_data.prev_button, &ts->ts_data.curr_button,
						sizeof(ts->ts_data.curr_button));

				report_enable = 1;
			} else if (ts->ts_data.curr_button.state == BUTTON_RELEASED /* button released */
					&& ts->ts_data.prev_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.key_code == ts->ts_data.curr_button.key_code) {
				/* button release */
				input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is released\n",
							ts->ts_data.prev_button.key_code);

				memset(&ts->ts_data.prev_button, 0x0, sizeof(ts->ts_data.prev_button));
				memset(&ts->ts_data.curr_button, 0x0, sizeof(ts->ts_data.curr_button));

				report_enable = 1;
			} else if(ts->ts_data.curr_button.state == BUTTON_RELEASE_ALL){
				if(ts->ts_data.prev_button.state == BUTTON_PRESSED)
					input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is released\n", ts->ts_data.prev_button.key_code);

				memset(&ts->ts_data.prev_button, 0x0, sizeof(ts->ts_data.prev_button));
				memset(&ts->ts_data.curr_button, 0x0, sizeof(ts->ts_data.curr_button));

				report_enable = 1;
			}

			if (report_enable)
				input_sync(ts->input_dev);
		}
	}

out:
	if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE)){
		next_work = atomic_read(&ts->next_work);

		if(unlikely(int_pin != 1 && next_work <= 0)){
			TOUCH_INFO_MSG("WARN: Interrupt pin is low - next_work: %d, try_count: %d]\n",
					next_work, ts->work_sync_err_cnt);
			goto err_out_retry;
		}
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_END]);
	if (next_work)
		memset(t_debug, 0x0, sizeof(t_debug));
	time_profile_result(ts);
#endif

	ts->work_sync_err_cnt = 0;

	return;

err_out_ghost:
	if (ts->pdata->role->ghost_detection_enable)
	{
			memset(&ts_prev_finger_press_data, 0x0, sizeof(ts_prev_finger_press_data));
			button_press_count = 0;
			need_to_rebase = 0;
			ts_rebase_count++;

			if(ts_rebase_count==1) {
					do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME]);
			} else {
					do_gettimeofday(&t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME]);

					if(((t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec) <= 5))
					{
							ts_rebase_count = 0;
							TOUCH_INFO_MSG("need_to_init\n");
							goto err_out_critical;
					} else {
						ts_rebase_count = 1;
						memcpy(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME], &t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME], sizeof(struct timeval));
					}
			}
			release_all_ts_event(ts);
			memset(&ts->ts_data, 0, sizeof(ts->ts_data));
			memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));
			ts->accuracy_filter.finish_filter = 0;
			if(touch_device_func->ic_ctrl){
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0){
					TOUCH_ERR_MSG("IC_CTRL_REBASE handling fail\n");
				}
			}
			TOUCH_INFO_MSG("need_to_rebase\n");
	}
	return;

err_out_retry:
	ts->work_sync_err_cnt++;
	atomic_inc(&ts->next_work);
	queue_work(touch_wq, &ts->work);

	return;

err_out_critical:
	ts->work_sync_err_cnt = 0;
	safety_reset(ts);
	touch_ic_init(ts);

	return;

}

static bool is_in_section(struct rect rt, u16 x, u16 y)
{
	return x >= rt.left && x <= rt.right && y >= rt.top && y <= rt.bottom;
}

static u16 find_button(const struct t_data data, const struct section_info sc)
{
	int i;

	if (is_in_section(sc.panel, data.x_position, data.y_position))
		return KEY_PANEL;

	for(i=0; i<sc.b_num; i++){
		if (is_in_section(sc.button[i], data.x_position, data.y_position))
			return sc.b_name[i];
	}

	return KEY_BOUNDARY;
}

static bool check_cancel(u16 button, u16 x, u16 y, const struct section_info sc)
{
	int i;

	for(i=0; i<sc.b_num; i++){
		if (sc.b_name[i] == button)
			break;
	}

	if (i < sc.b_num){
		if (is_in_section(sc.button_cancel[i], x, y))
			return false;
	}

	return true;
}

/* touch_work_func_b
 *
 * SOFT_TOUCH_KEY
 */
static void touch_work_func_b(struct work_struct *work)
{
	struct lge_touch_data *ts =
			container_of(work, struct lge_touch_data, work);

	u8  i;
	u8 op_mode = OP_NULL;
	u16 tmp_button = KEY_NULL;
	int int_pin = 0;
	int next_work = 0;

	atomic_dec(&ts->next_work);
	ts->ts_data.total_num = 0;

	if(unlikely(ts->work_sync_err_cnt >= MAX_RETRY_COUNT)){
		TOUCH_ERR_MSG("Work Sync Failed: Irq-pin has some unknown problems\n");
		goto err_out_critical;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_START]);
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func->data(ts->client, ts->ts_data.curr_data,
			&ts->ts_data.curr_button, &ts->ts_data.total_num, &ts->ts_data.palm_touch) < 0) {
		TOUCH_ERR_MSG("get data fail\n");
		goto err_out_critical;
	}

	if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE))
		int_pin = gpio_get_value(ts->pdata->int_pin);

	/* Ghost finger solution */
	if (unlikely(ts->gf_ctrl.stage)){
		if(ghost_finger_solution(ts)){
			TOUCH_ERR_MSG("ghost_finger_solution was failed\n");
			goto err_out_critical;
		}
	}

	/* Accuracy Solution */
	if (likely(ts->pdata->role->accuracy_filter_enable)){
		if (accuracy_filter_func(ts) < 0)
			goto out;
	}

	/* Jitter Solution */
	if (likely(ts->pdata->role->jitter_filter_enable)){
		if (jitter_filter_func(ts) < 0)
			goto out;
	}

	if (ts->ts_data.total_num == 0)
		op_mode = OP_RELEASE;
	else if (ts->ts_data.state == TOUCH_BUTTON_LOCK || ts->ts_data.state == TOUCH_ABS_LOCK)
		op_mode = OP_LOCK;
	else if (ts->ts_data.total_num == 1)
		op_mode = OP_SINGLE;
	else
		op_mode = OP_MULTI;

	switch(op_mode){
	case OP_RELEASE:
		if (ts->ts_data.prev_button.key_code == KEY_PANEL || ts->ts_data.prev_button.key_code == KEY_BOUNDARY
		    || ts->ts_data.prev_button.key_code == KEY_NULL)
				ts->ts_data.state = ABS_RELEASE;
			else
				ts->ts_data.state = BUTTON_RELEASE;

		ts->ts_data.curr_button.key_code = KEY_NULL;
		ts->ts_data.prev_total_num = 0;
		break;

	case OP_SINGLE:
		tmp_button = find_button(ts->ts_data.curr_data[0], ts->st_info);
		//if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			//TOUCH_INFO_MSG("button_now [%d]\n", tmp_button);

		if (ts->ts_data.prev_button.key_code != KEY_NULL && ts->ts_data.prev_button.key_code != KEY_BOUNDARY){
			if (ts->ts_data.prev_button.key_code == KEY_PANEL){
				if (ts->ts_data.prev_button.key_code != tmp_button)
					ts->ts_data.state = ABS_RELEASE;
				else
					ts->ts_data.state = ABS_PRESS;
			}
			else{
				if (check_cancel(ts->ts_data.prev_button.key_code, ts->ts_data.curr_data[0].x_position,
						ts->ts_data.curr_data[0].y_position, ts->st_info))
					ts->ts_data.state = BUTTON_CANCEL;
				else
					ts->ts_data.state = DO_NOT_ANYTHING;
			}
		}
		else{
			if (tmp_button == KEY_PANEL || tmp_button == KEY_BOUNDARY)
				ts->ts_data.state = ABS_PRESS;
			else
				ts->ts_data.state = BUTTON_PRESS;
		}

		if (ts->ts_data.state == ABS_PRESS || ts->ts_data.state == BUTTON_PRESS)
			ts->ts_data.curr_button.key_code = tmp_button;
		else if (ts->ts_data.state == BUTTON_RELEASE ||
				ts->ts_data.state == BUTTON_CANCEL || ts->ts_data.state == ABS_RELEASE)
			ts->ts_data.curr_button.key_code = KEY_NULL;
		break;

	case OP_MULTI:
		if (ts->ts_data.prev_button.key_code && ts->ts_data.prev_button.key_code != KEY_PANEL
				&& ts->ts_data.prev_button.key_code != KEY_BOUNDARY)
			ts->ts_data.state = BUTTON_CANCEL;
		else
			ts->ts_data.state = ABS_PRESS;
		ts->ts_data.curr_button.key_code = KEY_PANEL;
		break;

	case OP_LOCK:
		for(i=0; i<ts->ts_data.total_num; i++){
			if (ts->ts_data.curr_data[i].y_position < ts->pdata->caps->y_button_boundary){
				ts->ts_data.curr_button.key_code = KEY_PANEL;
				ts->ts_data.state = ABS_PRESS;
			}
		}
		break;

	default:
		break;
	}

//	if (unlikely(touch_debug_mask & (DEBUG_ABS |DEBUG_BUTTON)))
//		TOUCH_INFO_MSG("op_mode[%d] state[%d]\n", op_mode, ts->ts_data.state);

	switch(ts->ts_data.state){
	case ABS_PRESS:
abs_report:
		i=0;
		while(ts->ts_data.total_num--) {
			if (ts->ts_data.curr_data[ts->ts_data.total_num].y_position
					>= ts->pdata->caps->y_button_boundary)
				continue;

			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					ts->ts_data.curr_data[ts->ts_data.total_num].x_position);

			/* When a user's finger cross the boundary (from key to LCD),
			    a ABS-event will change its y-position to edge of LCD, automatically.*/
			if(ts->ts_data.curr_data[ts->ts_data.total_num].y_position < ts->pdata->caps->y_button_boundary &&
			   ts->ts_data.prev_data[ts->ts_data.total_num].y_position > ts->pdata->caps->y_button_boundary &&
			   ts->ts_data.prev_button.key_code != KEY_NULL)
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->pdata->caps->y_button_boundary);
			else
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					ts->ts_data.curr_data[ts->ts_data.total_num].y_position);

			if (ts->pdata->caps->is_pressure_supported)
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 ts->ts_data.curr_data[ts->ts_data.total_num].pressure);
			if (ts->pdata->caps->is_width_supported) {
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 ts->ts_data.curr_data[ts->ts_data.total_num].width_major);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR,
								 ts->ts_data.curr_data[ts->ts_data.total_num].width_minor);
				input_report_abs(ts->input_dev, ABS_MT_ORIENTATION,
								 ts->ts_data.curr_data[ts->ts_data.total_num].width_orientation);
			}
			if (ts->pdata->caps->is_id_supported)
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
								 ts->ts_data.curr_data[ts->ts_data.total_num].id);
			input_mt_sync(ts->input_dev);
			i++;

			if (unlikely(touch_debug_mask & DEBUG_ABS))
				TOUCH_INFO_MSG("<%d> pos[%4d,%4d] w_m[%2d] w_n[%2d] w_o[%2d] p[%3d]\n",
						ts->pdata->caps->is_id_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].id : 0,
						ts->ts_data.curr_data[ts->ts_data.total_num].x_position,
						ts->ts_data.curr_data[ts->ts_data.total_num].y_position,
						ts->pdata->caps->is_width_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].width_major: 0,
						ts->pdata->caps->is_width_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].width_minor: 0,
						ts->pdata->caps->is_width_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].width_orientation: 0,
						ts->pdata->caps->is_pressure_supported ?
						ts->ts_data.curr_data[ts->ts_data.total_num].pressure : 0);
		}

		if (!i) {
			input_mt_sync(ts->input_dev);

			ts->ts_data.prev_total_num = 0;
		}
		else {
			if (likely(touch_debug_mask & DEBUG_ABS)) {
				if (ts->ts_data.prev_total_num != i)
					TOUCH_INFO_MSG("%d finger pressed\n", i);
			}

			ts->ts_data.prev_total_num = i;
		}
		break;
	case ABS_RELEASE:
		input_mt_sync(ts->input_dev);
		break;
	case BUTTON_PRESS:
		input_report_key(ts->input_dev, ts->ts_data.curr_button.key_code, BUTTON_PRESSED);
			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_INFO_MSG("Touch KEY[%d] is pressed\n", ts->ts_data.curr_button.key_code);
		break;
	case BUTTON_RELEASE:
		input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);
			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_INFO_MSG("Touch KEY[%d] is released\n", ts->ts_data.prev_button.key_code);
		break;
	case BUTTON_CANCEL:
		input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_CANCLED);
			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_INFO_MSG("Touch KEY[%d] is canceled\n", ts->ts_data.prev_button.key_code);
		if (ts->ts_data.curr_data[0].y_position < ts->pdata->caps->y_button_boundary){
			input_sync(ts->input_dev);
			goto abs_report;
		}
		break;
	case TOUCH_BUTTON_LOCK:
	case TOUCH_ABS_LOCK:
		goto out;
		break;
	default:
		break;
	}

	input_sync(ts->input_dev);

	if (likely(touch_debug_mask & DEBUG_BASE_INFO)){
			if (ts->ts_data.state == ABS_RELEASE)
				TOUCH_INFO_MSG("touch_release : x[%4d] y[%4d]\n",
						ts->ts_data.curr_data[0].x_position, ts->ts_data.curr_data[0].y_position);
			if(ts->ts_data.state == BUTTON_RELEASE)
				TOUCH_INFO_MSG("touch_release : button[%d]\n", ts->ts_data.prev_button.key_code);
		}

	if (op_mode == OP_SINGLE && ts->ts_data.state == ABS_RELEASE)
			ts->ts_data.state = TOUCH_ABS_LOCK;

	if (ts->ts_data.state == BUTTON_CANCEL)
		ts->ts_data.state = TOUCH_BUTTON_LOCK;

	memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(ts->ts_data.curr_data));
	memcpy(&ts->ts_data.prev_button, &ts->ts_data.curr_button, sizeof(ts->ts_data.curr_button));

out:
	if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE)){
		next_work = atomic_read(&ts->next_work);

		if(unlikely(int_pin != 1 && next_work <= 0)){
			TOUCH_INFO_MSG("WARN: Interrupt pin is low - next_work: %d, try_count: %d]\n",
					next_work, ts->work_sync_err_cnt);
			goto err_out_retry;
		}
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_END]);
	if (next_work)
		memset(t_debug, 0x0, sizeof(t_debug));
	time_profile_result(ts);
#endif

	ts->work_sync_err_cnt = 0;

	return;

err_out_retry:
	ts->work_sync_err_cnt++;
	atomic_inc(&ts->next_work);
	queue_work(touch_wq, &ts->work);

	return;

err_out_critical:
	ts->work_sync_err_cnt = 0;
	safety_reset(ts);
	touch_ic_init(ts);

	return;
}

static void touch_work_func_c(struct work_struct *work)
{
	struct lge_touch_data *ts =
			container_of(work, struct lge_touch_data, work);
	u8 report_enable = 0;
	int int_pin = 0;
	int next_work = 0;

	atomic_dec(&ts->next_work);
	ts->ts_data.total_num = 0;

	if(unlikely(ts->work_sync_err_cnt >= MAX_RETRY_COUNT)){
		TOUCH_ERR_MSG("Work Sync Failed: Irq-pin has some unknown problems\n");
		goto err_out_critical;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_START]);
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func->data(ts->client, ts->ts_data.curr_data,
			&ts->ts_data.curr_button, &ts->ts_data.total_num, &ts->ts_data.palm_touch) < 0) {
		TOUCH_ERR_MSG("get data fail\n");
		goto err_out_critical;
	}

	if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE))
		int_pin = gpio_get_value(ts->pdata->int_pin);

	/* Ghost finger solution */
	if (unlikely(ts->gf_ctrl.stage)){
		if(ghost_finger_solution(ts)){
			TOUCH_ERR_MSG("ghost_finger_solution was failed\n");
			goto err_out_critical;
		}
	}

	/* Accuracy Solution */
	if (likely(ts->pdata->role->accuracy_filter_enable)){
		if (accuracy_filter_func(ts) < 0)
			goto out;
	}

	/* Jitter Solution */
	if (likely(ts->pdata->role->jitter_filter_enable)){
		if (jitter_filter_func(ts) < 0)
			goto out;
	}

	if (!ts->ts_data.total_num) {
		input_mt_sync(ts->input_dev);
		report_enable = 1;

		if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS))) {
			if (ts->ts_data.prev_total_num)
				TOUCH_INFO_MSG("touch_release : x[%4d] y[%4d]\n",
						ts->ts_data.prev_data[0].x_position, ts->ts_data.prev_data[0].y_position);
		}

		ts->ts_data.prev_total_num = 0;
	} else if (ts->ts_data.total_num <= MAX_FINGER) {
		if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS))) {
			if (ts->ts_data.prev_total_num != ts->ts_data.total_num)
				TOUCH_INFO_MSG("%d finger pressed\n", ts->ts_data.total_num);
		}

		ts->ts_data.prev_total_num = ts->ts_data.total_num;

		while(ts->ts_data.total_num--) {
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					ts->ts_data.curr_data[ts->ts_data.total_num].x_position);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					ts->ts_data.curr_data[ts->ts_data.total_num].y_position);
			if (ts->pdata->caps->is_pressure_supported)
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 ts->ts_data.curr_data[ts->ts_data.total_num].pressure);
			if (ts->pdata->caps->is_width_supported) {
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 ts->ts_data.curr_data[ts->ts_data.total_num].width_major);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR,
								 ts->ts_data.curr_data[ts->ts_data.total_num].width_minor);
				input_report_abs(ts->input_dev, ABS_MT_ORIENTATION,
								 ts->ts_data.curr_data[ts->ts_data.total_num].width_orientation);
			}
			if (ts->pdata->caps->is_id_supported)
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
								 ts->ts_data.curr_data[ts->ts_data.total_num].id);
			input_mt_sync(ts->input_dev);

			if (unlikely(touch_debug_mask & DEBUG_ABS))
				TOUCH_INFO_MSG("<%d> pos[%4d,%4d] w_m[%2d] w_n[%2d] w_o[%2d] p[%3d]\n",
						ts->pdata->caps->is_id_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].id : 0,
						ts->ts_data.curr_data[ts->ts_data.total_num].x_position,
						ts->ts_data.curr_data[ts->ts_data.total_num].y_position,
						ts->pdata->caps->is_width_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].width_major: 0,
						ts->pdata->caps->is_width_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].width_minor: 0,
						ts->pdata->caps->is_width_supported?
						ts->ts_data.curr_data[ts->ts_data.total_num].width_orientation: 0,
						ts->pdata->caps->is_pressure_supported ?
						ts->ts_data.curr_data[ts->ts_data.total_num].pressure : 0);
		}
		report_enable = 1;

		memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(ts->ts_data.curr_data));
	}

	/* Reset finger position data */
	memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));

	if (report_enable)
		input_sync(ts->input_dev);

out:
	if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE)){
		next_work = atomic_read(&ts->next_work);

		if(unlikely(int_pin != 1 && next_work <= 0)){
			TOUCH_INFO_MSG("WARN: Interrupt pin is low - next_work: %d, try_count: %d]\n",
					next_work, ts->work_sync_err_cnt);
			goto err_out_retry;
		}
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_END]);
	if (next_work)
		memset(t_debug, 0x0, sizeof(t_debug));
	time_profile_result(ts);
#endif

	ts->work_sync_err_cnt = 0;

	return;

err_out_retry:
	ts->work_sync_err_cnt++;
	atomic_inc(&ts->next_work);
	queue_work(touch_wq, &ts->work);

	return;

err_out_critical:
	ts->work_sync_err_cnt = 0;
	safety_reset(ts);
	touch_ic_init(ts);

	return;
}


/* touch_fw_upgrade_func
 *
 * it used to upgrade the firmware of touch IC.
 */
static void touch_fw_upgrade_func(struct work_struct *work_fw_upgrade)
{
	struct lge_touch_data *ts =
			container_of(work_fw_upgrade, struct lge_touch_data, work_fw_upgrade);
	u8	saved_state = ts->curr_pwr_state;

#if defined (CONFIG_MACH_MSM8960_FX1)
	u8  b_need_upgrade = 0;
	u8  config_id_ver;
	u8  fw_image_config_id_ver;
	long  config_id;
	long  fw_image_config_id;
	hw_rev_type board_type = 0;
#endif
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func->fw_upgrade == NULL) {
		TOUCH_INFO_MSG("There is no specific firmware upgrade function\n");
		goto out;
	}

#if defined (CONFIG_MACH_MSM8960_L1)
	printk("[TOUCH] Image FW Ver : [%s]\n", ts->fw_info.fw_img_config_id);
	printk("[TOUCH] Device FW Ver : [%s]\n",ts->fw_info.fw_config_id);
	printk("[TOUCH] IC ID : %s\n", ts->fw_info.product_id);
	printk("[TOUCH] Image ID : %s\n", ts->fw_info.fw_image_product_id);
	printk("[TOUCH] force_upgrade : [%d]\n", ts->fw_upgrade.fw_force_upgrade);
#else
	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("fw_rev[%d:%d] product_id[%s:%s] force_upgrade[%d]\n",
						ts->fw_info.fw_rev, ts->fw_info.fw_image_rev,
						ts->fw_info.product_id, ts->fw_info.fw_image_product_id,
						ts->fw_upgrade.fw_force_upgrade);
#endif

#if defined (CONFIG_MACH_MSM8960_L1)
	if (((0==strncmp(ts->fw_info.fw_img_config_id, ts->fw_info.fw_config_id, 4))
			/*|| strncmp(ts->fw_info.product_id , ts->fw_info.fw_image_product_id, 10)*/)
			&& !ts->fw_upgrade.fw_force_upgrade){
#if defined (CONFIG_MACH_MSM8960_L1m)
		synaptics_ts_classdev_register();
#endif
		TOUCH_INFO_MSG("FW-upgrade is not executed\n");
		goto out;
	}
#elif defined(CONFIG_MACH_MSM8960_FX1)
	/* LGE_CHANGE_S 20120710 mystery184.kim@lge.com S7020 firmware upgrade
        *  formal version :  S001, S002, ... ,S00N
	 *  test   version :  E001, E002, ... ,E00N
	 */

	// check the version( S: formal version, E: test version)
	// fw_image_config_id : build image ver, fw_config_id : current ver in touch IC

	printk(KERN_ERR "[Touch] FX1 Touch IC Firmware UPGRADE!!! \n");
	board_type = lge_get_board_revno();
	printk(KERN_ERR "[Touch] Current Board Revision : %d\n", board_type);
	if(board_type == HW_REV_A) {
	  #if defined(CONFIG_MACH_MSM8960_FX1S) || defined(CONFIG_MACH_MSM8960_FX1SU) || defined(CONFIG_MACH_MSM8960_FX1T) || defined(CONFIG_MACH_MSM8960_FX1A)
	    goto out;
	  #elif defined(CONFIG_MACH_MSM8960_FX1SK)
	    printk(KERN_ERR "[Touch] This is FX1S SKT Rev.A board\n");
	  #endif
	  }
	if(board_type >= HW_REV_B){
		#if defined(CONFIG_MACH_MSM8960_FX1S) || defined(CONFIG_MACH_MSM8960_FX1SU) || defined(CONFIG_MACH_MSM8960_FX1T) || defined(CONFIG_MACH_MSM8960_FX1A)
			// Sprint, USC, TMUS need to execute for fw upgrade
			printk(KERN_ERR "[Touch] This is Sprint, USC, Tmus Rev.B board\n");
		#elif defined(CONFIG_MACH_MSM8960_FX1SK)
			printk(KERN_ERR "[Touch] This is FX1S SKT Rev.B board\n");
	    #else
			//else not execute upgrade
			printk(KERN_ERR "[Touch] This is not FX1S board\n");
			goto out;
		#endif
	}
	
	if(ts->fw_info.fw_config_id[0] == 'S')
	   config_id_ver = CONFIG_ID_FORMAL_VER;
	else if(ts->fw_info.fw_config_id[0] == 'E')
	   config_id_ver = CONFIG_ID_TEST_VER;
	else
	   config_id_ver = CONFIG_ID_NONE;

	if(ts->fw_info.fw_img_config_id[0] == 'S')
	   fw_image_config_id_ver = CONFIG_ID_FORMAL_VER;
	else if(ts->fw_info.fw_img_config_id[0] == 'E')
	   fw_image_config_id_ver = CONFIG_ID_TEST_VER;
	else
	   fw_image_config_id_ver = CONFIG_ID_NONE;

	// to make config_id numeric value to compare id easily
	fw_image_config_id	= simple_strtol(&ts->fw_info.fw_img_config_id[1],NULL,10);
	config_id			= simple_strtol(&ts->fw_info.fw_config_id[1],NULL,10);
	TOUCH_INFO_MSG("config_id(current) :%ld fw_image_config_id(image) :%ld\n",
		       config_id,fw_image_config_id);

	printk(KERN_ERR "[Touch] version= config_id :%ld fw_image_config_id:%ld\n", config_id, fw_image_config_id);

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("config_id_ver[ic:%d/fw:%d] config_id[ic:%s/fw:%s] config_id_int[ic:%ld/fw:%ld]\n",
						config_id_ver, fw_image_config_id_ver,
						ts->fw_info.fw_config_id,	ts->fw_info.fw_img_config_id,
						config_id, fw_image_config_id);


	// check whether upgrading firmware is able or not
	if((config_id_ver == fw_image_config_id_ver) && (config_id < fw_image_config_id)){
		printk(KERN_ERR "[Touch] FX1 Touch IC Firmware case 1(image ver > current ver)!!! \n");
	    b_need_upgrade = 1;
	}
	else if ((config_id_ver != fw_image_config_id_ver)&& (fw_image_config_id_ver != CONFIG_ID_NONE )) // for test version
	{
		b_need_upgrade = 1;
		printk(KERN_ERR "[Touch] FX1 Touch IC Firmware case 2!!! \n");
	}
	else if ((config_id_ver == CONFIG_ID_NONE) && (config_id < fw_image_config_id))
	{
	    b_need_upgrade = 1;
		printk(KERN_ERR "[Touch] FX1 Touch IC Firmware case 3!!! \n");
	}
/*	else if (!strncmp(ts->fw_info.product_id , "PLG126:S7020", 12))
	    b_need_upgrade = 1; */
	else
	{
	    b_need_upgrade = 0;
		printk(KERN_ERR "[Touch] FX1 Touch IC Firmware case 4!!! \n");
	}

	if(!b_need_upgrade && !ts->fw_upgrade.fw_force_upgrade){
	    TOUCH_INFO_MSG("FW-upgrade is not executed\n");
	    goto out;
	}
	TOUCH_INFO_MSG("FW-upgrade is executed\n");
	/* LGE_CHANGE_E 20120710 mystery184.kim@lge.com S7020 firmware upgrade  */


#else
	if (((ts->fw_info.fw_rev >= ts->fw_info.fw_image_rev
			&& ts->fw_info.fw_rev < 100)	/* test revision should be over 100 */
			|| strncmp(ts->fw_info.product_id , ts->fw_info.fw_image_product_id, 10))
			&& !ts->fw_upgrade.fw_force_upgrade){
		TOUCH_INFO_MSG("FW-upgrade is not executed\n");
		goto out;
	}
#endif

	ts->fw_upgrade.is_downloading = UNDER_DOWNLOADING;

	touch_test_dev = ts;



	if (ts->curr_pwr_state == POWER_ON || ts->curr_pwr_state == POWER_WAKE) {
		if (ts->pdata->role->operation_mode)
			disable_irq(ts->client->irq);
		else
			hrtimer_cancel(&ts->timer);
	}

	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}

	if (ts->curr_pwr_state == POWER_OFF) {
		touch_power_cntl(ts, POWER_ON);
		msleep(ts->pdata->role->booting_delay);
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W upgrade - Start\n");

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_START]);
#endif

	if (touch_device_func->fw_upgrade(ts->client, ts->fw_upgrade.fw_path) < 0) {
		TOUCH_ERR_MSG("Firmware upgrade was failed\n");
		goto err_out;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);
#endif

	touch_power_cntl(ts, POWER_OFF);

	if (saved_state != POWER_OFF) {
		touch_power_cntl(ts, POWER_ON);
		msleep(ts->pdata->role->booting_delay);

		if (ts->pdata->role->operation_mode)
			enable_irq(ts->client->irq);
		else
			hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

		touch_ic_init(ts);

		if(saved_state == POWER_WAKE || saved_state == POWER_SLEEP)
			touch_power_cntl(ts, saved_state);
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE |DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W upgrade - Finish\n");

#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1m)
	synaptics_ts_classdev_register();
#endif

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);

	if (touch_time_debug_mask & DEBUG_TIME_FW_UPGRADE
			|| touch_time_debug_mask & DEBUG_TIME_PROFILE_ALL) {
		TOUCH_INFO_MSG("FW upgrade time is under %3lu.%06lusec\n",
				get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_sec, t_debug[TIME_FW_UPGRADE_START].tv_sec),
				get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_usec, t_debug[TIME_FW_UPGRADE_START].tv_usec));
	}
#endif

	goto out;

err_out:
	safety_reset(ts);
	touch_ic_init(ts);

out:
	memset(&ts->fw_upgrade, 0, sizeof(ts->fw_upgrade));
#if defined(CONFIG_MACH_MSM8960_FX1)
	touch_ic_init(ts);
#endif
	return;
}

/* touch_irq_handler
 *
 * When Interrupt occurs, it will be called before touch_thread_irq_handler.
 *
 * return
 * IRQ_HANDLED: touch_thread_irq_handler will not be called.
 * IRQ_WAKE_THREAD: touch_thread_irq_handler will be called.
 */
static irqreturn_t touch_irq_handler(int irq, void *dev_id)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

	if (unlikely(atomic_read(&ts->device_init) != 1))
		return IRQ_HANDLED;

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_ISR_START]);
#endif

	atomic_inc(&ts->next_work);
	return IRQ_WAKE_THREAD;
}

/* touch_thread_irq_handler
 *
 * 1. disable irq.
 * 2. enqueue the new work.
 * 3. enalbe irq.
 */
static irqreturn_t touch_thread_irq_handler(int irq, void *dev_id)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_THREAD_ISR_START]);
#endif

	/* disable interrupts to stabilize */
#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1)
	if(block_interrupt == false){
#elif defined(CONFIG_TS_INFO_CLASS) 
	#if defined(CONFIG_MACH_MSM8960_FX1S) || defined(CONFIG_MACH_MSM8960_FX1SU) || defined(CONFIG_MACH_MSM8960_FX1SK) 
		if(block_interrupt == false){
	#endif
#endif
		disable_irq_nosync(ts->client->irq);
		queue_work(touch_wq, &ts->work);
		enable_irq(ts->client->irq);
#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1)
	}
#elif defined(CONFIG_TS_INFO_CLASS) 
	#if defined(CONFIG_MACH_MSM8960_FX1S) || defined(CONFIG_MACH_MSM8960_FX1SU) || defined(CONFIG_MACH_MSM8960_FX1SK) 
		}
	#endif
#endif
	return IRQ_HANDLED;
}

/* touch_timer_handler
 *
 * it will be called when timer interrupt occurs.
 */
static enum hrtimer_restart touch_timer_handler(struct hrtimer *timer)
{
	struct lge_touch_data *ts =
			container_of(timer, struct lge_touch_data, timer);

	atomic_inc(&ts->next_work);
	queue_work(touch_wq, &ts->work);
	hrtimer_start(&ts->timer,
			ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

/* check_platform_data
 *
 * check-list
 * 1. Null Pointer
 * 2. lcd, touch screen size
 * 3. button support
 * 4. operation mode
 * 5. power module
 * 6. report period
 */
static int check_platform_data(struct touch_platform_data *pdata)
{
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (!pdata)
		return -1;

	if (!pdata->caps || !pdata->role || !pdata->pwr)
		return -1;

	if (!pdata->caps->lcd_x || !pdata->caps->lcd_y || !pdata->caps->x_max || !pdata->caps->y_max) {
		TOUCH_ERR_MSG("lcd_x, lcd_y, x_max, y_max are should be defined\n");
		return -1;
	}

	if (pdata->caps->button_support) {
		if (!pdata->role->key_type) {
			TOUCH_ERR_MSG("button_support = 1, but key_type is not defined\n");
			return -1;
		}
		if (!pdata->caps->y_button_boundary)
			pdata->caps->y_button_boundary =
				(pdata->caps->lcd_y * pdata->caps->x_max) / pdata->caps->lcd_x;
		if (pdata->caps->button_margin < 0 || pdata->caps->button_margin > 49) {
			TOUCH_ERR_MSG("0 < button_margin < 49\n");
			return -1;
		}
	}

	if (!pdata->role->operation_mode) {
		if (!pdata->role->report_period) {
			TOUCH_ERR_MSG("polling mode needs report_period\n");
			return -1;
		}
	}

	if (pdata->role->suspend_pwr == POWER_OFF || pdata->role->suspend_pwr == POWER_SLEEP){
		if (pdata->role->suspend_pwr == POWER_OFF)
			pdata->role->resume_pwr = POWER_ON;
		else
			pdata->role->resume_pwr = POWER_WAKE;
	}
	else {
		TOUCH_ERR_MSG("suspend_pwr = POWER_OFF or POWER_SLEEP\n");
	}

	if (pdata->pwr->use_regulator) {
		if (!pdata->pwr->vdd[0] || !pdata->pwr->vio[0]) {
			TOUCH_ERR_MSG("you should assign the name of vdd and vio\n");
			return -1;
		}
	}
	else{
		if (!pdata->pwr->power) {
			TOUCH_ERR_MSG("you should assign the power-control-function\n");
			return -1;
		}
	}

	if(pdata->role->report_period == 0)
		pdata->role->report_period = 10000000;

	return 0;
}

/* get_section
 *
 * it calculates the area of touch-key, automatically.
 */
void get_section(struct section_info* sc, struct touch_platform_data *pdata)
{
	int i;

	sc->panel.left = 0;
	sc->panel.right = pdata->caps->x_max;
	sc->panel.top = 0;
	sc->panel.bottom = pdata->caps->y_button_boundary;

	sc->b_width  = pdata->caps->x_max / pdata->caps->number_of_button;
	sc->b_margin = sc->b_width * pdata->caps->button_margin / 100;
	sc->b_inner_width = sc->b_width - (2*sc->b_margin);
	sc->b_height = pdata->caps->y_max - pdata->caps->y_button_boundary;
	sc->b_num = pdata->caps->number_of_button;

	for(i=0; i<sc->b_num; i++){
		sc->button[i].left = i * (pdata->caps->x_max / pdata->caps->number_of_button) + sc->b_margin;
		sc->button[i].right = sc->button[i].left + sc->b_inner_width;
		sc->button[i].top = pdata->caps->y_button_boundary + 1;
		sc->button[i].bottom = pdata->caps->y_max;

		sc->button_cancel[i].left = sc->button[i].left - (2*sc->b_margin) >= 0 ?
				sc->button[i].left - (2*sc->b_margin) : 0;
		sc->button_cancel[i].right = sc->button[i].right + (2*sc->b_margin) <= pdata->caps->x_max ?
				sc->button[i].right + (2*sc->b_margin) : pdata->caps->x_max;
		sc->button_cancel[i].top = sc->button[i].top;
		sc->button_cancel[i].bottom = sc->button[i].bottom;

		sc->b_name[i] = pdata->caps->button_name[i];
	}
}

/* Sysfs
 *
 * For debugging easily, we added some sysfs.
 */
static ssize_t show_platform_data(struct lge_touch_data *ts, char *buf)
{
	struct touch_platform_data *pdata = ts->pdata;
	int ret = 0;

	ret = sprintf(buf, "====== Platform data ======\n");
	ret += sprintf(buf+ret, "int_pin[%d] reset_pin[%d]\n", pdata->int_pin, pdata->reset_pin);
	ret += sprintf(buf+ret, "caps:\n");
	ret += sprintf(buf+ret, "\tbutton_support        = %d\n", pdata->caps->button_support);
	ret += sprintf(buf+ret, "\ty_button_boundary     = %d\n", pdata->caps->y_button_boundary);
	ret += sprintf(buf+ret, "\tbutton_margin         = %d\n", pdata->caps->button_margin);
	ret += sprintf(buf+ret, "\tnumber_of_button      = %d\n", pdata->caps->number_of_button);
#if defined(CONFIG_MACH_MSM8960_FX1)
	ret += sprintf(buf+ret, "\tbutton_name           = %d, %d\n", pdata->caps->button_name[0],
			pdata->caps->button_name[1]);
#else
	ret += sprintf(buf+ret, "\tbutton_name           = %d, %d, %d, %d\n", pdata->caps->button_name[0],
			pdata->caps->button_name[1], pdata->caps->button_name[2], pdata->caps->button_name[3]);
#endif
	ret += sprintf(buf+ret, "\tis_width_supported    = %d\n", pdata->caps->is_width_supported);
	ret += sprintf(buf+ret, "\tis_pressure_supported = %d\n", pdata->caps->is_pressure_supported);
	ret += sprintf(buf+ret, "\tis_id_supported       = %d\n", pdata->caps->is_id_supported);
	ret += sprintf(buf+ret, "\tmax_width             = %d\n", pdata->caps->max_width);
	ret += sprintf(buf+ret, "\tmax_pressure          = %d\n", pdata->caps->max_pressure);
	ret += sprintf(buf+ret, "\tmax_id                = %d\n", pdata->caps->max_id);
	ret += sprintf(buf+ret, "\tx_max                 = %d\n", pdata->caps->x_max);
	ret += sprintf(buf+ret, "\ty_max                 = %d\n", pdata->caps->y_max);
	ret += sprintf(buf+ret, "\tlcd_x                 = %d\n", pdata->caps->lcd_x);
	ret += sprintf(buf+ret, "\tlcd_y                 = %d\n", pdata->caps->lcd_y);
	ret += sprintf(buf+ret, "role:\n");
	ret += sprintf(buf+ret, "\toperation_mode        = %d\n", pdata->role->operation_mode);
	ret += sprintf(buf+ret, "\tkey_type              = %d\n", pdata->role->key_type);
	ret += sprintf(buf+ret, "\treport_mode           = %d\n", pdata->role->report_mode);
	ret += sprintf(buf+ret, "\tdelta_pos_threshold   = %d\n", pdata->role->delta_pos_threshold);
	ret += sprintf(buf+ret, "\torientation           = %d\n", pdata->role->orientation);
	ret += sprintf(buf+ret, "\treport_period         = %d\n", pdata->role->report_period);
	ret += sprintf(buf+ret, "\tbooting_delay         = %d\n", pdata->role->booting_delay);
	ret += sprintf(buf+ret, "\treset_delay           = %d\n", pdata->role->reset_delay);
	ret += sprintf(buf+ret, "\tsuspend_pwr           = %d\n", pdata->role->suspend_pwr);
	ret += sprintf(buf+ret, "\tresume_pwr            = %d\n", pdata->role->resume_pwr);
	ret += sprintf(buf+ret, "\tirqflags              = 0x%lx\n", pdata->role->irqflags);
	ret += sprintf(buf+ret, "pwr:\n");
	ret += sprintf(buf+ret, "\tuse_regulator         = %d\n", pdata->pwr->use_regulator);
	ret += sprintf(buf+ret, "\tvdd                   = %s\n", pdata->pwr->vdd);
	ret += sprintf(buf+ret, "\tvdd_voltage           = %d\n", pdata->pwr->vdd_voltage);
	ret += sprintf(buf+ret, "\tvio                   = %s\n", pdata->pwr->vio);
	ret += sprintf(buf+ret, "\tvio_voltage           = %d\n", pdata->pwr->vio_voltage);
	ret += sprintf(buf+ret, "\tpower                 = %s\n", pdata->pwr->power ? "YES" : "NO");
	return ret;
}

/* show_fw_info
 *
 * show only the firmware information
 */
static ssize_t show_fw_info(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "====== Firmware Info ======\n");
	ret += sprintf(buf+ret, "manufacturer_id  = %d\n", ts->fw_info.manufacturer_id);
	ret += sprintf(buf+ret, "product_id       = %s\n", ts->fw_info.product_id);
	ret += sprintf(buf+ret, "fw_rev           = %d\n", ts->fw_info.fw_rev);

	return ret;
}

/* store_fw_upgrade
 *
 * User can upgrade firmware, anytime, using this module.
 * Also, user can use both binary-img(SDcard) and header-file(Kernel image).
 */
static ssize_t store_fw_upgrade(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value = 0;
	int repeat = 0;
	char path[256] = {0};

	sscanf(buf, "%d %s", &value, path);

	printk(KERN_INFO "\n");
	TOUCH_INFO_MSG("Firmware image path: %s\n", path[0] != 0 ? path : "Internal");

	if (value) {
		for(repeat = 0; repeat < value; repeat++) {
			/* sync for n-th repeat test */
			while(ts->fw_upgrade.is_downloading);

			msleep(ts->pdata->role->booting_delay * 2);
			printk(KERN_INFO "\n");
			TOUCH_INFO_MSG("Firmware image upgrade: No.%d", repeat+1);

			/* for n-th repeat test - because ts->fw_upgrade is setted 0 after FW upgrade */
			memcpy(ts->fw_upgrade.fw_path, path, sizeof(ts->fw_upgrade.fw_path)-1);

			/* set downloading flag for sync for n-th test */
			ts->fw_upgrade.is_downloading = UNDER_DOWNLOADING;
			ts->fw_upgrade.fw_force_upgrade = 1;

			queue_work(touch_wq, &ts->work_fw_upgrade);
		}

		/* sync for fw_upgrade test */
		while(ts->fw_upgrade.is_downloading);
	}

	return count;
}

/* show_fw_ver
 *
 * show only firmware version.
 * It will be used for AT-COMMAND
 */
static ssize_t show_fw_ver(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
#if defined(CONFIG_LGE_TOUCH_SYNAPTICS_325)
	ret = sprintf(buf, "%c%c%c%c\n", ts->fw_info.fw_config_id[0],ts->fw_info.fw_config_id[1],
						   		   ts->fw_info.fw_config_id[2],ts->fw_info.fw_config_id[3]);
#else
	ret = sprintf(buf, "%d\n", ts->fw_info.fw_rev);
#endif
	return ret;
}

/* show_section_info
 *
 * User can check the information of touch-key-area, using this module.
 */
static ssize_t show_section_info(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	int i;

	ret = sprintf(buf, "====== Section Info ======\n");

		ret += sprintf(buf+ret, "Panel = [%4d,%4d,%4d,%4d]\n", ts->st_info.panel.left,
				ts->st_info.panel.right, ts->st_info.panel.top, ts->st_info.panel.bottom);

	if (ts->pdata->role->key_type == TOUCH_SOFT_KEY) {
		for(i=0; i<ts->st_info.b_num; i++){
			ret += sprintf(buf+ret, "Button[%4d] = [%4d,%4d,%4d,%4d]\n",
					ts->st_info.b_name[i], ts->st_info.button[i].left,
					ts->st_info.button[i].right, ts->st_info.button[i].top, ts->st_info.button[i].bottom);
		}
		for(i=0; i<ts->st_info.b_num; i++){
			ret += sprintf(buf+ret, "Button_cancel[%4d] = [%4d,%4d,%4d,%4d]\n",
					ts->st_info.b_name[i], ts->st_info.button_cancel[i].left,
					ts->st_info.button_cancel[i].right, ts->st_info.button_cancel[i].top,
					ts->st_info.button_cancel[i].bottom);
		}
		ret += sprintf(buf+ret, "button_width        = %d\n", ts->st_info.b_width);
		ret += sprintf(buf+ret, "button_height       = %d\n", ts->st_info.b_height);
		ret += sprintf(buf+ret, "button_inner_width  = %d\n", ts->st_info.b_inner_width);
		ret += sprintf(buf+ret, "button_margin       = %d\n", ts->st_info.b_margin);
		ret += sprintf(buf+ret, "button_number       = %d\n", ts->st_info.b_num);
	}

	return ret;
}

/* store_ts_reset
 *
 * Reset the touch IC.
 */
static ssize_t store_ts_reset(struct lge_touch_data *ts, const char *buf, size_t count)
{
	unsigned char string[5];
	u8 saved_state = ts->curr_pwr_state;
	int ret = 0;

	sscanf(buf, "%s", string);

	if (ts->pdata->role->operation_mode)
		disable_irq_nosync(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);

	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}

	cancel_work_sync(&ts->work);
	cancel_delayed_work_sync(&ts->work_init);
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	release_all_ts_event(ts);

	if (saved_state == POWER_ON || saved_state == POWER_WAKE) {
		if (!strncmp(string, "soft", 4)) {
			if(touch_device_func->ic_ctrl)
				touch_device_func->ic_ctrl(ts->client, IC_CTRL_RESET_CMD, 0);
			else
				TOUCH_INFO_MSG("There is no specific IC control function\n");
		} else if (!strncmp(string, "pin", 3)) {
			if (ts->pdata->reset_pin > 0) {
				gpio_set_value(ts->pdata->reset_pin, 0);
				msleep(ts->pdata->role->reset_delay);
				gpio_set_value(ts->pdata->reset_pin, 1);
			} else
				TOUCH_INFO_MSG("There is no reset pin\n");
		} else if (!strncmp(string, "vdd", 3)) {
			touch_power_cntl(ts, POWER_OFF);
			touch_power_cntl(ts, POWER_ON);
		}
		else {
			TOUCH_INFO_MSG("Usage: echo [soft | pin | vdd] > ts_reset\n");
			TOUCH_INFO_MSG(" - soft : reset using IC register setting\n");
			TOUCH_INFO_MSG(" - soft : reset using reset pin\n");
			TOUCH_INFO_MSG(" - hard : reset using VDD\n");
		}

		if (ret < 0) {
			TOUCH_ERR_MSG("reset fail\n");
		}else {
			atomic_set(&ts->device_init, 0);
		}

		msleep(ts->pdata->role->booting_delay);

	} else
		TOUCH_INFO_MSG("Touch is suspend state. Don't need reset\n");

	if (ts->pdata->role->operation_mode)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer,
				ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	if (saved_state == POWER_ON || saved_state == POWER_WAKE)
		touch_ic_init(ts);

	return count;
}

/* ic_register_ctrl
 *
 * User can see any register of touch_IC
 */
static ssize_t ic_register_ctrl(struct lge_touch_data *ts, const char *buf, size_t count)
{
	unsigned char string[6];
	int reg = 0;
	int value = 0;
	int ret = 0;
	u32 write_data;

	sscanf(buf, "%s %d %d", string, &reg, &value);

	if(touch_device_func->ic_ctrl) {
		if(ts->curr_pwr_state == POWER_ON || ts->curr_pwr_state == POWER_WAKE){
			if (!strncmp(string, "read", 4)) {
				do {
					ret = touch_device_func->ic_ctrl(ts->client, IC_CTRL_READ, reg);
					if(ret >= 0) {
						TOUCH_INFO_MSG("register[0x%x] = 0x%x\n", reg, ret);
					} else {
						TOUCH_INFO_MSG("cannot read register[0x%x]\n", reg);
					}
					reg++;
				} while (--value > 0);
			} else if (!strncmp(string, "write", 4)) {
				write_data = ((0xFFF & reg) << 8) | (0xFF & value);

				if((0xFF & value) == 64)
					ts->pdata->role->ghost_detection_enable = 0;
				else if((0xFF & value) == 192)
					ts->pdata->role->ghost_detection_enable = 1;

				ret = touch_device_func->ic_ctrl(ts->client, IC_CTRL_WRITE, write_data);
				if(ret >= 0) {
					TOUCH_INFO_MSG("register[0x%x] is set to 0x%x\n", reg, value);
				} else {
					TOUCH_INFO_MSG("cannot write register[0x%x]\n", reg);
				}
			} else{
				TOUCH_INFO_MSG("Usage: echo [read | write] reg_num value > ic_rw\n");
				TOUCH_INFO_MSG(" - reg_num : register address\n");
				TOUCH_INFO_MSG(" - value [read] : number of register starting form reg_num\n");
				TOUCH_INFO_MSG(" - value [write] : set value into reg_num\n");
			}
		}
		else
			TOUCH_INFO_MSG("state=[suspend]. we cannot use I2C, now\n");
	} else
		TOUCH_INFO_MSG("There is no specific IC control function\n");

	return count;
}

/* store_keyguard_info
 *
 * This function is related with Keyguard in framework.
 * We can prevent the ghost-finger problem, using this function.
 * If you need more information, see the 'ghost_finger_solution' function.
 */
static ssize_t store_keyguard_info(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	if(value == KEYGUARD_ENABLE)
	{
#if defined(CONFIG_MACH_MSM8960_L1) || defined(CONFIG_MACH_MSM8960_FX1)
#if defined(CONFIG_MACH_MSM8960_FX1)
		ts->gf_ctrl.stage = GHOST_STAGE_2;
#else
		ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2;
#endif
#else
		ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2 | GHOST_STAGE_3;
#endif
		printk("[TOUCH] Key guard value : %d\n[TOUCH] Ghost stage : %d", value, ts->gf_ctrl.stage);
	}
	else if(value == KEYGUARD_RESERVED)
	{
#if defined(CONFIG_MACH_MSM8960_L1) || defined(CONFIG_MACH_MSM8960_FX1)
		ts->gf_ctrl.stage = GHOST_STAGE_CLEAR;
#else
		ts->gf_ctrl.stage = ~GHOST_STAGE_2;
#endif
		printk("[TOUCH] Key guard value : %d\n[TOUCH] Ghost stage : %d", value, ts->gf_ctrl.stage);
	}
	if (touch_debug_mask & DEBUG_GHOST){
		TOUCH_INFO_MSG("ghost_stage = 0x%x\n", ts->gf_ctrl.stage);
		if(value == KEYGUARD_RESERVED)
			TOUCH_INFO_MSG("ghost_stage2 : cleared[0x%x]\n", ts->gf_ctrl.stage);
	}

	return count;
}

/* show_virtual_key
 *
 * /sys/devices/virtual/input/virtualkeys
 * for use the virtual_key supported by Google
 *
 * 0x01:key_code:center_x:center_y:x_width:y_width
 * 0x01 = start_point
 */
static ssize_t show_virtual_key(struct lge_touch_data *ts, char *buf)
{
	int i=0;
	int ret = 0;

	u32 center_x = (ts->pdata->caps->x_max / (ts->pdata->caps->number_of_button * 2));
	u32 center_y = (ts->pdata->caps->y_button_boundary + (ts->st_info.b_height / 2));

	/* Register sysfs for virtualkeys*/
	if (ts->pdata->caps->button_support && ts->pdata->role->key_type == VIRTUAL_KEY) {

		for(i=0; i<ts->pdata->caps->number_of_button; i++) {
			if (i)
				ret += sprintf(buf+ret, ":");
			ret += sprintf(buf+ret, "0x01:%d:%d:%d:%d:%d", ts->pdata->caps->button_name[i],
						center_x * (i*2 + 1), center_y,
						ts->st_info.b_inner_width, ts->st_info.b_height);
		}
		ret += sprintf(buf+ret, "\n");
		return ret;
	} else
		return -ENOSYS;

}

static ssize_t store_jitter_solution(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	memset(&ts->jitter_filter, 0, sizeof(ts->jitter_filter));

	ret = sscanf(buf, "%d %d",
				&ts->pdata->role->jitter_filter_enable,
				&ts->jitter_filter.adjust_margin);

	return count;
}

static ssize_t store_accuracy_solution(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	memset(&ts->accuracy_filter, 0, sizeof(ts->accuracy_filter));

	ret = sscanf(buf, "%d %d %d %d %d %d %d",
				&ts->pdata->role->accuracy_filter_enable,
				&ts->accuracy_filter.ignore_pressure_gap,
				&ts->accuracy_filter.delta_max,
				&ts->accuracy_filter.touch_max_count,
				&ts->accuracy_filter.max_pressure,
				&ts->accuracy_filter.direction_count,
				&ts->accuracy_filter.time_to_max_pressure);

	return count;
}

static ssize_t store_ghost_detection_enable(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;
	int ghost_detection_enable = 0;

	ret = sscanf(buf, "%d",
				&ghost_detection_enable
				);

	ts->pdata->role->ghost_detection_enable = ghost_detection_enable;
	return count;
}

#if defined(CONFIG_MACH_MSM8960_L1) || defined(CONFIG_MACH_MSM8960_FX1)
/* store_incoming_call
 *
 * This function is related with Keyguard in framework.
 * We can prevent the ghost-finger problem, using this function.
 * If you need more information, see the 'ghost_finger_solution' function.
*/
static ssize_t store_incoming_call(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	if(value == INCOMIMG_CALL_TOUCH)
		ts->gf_ctrl.incoming_call = 1;
	else if(value == INCOMIMG_CALL_RESERVED)
		ts->gf_ctrl.incoming_call = 0;

	if (touch_debug_mask & DEBUG_GHOST){
		TOUCH_INFO_MSG("incoming_call = %x\n", ts->gf_ctrl.incoming_call);
		if(value == INCOMIMG_CALL_RESERVED)
			TOUCH_INFO_MSG("incoming_call : cleared[%x]\n", ts->gf_ctrl.incoming_call);
	}

	return count;
}
#endif

#if defined(CONFIG_MACH_MSM8960_FX1)

#ifdef SYNAPTICS_TOUCH_F54

static ssize_t show_F54(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	ret += F54_FullRawCap(ts->client, 3, buf);
	mdelay(100);
	ret += F54_HighResistance(ts->client, buf + ret);
	return ret;
}

static ssize_t store_F54(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	char *temp = NULL;
	sscanf(buf, "%d", &value);

	switch(value){
		case 0:
			input_report_key(ts->input_dev, 0x2FF, BUTTON_PRESSED);
			input_sync(ts->input_dev);
			printk("F54 Test Start!\n");
			input_report_key(ts->input_dev, 0x2FF, BUTTON_RELEASED);
			input_sync(ts->input_dev);
			break;
		case 1:
			F54_FullRawCap(ts->client, 3, temp);
			break;
		case 2:
			F54_FullRawCap(ts->client, 4, temp);
			break;
		case 3:
			F54_TxToTxReport(ts->client);
			break;
		case 4:
			F54_TxToGndReport(ts->client);
			break;
		case 5:
			F54_RxToRxReport(ts->client);
			break;
		case 6:
			F54_HighResistance(ts->client, temp);
			break;
		case 7:
			F54_RxOpenReport(ts->client);
			break;
		case 8:
			F54_TxOpenReport(ts->client);
			break;
		default:
			TOUCH_INFO_MSG("Usage: echo [1-8] > f54\n");
			TOUCH_INFO_MSG(" - 1 : baseline\n");
			TOUCH_INFO_MSG(" - 2 : delta image\n");
			TOUCH_INFO_MSG(" - 3 : Tx-to-Tx report\n");
			TOUCH_INFO_MSG(" - 4 : Tx-to-Ground report\n");
			TOUCH_INFO_MSG(" - 5 : Rx-to-Rx report\n");
			TOUCH_INFO_MSG(" - 6 : HighResistance\n");
			TOUCH_INFO_MSG(" - 7 : Rx open report\n");
			TOUCH_INFO_MSG(" - 8 : Tx open report\n");
			break;
	}
	return count;

}

#endif
#endif

static LGE_TOUCH_ATTR(platform_data, S_IRUGO | S_IWUSR, show_platform_data, NULL);
static LGE_TOUCH_ATTR(firmware, S_IRUGO | S_IWUSR, show_fw_info, store_fw_upgrade);
static LGE_TOUCH_ATTR(fw_ver, S_IRUGO | S_IWUSR, show_fw_ver, NULL);
static LGE_TOUCH_ATTR(section, S_IRUGO | S_IWUSR, show_section_info, NULL);
static LGE_TOUCH_ATTR(reset, S_IRUGO | S_IWUSR, NULL, store_ts_reset);
static LGE_TOUCH_ATTR(ic_rw, S_IRUSR | S_IRGRP | S_IWUSR, NULL, ic_register_ctrl);
static LGE_TOUCH_ATTR(keyguard, S_IRUGO | S_IWUSR, NULL, store_keyguard_info);
static LGE_TOUCH_ATTR(virtualkeys, S_IRUGO | S_IWUSR, show_virtual_key, NULL);
static LGE_TOUCH_ATTR(jitter, S_IRUGO | S_IWUSR, NULL, store_jitter_solution);
static LGE_TOUCH_ATTR(accuracy, S_IRUGO | S_IWUSR, NULL, store_accuracy_solution);
static LGE_TOUCH_ATTR(ghost_detection_enable, S_IRUGO | S_IWUSR, NULL, store_ghost_detection_enable);

#if defined(CONFIG_MACH_MSM8960_L1)
static LGE_TOUCH_ATTR(send_touch_down, S_IRUGO | S_IWUSR, NULL, store_send_touch_down);
static LGE_TOUCH_ATTR(send_touch_up,   S_IRUGO | S_IWUSR, NULL, store_send_touch_up);
static LGE_TOUCH_ATTR(incoming_call, S_IRUGO | S_IWUSR, NULL, store_incoming_call);
#elif defined(CONFIG_MACH_MSM8960_FX1)
static LGE_TOUCH_ATTR(f54, S_IRUGO | S_IWUSR, show_F54, store_F54);
static LGE_TOUCH_ATTR(incoming_call, S_IRUGO | S_IWUSR, NULL, store_incoming_call);
#endif

static struct attribute *lge_touch_attribute_list[] = {
	&lge_touch_attr_platform_data.attr,
	&lge_touch_attr_firmware.attr,
	&lge_touch_attr_fw_ver.attr,
	&lge_touch_attr_section.attr,
	&lge_touch_attr_reset.attr,
	&lge_touch_attr_ic_rw.attr,
	&lge_touch_attr_keyguard.attr,
	&lge_touch_attr_virtualkeys.attr,
	&lge_touch_attr_jitter.attr,
	&lge_touch_attr_accuracy.attr,
#if defined(CONFIG_MACH_MSM8960_L1)
	&lge_touch_attr_send_touch_down.attr,
	&lge_touch_attr_send_touch_up.attr,
	&lge_touch_attr_incoming_call.attr,
#elif defined(CONFIG_MACH_MSM8960_FX1)
	&lge_touch_attr_f54.attr,
	&lge_touch_attr_incoming_call.attr,
#endif
	&lge_touch_attr_ghost_detection_enable.attr,
	NULL,
};

/* lge_touch_attr_show / lge_touch_attr_store
 *
 * sysfs bindings for lge_touch
 */
static ssize_t lge_touch_attr_show(struct kobject *lge_touch_kobj, struct attribute *attr,
			     char *buf)
{
	struct lge_touch_data *ts =
			container_of(lge_touch_kobj, struct lge_touch_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->show)
		ret = lge_touch_priv->show(ts, buf);

	return ret;
}

static ssize_t lge_touch_attr_store(struct kobject *lge_touch_kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	struct lge_touch_data *ts =
			container_of(lge_touch_kobj, struct lge_touch_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->store)
		ret = lge_touch_priv->store(ts, buf, count);

	return ret;
}

static const struct sysfs_ops lge_touch_sysfs_ops = {
	.show	= lge_touch_attr_show,
	.store	= lge_touch_attr_store,
};

static struct kobj_type lge_touch_kobj_type = {
	.sysfs_ops		= &lge_touch_sysfs_ops,
	.default_attrs 	= lge_touch_attribute_list,
};

static struct sysdev_class lge_touch_sys_class = {
	.name = LGE_TOUCH_SYNAPTICS_NAME,
};

static struct sys_device lge_touch_sys_device = {
	.id		= 0,
	.cls	= &lge_touch_sys_class,
};

void synaptics_ts_classdev_register(void)
{
#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1m)
        cdev.name = "touchscreen";
        classdev_version[0] = touch_test_dev->fw_info.fw_config_id[0];
        classdev_version[1] = touch_test_dev->fw_info.fw_config_id[1];
        classdev_version[2] = touch_test_dev->fw_info.fw_config_id[2];
        classdev_version[3] = touch_test_dev->fw_info.fw_config_id[3];
	classdev_version[4] = 0;

        cdev.version = classdev_version;
        ts_info_classdev_register(&touch_test_dev->client->dev, &cdev);
#endif
}

static int touch_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lge_touch_data *ts;
	int ret = 0;
	int one_sec = 0;

	printk("[touch] enter touch probe\n");

	/* disable interrupts to stabilize */
#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1)
	block_interrupt = true;
#elif defined(CONFIG_TS_INFO_CLASS) 
	#if defined(CONFIG_MACH_MSM8960_FX1S) || defined(CONFIG_MACH_MSM8960_FX1SU) || defined(CONFIG_MACH_MSM8960_FX1SK) 
		block_interrupt = true;
	#endif
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TOUCH_ERR_MSG("i2c functionality check error\n");
		ret = -EPERM;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct lge_touch_data), GFP_KERNEL);
	if (ts == NULL) {
		TOUCH_ERR_MSG("Can not allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->pdata = client->dev.platform_data;
	ret = check_platform_data(ts->pdata);
	if (ret < 0) {
		TOUCH_ERR_MSG("Can not read platform data\n");
		ret = -EINVAL;
		goto err_assign_platform_data;
	}

	one_sec = 1000000 / (ts->pdata->role->report_period/1000);
	ts->ic_init_err_cnt = 0;
	ts->work_sync_err_cnt = 0;
	ts->gf_ctrl.min_count = 50000 / (ts->pdata->role->report_period/1000);
	ts->gf_ctrl.max_count = one_sec;
	get_section(&ts->st_info, ts->pdata);

	ts->client = client;
	i2c_set_clientdata(client, ts);

	/* Specific device probe */
	if (touch_device_func->probe) {
		ret = touch_device_func->probe(client);
		if (ret < 0) {
			TOUCH_ERR_MSG("specific device probe fail\n");
			goto err_assign_platform_data;
		}
	}

	/* reset pin setting */
	if (ts->pdata->reset_pin > 0){
		ret = gpio_request(ts->pdata->reset_pin, "touch_reset");
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_reset gpio_request\n");
			goto err_assign_platform_data;
		}
		gpio_direction_output(ts->pdata->reset_pin, 1);
	}

	atomic_set(&ts->device_init, 0);

	/* Power on */
	if (touch_power_cntl(ts, POWER_ON) < 0)
		goto err_power_failed;

	msleep(ts->pdata->role->booting_delay);

	/* init work_queue */
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY) {
		INIT_WORK(&ts->work, touch_work_func_a);
		INIT_DELAYED_WORK(&ts->work_touch_lock, touch_lock_func);
	}
	else if (ts->pdata->role->key_type == TOUCH_SOFT_KEY)
		INIT_WORK(&ts->work, touch_work_func_b);
	else
		INIT_WORK(&ts->work, touch_work_func_c);

	INIT_DELAYED_WORK(&ts->work_init, touch_init_func);
	INIT_WORK(&ts->work_fw_upgrade, touch_fw_upgrade_func);

	/* input dev setting */
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		TOUCH_ERR_MSG("Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	/* Auto Test interface */
	touch_test_dev = ts;

	ts->input_dev->name = "synaptics_ts";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#endif

	if (ts->pdata->caps->button_support && ts->pdata->role->key_type != VIRTUAL_KEY) {
		set_bit(EV_KEY, ts->input_dev->evbit);
		for(ret = 0; ret < ts->pdata->caps->number_of_button; ret++) {
			set_bit(ts->pdata->caps->button_name[ret], ts->input_dev->keybit);
		}
	}

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->pdata->caps->x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
			ts->pdata->caps->y_button_boundary
			? ts->pdata->caps->y_button_boundary
			: ts->pdata->caps->y_max,
			0, 0);
	if (ts->pdata->caps->is_pressure_supported)
		input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, ts->pdata->caps->max_pressure, 0, 0);
	if (ts->pdata->caps->is_width_supported) {
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, ts->pdata->caps->max_width, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MINOR, 0, ts->pdata->caps->max_width, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	}
	if (ts->pdata->caps->is_id_supported)
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->pdata->caps->max_id, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret < 0) {
		TOUCH_ERR_MSG("Unable to register %s input device\n",
				ts->input_dev->name);
		goto err_input_register_device_failed;
	}

#if defined(CONFIG_MACH_MSM8960_L1)
		disable_irq(ts->client->irq);
#endif

	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_init(&hr_touch_trigger_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hr_touch_trigger_timer.function = touch_trigger_timer_handler;
	}


	ret = touch_ic_init(ts);
	if(ret == -1)
		goto	err_interrupt_failed;

	/* interrupt mode */
	if (ts->pdata->role->operation_mode) {
		ret = gpio_request(ts->pdata->int_pin, "touch_int");
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_int gpio_request\n");
			goto err_interrupt_failed;
		}
		gpio_direction_input(ts->pdata->int_pin);

		ret = request_threaded_irq(client->irq, touch_irq_handler,
				touch_thread_irq_handler,
				ts->pdata->role->irqflags | IRQF_ONESHOT, client->name, ts);

		if (ret < 0) {
			TOUCH_ERR_MSG("request_irq failed. use polling mode\n");
			goto err_interrupt_failed;
		}
	}
	/* polling mode */
	else{
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = touch_timer_handler;
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);
	}

	ts->gf_ctrl.probe = 1;

	/* Specific device initialization */
//	ret = touch_ic_init(ts);

	//ic_ctrl and fw_upgrade methods should be executed only when touch_ic_init is successful
	//If touch_ic_init is not successful, those methods will be called from the workqueue context
	if (ret == 0) {
#if defined(CONFIG_MACH_MSM8960_L1) || defined(CONFIG_MACH_MSM8960_FX1)
		ts->gf_ctrl.stage = GHOST_STAGE_CLEAR;
#else
		if (touch_device_func->ic_ctrl) {
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0)
			goto err_interrupt_failed;
		}
#endif

	/* Firmware Upgrade Check - use thread for booting time reduction */
		//ts->fw_upgrade.fw_force_upgrade = 1;
		if (touch_device_func->fw_upgrade) {
			queue_work(touch_wq, &ts->work_fw_upgrade);
		}
	}

	/* jitter solution */
	if (ts->pdata->role->jitter_filter_enable){
		ts->jitter_filter.adjust_margin = 50;
	}

	/* accuracy solution */
	if (ts->pdata->role->accuracy_filter_enable){
		ts->accuracy_filter.ignore_pressure_gap = 5;
		ts->accuracy_filter.delta_max = 30;
		ts->accuracy_filter.max_pressure = 255;
		ts->accuracy_filter.time_to_max_pressure = one_sec / 20;
		ts->accuracy_filter.direction_count = one_sec / 6;
		ts->accuracy_filter.touch_max_count = one_sec / 2;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = touch_early_suspend;
	ts->early_suspend.resume = touch_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	/* Register sysfs for making fixed communication path to framework layer */
	ret = sysdev_class_register(&lge_touch_sys_class);
	if (ret < 0) {
		TOUCH_ERR_MSG("sysdev_class_register is failed\n");
		goto err_lge_touch_sys_class_register;
	}

	ret = sysdev_register(&lge_touch_sys_device);
	if (ret < 0) {
		TOUCH_ERR_MSG("sysdev_register is failed\n");
		goto err_lge_touch_sys_dev_register;
	}

	ret = kobject_init_and_add(&ts->lge_touch_kobj, &lge_touch_kobj_type,
			ts->input_dev->dev.kobj.parent,
			"%s", LGE_TOUCH_SYNAPTICS_NAME);
	if (ret < 0) {
		TOUCH_ERR_MSG("kobject_init_and_add is failed\n");
		goto err_lge_touch_sysfs_init_and_add;
	}

	if (likely(touch_debug_mask & DEBUG_BASE_INFO))
		TOUCH_INFO_MSG("Touch driver is initialized\n");

	if (ts->pdata->role->ghost_detection_enable) {
		probe_flag = 1;
		do_gettimeofday(&t_ex_debug[TIME_EX_PROBE]);
	}

	/* disable interrupts to stabilize */
#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1)
	block_interrupt = false;
#elif defined(CONFIG_TS_INFO_CLASS) 
	#if defined(CONFIG_MACH_MSM8960_FX1S) || defined(CONFIG_MACH_MSM8960_FX1SU) || defined(CONFIG_MACH_MSM8960_FX1SK) 
		block_interrupt = false;
	#endif
#endif

	printk("[touch] leave touch probe\n");
	return 0;

err_lge_touch_sysfs_init_and_add:
	kobject_del(&ts->lge_touch_kobj);
err_lge_touch_sys_dev_register:
	sysdev_unregister(&lge_touch_sys_device);
err_lge_touch_sys_class_register:
	sysdev_class_unregister(&lge_touch_sys_class);
	unregister_early_suspend(&ts->early_suspend);
err_interrupt_failed:
	if (ts->pdata->role->operation_mode)
		free_irq(ts->client->irq, ts);
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	touch_power_cntl(ts, POWER_OFF);
err_power_failed:
err_assign_platform_data:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int touch_remove(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	/* Specific device remove */
	if (touch_device_func->remove)
		touch_device_func->remove(ts->client);

	/* Power off */
	touch_power_cntl(ts, POWER_OFF);

	kobject_del(&ts->lge_touch_kobj);
	sysdev_unregister(&lge_touch_sys_device);
	sysdev_class_unregister(&lge_touch_sys_class);

	unregister_early_suspend(&ts->early_suspend);

	if (ts->pdata->role->operation_mode)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);

	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}

	input_unregister_device(ts->input_dev);
#if defined(CONFIG_TS_INFO_CLASS) && defined(CONFIG_MACH_MSM8960_L1m)
       ts_info_classdev_unregister(&cdev);
#endif

	kfree(ts);

	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void touch_early_suspend(struct early_suspend *h)
{
	struct lge_touch_data *ts =
			container_of(h, struct lge_touch_data, early_suspend);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (ts->fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("early_suspend is not executed\n");
		return;
	}

	if (ts->pdata->role->ghost_detection_enable) {
		resume_flag = 0;
	}

	if (ts->pdata->role->operation_mode)
		disable_irq(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);

	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}

	cancel_work_sync(&ts->work);
	cancel_delayed_work_sync(&ts->work_init);
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	release_all_ts_event(ts);

	touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
}

static void touch_late_resume(struct early_suspend *h)
{
	struct lge_touch_data *ts =
			container_of(h, struct lge_touch_data, early_suspend);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (ts->fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("late_resume is not executed\n");
		return;
	}

	touch_power_cntl(ts, ts->pdata->role->resume_pwr);

	if (ts->pdata->role->ghost_detection_enable) {
		resume_flag = 1;
		ts_rebase_count = 0;
		do_gettimeofday(&t_ex_debug[TIME_EX_RESUME_ON]);
	}

	if (ts->pdata->role->operation_mode)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	if (ts->pdata->role->resume_pwr == POWER_ON)
		queue_delayed_work(touch_wq, &ts->work_init,
				msecs_to_jiffies(ts->pdata->role->booting_delay));
	else
		queue_delayed_work(touch_wq, &ts->work_init, 0);
}
#endif

#if defined(CONFIG_PM)
static int touch_suspend(struct device *device)
{
	return 0;
}

static int touch_resume(struct device *device)
{
	return 0;
}
#endif

static struct i2c_device_id lge_ts_id[] = {
	{LGE_TOUCH_SYNAPTICS_NAME, 0 },
};

#if defined(CONFIG_PM)
static struct dev_pm_ops touch_pm_ops = {
	.suspend 	= touch_suspend,
	.resume 	= touch_resume,
};
#endif

static struct i2c_driver lge_touch_driver = {
	.probe   = touch_probe,
	.remove	 = touch_remove,
	.id_table = lge_ts_id,
	.driver	 = {
		.name   = LGE_TOUCH_SYNAPTICS_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm		= &touch_pm_ops,
#endif
	},
};

int touch_driver_register(struct touch_device_driver* driver)
{
	int ret = 0;

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func != NULL) {
		TOUCH_ERR_MSG("CANNOT add new touch-driver\n");
		ret = -EMLINK;
		goto err_touch_driver_register;
	}

	touch_device_func = driver;

	touch_wq = create_singlethread_workqueue("touch_wq");
	if (!touch_wq) {
		TOUCH_ERR_MSG("CANNOT create new workqueue\n");
		ret = -ENOMEM;
		goto err_work_queue;
	}

	ret = i2c_add_driver(&lge_touch_driver);
	if (ret < 0) {
		TOUCH_ERR_MSG("FAIL: i2c_add_driver\n");
		goto err_i2c_add_driver;
	}

	return 0;

err_i2c_add_driver:
	destroy_workqueue(touch_wq);
err_work_queue:
err_touch_driver_register:
	return ret;
}

void touch_driver_unregister(void)
{
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	i2c_del_driver(&lge_touch_driver);
	touch_device_func = NULL;

	if (touch_wq)
		destroy_workqueue(touch_wq);
}

