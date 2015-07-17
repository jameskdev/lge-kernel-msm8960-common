/*
 *  felica_i2c.c
 *
 */

/*
 *  INCLUDE FILES FOR MODULE
 */
#include <linux/syscalls.h>
#include <linux/i2c-dev.h>

#include "felica_i2c.h"

/*
 *   INTERNAL DEFINITION
 */

#define I2C_FELICA_SLAVE_ADDRESS     0x56

/*
 *   INTERNAL VARIABLE
 */

static int fd = -1;

/*
 *   FUNCTION DEFINITION
 */

/*
* Description :
* Input :
* Output :
*/
int felica_i2c_open (void)
{
  mm_segment_t old_fs = get_fs();

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_I2C] felica_i2c_open\n");
  #endif

  set_fs(KERNEL_DS);
  fd = sys_open(FELICA_IC2_NAME, O_RDWR|O_NONBLOCK, 0);
  if (fd < 0)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_open : %d \n", fd);
    return fd;
  }

  set_fs(old_fs);

  return 0;
}

/*
* Description :
* Input :
* Output :
*/
int felica_i2c_release (void)
{
  int rc = 0;
  mm_segment_t old_fs = get_fs();

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_I2C] felica_i2c_release\n");
  #endif

  set_fs(KERNEL_DS);
  rc = sys_close(fd);
  if (rc < 0)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_release : %d \n", rc);
    return rc;
  }
  set_fs(old_fs);

  return 0;
}

/*
* Description :
* Input :
* Output :
*/
int felica_i2c_set_slave_address (unsigned char slave_address)
{
  int rc = -1;

  rc = sys_ioctl(fd, I2C_SLAVE, slave_address>>1); // 7-bit address

  if (rc < 0)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - sys_ioctl : %d \n",rc);
    return rc;
  }

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_I2C] slave address : 0x%02x\n",slave_address>>1);
  #endif

  return 0;
}

/*
* Description : 
* Input :
* Output :
*/
int felica_i2c_read(unsigned char reg, unsigned char *buf, size_t count)
{
  ssize_t rc = 0;
  mm_segment_t old_fs = get_fs();

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_I2C] felica_i2c_read\n");
  #endif

  set_fs(KERNEL_DS);

  /* dev/i2c-0 device file open */
  rc = felica_i2c_open();
  if (rc)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_open : %d \n",rc);
    return rc;
  }

  /* Set slave address */
  rc = felica_i2c_set_slave_address(I2C_FELICA_SLAVE_ADDRESS);
  if (rc)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_set_slave_address : %d \n",rc);
    return rc;
  }

  /* set register address */
  rc = sys_write(fd, &reg, 1);
  if (rc < 0)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - sys_write : %d \n",rc);
    return rc;
  }

  /* read register data */
  rc = sys_read(fd, buf, count);

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_I2C] read data : 0x%02x \n",*buf);
  #endif
  
  if (rc < 0)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - sys_read : %d \n",rc);
    return rc;
  }

  /* release i2c */
  rc = felica_i2c_release();
  if (rc)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_release : %d \n",rc);
    return rc;
  }

  set_fs(old_fs);
  return 0;
}

/*
* Description :
* Input :
* Output :
*/
int felica_i2c_write(unsigned char reg, unsigned char *buf, size_t count)
{
  ssize_t rc = 0;
  unsigned char write_buf[2];
  mm_segment_t old_fs = get_fs();

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_I2C] felica_i2c_write\n");
  #endif

  set_fs(KERNEL_DS);

  /* dev/i2c-0 device file open */
  rc = felica_i2c_open();
  if (rc)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_open : %d \n",rc);
    return rc;
  }

  /* set slave address */
  rc = felica_i2c_set_slave_address(I2C_FELICA_SLAVE_ADDRESS);
  if (rc)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_set_slave_address : %d \n",rc);
    return rc;
  }

  /* set register  */
  memset(write_buf,0x00,2*sizeof(unsigned char));
  write_buf[0] = reg;
  write_buf[1] = *buf;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_I2C] write_buf[0][1] : 0x%02x 0x%02x \n",write_buf[0],write_buf[1]);
  #endif

  /* write data */
  rc = sys_write(fd, write_buf, 2);
  if (rc < 0)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - sys_write : %d \n",rc);
    return rc;
  }

  /* release i2c */
  rc = felica_i2c_release();
  if (rc)
  {
    FELICA_DEBUG_MSG("[FELICA_I2C] ERROR - felica_i2c_release : %d \n",rc);
    return rc;
  }

  set_fs(old_fs);

  return 0;
}
