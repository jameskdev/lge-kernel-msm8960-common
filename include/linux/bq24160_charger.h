/*
 * TI BQ24160 Charger Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_BQ24160_CHARGER_H
#define __LINUX_BQ24160_CHARGER_H

#define WIRELESS_CHG_MPP_11 10 // TODO: Need to check this
#define WIRELESS_CHG_MPP_12 11 // TODO: Need to check this

struct bq24160_platform_data {
	u8  tmr_rst;		/* watchdog timer reset */
	u8  supply_sel;		/* supply selection */

	u8	reset;			/* reset all reg to default values */
	u8	iusblimit;		/* usb current limit */
	u8 	enstat;			/* enable STAT */
	u8	te;				/* enable charger termination */
	u8 	ce;				/* charger enable : 0 disable : 1 */
	u8	hz_mode;		/* high impedance mode */

	u8 	vbatt_reg;		/* battery regulation voltage */
	u8	inlimit_in;		/* input limit for IN input */
	u8	dpdm_en;		/* D+/D- detention */

	u8	chgcrnt;		/* charge current */
	u8	termcrnt;		/* termination current sense */

	u8	minsys_stat;	/* minimum system voltage mode */
	u8	dpm_stat;		/* Vin-DPM mode */
	u8	vindpm_usb;		/* usb input Vin-dpm voltage */
	u8	vindpm_in;		/* IN input Vin-dpm voltage */

	u8	tmr2x_en;		/* timer slowed by 2x */
	u8	safety_tmr;		/* safety timer */
	u8	ts_en;			/* ts function enable */
	u8	ts_fault;		/* ts fault mode */
	
#ifdef CONFIG_LGE_WIRELESS_CHARGER_BQ24160
	int 	valid_n_gpio;
	int 	(*chg_detection_config) (void);
#endif
};


extern void check_wireless_charger(void);
extern int wlc_is_plugged(void);
extern int wlc_batt_status(void);

#endif

