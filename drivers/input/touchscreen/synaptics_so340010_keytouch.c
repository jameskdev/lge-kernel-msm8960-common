/*
 * drivers/input/touchscreen/Synaptics_so340010_keytouch.c - Touch keypad driver
 *
 * Copyright (C) 2010 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <mach/vreg.h>
#include <mach/board_lge.h>
#include <mach/lge/board_l_dcm.h>
#include <linux/jiffies.h>
#include <linux/i2c/synaptics_i2c_rmi4.h>

#define TSKEY_REG_INTER			(0x00)
#define TSKEY_REG_GEN			(0x01)
#define TSKEY_REG_BUT_ENABLE	(0x04)
#define TSKEY_REG_SEN1			(0x10)
#define TSKEY_REG_SEN2			(0x11)
#define TSKEY_REG_GPIO_CTRL		(0x0E)
#define TSKEY_REG_LED_EN		(0x22)
#define TSKEY_REG_LED_PERIOD	(0x23)
#define TSKEY_REG_LED_CTRL1		(0x24)
#define TSKEY_REG_LED_CTRL2		(0x25)

#define TSKEY_REG_GPIO_STATE1	(0x01)
#define TSKEY_REG_GPIO_STATE2	(0x08)
#define TSKEY_REG_BUT_STATE1	(0x01)
#define TSKEY_REG_BUT_STATE2	(0x09)

#define TSKEY_REG_DUMMY			(0x00)

#define TSKEY_VAL_SEN0			(0x00)
#define TSKEY_VAL_SEN1			(0xC8) /*Home*/
#define TSKEY_VAL_SEN2			(0xC8) /*Back*/
#define TSKEY_VAL_SEN3			(0xC8) /*Menu*/
#define TSKEY_VAL_PERIOD_A		(0x00)
#define TSKEY_VAL_PERIOD_B		(0x00)

#define TSKEY_VAL_S1		(0x200) /*Home*/
#define TSKEY_VAL_S2		(0x400) /*Back*/
#define TSKEY_VAL_S3		(0x800) /*Menu*/
#define TSKEY_VAL_ALL			(TSKEY_VAL_S1 | TSKEY_VAL_S2 | TSKEY_VAL_S3) /*All*/

#define KEYCODE_CANCEL			0xFF
#define PRESS					1
#define RELEASE					0
#define KEY_DELAYED_SEC			40000000 /*ns*/
#define KEY_RELEASE_DELAYED_SEC     20000000 /*ns*/
#define KEY_CANCEL_CHECK_SEC	200

atomic_t tk_irq_flag;
struct class *touch_key_class;
EXPORT_SYMBOL(touch_key_class);

struct device *sen_test;
EXPORT_SYMBOL(sen_test);

static u16 TSKEY_VAL_HOME = TSKEY_VAL_S1;
static u16 TSKEY_VAL_BACK = TSKEY_VAL_S2;
static u16 TSKEY_VAL_MENU = TSKEY_VAL_S3;

static u8 touch_sen1, touch_sen2, touch_sen3;

static u32 KEY_DELAYED_NS_TIME = KEY_DELAYED_SEC;
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

static struct key_touch_platform_data *so340010_pdata;
struct so340010_device *so340010_pdev;
static struct workqueue_struct *so340010_wq;


struct workqueue_struct *touch_dev_reset_wq;
struct workqueue_struct *touch_dev_init_delayed_wq;


/*struct work_struct		touch_init_work;*/ 	/* work for reset touch & key */
struct delayed_work		touch_reset_work; 	/* work for reset touch & key */
static int           touch_reset_retry_cnt;



static u16 last_key;
static u16 reported_key;

static u8 keytouch_lock;
static u8 is_pressing;

extern struct workqueue_struct *synaptics_wq;
extern struct synaptics_ts_data *ts_data_reset;
extern int fw_upgrade_status;
extern int checked_fw_upgrade;
extern u8 is_fw_upgrade;
extern atomic_t ts_irq_flag;

extern int s3200_initialize(void);
extern irqreturn_t synaptics_ts_irq_handler(int irq, void *dev_id);
extern int synaptics_ts_check_product_id(void);
extern void synaptics_ts_classdev_register(void);
extern u8 synaptics_ts_need_upgrade(struct i2c_client *client);
extern int synaptics_ts_fw_upgrade_work(struct synaptics_ts_data *ts);

void touch_dev_suspend(void);
int touch_dev_resume(void);


static int so340010_i2c_write(u8 reg_h, u8 reg_l, u8 val_h, u8 val_l)
{
	int ret;
	u8 buf[4];
	struct i2c_msg msg = {
		so340010_pdev->client->addr, 0, sizeof(buf), buf
	};

	buf[0] = reg_h;
	buf[1] = reg_l;
	buf[2] = val_h;
	buf[3] = val_l;

	ret = i2c_transfer(so340010_pdev->client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&so340010_pdev->client->dev, "i2c write error\n");
	}

	return ret;
}

static int so340010_i2c_read(u8 reg_h, u8 reg_l, u16 *ret)
{
	int err;
	u8 buf[2];

	struct i2c_msg msg[2] = {
		{ so340010_pdev->client->addr, 0, sizeof(buf), buf },
		{ so340010_pdev->client->addr, I2C_M_RD, sizeof(buf), (__u8 *)ret }
	};

	buf[0] = reg_h;
	buf[1] = reg_l;

	err = i2c_transfer(so340010_pdev->client->adapter, msg, 2);
	if (err < 0) {
		dev_err(&so340010_pdev->client->dev, "i2c read error\n");
	}

	return err;
}

struct reg_code_table {
	u8 val1;
	u8 val2;
	u8 val3;
	u8 val4;
};

static struct reg_code_table initial_code_table[] = {
	{TSKEY_REG_DUMMY, 	TSKEY_REG_INTER, 		TSKEY_REG_DUMMY, 	0x07},
	{TSKEY_REG_DUMMY, 	TSKEY_REG_GEN, 			TSKEY_REG_DUMMY, 	0x30},
	{TSKEY_REG_DUMMY, 	TSKEY_REG_BUT_ENABLE, 	TSKEY_REG_DUMMY, 	0x0E},
	{TSKEY_REG_DUMMY, 	TSKEY_REG_SEN1, 		TSKEY_VAL_SEN1, 	TSKEY_VAL_SEN0},
	{TSKEY_REG_DUMMY, 	TSKEY_REG_SEN2, 		TSKEY_VAL_SEN3, 	TSKEY_VAL_SEN2},
};


static int so340010_initialize(void)
{
	int ret = 0;
	u16 data;
	int i;


	for (i = 0; i < ARRAY_SIZE(initial_code_table); i++) {
		ret = so340010_i2c_write(
				initial_code_table[i].val1,
				initial_code_table[i].val2,
				initial_code_table[i].val3,
				initial_code_table[i].val4);
		if (ret < 0)
			goto err_out_retry;
	}

	/*DUMMY read(datasheet value)*/
	/*ret = so340010_i2c_read(TSKEY_REG_BUT_STATE1, TSKEY_REG_BUT_STATE2, &data);*/
	ret = so340010_i2c_read(TSKEY_REG_BUT_STATE1, TSKEY_REG_BUT_STATE2, &data);
	/*ret = so340010_i2c_read(TSKEY_REG_GPIO_STATE1, TSKEY_REG_GPIO_STATE2, &data);*/
	if (ret < 0)
		goto err_out_retry;

	so340010_pdev->ic_init_err_cnt = 0;
	return 0;

err_out_retry:
	so340010_pdev->ic_init_err_cnt++;
	return -1;
}

static void so340010_report_event(u16 state)
{
	int keycode = 0;
	static unsigned char old_keycode;

	if (state == 0) { /*release*/
		printk("keycode:%x is RELEASE\n", old_keycode);
		input_report_key(so340010_pdev->input_dev, so340010_pdata->keycode[2], RELEASE);
		input_report_key(so340010_pdev->input_dev, so340010_pdata->keycode[4], RELEASE);
		input_report_key(so340010_pdev->input_dev, so340010_pdata->keycode[6], RELEASE);
		input_report_key(so340010_pdev->input_dev, BTN_TOUCH, RELEASE);
		is_pressing = 0;
	} else if (state == TSKEY_VAL_HOME) { /*BTN1(S1), Home*/
		keycode = so340010_pdata->keycode[2];
		printk("keycode:%x [HOME] is PRESS\n", keycode);
	} else if (state == TSKEY_VAL_BACK) { /*BTN2(S2), Back*/
		keycode = so340010_pdata->keycode[4];
		printk("keycode:%x [BACK] is PRESS\n", keycode);
	} else if (state == TSKEY_VAL_MENU) { /*BTN3(S3), Menu*/
		keycode = so340010_pdata->keycode[6];
		printk("keycode:%x [MENU] is PRESS\n", keycode);
	} else if (state == KEYCODE_CANCEL) { /*previous pressed key cancel*/
		printk("%s cancel key![%x]\n", __func__, old_keycode);
		input_report_key(so340010_pdev->input_dev, old_keycode, KEYCODE_CANCEL);
		is_pressing = 0;
	} else {
		pr_info("Unknown key type(0x%x)\n", state);
		return;
	}

	if (state & TSKEY_VAL_ALL) { /*any key pressed*/
		input_report_key(so340010_pdev->input_dev, keycode, PRESS);
		input_report_key(so340010_pdev->input_dev, BTN_TOUCH, PRESS);
		is_pressing = 1;
		old_keycode = keycode;
	}
	input_sync(so340010_pdev->input_dev);
}

void so340010_keytouch_cancel(void)
{
	cancel_delayed_work_sync(&so340010_pdev->lock_delayed_work);
	keytouch_lock = 1;
	if (is_pressing) {
		so340010_report_event(KEYCODE_CANCEL);
	}
}

void so340010_keytouch_lock_free(void)
{
	if (keytouch_lock == 1) {
		if (so340010_pdev->lock_delayed_work.work.func != NULL)
			schedule_delayed_work(&so340010_pdev->lock_delayed_work, msecs_to_jiffies(KEY_CANCEL_CHECK_SEC));
	}
}

static void so340010_keytouch_delayed_work(struct work_struct *work)
{
	if (keytouch_lock) {
		keytouch_lock = 0;
	}
}

static irqreturn_t so340010_irq_handler(int irq, void *dev_id)
{
	struct so340010_device *pdev = (struct so340010_device *)dev_id;
	int gpio_state;

	gpio_state = gpio_get_value(pdev->gpio_irq);
	if (gpio_state == 0) {
		if (atomic_read(&tk_irq_flag) == 1) {
			atomic_set(&tk_irq_flag, 0);
			disable_irq_nosync(so340010_pdev->client->irq);
		}
		queue_work(so340010_wq, &pdev->key_work);
	} else {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[KEY] IRQ_handler::-IRQ_HANDLER-\n");
	}

	return IRQ_HANDLED;
}


static void so340010_key_work_func(struct work_struct *work)
{
	int ret;
	u16 state;
	struct so340010_device *pdev = container_of(work, struct so340010_device, key_work);
	static u8 report_event_release;

key_work_retry:

	ret = so340010_i2c_read(TSKEY_REG_BUT_STATE1, TSKEY_REG_BUT_STATE2, &state);
	if (ret < 0) {
		goto err_out_retry;
	}

	if (keytouch_lock) { /*for key cancel*/
		goto exit_polling_mode;
	}

	if (unlikely(report_event_release)) {
		report_event_release = 0;
		so340010_report_event(0);
		goto exit_polling_mode;
	}

	if (last_key != 0) {					/*polling mode*/
		if (reported_key != state) {
			if (last_key == state) {		/*first press*/
				so340010_report_event(state);
				reported_key = state;
				hrtimer_start(&pdev->timer, ktime_set(0, KEY_DELAYED_NS_TIME), HRTIMER_MODE_REL);
			} else {						/*release*/
				last_key = 0;
				reported_key = 0;
				/*so340010_report_event(0);*/
				/*goto exit_polling_mode;*/

				report_event_release = 1;
				hrtimer_start(&pdev->timer, ktime_set(0, KEY_RELEASE_DELAYED_SEC), HRTIMER_MODE_REL);
			}
		} else {
			if (last_key != state) {		/*shot press(<40ms), ignore*/
				goto exit_polling_mode;
			} else {						/*(reported key)press continue, polling mode*/
				hrtimer_start(&pdev->timer, ktime_set(0, KEY_DELAYED_NS_TIME), HRTIMER_MODE_REL);
			}
		}

	} else {								/*press start, polling mode start*/
		if (state != 0) {
			last_key = state;
			hrtimer_start(&pdev->timer, ktime_set(0, KEY_DELAYED_NS_TIME), HRTIMER_MODE_REL);
		} else {							/*interrupt occured, but nofinger, when multi touch*/
			goto exit_polling_mode;
		}
	}

	return; /* continue polling mode */

err_out_retry:
	so340010_pdev->ic_init_err_cnt++;

	if (atomic_read(&tk_irq_flag) == 1) {
		atomic_set(&tk_irq_flag, 0);
		disable_irq_nosync(pdev->client->irq);
	}

	queue_delayed_work(touch_dev_reset_wq, &touch_reset_work, msecs_to_jiffies(100));

	return;

exit_polling_mode:
	last_key = 0;
	reported_key = 0;

	if (gpio_get_value(pdev->gpio_irq) == 0) {
		pr_info("%s : There is another button data in S0340010 \n", __func__);
    goto key_work_retry;
	}

	if (atomic_read(&tk_irq_flag) == 0) {
		atomic_set(&tk_irq_flag, 1);
		enable_irq(so340010_pdev->client->irq);
	}
}

static enum hrtimer_restart so340010_key_timer_func(struct hrtimer *timer)
{
	struct so340010_device *pdev = container_of(timer, struct so340010_device, timer);

	queue_work(so340010_wq, &pdev->key_work);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void so340010_early_suspend(struct early_suspend *h)
{
	/*struct so340010_device *pdev = container_of(h, struct so340010_device, earlysuspend);*/

	if (atomic_read(&tk_irq_flag) == 1) {
		atomic_set(&tk_irq_flag, 0);
		disable_irq_nosync(so340010_pdev->client->irq);
	}

	cancel_work_sync(&so340010_pdev->key_work);
	hrtimer_cancel(&so340010_pdev->timer);

	if (fw_upgrade_status == 1) {
		checked_fw_upgrade = 1;
		return;
	}

	touch_dev_suspend();

	return;
}

static void so340010_late_resume(struct early_suspend *h)
{
	return;
}
#endif

static ssize_t so340010_set_sen_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "HOME[%d] MENU[%d] BACK[%d]\n", touch_sen1, touch_sen2, touch_sen3);
}

static ssize_t so340010_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u16 a, b, c, d;
	ret = so340010_i2c_read(0x00, 0x00, &a);
	if (ret < 0)
		printk("[tk] i2c read fail a\n");
	ret = so340010_i2c_read(0x00, 0x01, &b);
	if (ret < 0)
		printk("[tk] i2c read fail b\n");
	ret = so340010_i2c_read(0x00, 0x04, &c);
	if (ret < 0)
		printk("[tk] i2c read fail c\n");
	ret = so340010_i2c_read(0x01, 0x09, &d);
	if (ret < 0)
		printk("[tk] i2c read fail d\n");

	return sprintf(buf, "0x0000[%x] 0x0001[%x] 0x0004[%x] 0x0109[%x]\n", a, b, c, d);
}

static ssize_t so340010_set_sen_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int cmd_no, config_value = 0;
	unsigned long after;
	int ret;

	ret = strict_strtoul(buf, 10, &after);
	if (ret)
		return -EINVAL;

	pr_info("[SO340010] %s\n", __func__);
	cmd_no = (int)(after / 1000);
	config_value = (int)(after % 1000);

	if (cmd_no == 0) {
		touch_sen1 = config_value;
		pr_info("[SO340010] Set Home Key Sen[%d]\n", touch_sen1);
		so340010_i2c_write(
			TSKEY_REG_DUMMY,
			TSKEY_REG_SEN1,
			touch_sen1,
			0);
	} else if (cmd_no == 1) {
		touch_sen2 = config_value;
		pr_info("[SO340010] Set Menu Key Sen[%d]\n", touch_sen2);
		so340010_i2c_write(
			TSKEY_REG_DUMMY,
			TSKEY_REG_SEN2,
			touch_sen3,
			touch_sen2);
	} else if (cmd_no == 2) {
		touch_sen3 = config_value;
		pr_info("[SO340010] Set Back Key Sen[%d]\n", touch_sen3);
		so340010_i2c_write(
			TSKEY_REG_DUMMY,
			TSKEY_REG_SEN2,
			touch_sen3,
			touch_sen2);
	} else {
		pr_info("[%s] unknown CMD\n", __func__);
	}

	return size;
}

static ssize_t
tk_reset_test(struct device *dev, struct device_attribute *attr, char *buf)
{
	so340010_pdata->tk_power(0, true);
	queue_delayed_work(touch_dev_reset_wq, &touch_reset_work, msecs_to_jiffies(100));
	snprintf(buf, PAGE_SIZE, "ESD!\n");
	return 4;
}

static ssize_t so340010_delayed_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "KEY_DELAYED_NS_TIME = %d(ms)\n", KEY_DELAYED_NS_TIME/1000000);
}

static ssize_t so340010_delayed_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int input_time = 0;
	sscanf(buf, "%d", &input_time);
	KEY_DELAYED_NS_TIME = input_time * 1000000;
	printk("%s KEY_DELAYED_NS_TIME changed to %d(ms)\n", __func__, KEY_DELAYED_NS_TIME/1000000);
	return size;
}

static struct device_attribute dev_attr_device_test[] = {
	__ATTR(set_sen, S_IRUGO | S_IWUSR | S_IXOTH, so340010_set_sen_show, so340010_set_sen_store),
	__ATTR(test, S_IRUGO | S_IWUSR | S_IXOTH, so340010_test_show, NULL),
	__ATTR(reset, S_IRUGO | S_IWUSR | S_IXOTH, tk_reset_test, NULL),
	__ATTR(delayed_time, S_IRUGO | S_IWUSR | S_IXOTH, so340010_delayed_time_show, so340010_delayed_time_store),
};


void touch_dev_suspend(void)
{
	/* 2. Touch Device Power Off */
	so340010_pdata->tk_power(0, true);
}


int touch_dev_resume(void)
{
	int ret = 0;
	int gpio_state = 0;

	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_resume !!!! START\n");
init_retry:

	if (touch_reset_retry_cnt > 3) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_resume - MAX FAIL !!!\n");
		return -1;
	}

	/* 1. Touch Device Power On */
	so340010_pdata->tk_power(1, true);


	/* 2. Initialize so340010 touch key */
	ret = so340010_initialize();
	if (ret < 0) {
		pr_err("%s: failed to init\n", __func__);
		goto err_out_retry;
	}

	mdelay(130);

	/* 3. Initialize s3200 touch screen  */
	ret = s3200_initialize();
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR,
				"[TOUCH] %s::i2c s3200_initialize fail\n", __func__);
		goto err_out_retry;
	}

	if (keytouch_lock == 1)
	  keytouch_lock = 0;

	/* 4. Enable Touch screen IRQ */
	gpio_state = gpio_get_value(ts_data_reset->pdata->i2c_int_gpio);
	if (gpio_state == 0) {
		printk("%s gpio_state == 0\n", __func__);
		queue_work(synaptics_wq, &ts_data_reset->t_work);
	} else {
		printk("%s gpio_state != 0\n", __func__);
		if (atomic_read(&ts_irq_flag) == 0) {
			atomic_set(&ts_irq_flag, 1);
			enable_irq(ts_data_reset->client->irq);
		}
	}
	pr_info("gpio_state = %d\n", gpio_state);

	/* 5. Enable touch key IRQ */
	if (atomic_read(&tk_irq_flag) == 0) {
		atomic_set(&tk_irq_flag, 1);
		enable_irq(so340010_pdev->client->irq);
	}

	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_resume !!!! END\n");
	return 0;
err_out_retry:
	touch_reset_retry_cnt++;
	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_resume - fail : retry(%d) !!!! END\n", touch_reset_retry_cnt);
	so340010_pdata->tk_power(0, true);
	goto init_retry;
}

static void touch_dev_reset(struct work_struct *reset_work)
{
	int ret = 0;

	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_reset !!!! START\n");

	/* 0. Check Retry Count */
	if (touch_reset_retry_cnt > 3 && !is_fw_upgrade) {
		pr_err("%s: touch dev reset exceed max count \n", __func__);
		return;
	}

    /* 1. Touch Device Power On */
	so340010_pdata->tk_power(1, true);


	/* 2. Initialize so340010 touch key */
	ret = so340010_initialize();
	if (ret < 0) {
		pr_err("%s: failed to init\n", __func__);
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "%s: failed to init so340010\n", __func__);
		goto err_out_retry;
	}

  mdelay(130);

	/* 3. Initialize s3200 touch screen  */
	ret = s3200_initialize();
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c s3200_initialize fail\n", __func__);
		goto err_out_retry;
	}

	if (keytouch_lock == 1)
		keytouch_lock = 0;

	/* 4. Enable Touch screen IRQ */
	if (atomic_read(&ts_irq_flag) == 0) {
		atomic_set(&ts_irq_flag, 1);
		enable_irq(ts_data_reset->client->irq);
	}

	/* 5. Enable touch key IRQ */
	if (atomic_read(&tk_irq_flag) == 0) {
		atomic_set(&tk_irq_flag, 1);
		enable_irq(so340010_pdev->client->irq);
	}

	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_reset !!!! END\n");
	touch_reset_retry_cnt = 0;
	is_fw_upgrade = 0;
	return;
err_out_retry:
	touch_reset_retry_cnt++;
	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_reset - fail : retry(%d) !!!! END\n", touch_reset_retry_cnt);
	so340010_pdata->tk_power(0, true);
	queue_delayed_work(touch_dev_reset_wq, &touch_reset_work, msecs_to_jiffies(100));
	return;
}


static void touch_dev_initialize(void)
{
	int ret = 0;

	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_initialize !!!! START\n");

initialize_retry:
	if (touch_reset_retry_cnt > 3) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_initialize - FAIL MAX !!!\n");
		return ;
	}

    /* 1. Touch Device Power ON */
	so340010_pdata->tk_power(1, true);

  SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH_KEY] Dev_initialize !!!! START\n");
	/* 2. Initialize so340010 touch key */
	ret = so340010_initialize();
	if (ret < 0) {
		pr_err("%s: failed to init\n", __func__);
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "%s: failed to init so340010\n", __func__);
		goto err_out_retry;
	}
  SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH_KEY] Dev_initialize !!!! END\n");

  mdelay(130);

  SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] Dev_initialize !!!! START\n");
	/* 3. Initialize s3200 touch screen  */
	ret = s3200_initialize();
	if (ret < 0) {
		SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] %s::i2c s3200_initialize fail\n", __func__);
		goto err_out_retry;
	}
  SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] Dev_initialize !!!! END\n");

	/* 4. check s3200 product id */
	synaptics_ts_check_product_id();

	/* 5. check touch firmware upgrade */
	if (synaptics_ts_need_upgrade(ts_data_reset->client)) {
		pr_info("%s : synaptics_ts_need_upgrade!\n", __func__);
		ret = synaptics_ts_fw_upgrade_work(ts_data_reset);

		 if (ret < 0) {
			touch_reset_retry_cnt++;
			SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] firmware upgrade fail(%d) !!!\n", touch_reset_retry_cnt);
			so340010_pdata->tk_power(0, true);
			goto initialize_retry;
		} else {
      so340010_pdata->tk_power(0, true);
		goto initialize_retry;
			pr_info("%s : ret == 0 firmware upgrade success!!!!\n", __func__);
		}
	}

	/* 6. check s3200 classdev_register */
	synaptics_ts_classdev_register();

	/* 7. Enable Touch screen IRQ */
	if (atomic_read(&ts_irq_flag) == 0) {
		atomic_set(&ts_irq_flag, 1);
		enable_irq(ts_data_reset->client->irq);
	}

	/* 8. Enable touch key IRQ */
	if (atomic_read(&tk_irq_flag) == 0) {
		atomic_set(&tk_irq_flag, 1);
		enable_irq(so340010_pdev->client->irq);
	}

	/* 9. Request irq for touch screen  */
	ret = request_irq(ts_data_reset->client->irq, synaptics_ts_irq_handler,
						ts_data_reset->pdata->irqflags, ts_data_reset->client->name, ts_data_reset);

	pr_info("[s3200] request_irq :%d\n", ret);

	/* 10. Request irq for touch key	*/
	ret = request_irq(so340010_pdev->client->irq, so340010_irq_handler,
						IRQF_TRIGGER_LOW, "so340010_irq", (void *)so340010_pdev);

	pr_info("[so340010] request_irq :%d\n", ret);

	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_initialize !!!! END\n");
	touch_reset_retry_cnt = 0;

	return;
err_out_retry:
	touch_reset_retry_cnt++;
	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_dev_initialize - fail : retry(%d) !!!! END\n", touch_reset_retry_cnt);
	so340010_pdata->tk_power(0, true);
	queue_delayed_work(touch_dev_reset_wq, &touch_reset_work, msecs_to_jiffies(100));
	return;
}


static void touch_init_delayed_work(struct work_struct *work)
{
	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_init_delayed_work !!!! START\n");

	touch_dev_initialize();

	destroy_workqueue(touch_dev_init_delayed_wq);

	touch_dev_init_delayed_wq = NULL;

	SYNAPTICS_DEBUG_MSG(ERR_INFO, KERN_ERR, "[TOUCH] touch_init_delayed_work !!!! END\n");
}

static int so340010_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	int i;
	unsigned keycode = KEY_UNKNOWN;
	atomic_set(&tk_irq_flag, 1);

	pr_info("%s: start\n", __func__);

	/*if (lge_get_board_revno() >= HW_REV_C) {*/
		TSKEY_VAL_BACK = TSKEY_VAL_S3;
		TSKEY_VAL_MENU = TSKEY_VAL_S2;
		/*printk("%s lge_get_board_revno() == HW_REV_C\n", __func__);*/
	/*}*/

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("%s: it is not support I2C_FUNC_I2C.\n", __func__);
		return -ENODEV;
	}

	so340010_pdev = kzalloc(sizeof(struct so340010_device), GFP_KERNEL);
	if (so340010_pdev == NULL) {
		pr_info("%s: failed to allocation\n", __func__);
		return -ENOMEM;
	}

	so340010_pdev->client = client;
	so340010_pdata = client->dev.platform_data;
	i2c_set_clientdata(so340010_pdev->client, so340010_pdev);

	so340010_pdev->input_dev = input_allocate_device();
	if (NULL == so340010_pdev->input_dev) {
		dev_err(&client->dev, "failed to allocation\n");
		goto err_input_allocate_device;
	}

	so340010_pdev->input_dev->name = "touch_keypad";
	so340010_pdev->input_dev->phys = "touch_keypad/i2c";
	so340010_pdev->input_dev->id.vendor = VENDOR_LGE;
	so340010_pdev->input_dev->evbit[0] = BIT_MASK(EV_KEY);
	so340010_pdev->input_dev->keycode = so340010_pdata->keycode;
	so340010_pdev->input_dev->keycodesize = sizeof(unsigned short);
	so340010_pdev->input_dev->keycodemax = so340010_pdata->keycodemax;

	for (i = 0; i < so340010_pdata->keycodemax; i++) {
		keycode = so340010_pdata->keycode[2 * i];
		set_bit(keycode, so340010_pdev->input_dev->keybit);
	}
	ret = input_register_device(so340010_pdev->input_dev);
	if (ret < 0) {
		pr_info("%s: failed to register input\n", __func__);
		goto err_input_register_device;
	}

	touch_sen1 = TSKEY_VAL_SEN1;
	touch_sen2 = TSKEY_VAL_SEN2;
	touch_sen3 = TSKEY_VAL_SEN3;

	for (i = 0; i < ARRAY_SIZE(dev_attr_device_test); i++) {
		ret = device_create_file(&client->dev, &dev_attr_device_test[i]);
		if (ret) {
			pr_info("%s: device_create_file fail\n", __func__);
		}
	}

	so340010_pdev->gpio_irq = so340010_pdata->irq;
	so340010_pdev->client->irq = gpio_to_irq(so340010_pdata->irq);

	INIT_DELAYED_WORK(&so340010_pdev->lock_delayed_work, so340010_keytouch_delayed_work);
	INIT_WORK(&so340010_pdev->key_work, so340010_key_work_func);

	/* Init work for touch device reset(Touch screen & Key ) */
	INIT_DELAYED_WORK(&touch_reset_work, touch_dev_reset);
	INIT_DELAYED_WORK(&so340010_pdev->init_delayed_work, touch_init_delayed_work);

	hrtimer_init(&so340010_pdev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	so340010_pdev->timer.function = so340010_key_timer_func;

#ifdef CONFIG_HAS_EARLYSUSPEND
	so340010_pdev->earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	so340010_pdev->earlysuspend.suspend = so340010_early_suspend;
	so340010_pdev->earlysuspend.resume = so340010_late_resume;
	register_early_suspend(&so340010_pdev->earlysuspend);
#endif

	/* Initialize Touch Device */
	queue_delayed_work(touch_dev_init_delayed_wq, &so340010_pdev->init_delayed_work, msecs_to_jiffies(300));

	return 0;

err_input_register_device:
	input_free_device(so340010_pdev->input_dev);
err_input_allocate_device:
	kfree(so340010_pdev);

	return ret;
}

static int so340010_i2c_remove(struct i2c_client *client)
{
	int i;
	struct so340010_device *pdev = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&pdev->earlysuspend);
#endif
	free_irq(pdev->client->irq, pdev);
	hrtimer_cancel(&so340010_pdev->timer);
	input_unregister_device(pdev->input_dev);
	for (i = 0; i < ARRAY_SIZE(dev_attr_device_test); i++) {
		device_remove_file(&client->dev, &dev_attr_device_test[i]);
	}
	kfree(pdev);
	return 0;
}

static const struct i2c_device_id so340010_i2c_ids[] = {
		{SYNAPTICS_KEYTOUCH_NAME, 0 },
		{ },
};

static struct i2c_driver so340010_i2c_driver = {
	.probe		= so340010_i2c_probe,
	.remove		= so340010_i2c_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= so340010_i2c_suspend,
	.resume		= so340010_i2c_resume,
#endif
	.id_table	= so340010_i2c_ids,
	.driver = {
		.name	= SYNAPTICS_KEYTOUCH_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init so340010_init(void)
{
	int ret;

	pr_info("%s: start Init\n", __func__);

	so340010_wq = create_singlethread_workqueue("so340010_wq");
	if (!so340010_wq) {
		pr_err("failed to create singlethread workqueue\n");
		return -ENOMEM;
	}

	touch_dev_reset_wq = create_singlethread_workqueue("touch_dev_reset_wq");
	if (!touch_dev_reset_wq) {
		pr_err("failed to create singlethread workqueue\n");
		destroy_workqueue(so340010_wq);
		return -ENOMEM;
	}

	touch_dev_init_delayed_wq = create_singlethread_workqueue("touch_dev_init_delayed_wq");
	if (!touch_dev_init_delayed_wq) {
		pr_err("failed to create singlethread workqueue\n");
		destroy_workqueue(so340010_wq);
		destroy_workqueue(touch_dev_reset_wq);
		return -ENOMEM;
	}

	ret = i2c_add_driver(&so340010_i2c_driver);
	if (ret < 0) {
		pr_err("%s: failed to i2c_add_driver\n", __func__);
		destroy_workqueue(so340010_wq);
		destroy_workqueue(touch_dev_reset_wq);
		destroy_workqueue(touch_dev_init_delayed_wq);
		return ret;
	}

	return 0;
}

static void __exit so340010_exit(void)
{

	if (so340010_wq)
		destroy_workqueue(so340010_wq);
	if (touch_dev_reset_wq)
		destroy_workqueue(touch_dev_reset_wq);
	if (touch_dev_init_delayed_wq)
		destroy_workqueue(touch_dev_init_delayed_wq);

	return;
}

module_init(so340010_init);
module_exit(so340010_exit);

MODULE_DESCRIPTION("synaptics so340010 touch key driver");
MODULE_LICENSE("GPL");
