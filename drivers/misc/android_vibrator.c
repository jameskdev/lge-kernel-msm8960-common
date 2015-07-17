/*
 * android vibrator driver (msm7x27, Motor IC)
 *
 * Copyright (C) 2009 LGE, Inc.
 *
 * Author: Jinkyu Choi <jinkyu@lge.com>
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
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/stat.h>
#include "../staging/android/timed_output.h"
#include <linux/android_vibrator.h>

struct timed_vibrator_data {
	struct timed_output_dev dev;
	struct hrtimer timer;
	atomic_t vib_status;			/* on/off */

	int max_timeout;
	atomic_t vibe_gain;				/* default max gain */
	atomic_t vibe_pwm;
	struct android_vibrator_platform_data *vibe_data;
	struct work_struct work_vibrator_off;
};

static int android_vibrator_force_set(struct timed_vibrator_data *vib, int nForce, int n_value)
{
	/* Check the Force value with Max and Min force value */

	INFO_MSG("nForce : %d\n", nForce);
#if 0
	if (nForce > 128)
		nForce = 128;
	if (nForce < -128)
		nForce = -128;
#endif
	/* TODO: control the gain of vibrator */

	if (nForce == 0) {
		if (!atomic_read(&vib->vib_status))
			return 0;

		vib->vibe_data->ic_enable_set(0);
		vib->vibe_data->pwm_set(0, nForce, n_value);
		vib->vibe_data->power_set(0); /* should be checked for vibrator response time */

		atomic_set(&vib->vib_status, false);
	} else {
		if (atomic_read(&vib->vib_status))
			return 0;

		vib->vibe_data->power_set(1); /* should be checked for vibrator response time */
		vib->vibe_data->pwm_set(1, nForce, n_value);
		vib->vibe_data->ic_enable_set(1);

		atomic_set(&vib->vib_status, true);
	}
	return 0;
}

static void android_vibrator_off(struct work_struct *work)
{
	struct timed_vibrator_data *vib = container_of(work, struct timed_vibrator_data, work_vibrator_off);

	android_vibrator_force_set(vib, 0, vib->vibe_data->vibe_n_value);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct timed_vibrator_data *vib = container_of(timer, struct timed_vibrator_data, timer);

	schedule_work(&vib->work_vibrator_off);
	return HRTIMER_NORESTART;
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct timed_vibrator_data *vib = container_of(dev, struct timed_vibrator_data, dev);

	if (hrtimer_active(&vib->timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->timer);
		return ktime_to_ms(r);
	} else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct timed_vibrator_data *vib = container_of(dev, struct timed_vibrator_data, dev);
	int gain = atomic_read(&vib->vibe_gain);
	int n_value = atomic_read(&vib->vibe_pwm);

	INFO_MSG("gain = %d\n", gain);

	hrtimer_cancel(&vib->timer);
	if (value > 0) {
		if (value > vib->max_timeout)
			value = vib->max_timeout;
		android_vibrator_force_set(vib, gain, n_value);
		hrtimer_start(&vib->timer, ktime_set(value / 1000, (value % 1000) * 1000000), HRTIMER_MODE_REL);
	} else {
		android_vibrator_force_set(vib, 0, vib->vibe_data->vibe_n_value);
	}
}

static ssize_t vibrator_amp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);
	int gain = atomic_read(&(vib->vibe_gain));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t vibrator_amp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);

	int gain;
	sscanf(buf, "%d", &gain);

#if 0
	if (gain > 128 || gain < -128) {
		printk(KERN_ERR "%s invalid value: should be -128 ~ +128\n", __FUNCTION__);
		return -EINVAL;
	}
#endif
	atomic_set(&vib->vibe_gain, gain);

	return size;
}

static ssize_t vibrator_pwm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);
	int gain = atomic_read(&(vib->vibe_pwm));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t vibrator_pwm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);

	int gain;
	sscanf(buf, "%d", &gain);
	atomic_set(&vib->vibe_pwm, gain);

	return size;
}

static struct device_attribute sm100_device_attrs[] = {
	__ATTR(amp, S_IRUGO | S_IWUSR, vibrator_amp_show, vibrator_amp_store),
	__ATTR(n_val, S_IRUGO | S_IWUSR, vibrator_pwm_show, vibrator_pwm_store),
};

struct timed_vibrator_data android_vibrator_data = {
	.dev.name = "vibrator",
	.dev.enable = vibrator_enable,
	.dev.get_time = vibrator_get_time,
	.max_timeout = 30000, /* max time for vibrator enable 30 sec. */
	.vibe_data = NULL,
};

static int android_vibrator_probe(struct platform_device *pdev)
{

	int i, ret = 0;
	struct timed_vibrator_data *vib;

	platform_set_drvdata(pdev, &android_vibrator_data);
	vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);

	vib->vibe_data = (struct android_vibrator_platform_data *)pdev->dev.platform_data;

	if (vib->vibe_data->vibrator_init() < 0) {
		ERR_MSG("Android Vreg, GPIO set failed\n");
		return -ENODEV;
	}

	atomic_set(&vib->vibe_gain, vib->vibe_data->amp); /* max value is 128 */
	atomic_set(&vib->vibe_pwm, vib->vibe_data->vibe_n_value);
	atomic_set(&vib->vib_status, false);

#if 0
	android_vibrator_force_set(vib, 0, vib->vibe_data->vibe_n_value); /* disable vibrator just for initializing */
#endif

	INIT_WORK(&vib->work_vibrator_off, android_vibrator_off);
	hrtimer_init(&vib->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->timer.function = vibrator_timer_func;

	ret = timed_output_dev_register(&vib->dev);
	if (ret < 0) {
		timed_output_dev_unregister(&vib->dev);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(sm100_device_attrs); i++) {
		ret = device_create_file(vib->dev.dev, &sm100_device_attrs[i]);
		if (ret < 0) {
			timed_output_dev_unregister(&vib->dev);
			device_remove_file(vib->dev.dev, &sm100_device_attrs[i]);
			return -ENODEV;
		}
	}

	INFO_MSG("Android Vibrator Initialization was done\n");

	return 0;
}

static int android_vibrator_remove(struct platform_device *pdev)
{
	struct timed_vibrator_data *vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);

	android_vibrator_force_set(vib, 0, vib->vibe_data->vibe_n_value);
	timed_output_dev_unregister(&vib->dev);

	return 0;
}

#ifdef CONFIG_PM
static int android_vibrator_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct timed_vibrator_data *vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);
	INFO_MSG("Android Vibrator Driver Suspend\n");
	android_vibrator_force_set(vib, 0, vib->vibe_data->vibe_n_value);
	gpio_tlmm_config(GPIO_CFG(3, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(41, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(47, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);


	return 0;
}

static int android_vibrator_resume(struct platform_device *pdev)
{
	struct timed_vibrator_data *vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);

	INFO_MSG("Android Vibrator Driver Resume\n");

	android_vibrator_force_set(vib, 0, vib->vibe_data->vibe_n_value);
	gpio_tlmm_config(GPIO_CFG(3, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(41, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(47, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);


	return 0;
}
#endif

static void android_vibrator_shutdown(struct platform_device *pdev)
{
	struct timed_vibrator_data *vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);

	android_vibrator_force_set(vib, 0, vib->vibe_data->vibe_n_value);
}

static struct platform_driver android_vibrator_driver = {
	.probe = android_vibrator_probe,
	.remove = android_vibrator_remove,
	.shutdown = android_vibrator_shutdown,
#ifdef CONFIG_PM
	.suspend = android_vibrator_suspend,
	.resume = android_vibrator_resume,
#else
	.suspend = NULL,
	.resume = NULL,
#endif
	.driver = {
		.name = "android-vibrator",
	},
};

static int __init android_vibrator_init(void)
{
	INFO_MSG("Android Vibrator Driver Init\n");
	return platform_driver_register(&android_vibrator_driver);
}

static void __exit android_vibrator_exit(void)
{
	INFO_MSG("Android Vibrator Driver Exit\n");
	platform_driver_unregister(&android_vibrator_driver);
}

MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("Android Common Vibrator Driver");
MODULE_LICENSE("GPL");

late_initcall_sync(android_vibrator_init);
module_exit(android_vibrator_exit);
