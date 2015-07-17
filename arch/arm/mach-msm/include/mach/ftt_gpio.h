#ifndef __FTT_GPIO_H__
#define __FTT_GPIO_H__ 	__FILE__

//#include <mach/gpio-exynos4.h>
//#include <plat/gpio-cfg.h>  //hyunjun.park
#include <linux/interrupt.h> //hyunjun.park
#include <linux/irq.h>  //hyunjun.park

#define	PM8921_GPIO_BASE			NR_GPIO_IRQS //hyunjun.park
#define	PM8921_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio - 1 + PM8921_GPIO_BASE) //hyunjun.park

#define FTT_CHIP_ENABLE		PM8921_GPIO_PM_TO_SYS(25)//EXYNOS4_GPX1(0) //hyunjun.park   ACTIVE_N
#define FTT_FREQUANCY		90//EXYNOS4_GPX1(1)//hyunjun.park
#define FTT_FREQUANCY_RevA		PM8921_GPIO_PM_TO_SYS(35) //yeonhwa.so
#define FTT_DETECT			PM8921_GPIO_PM_TO_SYS(42)//	EXYNOS4_GPX0(3)//hyunjun.park  CHG_STAT


#define FTT_CHARGE_EN		1//EXYNOS4_GPX3(4)//hyunjun.park

#endif //__FTT_GPIO_H__

