#ifndef __FTT_CARGER_H__
#define __FTT_CARGER_H__ 	__FILE__

struct ftt_charger_pdata {
	int en1;
	int en2;
	int detect;
	int ftt;
	bool (*batt_charge_enable)(bool enable);
};

#endif //__FTT_CARGER_H__
