/***************************/
/* Made by dongwook.lee@lge.com */
/* Register control driver for MSM8960 */
/***************************/

#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/types.h>

#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>

#include <linux/slab.h>                                // kmalloc() & kfree()

#include "amas_hdmi_control.h"

#include <mach/msm_hdmi_audio.h>
//#include "mach/msm_fb.h"
#include "mach/amas_hdmi.h"


#define HDMI_OUTP(offset, value)	outpdw(MSM_HDMI_BASE+(offset), (value))
#define HDMI_INP(offset)		inpdw(MSM_HDMI_BASE+(offset))


#define MAJOR_NUMBER 250
#define HDMI_DRIVER_NAME "hdmi_set"
#define HDMI_DEVICE_DRIVER_NAME "ioctl_hdmi"


/* for POLL function */
//extern unsigned short play_flag_poll; /* dongwook.lee@lge.com, 2011-09-28 */
static ssize_t hdmi_dev_write (struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[6] = {0};
	int cnt, sampleFreq;

	cnt = copy_from_user(buf, buffer, count);

	sampleFreq = simple_strtoul(buf, NULL, 10);

//	hdmi_msm_acr_setting(sampleFreq);

	return count;
}

#if 1

/* ++ glen.lee (dongwook.lee@lge.com) start [2011/12/26] */
unsigned char ext_edid[0x80 * 4];
/* ++ glen.lee (dongwook.lee@lge.com) stop [2011/12/26] */

extern int msm_compressed_open_select_flag;
extern unsigned int compressed_open_format;	// dongwook.lee ++

static long hdmi_dev_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	hdmi_dev_info ctrl_info;
	int size, ret;
	unsigned char *edid_buf; //buf[256];

	edid_buf = (unsigned char *)kmalloc(300, GFP_KERNEL);


	if(_IOC_TYPE(cmd) != HDMI_DEV_MAGIC) return -EINVAL;
	if(_IOC_NR(cmd) >= HDMI_DEV_MAGIC) return -EINVAL;

	size = _IOC_SIZE(cmd);

	switch(cmd)
	{
		case HDMI_DEV_FS_WR:

			ret = copy_from_user((void *)&ctrl_info, (const void *)arg, size);

			switch(ctrl_info.mode)
			{
				case AMAS_COMPRESSED_OPEN_SEL:			
					msm_compressed_open_select_flag=1;
					compressed_open_format = ctrl_info.fs;
					break;
					
				case AMAS_NORMAL_OPEN_SEL: 
					msm_compressed_open_select_flag=0;
					break;
				default:
					break;
			}

			break;

		case HDMI_DEV_FS_RD:
	
			break;

		case HDMI_DEV_EDID_RD:
#if 0
			ret = hdmi_msm_read_ext_edid_block(0, edid_buf);
			if(!ret)
			{
				memcpy((unsigned char *)(ctrl_info.buff) + 1, edid_buf, 256);
				ctrl_info.buff[0] = 0x2;
				ret = copy_to_user((void *)arg, (const void *)&ctrl_info, size);
			}
#endif			
			memcpy((unsigned char *)(ctrl_info.buff) + 1, ext_edid, 256);
			ctrl_info.buff[0] = 0x2;
			ret = copy_to_user((void *)arg, (const void *)&ctrl_info, size);
			break;

		case HDMI_DEV_MODE_WR:
	
			break;

		default:
			break;
	}
	
	kfree(edid_buf);
	return 0;
}
#endif

static int hdmi_dev_open(struct inode *inode,struct file *filp)
{
   printk("hdmi_ioctl port open\n");
   return 0;
}
static int hdmi_dev_release(struct inode *inode,struct file *filp)
{
   printk("hdmi_ioctl release\n");
   return 0;
}


static const struct file_operations hdmi_fopss =
{
	.owner = THIS_MODULE,
	.open = hdmi_dev_open,
      	.release = hdmi_dev_release,
      	.write = hdmi_dev_write,
 	.unlocked_ioctl = hdmi_dev_ioctl,
};


static struct miscdevice hdmi_misc_driver = {
        .fops           = &hdmi_fopss,
        .minor          = 150,
        .name           = HDMI_DEVICE_DRIVER_NAME,
};

static int __init hdmi_set_device_init(void)
{
        return misc_register(&hdmi_misc_driver);
}

static void __exit hdmi_set_device_exit(void)
{
        misc_deregister(&hdmi_misc_driver);
}


module_init(hdmi_set_device_init);
module_exit(hdmi_set_device_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HDMI DEVICE DRIVER");



