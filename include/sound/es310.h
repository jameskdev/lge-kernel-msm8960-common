/* include/linux/es310 .h - Audience es310   voice processor driver
 *
 * Copyright (C) 2012 LGE, Inc.
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

#ifndef __LINUX_ES310_H
#define __LINUX_ES310_H

#include <linux/ioctl.h>


#define GPIO_ES310_RESET_PIN		18
#define GPIO_ES310_WAKEUP_PIN		19
#define GPIO_ES310_AUD_CODEC_LDO_EN 47 /* Audience VDD Codec */
#define GPIO_ES310_UART_TXD			39
#define GPIO_ES310_UART_RXD			38

enum {
	ES310_INVALID = 0,
	ES310_SLEEP,
	ES310_CT,		// CT
	ES310_HHS,		// HHS
	ES310_DV,		// DV
	ES310_LB_RCV,	// Loopback Receiver
	ES310_LB_SPK,	// Loopback Speaker
	ES310_APT0,		// APT for ADC0 to DAC0
	ES310_APT1,		// APT for ADC1 to DAC0
	ES310_MODE_CNT, // indicator, max count of ES310 mode
	ES310_UPDATE_FILE_FW,
};

struct es310_platform_data {
	int (*power)(bool);
	int (*aud_clk)(bool);	
};

void es310_run(int mode);

#endif /* __LINUX_ES310_H */
