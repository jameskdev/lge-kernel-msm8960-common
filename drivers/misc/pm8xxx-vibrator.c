/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/vibrator.h>

#include "../staging/android/timed_output.h"

#define VIB_DRV			0x4A

#define VIB_DRV_SEL_MASK	0xf8
#define VIB_DRV_SEL_SHIFT	0x03
#define VIB_DRV_EN_MANUAL_MASK	0xfc
#define VIB_DRV_LOGIC_SHIFT	0x2

#define VIB_MAX_LEVEL_mV	3100
#define VIB_MIN_LEVEL_mV	1200

struct pm8xxx_vib {
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	spinlock_t lock;
	struct work_struct work;
	struct device *dev;
	const struct pm8xxx_vibrator_platform_data *pdata;
	int state;
	int level;
	int pre_value;
	struct hrtimer vib_overdrive_timer;
	int active_level;
	int default_level;
	int request_normal_level;
	int request_haptic_level;
	int remain_vib_ms;
	int min_timeout_ms;
	int min_stop_ms;
	int overdrive_level;
	int overdrive_ms;
	int overdrive_range_ms;
	int normal_range_min_level;
	int normal_range_max_level;
	int haptic_range_min_level;
	int haptic_range_max_level;
	int haptic_default_level;
	int vib_state;
	struct timeval start_tv;
	struct timeval stop_tv;
	u8  reg_vib_drv;
	struct pm8xxx_vibrator_lvl_mv_tbl normal_lvl_tbl[PM8XXX_VIBATOR_UI_LVL_TBL_MAX];
};

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_REST_POWER
// atv is current, btv is stored, rtv is inverv
void get_timeval_interval(struct timeval *atv, struct timeval *btv, struct timeval *rtv)
{
	rtv->tv_sec = atv->tv_sec - btv->tv_sec;
	if(atv->tv_usec >= btv->tv_usec) {
		rtv->tv_usec = atv->tv_usec - btv->tv_usec;
	}
	else {
		rtv->tv_usec = 1000000 + atv->tv_usec - btv->tv_usec;
		rtv->tv_sec = rtv->tv_sec - 1;
	}
}

// if atv is greater than btv, return 1, else return 0;
int compare_timeval_interval(struct timeval *atv, struct timeval *btv)
{
	if(atv->tv_sec > btv->tv_sec)
		return 1;
	if(atv->tv_usec > btv->tv_usec)
		return 1;

	return 0;
}
#endif

static struct pm8xxx_vib *vib_dev;

int pm8xxx_vibrator_config(struct pm8xxx_vib_config *vib_config)
{
	u8 reg = 0;
	int rc;

	if (vib_dev == NULL) {
		pr_err("%s: vib_dev is NULL\n", __func__);
		return -EINVAL;
	}

	if (vib_config->drive_mV) {
		if ((vib_config->drive_mV < VIB_MIN_LEVEL_mV) ||
			(vib_config->drive_mV > VIB_MAX_LEVEL_mV)) {
			pr_err("Invalid vibrator drive strength\n");
			return -EINVAL;
		}
	}

	reg = (vib_config->drive_mV / 100) << VIB_DRV_SEL_SHIFT;

	reg |= (!!vib_config->active_low) << VIB_DRV_LOGIC_SHIFT;

	reg |= vib_config->enable_mode;

	rc = pm8xxx_writeb(vib_dev->dev->parent, VIB_DRV, reg);
	if (rc)
		pr_err("%s: pm8xxx write failed: rc=%d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_vibrator_config);

/* REVISIT: just for debugging, will be removed in final working version */
static void __dump_vib_regs(struct pm8xxx_vib *vib, char *msg)
{
	u8 temp;

	dev_dbg(vib->dev, "%s\n", msg);

	pm8xxx_readb(vib->dev->parent, VIB_DRV, &temp);
	dev_dbg(vib->dev, "VIB_DRV - %X\n", temp);
}

static int pm8xxx_vib_read_u8(struct pm8xxx_vib *vib,
				 u8 *data, u16 reg)
{
	int rc;

	rc = pm8xxx_readb(vib->dev->parent, reg, data);
	if (rc < 0)
		dev_warn(vib->dev, "Error reading pm8xxx: %X - ret %X\n",
				reg, rc);

	return rc;
}

static int pm8xxx_vib_write_u8(struct pm8xxx_vib *vib,
				 u8 data, u16 reg)
{
	int rc;

	rc = pm8xxx_writeb(vib->dev->parent, reg, data);
	if (rc < 0)
		dev_warn(vib->dev, "Error writing pm8xxx: %X - ret %X\n",
				reg, rc);
	return rc;
}

static int pm8xxx_vib_set(struct pm8xxx_vib *vib, int on)
{
	int rc;
	u8 val;
	unsigned long flags;

	spin_lock_irqsave(&vib->lock, flags);

	if (on) {
		val = vib->reg_vib_drv;
		val &= ~VIB_DRV_SEL_MASK;
		val |= ((vib->level << VIB_DRV_SEL_SHIFT) & VIB_DRV_SEL_MASK);
		rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;
	} else {
		val = vib->reg_vib_drv;
		val &= ~VIB_DRV_SEL_MASK;
		rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;
	}
	__dump_vib_regs(vib, "vib_set_end");

	printk(KERN_INFO "pm8xxx_vib_set vib->level:%d, val:0x%x\n",vib->level,val);

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_REST_POWER
	if(on)
	{
		if(vib->vib_state == 0)
		{
			vib->vib_state = 1;
			do_gettimeofday(&(vib->start_tv));
		}
	}
	else
	{
		if(vib->vib_state == 1)
		{
			vib->vib_state = 0;
			do_gettimeofday(&(vib->stop_tv));
		}
	}
#endif

	spin_unlock_irqrestore(&vib->lock, flags);

	return rc;
}

static void pm8xxx_vib_enable(struct timed_output_dev *dev, int value)
{
	struct pm8xxx_vib *vib = container_of(dev, struct pm8xxx_vib,
					 timed_dev);
	unsigned long flags;

	int origin_value;
#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_REST_POWER
	struct timeval current_tv;
	struct timeval interval_tv;
#endif	

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
	int over_ms = vib->overdrive_ms;
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_MIN_TIMEOUT
	spin_lock_irqsave(&vib->lock, flags);
	if (value == 0 && vib->pre_value <= vib->min_timeout_ms) {
		spin_unlock_irqrestore(&vib->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&vib->lock, flags);
#endif

	printk(KERN_INFO "pm8xxx_vib_enable value:%d\n",value);

retry:
	spin_lock_irqsave(&vib->lock, flags);
	if (hrtimer_try_to_cancel(&vib->vib_timer) < 0) {
		spin_unlock_irqrestore(&vib->lock, flags);
		cpu_relax();
		goto retry;
	}

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
	if (hrtimer_try_to_cancel(&vib->vib_overdrive_timer) < 0) {
		spin_unlock_irqrestore(&vib->lock, flags);
		cpu_relax();
		goto retry;
	}
#endif

	origin_value = value;

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_VOL
	if (value == 0 || vib->request_normal_level == 0 || vib->request_haptic_level == 0) {
#else
	if (value == 0) {
#endif
		vib->state = 0;
	}
	else {
		/* Set Min Timeout for normal fuction */
#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_MIN_TIMEOUT
		value = (value < vib->min_timeout_ms ?
			vib->min_timeout_ms : value);
#endif
			
		value = (value > vib->pdata->max_timeout_ms ?
				 vib->pdata->max_timeout_ms : value);
		vib->state = 1;

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_MIN_TIMEOUT
		vib->pre_value = value;
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
		if(vib->overdrive_ms > 0 && value <= vib->overdrive_range_ms) {

			vib->remain_vib_ms = value - over_ms;	
			vib->level = vib->overdrive_level;
			vib->active_level = vib->request_haptic_level;

			printk(KERN_INFO "start overdrive over_level:%d over_ms:%d \n",vib->level,over_ms);

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_REST_POWER
			do_gettimeofday(&current_tv);
			if(vib->vib_state) { // vibrator is working now.
				struct timeval min_timeout_tv;
				min_timeout_tv.tv_sec = vib->min_timeout_ms / 1000;
				min_timeout_tv.tv_usec = (vib->min_timeout_ms % 1000) * 1000;

				get_timeval_interval(&current_tv, &(vib->start_tv), &interval_tv);
				printk(KERN_INFO "vib_state is true, cur:%ld.%06ld, sta:%ld.%06ld, itv:%ld.%06ld\n",
								current_tv.tv_sec, current_tv.tv_usec,
								vib->start_tv.tv_sec, vib->start_tv.tv_usec,
								interval_tv.tv_sec, interval_tv.tv_usec );

				// if greater than min_timeout, no need over drive and min time.
				if(compare_timeval_interval(&interval_tv, &min_timeout_tv)==1) {
					value = origin_value;
					printk(KERN_INFO "interval greater than min_timeout, start normal vib %dms\n",value);
					goto NORMAL_VIB_START;
				}
				// if less than min_timeout, need corrected value 
				else {
					int interval_ms;
					interval_ms = (interval_tv.tv_sec * 1000) + (interval_tv.tv_usec / 1000000);
					if(over_ms > interval_ms) {
						over_ms = over_ms - interval_ms;
						vib->remain_vib_ms = origin_value;
						printk(KERN_INFO "interval less than min_timeout, start overdrive %dms, remain %dms\n", over_ms, vib->remain_vib_ms);
						goto OVERDRIVE_VIB_START;
					}
					else 
					{
						value = value - interval_ms;
						printk(KERN_INFO "interval less than min_timeout, start normal vib %dms\n",value);
						goto NORMAL_VIB_START;
					}
				}
			}	
			else { // vibrator is not working now.
				struct timeval min_stop_tv;
				min_stop_tv.tv_sec = vib->min_stop_ms / 1000;
				min_stop_tv.tv_usec = (vib->min_stop_ms % 1000) * 1000;

				get_timeval_interval(&current_tv, &(vib->stop_tv), &interval_tv);

				printk(KERN_INFO "vib_state is false, cur:%ld.%06ld, sto:%ld.%06ld, itv:%ld.%06ld\n",
								current_tv.tv_sec, current_tv.tv_usec,
								vib->stop_tv.tv_sec, vib->stop_tv.tv_usec,
								interval_tv.tv_sec, interval_tv.tv_usec );

				// if greater than min_stop_tv, start vibration over drive and value.
				if(compare_timeval_interval(&interval_tv, &min_stop_tv)==1) {
					printk(KERN_INFO "greater than min_stop_timeout, start overdrive %dms, remain %dms\n", over_ms, vib->remain_vib_ms);
					goto OVERDRIVE_VIB_START;
				}
				// if less than min_stop_tv, reduce over drive time.
				else {
					int interval_ms;
					interval_ms = (interval_tv.tv_sec * 1000) + (interval_tv.tv_usec / 1000);
					over_ms = interval_ms / (vib->min_stop_ms / vib->overdrive_ms) / 2;
					vib->remain_vib_ms = (value - over_ms) / 2;
					printk(KERN_INFO "less than min_stop_timeout, start overdrive %dms, remain %dms\n", over_ms, vib->remain_vib_ms);
					goto OVERDRIVE_VIB_START;
				}
			}
#else
			goto OVERDRIVE_VIB_START;
#endif
		}
		else 
#endif			
		{
			goto NORMAL_VIB_START;
		}		
	}

NORMAL_VIB_START:
#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_VOL
	vib->level = vib->request_normal_level;
#else
	vib->level = vib->default_level;
#endif	
	hrtimer_start(&vib->vib_timer,
		ktime_set(value / 1000, (value % 1000) * 1000000),
		HRTIMER_MODE_REL);

	goto FINISH_VIB_ENABLE;

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
OVERDRIVE_VIB_START:
	hrtimer_start(&vib->vib_overdrive_timer,
		ktime_set(over_ms / 1000, (over_ms % 1000) * 1000000),
		HRTIMER_MODE_REL);
#endif

FINISH_VIB_ENABLE:
	spin_unlock_irqrestore(&vib->lock, flags);
	schedule_work(&vib->work);
}

static void pm8xxx_vib_update(struct work_struct *work)
{
	struct pm8xxx_vib *vib = container_of(work, struct pm8xxx_vib,
					 work);

	pm8xxx_vib_set(vib, vib->state);
}

static int pm8xxx_vib_get_time(struct timed_output_dev *dev)
{
	struct pm8xxx_vib *vib = container_of(dev, struct pm8xxx_vib,
							 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart pm8xxx_vib_timer_func(struct hrtimer *timer)
{
	struct pm8xxx_vib *vib = container_of(timer, struct pm8xxx_vib,
							 vib_timer);

	printk(KERN_INFO "expire vib timer\n");
	vib->state = 0;
	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
static enum hrtimer_restart pm8xxx_vib_overdrive_timer_func(struct hrtimer *timer)
{
	struct pm8xxx_vib *vib = container_of(timer, struct pm8xxx_vib,
							 vib_overdrive_timer);

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_VOL
	vib->level = vib->active_level;
#else
	vib->level = vib->default_level;
#endif	

	if(vib->remain_vib_ms > 0) {
		int value = vib->remain_vib_ms;
		vib->state = 1;
		printk(KERN_INFO "finished overdrive timer. start remain vib timer level:%d remain_ms:%d\n",vib->level,value);
		hrtimer_start(&vib->vib_timer,
			  ktime_set(value / 1000, (value % 1000) * 1000000),
		      HRTIMER_MODE_REL);
	}
	else {
		printk(KERN_INFO "finished overdrive timer. no remain vib timer\n");
		vib->state = 0;
	}

	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}
#endif

#ifdef CONFIG_PM
static int pm8xxx_vib_suspend(struct device *dev)
{
	struct pm8xxx_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	pm8xxx_vib_set(vib, 0);

	return 0;
}

static const struct dev_pm_ops pm8xxx_vib_pm_ops = {
	.suspend = pm8xxx_vib_suspend,
};
#endif

/* LGE_CHANGE
* control the volume of vibration
* 2012-01-02, donggyun.kim@lge.com
*/
#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_VOL
#define MAGNITUDE_MAX 128
#define MAGNITUDE_MIN 1
#define MAGNITUDE_DEFAULT 256
#define FLOATING_POINT_CAL 100

static ssize_t pm8xxx_vib_lvl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	int gain,haptic_gain;
	int level_mV = (vib->request_normal_level) * 100;
	int haptic_level_mV = (vib->request_haptic_level) * 100;

#ifdef VIB_SHOW_GAIN_COMPUTED
	/* Convert PMIC mV level to magnitude value */
	if (level_mV >= VIB_MAX_LEVEL_mV)
		gain = MAGNITUDE_MAX;
	else if (level_mV <= VIB_MIN_LEVEL_mV)
		gain = MAGNITUDE_MIN;
	else {
		int buf;
		buf = ((level_mV - VIB_MIN_LEVEL_mV) * FLOATING_POINT_CAL) / (VIB_MAX_LEVEL_mV - VIB_MIN_LEVEL_mV);
		gain = (MAGNITUDE_MAX * buf) / FLOATING_POINT_CAL;
	}
#else
	gain = level_mV;
	haptic_gain = haptic_level_mV;
#endif	
	
	return sprintf(buf, "normal:%dmv, haptic:%dmv\n", gain, haptic_gain);
}

static ssize_t pm8xxx_vib_lvl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);

	int level_mV,haptic_level_mV;
	int gain;
	sscanf(buf, "%d", &gain);

	/* Convert magnitude value to PMIC mV level */
	if (gain == MAGNITUDE_DEFAULT) {
		level_mV        = vib->default_level * 100;
		haptic_level_mV = vib->haptic_default_level * 100;
	}
	else if (gain == 0) {
		level_mV        = 0;
		haptic_level_mV = 0;
	}
	else if (gain >= MAGNITUDE_MAX) {
		level_mV        = vib->normal_range_max_level * 100;
		haptic_level_mV = vib->haptic_range_max_level * 100;
	}
	else if (gain < MAGNITUDE_MIN) {
		level_mV        = vib->normal_range_min_level * 100;
		haptic_level_mV = vib->haptic_range_min_level * 100;
	}
	else {
		int i = 0;

		/* compute from equation */
		level_mV        = ((((vib->normal_range_max_level * 100) - (vib->normal_range_min_level * 100)) * gain) / MAGNITUDE_MAX) + (vib->normal_range_min_level * 100);

		/* compute from table */
		while (i < PM8XXX_VIBATOR_UI_LVL_TBL_MAX) {
			if( gain <= vib->normal_lvl_tbl[i].hal_lvl ) {
				level_mV = vib->normal_lvl_tbl[i].vib_mV;
				break;
			}
			i++;
		}

		haptic_level_mV = ((((vib->haptic_range_max_level * 100) - (vib->haptic_range_min_level * 100)) * gain) / MAGNITUDE_MAX) + (vib->haptic_range_min_level * 100);
	}

	if(level_mV != 0 && level_mV < VIB_MIN_LEVEL_mV)
		level_mV = VIB_MIN_LEVEL_mV;
	else if(level_mV > VIB_MAX_LEVEL_mV)
		level_mV = VIB_MAX_LEVEL_mV;

	if(level_mV != 0 && haptic_level_mV < VIB_MIN_LEVEL_mV)
		haptic_level_mV = VIB_MIN_LEVEL_mV;
	else if(haptic_level_mV > VIB_MAX_LEVEL_mV)
		haptic_level_mV = VIB_MAX_LEVEL_mV;

	vib->request_normal_level = level_mV / 100;
	vib->request_haptic_level = haptic_level_mV / 100;

	printk(KERN_INFO "VIB_DRV - M:[%d], L:[%d], HL:[%d]\n", gain, vib->request_normal_level, vib->request_haptic_level);
	return size;
}

static DEVICE_ATTR(amp, S_IRUGO | S_IWUSR, pm8xxx_vib_lvl_show, pm8xxx_vib_lvl_store);

#endif /* CONFIG_LGE_PMIC8XXX_VIBRATOR_VOL */

static ssize_t pm8xxx_vib_min_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	return sprintf(buf, "%d\n", vib->min_timeout_ms);
}
static ssize_t pm8xxx_vib_min_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	int ms;
	sscanf(buf, "%d", &ms);
	vib->min_timeout_ms = ms;

	return size;
}
static DEVICE_ATTR(min_ms, S_IRUGO | S_IWUSR, pm8xxx_vib_min_ms_show, pm8xxx_vib_min_ms_store);

static ssize_t pm8xxx_vib_default_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	return sprintf(buf, "%d\n", (vib->default_level)*100);
}
static ssize_t pm8xxx_vib_default_level_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	int mv;
	sscanf(buf, "%d", &mv);
	vib->default_level = mv/100;

	return size;
}
static DEVICE_ATTR(default_level, S_IRUGO | S_IWUSR, pm8xxx_vib_default_level_show, pm8xxx_vib_default_level_store);


#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
static ssize_t pm8xxx_vib_over_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	return sprintf(buf, "%d\n", vib->overdrive_ms);
}
static ssize_t pm8xxx_vib_over_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	int ms;
	sscanf(buf, "%d", &ms);
	vib->overdrive_ms = ms;

	return size;
}
static DEVICE_ATTR(over_ms, S_IRUGO | S_IWUSR, pm8xxx_vib_over_ms_show, pm8xxx_vib_over_ms_store);


static ssize_t pm8xxx_vib_over_range_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	return sprintf(buf, "%d\n", vib->overdrive_range_ms);
}
static ssize_t pm8xxx_vib_over_range_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	int ms;
	sscanf(buf, "%d", &ms);
	vib->overdrive_range_ms = ms;

	return size;
}
static DEVICE_ATTR(over_range_ms, S_IRUGO | S_IWUSR, pm8xxx_vib_over_range_ms_show, pm8xxx_vib_over_range_ms_store);
#endif


#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_REST_POWER
static ssize_t pm8xxx_vib_min_stop_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	return sprintf(buf, "%d\n", vib->min_stop_ms);
}
static ssize_t pm8xxx_vib_min_stop_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(dev_, struct pm8xxx_vib, timed_dev);
	int ms;
	sscanf(buf, "%d", &ms);
	vib->min_stop_ms = ms;

	return size;
}
static DEVICE_ATTR(min_stop_ms, S_IRUGO | S_IWUSR, pm8xxx_vib_min_stop_ms_show, pm8xxx_vib_min_stop_ms_store);

#endif

static int __devinit pm8xxx_vib_probe(struct platform_device *pdev)

{
	const struct pm8xxx_vibrator_platform_data *pdata =
						pdev->dev.platform_data;
	struct pm8xxx_vib *vib;
	u8 val;
	int rc;

	if (!pdata)
		return -EINVAL;

	if (pdata->level_mV < VIB_MIN_LEVEL_mV ||
			 pdata->level_mV > VIB_MAX_LEVEL_mV)
		return -EINVAL;

	vib = kzalloc(sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	vib->pdata	= pdata;
	vib->level	= pdata->level_mV / 100;
	vib->dev	= &pdev->dev;

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_VOL
	vib->default_level          = vib->level;
	vib->request_normal_level   = vib->level;
	vib->normal_range_min_level = pdata->normal_range_min_mV / 100;
	vib->normal_range_max_level = pdata->normal_range_max_mV / 100;
	vib->haptic_range_min_level = pdata->haptic_range_min_mV / 100;
	vib->haptic_range_max_level = pdata->haptic_range_max_mV / 100;
	vib->haptic_default_level   = pdata->haptic_default_mV   / 100;
	vib->request_haptic_level   = vib->haptic_default_level;
	vib->active_level           = vib->haptic_default_level;
	memcpy(vib->normal_lvl_tbl, pdata->normal_lvl_table,
		sizeof(struct pm8xxx_vibrator_lvl_mv_tbl) * PM8XXX_VIBATOR_UI_LVL_TBL_MAX);
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_MIN_TIMEOUT
	vib->min_timeout_ms  = pdata->min_timeout_ms;
	vib->pre_value  = 0;
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
	vib->overdrive_level     = pdata->overdrive_mV / 100;
	vib->overdrive_ms        = pdata->overdrive_ms;
	vib->overdrive_range_ms  = pdata->overdrive_range_ms;
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_REST_POWER
	vib->min_stop_ms  = pdata->min_stop_ms;
	vib->start_tv.tv_sec = 0;
	vib->start_tv.tv_usec = 0;
	vib->stop_tv.tv_sec = 0;
	vib->stop_tv.tv_usec = 0;
#endif

	spin_lock_init(&vib->lock);
	INIT_WORK(&vib->work, pm8xxx_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = pm8xxx_vib_timer_func;

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
	hrtimer_init(&vib->vib_overdrive_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_overdrive_timer.function = pm8xxx_vib_overdrive_timer_func;
#endif

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = pm8xxx_vib_get_time;
	vib->timed_dev.enable = pm8xxx_vib_enable;

	__dump_vib_regs(vib, "boot_vib_default");

	/*
	 * Configure the vibrator, it operates in manual mode
	 * for timed_output framework.
	 */
	rc = pm8xxx_vib_read_u8(vib, &val, VIB_DRV);
	if (rc < 0)
		goto err_read_vib;
	val &= ~VIB_DRV_EN_MANUAL_MASK;
	rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);
	if (rc < 0)
		goto err_read_vib;

	vib->reg_vib_drv = val;

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0)
		goto err_read_vib;

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_VOL
	rc = device_create_file(vib->timed_dev.dev, &dev_attr_amp);
	if (rc < 0)
		goto err_read_vib;

	rc = device_create_file(vib->timed_dev.dev, &dev_attr_default_level);
	if (rc < 0)
		goto err_read_vib;
#endif

// LGE does not use this function. power on vib effect is played at SBL3
#ifndef CONFIG_MACH_LGE
	pm8xxx_vib_enable(&vib->timed_dev, pdata->initial_vibrate_ms);
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_MIN_TIMEOUT
	rc = device_create_file(vib->timed_dev.dev, &dev_attr_min_ms);
	if (rc < 0)
		goto err_read_vib;
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_OVERDRIVE
	rc = device_create_file(vib->timed_dev.dev, &dev_attr_over_ms);
	if (rc < 0)
		goto err_read_vib;

	rc = device_create_file(vib->timed_dev.dev, &dev_attr_over_range_ms);
	if (rc < 0)
		goto err_read_vib;
#endif

#ifdef CONFIG_LGE_PMIC8XXX_VIBRATOR_REST_POWER
	rc = device_create_file(vib->timed_dev.dev, &dev_attr_min_stop_ms);
	if (rc < 0)
		goto err_read_vib;
#endif

	platform_set_drvdata(pdev, vib);

	vib_dev = vib;

	return 0;

err_read_vib:
	kfree(vib);
	return rc;
}

static int __devexit pm8xxx_vib_remove(struct platform_device *pdev)
{
	struct pm8xxx_vib *vib = platform_get_drvdata(pdev);

	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	timed_output_dev_unregister(&vib->timed_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(vib);

	return 0;
}

static struct platform_driver pm8xxx_vib_driver = {
	.probe		= pm8xxx_vib_probe,
	.remove		= __devexit_p(pm8xxx_vib_remove),
	.driver		= {
		.name	= PM8XXX_VIBRATOR_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm8xxx_vib_pm_ops,
#endif
	},
};

static int __init pm8xxx_vib_init(void)
{
	return platform_driver_register(&pm8xxx_vib_driver);
}
module_init(pm8xxx_vib_init);

static void __exit pm8xxx_vib_exit(void)
{
	platform_driver_unregister(&pm8xxx_vib_driver);
}
module_exit(pm8xxx_vib_exit);

MODULE_ALIAS("platform:" PM8XXX_VIBRATOR_DEV_NAME);
MODULE_DESCRIPTION("pm8xxx vibrator driver");
MODULE_LICENSE("GPL v2");
