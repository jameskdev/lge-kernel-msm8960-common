#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
//#include <plat/gpio-cfg.h> hyunjun.park
#include <mach/ftt_gpio.h>
#include <mach/ftt_charger.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#define FTT_CHAEGER_DEBUG			2

#if (FTT_CHAEGER_DEBUG == 1)
#define DPRINTK(x...) printk(KERN_INFO"[FTT-CHARGER] : " x)
#elif (FTT_CHAEGER_DEBUG == 2)
#define DPRINTK(x...) {\
if(ftt_is_debug == 1) printk(KERN_INFO"[FTT-CHARGER] : " x); \
}
#else
#define DPRINTK(x...)   /* !!!! */
#endif


#define FTT_BOARD_VERSION							2

#define GET_FTT_FREQUNCY_POLL_VERSION	2

#define FTT_CHARGER_SYSFS							1

#define FTT_CHARGER_TIMER							0
#define FTT_CHARGER_STATUS_TIMER			1
#define FTT_SPIN_LOCK									2
#define FTT_MAGNETIC_SENSOR						1

#define FTT_FIX_CHARGING_STATUS				1

#if (FTT_BOARD_VERSION == 1)
#define FTT_EN2_HIGH_ACTIVE						0
#elif (FTT_BOARD_VERSION == 2)
#define FTT_EN2_HIGH_ACTIVE						1
#else
#define FTT_EN2_HIGH_ACTIVE						0
#endif

#define DEVICE_NAME "ftt_charger"

#define FTT_DD_MAJOR_VERSION					0
#define FTT_DD_MINOR_VERSION					0

#if FTT_CHARGER_TIMER
#define FTT_TIMER_DIS_MSEC						(3000)
#define FTT_TIMER_ENA_MSEC						(30000)
#endif

#define FTT_FREQUENCY_COMPARE_COUNT					(10)

#define FTT_PING_POLLING_COUNT								(2)//(10)

#define FTT_CHARGER_STATUS_PING_DETECT_LOOP_COUNT										(50)		// A2, A3가 ping이 늦게 나온다.

#define FTT_CHARGER_STATUS_CHARGING_LOOP_COUNT											(2)

#define FTT_BATT_CHARGING_LOW_LEVEL						(1)
#define FTT_BATT_CHARGING_WARRNING						(-1)
#define FTT_BATT_CHARGING_NO_CHARGING					(-2)
#define FTT_BATT_CHARGING_NO_DEFINED_TYPE			(-3)


#define FTT_1n_SEC						(1)
#define FTT_10n_SEC						(10)
#define FTT_100n_SEC					(100)
#define FTT_1u_SEC						(1000)
#define FTT_10u_SEC						(10000)
#define FTT_100u_SEC					(100000)
#define FTT_1m_SEC						(1000000)					/* 가장 정확함 */
#define FTT_10m_SEC					(10000000)
#define FTT_100m_SEC					(100000000)
#define FTT_1_SEC						(1000000000)
#define FTT_AVERIGE_TIME				FTT_1m_SEC


#if FTT_CHARGER_TIMER
static struct timer_list ftt_charger_timer;
#endif
#if FTT_CHARGER_STATUS_TIMER
static struct timer_list ftt_charger_status_timer;
#endif

struct ftt_charger_data {
	struct ftt_charger_pdata 	*pdata;
	struct device 			*dev;
};

static struct ftt_charger_pdata *gpdata;

struct pad_type {
	char pad_num;
	char pad_type_name[10];
	u32 ping_freq;
	u32 ping_freq_min;
	u32 ping_freq_max;
};

enum ftt_pad_type {
	FTT_PAD_A1_TYPE = 1,
};

static struct pad_type pad_type_table[] = {
		{FTT_PAD_A1_TYPE, "A1", 175000, 170000, 180000},
};

struct ant_level_type {
	u32 ping_freq;
	char ant_level;
};

static struct ant_level_type ant_level_type_A1_A5_table[] = {
		{165, 5},
		{155, 4},
		{145, 3},
		{135, 2},
		{125, 1},
		{120, 0},
};


#if FTT_CHARGER_STATUS_TIMER
#define FTT_STATUS_DEFAULT_TIMER_MSEC							(1000)
#define FTT_STATUS_START_TIMER_MSEC								(5000)
#define FTT_STATUS_STANDBY_TIMER_MSEC							(1000)
#define FTT_STATUS_PRE_DETECT_TIMER_MSEC						(100)
#define FTT_STATUS_RESET_TIMER_MSEC								(1000)//(3000)			/* 1000ms에서 3000ms로 수정 : A2, A3 빨리 리셋하면 Ping이 틀리게 나옴 */
#define FTT_STATUS_PING_DETECT_TIMER_MSEC					(100)
#define FTT_STATUS_PRE_CHARGING_TIMER_MSEC					(1000)					/* 3000ms로 수정 : B1, B2, B3 는 빨리 주파수가 아오지 않는다. */
#define FTT_STATUS_NO_PRE_CHARGING_TIMER_MSEC			(100)
#define FTT_STATUS_CHARGING_TIMER_MSEC						(1000)
#define FTT_STATUS_LOW_LEVEL_CHARGING_TIMER_MSEC		(1000)
#endif


enum ftt_charger_status {
	FTT_CHARGER_STATUS_NULL,
	FTT_CHARGER_STATUS_STANDBY,
	FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING,
	FTT_CHARGER_STATUS_CHARGING,
};

static struct ftt_status_data {
	u32 next_timer;
	enum ftt_charger_status prev_ftt_status;
	enum ftt_charger_status ftt_status;
	enum ftt_charger_status ftt_next_status;
} ftt_status_d = {FTT_STATUS_STANDBY_TIMER_MSEC, FTT_CHARGER_STATUS_NULL, FTT_CHARGER_STATUS_STANDBY, FTT_CHARGER_STATUS_STANDBY};

static u32 pad_type_num = FTT_PAD_A1_TYPE;

#if (FTT_SPIN_LOCK == 1)
spinlock_t ftt_frequency_lock;
#elif (FTT_SPIN_LOCK == 2)
spinlock_t ftt_frequency_lock;
unsigned long ftt_frequency_lock_flags;
#elif (FTT_SPIN_LOCK == 3)
static DEFINE_MUTEX(ftt_frequency_lock);
#endif

#if (FTT_MAGNETIC_SENSOR == 1)
u32 ftt_magnetic_utesla = 0;
#endif

#if (FTT_CHAEGER_DEBUG == 2)
static u32 ftt_is_debug = 0;
#endif

#if (FTT_FIX_CHARGING_STATUS == 1)
static u32 ftt_fix_charge = 0;
#endif

static u32 ftt_pre_detect_flag = 0;

static bool is_ftt_charging(void);
static u32 get_ftt_charger_next_time(void);
static bool is_ftt_charge_status(void);
static enum ftt_charger_status get_ftt_charger_current_status(void);

#if (GET_FTT_FREQUNCY_POLL_VERSION == 1)
u32 get_ftt_frequency_poll()
{
	u32 i;
	u32 count_pulse = 0;
	u32 freq;
	u8 prev, curr;
	u64 s_time, e_time;
	u32 d_time;

	s_time = ktime_to_ns(ktime_get());
	for (i=0;i<100000;i++) {
		prev = curr;
		curr = gpio_get_value(gpdata->ftt);
		if (curr == 0 && prev == 1) {
			count_pulse++;
		}
	}
	e_time = ktime_to_ns(ktime_get());
	d_time = e_time - s_time;
	freq = (u32)(FTT_1_SEC / d_time * count_pulse);
	DPRINTK("%s count= %u delta=%u %uHz\n", __FUNCTION__, count_pulse, d_time, freq);

	return freq;
}
#elif (GET_FTT_FREQUNCY_POLL_VERSION == 2)
u32 get_ftt_frequency_poll(void)
{
	u32 count_pulse = 0;
	u32 freq;
	u8 prev, curr = 0;
	u64 s_time, e_time;
	u32 d_time;

#if (FTT_SPIN_LOCK == 1)
	spin_lock(&ftt_frequency_lock);
#elif (FTT_SPIN_LOCK == 2)
	spin_lock_irqsave(&ftt_frequency_lock, ftt_frequency_lock_flags);
#elif (FTT_SPIN_LOCK == 3)
	mutex_lock(&ftt_frequency_lock);
#endif
	s_time = ktime_to_ns(ktime_get());
	for (;;) {
		prev = curr;
		curr = gpio_get_value(gpdata->ftt);
		if (curr == 0 && prev == 1) {
			count_pulse++;
		}
		e_time = ktime_to_ns(ktime_get());
		d_time = e_time - s_time;
		if (d_time >= FTT_AVERIGE_TIME) break;
	}
#if (FTT_SPIN_LOCK == 1)
	spin_unlock(&ftt_frequency_lock);
#elif (FTT_SPIN_LOCK == 2)
	spin_unlock_irqrestore(&ftt_frequency_lock, ftt_frequency_lock_flags);
#elif (FTT_SPIN_LOCK == 3)
	mutex_unlock(&ftt_frequency_lock);
#endif

	freq = (u32)(FTT_1_SEC / d_time * count_pulse);
//	DPRINTK("%s count= %u delta=%u %uHz\n", __FUNCTION__, count_pulse, d_time, freq);

	return freq;
}
#endif
u32 get_ftt_frequency_poll_ex(void)
{
	u32 max_freq = 0;
	u32 ret_freq;
	u32 i;

	for(i=0;i<FTT_FREQUENCY_COMPARE_COUNT;i++) {
		ret_freq = get_ftt_frequency_poll();
		if (max_freq < ret_freq) max_freq = ret_freq;
	}

	return max_freq;
}

u32 get_ftt_ping_frequency(void)
{
	u32 ftt_ping_frequency;
	u32 i;

	for(i=0;i<FTT_PING_POLLING_COUNT;i++) {
		ftt_ping_frequency = get_ftt_frequency_poll_ex();
		if (ftt_ping_frequency != 0) break;
	}
	DPRINTK("### PING FTT_FREQUANCY=%uHz  %u.%01ukHz\n", ftt_ping_frequency, ftt_ping_frequency/1000, ftt_ping_frequency%1000/100);

	return ftt_ping_frequency;
}

u32 get_ftt_pad_type_num(void)
{
	u32 ftt_ping_frequency;
	u32 i;
	u32 table_size;

	ftt_ping_frequency = get_ftt_ping_frequency();
	table_size = sizeof(pad_type_table)/sizeof(struct pad_type);
	for (i=0;i<table_size;i++) {
		if ( (ftt_ping_frequency >= pad_type_table[i].ping_freq_min) && (ftt_ping_frequency <= pad_type_table[i].ping_freq_max) )
			return pad_type_table[i].pad_num;
	}
	return 0;
}

char* get_ftt_pad_type_str(void)
{
	u32 ftt_ping_frequency;
	u32 i;
	u32 table_size;

	ftt_ping_frequency = get_ftt_ping_frequency();
	table_size = sizeof(pad_type_table)/sizeof(struct pad_type);
	for (i=0;i<table_size;i++) {
		if ( (ftt_ping_frequency >= pad_type_table[i].ping_freq_min) && (ftt_ping_frequency <= pad_type_table[i].ping_freq_max) )
			return pad_type_table[i].pad_type_name;
	}
	return NULL;
}

u32 get_ftt_saved_pad_type_num(void)
{
	switch (get_ftt_charger_current_status()) {
	case	FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING :
	case	FTT_CHARGER_STATUS_CHARGING :
		return pad_type_num;
		break;
	default :
		return 0;
	}
	return 0;
}

char* get_ftt_pad_type_num_to_str(u32 pad_num)
{
	u32 i;
	u32 table_size;

	table_size = sizeof(pad_type_table)/sizeof(struct pad_type);
	for (i=0;i<table_size;i++) {
		if ( pad_num == pad_type_table[i].pad_num ) {
			return pad_type_table[i].pad_type_name;
		}
	}
	return NULL;
}

char* get_ftt_saved_pad_type_str(void)
{
//	if ((pad_type_num != 0) && (is_ftt_charge_status())) {
	if (pad_type_num != 0) {
//		DPRINTK("%s ***************\n", __FUNCTION__);
		switch (get_ftt_charger_current_status()) {
		case	FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING :
		case	FTT_CHARGER_STATUS_CHARGING :
			return get_ftt_pad_type_num_to_str(pad_type_num);
			break;
		default :
			return NULL;
		}
	}
	return NULL;
}

u32 ftt_chip_enable(void)
{
#if FTT_EN2_HIGH_ACTIVE
	gpio_direction_output(gpdata->en1, 1);
#else
	gpio_direction_output(gpdata->en1, 0);
#endif
//	DPRINTK("%s FTT_CHIP_ENABLE***************\n", __FUNCTION__);

	return 0;
}

u32 ftt_chip_disable(void)
{
#if FTT_EN2_HIGH_ACTIVE
	gpio_direction_output(gpdata->en1, 0);
#else
	gpio_direction_output(gpdata->en1, 1);
#endif
//	DPRINTK("%s FTT_CHIP_DISABLE**************\n", __FUNCTION__);

	return 0;
}

bool ftt_get_detect(void)
{
	return !gpio_get_value(gpdata->detect);
}

#if FTT_CHARGER_TIMER
static void ftt_charger_timer_handler(unsigned long data)
{
	int i;
	static int chip_flag = 1;

	if (chip_flag) {
		ftt_chip_disable();
		chip_flag = 0;
		mod_timer(&ftt_charger_timer, jiffies + msecs_to_jiffies(FTT_TIMER_DIS_MSEC));
	}
	else {
		ftt_chip_enable();
		chip_flag = 1;
		DPRINTK("############ PAD TYPE = %s ############\n", get_ftt_pad_type_str());
		mod_timer(&ftt_charger_timer, jiffies + msecs_to_jiffies(FTT_TIMER_ENA_MSEC));
	}

	for(i=0;i<100000;i++);

//	mod_timer(&ftt_charger_timer, jiffies + msecs_to_jiffies(FTT_TIMER_MSEC));
	return;
}
#endif

#if FTT_CHARGER_STATUS_TIMER
static void set_ftt_charger_status(enum ftt_charger_status next_status)
{
	ftt_status_d.ftt_next_status = next_status;

	switch (ftt_status_d.ftt_next_status) {
	case FTT_CHARGER_STATUS_STANDBY :
		ftt_status_d.next_timer = FTT_STATUS_STANDBY_TIMER_MSEC;
		break;
	case FTT_CHARGER_STATUS_CHARGING :
		ftt_status_d.next_timer = FTT_STATUS_CHARGING_TIMER_MSEC;
		break;
	case FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING :
		ftt_status_d.next_timer = FTT_STATUS_LOW_LEVEL_CHARGING_TIMER_MSEC;
		break;
	default :
		ftt_status_d.next_timer = FTT_STATUS_STANDBY_TIMER_MSEC;
		break;
	}
	return;
}

static enum ftt_charger_status get_ftt_charger_current_status(void)
{
	return ftt_status_d.ftt_status;
}

static void set_ftt_charger_save_status(void)
{
	ftt_status_d.prev_ftt_status = ftt_status_d.ftt_status;
	ftt_status_d.ftt_status = ftt_status_d.ftt_next_status;
//	DPRINTK("ftt_next_status = %d, get_ftt_charger_next_time() = %d\n", ftt_status_d.ftt_next_status, get_ftt_charger_next_time());
	mod_timer(&ftt_charger_status_timer, jiffies + msecs_to_jiffies(get_ftt_charger_next_time()));
	return;
}

static bool is_ftt_charger_nochange_status(void)
{
	return ((ftt_status_d.prev_ftt_status == ftt_status_d.ftt_status) ? 1 : 0);
}

static u32 get_ftt_charger_next_time(void)
{
	return ftt_status_d.next_timer;
}

static bool is_ftt_charging(void)
{
//	if ((ftt_get_detect() == 1) && (get_ftt_charger_current_status() == FTT_CHARGER_STATUS_CHARGING)) {
	if (ftt_get_detect() == 1) {
		return 1;
	}
	return 0;
}

static bool is_ftt_low_level_charging(void)
{
	if ((ftt_get_detect() == 1) && (get_ftt_charger_current_status() == FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING)) {
		return 1;
	}
	return 0;
}

static bool is_ftt_charge_status(void)
{
	if ((ftt_get_detect() == 1) && ((get_ftt_charger_current_status() == FTT_CHARGER_STATUS_CHARGING)
			|| (get_ftt_charger_current_status() == FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING)
			)) {
		return 1;
	}
	return 0;
}

static int get_ftt_ant_level(void)
{
	int level = FTT_BATT_CHARGING_WARRNING;
	u32 ftt_frequency;
	u32 table_size;
	u32 i;
	char pad_type;

	if (is_ftt_charge_status()) {
		ftt_frequency = get_ftt_frequency_poll_ex()/1000;
		pad_type = FTT_PAD_A1_TYPE;

		switch (pad_type) {
		case FTT_PAD_A1_TYPE :
			table_size = sizeof(ant_level_type_A1_A5_table)/sizeof(struct ant_level_type);
			for (i=0;i<table_size;i++) {
				if (ant_level_type_A1_A5_table[i].ping_freq <= ftt_frequency) {
					level = ant_level_type_A1_A5_table[i].ant_level;
					break;
				}
				else {
					level = FTT_BATT_CHARGING_WARRNING;
				}
			}
			break;
		default :
			level = FTT_BATT_CHARGING_NO_DEFINED_TYPE;
			break;
		}
//		DPRINTK("FTT_FREQUANCY=%uHz  %u.%01ukHz PAD TYPE=%u LEVEL=%d\n", ftt_frequency, ftt_frequency/1000, ftt_frequency%1000/100, pad_type, level);
	}
	else {
		if (!ftt_pre_detect_flag || (get_ftt_charger_current_status() == FTT_CHARGER_STATUS_STANDBY)) {
			level = FTT_BATT_CHARGING_NO_CHARGING;
		}
		else {
			level = 5;
		}
	}

	return level;
}

static void ftt_charger_status_timer_handler(unsigned long data)
{
	u32 ftt_frequency;
	int ant_level;
	static char curr_detect = 0;
	static char batt_chg_enable = 0;

	curr_detect = ftt_get_detect();

	switch (get_ftt_charger_current_status()) {
	case	FTT_CHARGER_STATUS_STANDBY :
		if (!is_ftt_charger_nochange_status()) {
			DPRINTK("++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
			DPRINTK("+++++++++++++ FTT_CHARGER_STATUS_STANDBY +++++++++++++\n");
			DPRINTK("++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		}
		ftt_chip_enable();
		ftt_frequency = get_ftt_frequency_poll_ex();
		if (ftt_frequency/*	 && (curr_detect == 1)*/) {
			DPRINTK("@@@ STANDBY FTT_Frequency=%uHz(%u.%01ukHz)\n", ftt_frequency, ftt_frequency/1000, ftt_frequency%1000/100);
				set_ftt_charger_status(FTT_CHARGER_STATUS_CHARGING);
		}
		else {
			set_ftt_charger_status(FTT_CHARGER_STATUS_STANDBY);
		}
		break;
	case	FTT_CHARGER_STATUS_CHARGING :
		if (!is_ftt_charger_nochange_status()) {
			DPRINTK("+++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
			DPRINTK("+++++++++++++ FTT_CHARGER_STATUS_CHARGING +++++++++++++\n");
			DPRINTK("+++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		}
//		if (curr_detect == 1) {
		ftt_frequency = get_ftt_frequency_poll_ex();
		ant_level = get_ftt_ant_level();
		DPRINTK("FTT_Frequency=%uHz(%u.%01ukHz) PAD_type=%s Level=%d detect=%d\n", ftt_frequency, ftt_frequency/1000, ftt_frequency%1000/100, get_ftt_pad_type_num_to_str(pad_type_num), ant_level, curr_detect);
#if (FTT_FIX_CHARGING_STATUS == 1)
		if (ftt_fix_charge) {
			DPRINTK("FTT_FIX_CHARGING_STATUS\n");
			set_ftt_charger_status(FTT_CHARGER_STATUS_CHARGING);
			break;
		}
#endif
		if (	ftt_frequency) {
			if (ant_level <= FTT_BATT_CHARGING_WARRNING) {			//Disable Charge
				set_ftt_charger_status(FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING);
			}
			else {																			//Continue Charge
				if (!batt_chg_enable) {
					batt_chg_enable = 1;
					if (gpdata->batt_charge_enable != NULL)	gpdata->batt_charge_enable(true);
				}
				set_ftt_charger_status(FTT_CHARGER_STATUS_CHARGING);
			}
		}
		else {
			set_ftt_charger_status(FTT_CHARGER_STATUS_STANDBY);
		}
		break;
	case	FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING :
		if (!is_ftt_charger_nochange_status()) {
			DPRINTK("+++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
			DPRINTK("++++++++ FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING ++++++++\n");
			DPRINTK("+++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		}
		ftt_frequency = get_ftt_frequency_poll_ex();
		if (	ftt_frequency && (curr_detect == 1)) {
			ant_level = get_ftt_ant_level();
			DPRINTK("FTT_Frequency=%uHz(%u.%01ukHz) PAD_type=%s Level=%d\n", ftt_frequency, ftt_frequency/1000, ftt_frequency%1000/100, get_ftt_pad_type_num_to_str(pad_type_num), ant_level);
			if (ant_level >= FTT_BATT_CHARGING_LOW_LEVEL) {			//Enable Charge
				set_ftt_charger_status(FTT_CHARGER_STATUS_CHARGING);
			}
			else {																			//Disable Charge
				if (batt_chg_enable) {
					batt_chg_enable = 0;
					if (gpdata->batt_charge_enable != NULL)	gpdata->batt_charge_enable(false);
				}
			}
		}
		else {
			set_ftt_charger_status(FTT_CHARGER_STATUS_STANDBY);
		}
		break;
	default :
		set_ftt_charger_status(FTT_CHARGER_STATUS_STANDBY);
		break;
	}

	set_ftt_charger_save_status();
	return;
}
#endif

#if FTT_CHARGER_SYSFS

static	ssize_t show_ftt_total	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_total, S_IRWXUGO, show_ftt_total, NULL);
static	ssize_t show_ftt_frequency	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_frequency, S_IRWXUGO, show_ftt_frequency, NULL);
static	ssize_t show_ftt_is_charging	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_is_charging, S_IRWXUGO, show_ftt_is_charging, NULL);
static	ssize_t show_ftt_ant_level	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_ant_level, S_IRWXUGO, show_ftt_ant_level, NULL);
static	ssize_t show_ftt_pad_type	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_pad_type, S_IRWXUGO, show_ftt_pad_type, NULL);
static	ssize_t show_ftt_is_low_level	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_is_low_level, S_IRWXUGO, show_ftt_is_low_level, NULL);
static	ssize_t show_ftt_status(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_status, S_IRWXUGO, show_ftt_status, NULL);
#if (FTT_MAGNETIC_SENSOR == 1)
static	ssize_t show_ftt_magnetic_sensor_utesla(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_ftt_magnetic_sensor_utesla(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(ftt_magnetic_sensor_utesla, S_IRWXUGO, show_ftt_magnetic_sensor_utesla, set_ftt_magnetic_sensor_utesla);
#endif
#if (FTT_CHAEGER_DEBUG == 2)
static	ssize_t show_ftt_debug(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_ftt_debug(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(ftt_debug, S_IRWXUGO, show_ftt_debug, set_ftt_debug);
#endif
#if (FTT_FIX_CHARGING_STATUS == 1)
static	ssize_t show_ftt_fix_charge(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_ftt_fix_charge(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(ftt_fix_charge, S_IRWXUGO, show_ftt_fix_charge, set_ftt_fix_charge);
#endif
static	ssize_t show_ftt_ver(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_ver, S_IRWXUGO, show_ftt_ver, NULL);
static	ssize_t show_ftt_version(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(ftt_version, S_IRWXUGO, show_ftt_version, NULL);

static struct attribute *ftt_frequency_sysfs_entries[] = {
		&dev_attr_ftt_total.attr,
		&dev_attr_ftt_frequency.attr,
		&dev_attr_ftt_is_charging.attr,
		&dev_attr_ftt_ant_level.attr,
		&dev_attr_ftt_pad_type.attr,
		&dev_attr_ftt_is_low_level.attr,
		&dev_attr_ftt_status.attr,
#if (FTT_MAGNETIC_SENSOR == 1)
		&dev_attr_ftt_magnetic_sensor_utesla.attr,
#endif
#if (FTT_CHAEGER_DEBUG == 2)
		&dev_attr_ftt_debug.attr,
#endif
#if (FTT_FIX_CHARGING_STATUS == 1)
		&dev_attr_ftt_fix_charge.attr,
#endif
		&dev_attr_ftt_ver.attr,
		&dev_attr_ftt_version.attr,
		NULL
};

static struct attribute_group ftt_frequency_sysfs_attr_group = {
	.name   = NULL,
	.attrs  = ftt_frequency_sysfs_entries,
};

static	ssize_t show_ftt_total	(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf,
			"PAD TYPE : %s\n"
			"PAD NUMBER : %u\n"
			"FTT Frequency : %uHz\n"
			"LEVEL : %d\n"
#if (FTT_MAGNETIC_SENSOR == 1)
			"Megnetic uTesla : %u\n"
#endif
			,get_ftt_saved_pad_type_str()
			,get_ftt_saved_pad_type_num()
			,get_ftt_frequency_poll_ex()
			,get_ftt_ant_level()
#if (FTT_MAGNETIC_SENSOR == 1)
			,ftt_magnetic_utesla
#endif
			);
}

static 	ssize_t show_ftt_frequency		(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", get_ftt_frequency_poll_ex());
}

static	ssize_t show_ftt_is_charging	(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", is_ftt_charging());
}

static	ssize_t show_ftt_ant_level	(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_ftt_ant_level());
}

static	ssize_t show_ftt_pad_type	(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_ftt_saved_pad_type_str());
}

static	ssize_t show_ftt_is_low_level	(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", is_ftt_low_level_charging());
}

static	ssize_t show_ftt_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	switch (get_ftt_charger_current_status()) {
	case	FTT_CHARGER_STATUS_STANDBY :
		return sprintf(buf, "%s\n", "STANDBY");
		break;
	case	FTT_CHARGER_STATUS_CHARGING :
		return sprintf(buf, "%s\n", "CHARGING");
		break;
	case	FTT_CHARGER_STATUS_LOW_LEVEL_CHARGING :
		return sprintf(buf, "%s\n", "LOW LEVEL CHARGING");
		break;
	default :
		return sprintf(buf, "%s\n", "NULL");
		break;
	}
}

#if (FTT_MAGNETIC_SENSOR == 1)
static	ssize_t show_ftt_magnetic_sensor_utesla(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", ftt_magnetic_utesla);
}

static 	ssize_t set_ftt_magnetic_sensor_utesla(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%u", &ftt_magnetic_utesla);
	return count;
}
#endif

#if (FTT_CHAEGER_DEBUG == 2)
static	ssize_t show_ftt_debug(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", ftt_is_debug);
}

static 	ssize_t set_ftt_debug(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%u", &ftt_is_debug);
	return count;
}
#endif

#if (FTT_FIX_CHARGING_STATUS == 1)
static	ssize_t show_ftt_fix_charge(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", ftt_fix_charge);
}

static 	ssize_t set_ftt_fix_charge(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%u", &ftt_fix_charge);
	return count;
}
#endif

static	ssize_t show_ftt_ver(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "v%u.%u\n", FTT_DD_MAJOR_VERSION, FTT_DD_MINOR_VERSION);
}

static	ssize_t show_ftt_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s : v%u.%u ,compile : %s, %s\n", DEVICE_NAME, FTT_DD_MAJOR_VERSION, FTT_DD_MINOR_VERSION, __DATE__, __TIME__);
}

#endif

static int ftt_probe(struct platform_device *pdev)
{
//	struct ftt_charger_data *data;
	struct device *dev = &pdev->dev;
	struct ftt_charger_pdata *pdata = pdev->dev.platform_data;
	int err;

	gpdata = pdata;
//	data = kzalloc(sizeof(struct ftt_charger_data), GFP_KERNEL);
//	if (data == NULL) {
//		dev_err(dev, "Cannot allocate memory.\n");
//		return -ENOMEM;
//	}
	if(pdata == NULL)	{
		dev_err(dev, "Cannot find platform data.\n");
		return -EIO;
	}

//	data->pdata = pdata;
//	data->dev = dev;
//	platform_set_drvdata(pdev, data);
//	pdata->en1;
//	pdata->detect;
//	pdata->ftt;

	DPRINTK("%s\n", __FUNCTION__);
/* hyunjun.park
	err = gpio_request(pdata->en1, "FTT_CHIP_ENABLE");
	if (err)
		dev_err(dev, "#### failed to request FTT_CHIP_ENABLE ####\n");
	s3c_gpio_cfgpin(pdata->en1, S3C_GPIO_OUTPUT);
hyunjun.park */ 
	err = gpio_request(pdata->ftt, "FTT_FREQUANCY_SKT_REVA");
	if (err) {
		dev_err(dev, "#### failed to request FTT_FREQUANCY ####\n");
		goto err_gpio_1;
	}
	gpio_direction_input(pdata->ftt); //hyunjun.park
//	s3c_gpio_cfgpin(pdata->ftt, S3C_GPIO_INPUT); hyunjun.park

#if (FTT_SPIN_LOCK == 1)
	spin_lock_init(&ftt_frequency_lock);
#elif (FTT_SPIN_LOCK == 2)
	spin_lock_init(&ftt_frequency_lock);
#elif (FTT_SPIN_LOCK == 3)
//	mutex_init(&ftt_frequency_lock);
#endif

#if FTT_CHARGER_TIMER
	init_timer(&ftt_charger_timer);
	ftt_charger_timer.function = ftt_charger_timer_handler;
	ftt_charger_timer.data = (unsigned long)pdata;
	ftt_charger_timer.expires = jiffies + msecs_to_jiffies(FTT_TIMER_ENA_MSEC);
	add_timer(&ftt_charger_timer);
#endif

#if FTT_CHARGER_STATUS_TIMER
	init_timer(&ftt_charger_status_timer);
	ftt_charger_status_timer.function = ftt_charger_status_timer_handler;
	ftt_charger_status_timer.data = (unsigned long)pdata;
	ftt_charger_status_timer.expires = jiffies + msecs_to_jiffies(FTT_STATUS_START_TIMER_MSEC);
	add_timer(&ftt_charger_status_timer);
#endif

#if FTT_CHARGER_SYSFS
	err = sysfs_create_group(&pdev->dev.kobj, &ftt_frequency_sysfs_attr_group);
	if (err) {
		printk(KERN_ERR "#### failed to sysfs_create_group ####\n");
		goto err_ftt_sysfs;
	}
#endif

	printk(DEVICE_NAME " Initialized\n");
	return 0;

err_ftt_sysfs:
#if FTT_CHARGER_TIMER
	del_timer(&ftt_charger_timer);
#endif
#if FTT_CHARGER_STATUS_TIMER
	del_timer(&ftt_charger_status_timer);
#endif

//err_gpio_2:
	gpio_free(pdata->ftt);

err_gpio_1:
	gpio_free(pdata->en1);

	return err;
}

static int __exit ftt_remove(struct platform_device *pdev)
{
	struct ftt_charger_pdata *pdata = pdev->dev.platform_data;

	DPRINTK("%s\n", __FUNCTION__);

	gpio_free(pdata->ftt);
	gpio_free(pdata->en1);

#if FTT_CHARGER_TIMER
	del_timer(&ftt_charger_timer);
#endif
#if FTT_CHARGER_STATUS_TIMER
	del_timer(&ftt_charger_status_timer);
#endif

#if FTT_CHARGER_SYSFS
    sysfs_remove_group(&pdev->dev.kobj, &ftt_frequency_sysfs_attr_group);
#endif

	return 0;
}

static struct platform_driver ftt_charger_driver = {
	.probe		= ftt_probe, //hyunjun.park
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.remove = __exit_p(ftt_remove),
};

static int __init ftt_init(void)
{
	return platform_driver_register(&ftt_charger_driver); //hyunjun.park
//	return platform_driver_probe(&ftt_charger_driver, ftt_probe); //hyunjun.park
}
module_init(ftt_init);

static void __exit ftt_exit(void)
{
	platform_driver_unregister(&ftt_charger_driver);
}
module_exit(ftt_exit);

MODULE_AUTHOR("Uhm choon ho <query91@weaverbrige.co.kr>");
MODULE_DESCRIPTION("Driver for FTT Charget module");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:WEAVER FTT");
