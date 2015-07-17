#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include "devices.h"

//#include CONFIG_BOARD_HEADER_FILE

#if defined(CONFIG_LGE_NFC)

#include <linux/nfc/pn544_lge.h>

#define MSM_8960_GSBI8_QUP_I2C_BUS_ID 8

static struct pn544_i2c_platform_data nfc_platform_data[] = {
	{
		.ven_gpio 	= NFC_GPIO_VEN,
		.irq_gpio 	 	= NFC_GPIO_IRQ,
		.firm_gpio	= NFC_GPIO_FIRM,
	},
};

static struct i2c_board_info msm_i2c_nxp_nfc_info[] = {
	{
		I2C_BOARD_INFO("pn544", NFC_I2C_SLAVE_ADDR),
		.platform_data = &nfc_platform_data,
		.irq = MSM_GPIO_TO_INT(NFC_GPIO_IRQ),
	}
};

static struct i2c_registry b2l_i2c_nfc_device __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8960_GSBI8_QUP_I2C_BUS_ID,
	msm_i2c_nxp_nfc_info,
	ARRAY_SIZE(msm_i2c_nxp_nfc_info),
};

static void __init lge_add_i2c_nfc_devices(void)
{
	i2c_register_board_info(b2l_i2c_nfc_device.bus,
				b2l_i2c_nfc_device.info,
				b2l_i2c_nfc_device.len);
}

void __init lge_add_nfc_devices(void)
{
	lge_add_i2c_nfc_devices();
}
#endif
