
#ifndef _UEI_IRRC_UART_H
#define _UEI_IRRC_UART_H

#define GPIO_RC_LEVEL_EN	38

#define UEI_IRRC_NAME "UEI_IRRC"

struct uei_irrc_pdata_type{
	unsigned int en_gpio;
	int en_state;
	struct work_struct uei_irrc_work;
};



#endif /* _UEI_IRRC_UART_H */

