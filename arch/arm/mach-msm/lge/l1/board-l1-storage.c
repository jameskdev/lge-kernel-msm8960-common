/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/board_lge.h>
#include <mach/gpiomux.h>
#include "devices.h"
/*
 * include LGE board specific header file
 */
#include "board-l1.h"
#include CONFIG_BOARD_HEADER_FILE
#include "board-storage-common-a.h"
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
#include <linux/random.h> //for test . Get Random MAC address when booting time.

/* MSM8960 has 5 SDCC controllers */
enum sdcc_controllers {
	SDCC1,
	SDCC2,
	SDCC3,
	SDCC4,
	SDCC5,
	MAX_SDCC_CONTROLLER
};

/* All SDCC controllers require VDD/VCC voltage */
/*LGE_CHANGE_S [jyothishre.nk@lge.com] 2012-07-02*/
/*Due to insufficient drive strengths for SDC GPIO lines SD/MMC cards 
 *may cause data CRC errors. Hence, set optimal values for SDC slots 
 *based on timing closure and marginality.
 *SDC1 slot require higher value since it should handle bad signal quality due
 *to size of T-flash adapters.
 *sleep current S_C_VCC is changed from 9mA to 18mA
*/
static struct msm_mmc_reg_data mmc_vdd_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.name = "sdc_vdd",
		.high_vol_level = 2950000,
		.low_vol_level = 2950000,
		.always_on = 1,
		.lpm_sup = 1,
		.lpm_uA = 18000, //9000-->18000 (18mA) 
		.hpm_uA = 200000, /* 200mA */
	},
	/* SDCC2 : SDIO slot connected */
	[SDCC2] = {
		.name = "sdc_vdd",
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.always_on = 1,
		.lpm_sup = 1,
		.lpm_uA = 9000,
		.hpm_uA = 200000, /* 200mA */
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.name = "sdc_vdd",
		.high_vol_level = 2950000,
		.low_vol_level = 2950000,
		.hpm_uA = 600000, /* 600mA */
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	},
	/* SDCC4 : External card slot connected */
	[SDCC4] = {
		.name = "sdc_vdd",
		.always_on = 1, // for test
		.set_voltage_sup = 0,
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.hpm_uA = 600000, /* 600mA */
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
	}
};

/* SDCC controllers may require voting for IO operating voltage */
static struct msm_mmc_reg_data mmc_vdd_io_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.name = "sdc_vdd_io",
		.always_on = 1,
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.hpm_uA = 200000, /* 200mA */
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.name = "sdc_vdd_io",
		.high_vol_level = 2950000,
		.low_vol_level = 1850000,
		.always_on = 1,
		.lpm_sup = 1,
		/* Max. Active current required is 16 mA */
		.hpm_uA = 16000,
		/*
		 * Sleep current required is ~300 uA. But min. vote can be
		 * in terms of mA (min. 1 mA). So let's vote for 2 mA
		 * during sleep.
		 */
		.lpm_uA = 2000,
	},
	/* SDCC4 : SDIO slot connected */
	[SDCC4] = {
		.name = "sdc_vdd_io",
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.always_on = 1,
		.lpm_sup = 1,
		.hpm_uA = 200000, /* 200mA */
		.lpm_uA = 2000,
	},
};

static struct msm_mmc_slot_reg_data mmc_slot_vreg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC1],
		.vdd_io_data = &mmc_vdd_io_reg_data[SDCC1],
	},
	/* SDCC2 : SDIO card slot connected */
	[SDCC2] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC2],
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC3],
		.vdd_io_data = &mmc_vdd_io_reg_data[SDCC3],
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	},
	/* SDCC4 : External card slot connected */
	[SDCC4] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC4],
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
	},
};

/* SDC1 pad data */
static struct msm_mmc_pad_drv sdc1_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_16MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_10MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_10MA}
};

static struct msm_mmc_pad_drv sdc1_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

/* SDC3 pad data */
static struct msm_mmc_pad_drv sdc3_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_8MA}
};

static struct msm_mmc_pad_drv sdc3_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc3_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC3_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc3_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC3_CLK, GPIO_CFG_NO_PULL},
	/*
	 * SDC3 CMD line should be PULLed UP otherwise fluid platform will
	 * see transitions (1 -> 0 and 0 -> 1) on card detection line,
	 * which would result in false card detection interrupts.
	 */
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_UP},
	/*
	 * Keeping DATA lines status to PULL UP will make sure that
	 * there is no current leak during sleep if external pull up
	 * is connected to DATA lines.
	 */
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_UP}
};

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
/* SDC4 pad data */
static struct msm_mmc_pad_drv sdc4_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC4_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC4_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC4_DATA, GPIO_CFG_8MA}
};

static struct msm_mmc_pad_drv sdc4_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC4_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC4_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC4_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc4_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC4_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC4_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc4_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC4_CMD, GPIO_CFG_PULL_DOWN},
	{TLMM_PULL_SDC4_DATA, GPIO_CFG_PULL_DOWN}
};
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

static struct msm_mmc_pad_pull_data mmc_pad_pull_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_pull_on_cfg,
		.off = sdc1_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_pull_on_cfg)
	},
	[SDCC3] = {
		.on = sdc3_pad_pull_on_cfg,
		.off = sdc3_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc3_pad_pull_on_cfg)
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.on = sdc4_pad_pull_on_cfg,
		.off = sdc4_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc4_pad_pull_on_cfg)
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};

static struct msm_mmc_pad_drv_data mmc_pad_drv_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_drv_on_cfg,
		.off = sdc1_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_drv_on_cfg)
	},
	[SDCC3] = {
		.on = sdc3_pad_drv_on_cfg,
		.off = sdc3_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc3_pad_drv_on_cfg)
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.on = sdc4_pad_drv_on_cfg,
		.off = sdc4_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc4_pad_drv_on_cfg)
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

};
struct msm_mmc_gpio sdc2_gpio[] = {
	{92, "sdc2_dat_3"},
	{91, "sdc2_dat_2"},
	{90, "sdc2_dat_1"},
	{89, "sdc2_dat_0"},
	{97, "sdc2_cmd"},
	{98, "sdc2_clk"}
};

struct msm_mmc_gpio sdc4_gpio[] = {
	{83, "sdc4_dat_3"},
	{84, "sdc4_dat_2"},
	{85, "sdc4_dat_1"},
	{86, "sdc4_dat_0"},
	{87, "sdc4_cmd"},
	{88, "sdc4_clk"}
};

struct msm_mmc_gpio_data mmc_gpio_data[MAX_SDCC_CONTROLLER] = {
	[SDCC2] = {
		.gpio = sdc2_gpio,
		.size = ARRAY_SIZE(sdc2_gpio),
	},
	[SDCC4] = {
		.gpio = sdc4_gpio,
		.size = ARRAY_SIZE(sdc4_gpio),
	},
};

static struct msm_mmc_pad_data mmc_pad_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pull = &mmc_pad_pull_data[SDCC1],
		.drv = &mmc_pad_drv_data[SDCC1]
	},
	[SDCC3] = {
		.pull = &mmc_pad_pull_data[SDCC3],
		.drv = &mmc_pad_drv_data[SDCC3]
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.pull = &mmc_pad_pull_data[SDCC4],
		.drv = &mmc_pad_drv_data[SDCC4]
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};

static struct msm_mmc_pin_data mmc_slot_pin_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pad_data = &mmc_pad_data[SDCC1],
	},
	[SDCC2] = {
		.is_gpio = 1,
		.gpio_data = &mmc_gpio_data[SDCC2],
	},
	[SDCC3] = {
		.pad_data = &mmc_pad_data[SDCC3],
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.pad_data = &mmc_pad_data[SDCC4],
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};

#define MSM_MPM_PIN_SDC1_DAT1	17
#define MSM_MPM_PIN_SDC3_DAT1	21

static unsigned int sdc1_sup_clk_rates[] = {
	400000, 24000000, 48000000, 96000000
};

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static unsigned int sdc3_sup_clk_rates[] = {
	400000, 24000000, 48000000, 96000000, 192000000
};
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct mmc_platform_data msm8960_sdc1_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
#else
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#endif
	.sup_clk_table	= sdc1_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc1_sup_clk_rates),
	.nonremovable	= 1,
	.vreg_data	= &mmc_slot_vreg_data[SDCC1],
	.pin_data	= &mmc_slot_pin_data[SDCC1],
	.mpm_sdiowakeup_int = MSM_MPM_PIN_SDC1_DAT1,
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
	.uhs_caps2	= MMC_CAP2_HS200_1_8V_SDR,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static unsigned int sdc2_sup_clk_rates[] = {
	400000, 24000000, 48000000
};

static struct mmc_platform_data msm8960_sdc2_data = {
	.ocr_mask       = MMC_VDD_165_195,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table  = sdc2_sup_clk_rates,
	.sup_clk_cnt    = ARRAY_SIZE(sdc2_sup_clk_rates),
	.vreg_data      = &mmc_slot_vreg_data[SDCC2],
	.pin_data       = &mmc_slot_pin_data[SDCC2],
	.sdiowakeup_irq = MSM_GPIO_TO_INT(90),
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct mmc_platform_data msm8960_sdc3_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc3_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc3_sup_clk_rates),
#ifdef CONFIG_MMC_MSM_SDC3_WP_SUPPORT
	.wpswitch_gpio	= PM8921_GPIO_PM_TO_SYS(16),
#endif
	.vreg_data	= &mmc_slot_vreg_data[SDCC3],
	.pin_data	= &mmc_slot_pin_data[SDCC3],
//#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
	.status_gpio	= PM8921_GPIO_PM_TO_SYS(26),
	.status_irq	= PM8921_GPIO_IRQ(PM8921_IRQ_BASE, 26),
	.irq_flags	= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.is_status_gpio_active_low = false,
//#endif
	.xpc_cap	= 1,
	.uhs_caps	= (MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_DDR50 |
			MMC_CAP_UHS_SDR104 | MMC_CAP_MAX_CURRENT_600),
	.mpm_sdiowakeup_int = MSM_MPM_PIN_SDC3_DAT1,
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static unsigned int sdc4_sup_clk_rates[] = {
	400000, 24000000, 48000000
};
static unsigned int g_wifi_detect;
static void *sdc4_dev;
void (*sdc4_status_cb)(int card_present, void *dev);
#ifdef CONFIG_MACH_MSM8960_L1v
extern void
msmsdcc_set_mmc_enable(int card_present, void *dev_id);
#endif

int sdc4_status_register(void (*cb)(int card_present, void *dev), void *dev)
{
	if(sdc4_status_cb) {
		return -EINVAL;
	}
	sdc4_status_cb = cb;
	sdc4_dev = dev;
	return 0;
}

unsigned int sdc4_status(struct device *dev)
{
	return g_wifi_detect;
}
static struct mmc_platform_data msm8960_sdc4_data = {
	.ocr_mask       = MMC_VDD_165_195 | MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc4_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc4_sup_clk_rates),
	//moon-wifi@lge.com by kwisuk.kwon 20120813 : pclk_src_dfab field is deleted on JB.
	.nonremovable	= 1,
	.vreg_data	= &mmc_slot_vreg_data[SDCC4],
	.pin_data	= &mmc_slot_pin_data[SDCC4],

	.status		= sdc4_status,
	.register_status_notify	= sdc4_status_register,
};
#endif

#if defined(CONFIG_MMC_MSM_SDC4_SUPPORT)


#define WLAN_HOSTWAKE 81
#define WLAN_POWER 82
static unsigned wlan_wakes_msm[] = {
	GPIO_CFG(WLAN_HOSTWAKE, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };

/* for wifi power supply */
static unsigned wifi_config_power_on[] = {
	GPIO_CFG(WLAN_POWER, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };


// For broadcom
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};
#ifdef CONFIG_MACH_MSM8960_L1v
static int sdc4_clock_force_disable=0;
static int get_sdc4_clock_force_disable(void)
{

	return sdc4_clock_force_disable;
}
static void set_sdc4_clock_force_disable(int enable)
{
	sdc4_clock_force_disable=enable;
	return;
}
EXPORT_SYMBOL(get_sdc4_clock_force_disable);
EXPORT_SYMBOL(set_sdc4_clock_force_disable);

#endif
void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;
	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
}
	wlan_static_scan_buf0 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf0)
		goto err_mem_alloc;
	wlan_static_scan_buf1 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf1)
		goto err_mem_alloc;

	printk("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

int bcm_wifi_set_power(int enable)
{
	int ret = 0;
#ifdef CONFIG_MACH_MSM8960_L1v
	msmsdcc_set_mmc_enable(enable,sdc4_dev);
	set_sdc4_clock_force_disable(enable);
#endif
	if (enable)
	{
		ret = gpio_direction_output(WLAN_POWER, 1); 
		if (ret) 
		{
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up (%d)\n",
					__func__, ret);
			return -EIO;
		}

		// WLAN chip to reset
		//mdelay(300);
		mdelay(120); //to save booting time
		printk(KERN_ERR "%s: wifi power successed to pull up\n",__func__);

	}
	else{
		ret = gpio_direction_output(WLAN_POWER, 0); 
		if (ret) 
		{
			printk(KERN_ERR "%s:  WL_REG_ON  failed to pull down (%d)\n",
					__func__, ret);
			return -EIO;
		}

		// WLAN chip down 
		//mdelay(50);
		mdelay(10);  //to save booting time
		printk(KERN_ERR "%s: wifi power successed to pull down\n",__func__);
	}

	return ret;
}

EXPORT_SYMBOL(bcm_wifi_set_power);

int __init bcm_wifi_init_gpio_mem(void)
{
	int rc=0;

	if (gpio_tlmm_config(wifi_config_power_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure WLAN_POWER\n", __func__);

	if (gpio_request(WLAN_POWER, "WL_REG_ON"))		
		printk("Failed to request gpio %d for WL_REG_ON\n", WLAN_POWER);	

	if (gpio_direction_output(WLAN_POWER, 0)) 
		printk(KERN_ERR "%s: WL_REG_ON  failed to pull down \n", __func__);
	//gpio_free(WLAN_POWER);

	if (gpio_request(WLAN_HOSTWAKE, "wlan_wakes_msm"))		
		printk("Failed to request gpio %d for wlan_wakes_msm\n", WLAN_HOSTWAKE);			

	rc = gpio_tlmm_config(wlan_wakes_msm[0], GPIO_CFG_ENABLE);	
	if (rc)		
		printk(KERN_ERR "%s: Failed to configure wlan_wakes_msm = %d\n",__func__, rc);

	//gpio_free(WLAN_HOSTWAKE); 

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

	printk("bcm_wifi_init_gpio_mem successfully \n");
	
	return 0;
}

static int bcm_wifi_reset(int on)
{
	return 0;
}

static int bcm_wifi_carddetect(int val)
{
	g_wifi_detect = val;
	if(sdc4_status_cb)
		sdc4_status_cb(val, sdc4_dev);
	else
		printk("%s:There is no callback for notify\n", __FUNCTION__);
	return 0;
}

static int bcm_wifi_get_mac_addr(unsigned char* buf)
{
    uint rand_mac;
    static unsigned char mymac[6] = {0,};
    const unsigned char nullmac[6] = {0,};
    pr_debug("%s: %p\n", __func__, buf);

    printk("[%s] Entering...in Board-l1-mmc.c\n", __func__  );

    if( buf == NULL ) return -EAGAIN;

    if( memcmp( mymac, nullmac, 6 ) != 0 )
    {
        /* Mac displayed from UI are never updated..
           So, mac obtained on initial time is used */
        memcpy( buf, mymac, 6 );
        return 0;
    }

    srandom32((uint)jiffies);
    rand_mac = random32();
    buf[0] = 0x00;
    buf[1] = 0x90;
    buf[2] = 0x4c;
    buf[3] = (unsigned char)rand_mac;
    buf[4] = (unsigned char)(rand_mac >> 8);
    buf[5] = (unsigned char)(rand_mac >> 16);

    memcpy( mymac, buf, 6 );
	
    printk("[%s] Exiting. MyMac :  %x : %x : %x : %x : %x : %x \n",__func__ , buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );

    return 0;
}

static struct wifi_platform_data bcm_wifi_control = {
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	.set_power	= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr, // Get custom MAC address
#ifdef CONFIG_MACH_MSM8960_L1v
	.get_sdc4_clock_force_disable = get_sdc4_clock_force_disable,
#endif
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = MSM_GPIO_TO_INT(WLAN_HOSTWAKE),
		.end   = MSM_GPIO_TO_INT(WLAN_HOSTWAKE),
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(wifi_resource),
	.resource       = wifi_resource,
	.dev            = {
		.platform_data = &bcm_wifi_control,
	},
};
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

void __init msm8960_init_mmc(void)
{
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	/*
	 * When eMMC runs in DDR mode on CDP platform, we have
	 * seen instability due to DATA CRC errors. These errors are
	 * attributed to long physical path between MSM and eMMC on CDP.
	 * So let's not enable the DDR mode on CDP platform but let other
	 * platforms take advantage of eMMC DDR mode.
	 */
	if (!machine_is_msm8960_cdp())
		msm8960_sdc1_data.uhs_caps |= (MMC_CAP_1_8V_DDR |
					       MMC_CAP_UHS_DDR50);
	/* SDC1 : eMMC card connected */
	msm_add_sdcc(1, &msm8960_sdc1_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	/* SDC2: SDIO slot for WLAN*/
	msm_add_sdcc(2, &msm8960_sdc2_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	/* SDC3: External card slot */
	msm_add_sdcc(3, &msm8960_sdc3_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	/* SDC4: SDIO slot for WLAN */
	msm_add_sdcc(4, &msm8960_sdc4_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	bcm_wifi_init_gpio_mem();
	platform_device_register(&bcm_wifi_device);
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

}
void __init lge_add_mmc_devices(void)
{
        msm8960_init_mmc();
}

