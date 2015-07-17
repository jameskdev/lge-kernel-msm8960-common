
/*
UEI_IRRC_DRIVER_FOR_MSM9860 
*/

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/sysdev.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/version.h>
#include <mach/gpio.h>

#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/regulator/consumer.h>

#include "lge_uei_irrc.h"


static struct uei_irrc_pdata_type uei_irrc_pdata = {
	.en_gpio = GPIO_RC_LEVEL_EN,
	.en_state = 0,
};

static int uei_irrc_probe(struct platform_device *pdev)
{
	struct uei_irrc_pdata_type *pdata = pdev->dev.platform_data;
	static struct regulator *IrRC_3v;
	int rc = -EINVAL;

	printk("%s\n", __FUNCTION__);

	IrRC_3v = regulator_get(NULL, "8921_l10");		/* +3V0_IrRC */
	if (IS_ERR(IrRC_3v)) 
	{
		pr_err("%s: regulator get of 8921_l10 failed (%ld)\n", __func__, PTR_ERR(IrRC_3v));
		rc = PTR_ERR(IrRC_3v);
	}

	rc = regulator_set_voltage(IrRC_3v, 3000000, 3000000);
	
	if(rc)
	{
		pr_err("%s: regulator set of 8921_l10 failed (%ld)\n", __func__, PTR_ERR(IrRC_3v));
	}

	rc = regulator_enable(IrRC_3v);

	if(rc)
	{
		pr_err("%s: regulator enable of 8921_l10 failed (%ld)\n", __func__, PTR_ERR(IrRC_3v));
	}

	
	gpio_request(pdata->en_gpio, "RC_LEVEL_EN");
	gpio_tlmm_config(GPIO_CFG(pdata->en_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(pdata->en_gpio,1);


	return rc;
}

static int uei_irrc_remove(struct platform_device *pdev)
{

    struct uei_irrc_pdata_type *pdata = platform_get_drvdata(pdev);

	printk(KERN_ERR "[UEI IcRC] remove (err:%d)\n", 104);

	pdata = NULL;

	return 0;
}

static int uei_irrc_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("%s\n", __FUNCTION__);
	return 0;
}

static int uei_irrc_resume(struct platform_device *pdev)
{
	printk("%s\n", __FUNCTION__);	
	return 0;
}

static struct platform_driver uei_irrc_driver = {
	.probe		= uei_irrc_probe,
	.remove		= uei_irrc_remove,
	.suspend		= uei_irrc_suspend,
	.resume		= uei_irrc_resume,
	.driver		= {
		.name	= UEI_IRRC_NAME,
		.owner	= THIS_MODULE,
	},
};
static struct platform_device *uei_irrc_platform_device = NULL;

static int __init uei_irrc_init(void)
{
	int ret=0;
	printk("%s\n", __FUNCTION__);

	ret = platform_driver_register(&uei_irrc_driver);

	if(ret)
		printk("%s : init fail\n" ,__FUNCTION__);

	uei_irrc_platform_device = platform_device_alloc(UEI_IRRC_NAME, -1);
	if(!uei_irrc_platform_device){
		ret = -ENOMEM;
		printk("%s : fail to driver unregister\n", __FUNCTION__);
		return ret;
	}
	uei_irrc_platform_device->dev.platform_data = &uei_irrc_pdata;

	ret = platform_device_add(uei_irrc_platform_device);

	if(ret)
		printk("%s : init fail\n" ,__FUNCTION__);
	
	return ret;
}

static void __exit uei_irrc_exit(void)
{
	printk("%s\n", __FUNCTION__);

	platform_driver_unregister(&uei_irrc_driver);
}

module_init(uei_irrc_init);
module_exit(uei_irrc_exit);

MODULE_AUTHOR("LG Electronics");
MODULE_DESCRIPTION("UEI IrRC Driver");
MODULE_LICENSE("GPL");


