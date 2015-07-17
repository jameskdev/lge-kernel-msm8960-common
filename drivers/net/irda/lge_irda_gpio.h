#ifndef __LGE_IRDA_GPIO_H__
#define __LGE_IRDA_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  INCLUDE FILES FOR MODULE
 */
#include <linux/kernel.h>
#include <mach/gpio.h>

#define IRDA_GPIO_CFG(gpio, func, dir, pull, drvstr) \
    ((((gpio) & 0x3FF) << 4)        |   \
    ((func) & 0xf)                  |   \
    (((dir) & 0x1) << 14)           |   \
    (((pull) & 0x3) << 15)          |   \
    (((drvstr) & 0xF) << 17))

enum{
  GPIO_DIRECTION_IN = 0,
  GPIO_DIRECTION_OUT,
};

enum{
  GPIO_LOW_VALUE = 0,
  GPIO_HIGH_VALUE,
};

enum{
  GPIO_CONFIG_ENABLE = 0,
  GPIO_CONFIG_DISABLE,
};

#define GPIO_IRDA_SW_EN_ENABLE	GPIO_HIGH_VALUE
#define GPIO_IRDA_SW_EN_DISABLE	GPIO_LOW_VALUE

#define GPIO_IRDA_PWDN_ENABLE	GPIO_LOW_VALUE
#define GPIO_IRDA_PWDN_DISABLE	GPIO_HIGH_VALUE

#define GPIO_IRDA_SW_EN	38
#define GPIO_IRDA_PWDN	63

int lge_irda_gpio_open(int gpionum, int direction, int value);
void lge_irda_gpio_write(int gpionum, int value);
int lge_irda_gpio_read(int gpionum);


#ifdef __cplusplus
}
#endif

#endif /* __LGE_IRDA_GPIO_H__ */
