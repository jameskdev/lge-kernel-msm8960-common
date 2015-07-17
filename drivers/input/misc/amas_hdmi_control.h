/***************************/
/* Made by dongwook.lee@lge.com */
/* Register control driver for MSM8960 */
/***************************/
#ifndef _HDMI_DEV_H_
#define _HDMI_DEV_H_
#define HDMI_DEV_MAGIC	't'

//HDMI_DEV_FS_WR : hdmi_dev_info.mode;
#define AMAS_COMPRESSED_OPEN_SEL		 1
#define AMAS_NORMAL_OPEN_SEL	 2

typedef struct
{
	unsigned int fs;
	unsigned int mode;
	unsigned char buff[300];
}__attribute__((packed)) hdmi_dev_info;


// IOW는 write용으로
// IOR은 read 용으로
// IO는 그냥 파라메터 없는 놈
#define HDMI_DEV_FS_WR				_IOW (HDMI_DEV_MAGIC, 0, hdmi_dev_info)
#define HDMI_DEV_MODE_WR			_IOW (HDMI_DEV_MAGIC, 1, hdmi_dev_info)
#define HDMI_DEV_MODE_RD			_IOR (HDMI_DEV_MAGIC, 2, hdmi_dev_info)
#define HDMI_DEV_FS_RD				_IOR (HDMI_DEV_MAGIC, 3, hdmi_dev_info)
#define HDMI_DEV_EDID_RD			_IOR (HDMI_DEV_MAGIC, 4, hdmi_dev_info)
#define HDMI_DEV_ACCESS_WR			_IOW (HDMI_DEV_MAGIC, 5, hdmi_dev_info)		// set access authorization
#define HDMI_DEV_FMT_WR 			_IOW (HDMI_DEV_MAGIC, 6, hdmi_dev_info)
/* 2011-08-29 stop */


#endif	//_HDMI_DEV_H_
