/* IrDA driver for GPIO control
 * LG Electronics
 */

#include "lge_irda_gpio.h"

int lge_irda_gpio_open(int gpionum, int direction, int value)
{
	int rc = 0;
	unsigned gpio_cfg = 0;

	if(direction == GPIO_DIRECTION_IN) {
		gpio_cfg = IRDA_GPIO_CFG(gpionum, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA);

		rc = gpio_tlmm_config(gpio_cfg, GPIO_CONFIG_ENABLE);

		if(rc)
		{
			printk(KERN_ERR "[IRDA] %s : gpio_tlmm_config for GPIO_DIRECTION_IN failed", __func__);
			return rc;
		}
	}
	else {
		gpio_cfg = IRDA_GPIO_CFG(gpionum, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);

		rc = gpio_tlmm_config(gpio_cfg, GPIO_CONFIG_ENABLE);
		if(rc)
		{
			printk(KERN_ERR "[IRDA] %s : gpio_tlmm_config for GPIO_DIRECTION_OUT failed", __func__);
			return rc;
		}
   	}  
	return rc;
}

void lge_irda_gpio_write(int gpionum, int value)
{
	gpio_set_value(gpionum, value);
}

int lge_irda_gpio_read(int gpionum)
{
	return gpio_get_value(gpionum);
}

