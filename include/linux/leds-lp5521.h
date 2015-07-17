/*
 * LP5521 LED chip driver.
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contact: Samu Onkalo <samu.p.onkalo@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __LINUX_LP5521_H
#define __LINUX_LP5521_H

/* See Documentation/leds/leds-lp5521.txt */

struct lp5521_led_config {
	char		*name;
	u8		chan_nr;
	u8		led_current; /* mA x10, 0 if led is not connected */
	u8		max_current;
};

struct lp5521_led_pattern {
	u8 *r;
	u8 *g;
	u8 *b;
	u8 size_r;
	u8 size_g;
	u8 size_b;
};

#define LP5521_CLOCK_AUTO	0
#define LP5521_CLOCK_INT	1
#define LP5521_CLOCK_EXT	2

/* Bits in CONFIG register */
#define LP5521_PWM_HF			0x40	/* PWM: 0 = 256Hz, 1 = 558Hz */
#define LP5521_PWRSAVE_EN		0x20	/* 1 = Power save mode */
#define LP5521_CP_MODE_OFF		0	/* Charge pump (CP) off */
#define LP5521_CP_MODE_BYPASS		8	/* CP forced to bypass mode */
#define LP5521_CP_MODE_1X5		0x10	/* CP forced to 1.5x mode */
#define LP5521_CP_MODE_AUTO		0x18	/* Automatic mode selection */
#define LP5521_R_TO_BATT		4	/* R out: 0 = CP, 1 = Vbat */
#define LP5521_CLK_SRC_EXT		0	/* Ext-clk source (CLK_32K) */
#define LP5521_CLK_INT			1	/* Internal clock */
#define LP5521_CLK_AUTO			2	/* Automatic clock selection */

struct lp5521_platform_data {
	struct lp5521_led_config *led_config;
	u8	num_channels;
	u8	clock_mode;
	int	(*setup_resources)(void);
	void	(*release_resources)(void);
	void	(*enable)(bool state);
	const char *label;
	u8	update_config;
	struct lp5521_led_pattern *patterns;
	int num_patterns;
};
#define LP5521_INFO_PRINT   	(1)

#if defined(LP5521_INFO_PRINT)
#define LP5521_INFO_MSG(fmt, args...) \
		printk(KERN_INFO "[LP5521] " fmt, ##args);
#else
#define LP5521_INFO_MSG(fmt, args...)     {};
#endif

static const u8 current_index_mapped_value[256] = {
	7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,   // 14
	7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,   // 29
	8,  8,  8,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10,   // 44
	11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13,  // 59
	14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 17, 17, 17, 19,  // 74
	18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 22, 22, 23, 23, 24,  // 89
	24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31,  // 104
	31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38,  // 119
	39, 39, 40, 40, 41, 41, 42, 42, 43, 43, 44, 45, 46, 47, 48,  // 134
	49, 50, 51, 52, 52, 53, 54, 55, 56, 57, 58, 59, 60, 60, 61,  // 149
	61, 62, 62, 63, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,  // 164
	73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,  // 179
	88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,  // 194
	103,104,105,106,107,108,109,111,112,113,114,115,116,117,118, // 209
	119,121,123,124,126,127,128,129,130,131,132,133,134,135,136, // 224
	138,140,142,143,144,146,148,150,151,152,154,155,157,158,159, // 239
	160,161,162,163,164,165,166,167,168,169,170,172,174,176,178, // 254
	180
};
#endif /* __LINUX_LP5521_H */
