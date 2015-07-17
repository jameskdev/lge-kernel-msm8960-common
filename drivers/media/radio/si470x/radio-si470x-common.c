/*
 *  drivers/media/radio/si470x/radio-si470x-common.c
 * 
 *  Driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/*
 * History:
 * 2008-01-12	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.0
 *		- First working version
 * 2008-01-13	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.1
 *		- Improved error handling, every function now returns errno
 *		- Improved multi user access (start/mute/stop)
 *		- Channel doesn't get lost anymore after start/mute/stop
 *		- RDS support added (polling mode via interrupt EP 1)
 *		- marked default module parameters with *value*
 *		- switched from bit structs to bit masks
 *		- header file cleaned and integrated
 * 2008-01-14	Tobias Lorenz <tobias.lorenz@gmx.net>
 * 		Version 1.0.2
 * 		- hex values are now lower case
 * 		- commented USB ID for ADS/Tech moved on todo list
 * 		- blacklisted si470x in hid-quirks.c
 * 		- rds buffer handling functions integrated into *_work, *_read
 * 		- rds_command in si470x_poll exchanged against simple retval
 * 		- check for firmware version 15
 * 		- code order and prototypes still remain the same
 * 		- spacing and bottom of band codes remain the same
 * 2008-01-16	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.3
 * 		- code reordered to avoid function prototypes
 *		- switch/case defaults are now more user-friendly
 *		- unified comment style
 *		- applied all checkpatch.pl v1.12 suggestions
 *		  except the warning about the too long lines with bit comments
 *		- renamed FMRADIO to RADIO to cut line length (checkpatch.pl)
 * 2008-01-22	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.4
 *		- avoid poss. locking when doing copy_to_user which may sleep
 *		- RDS is automatically activated on read now
 *		- code cleaned of unnecessary rds_commands
 *		- USB Vendor/Product ID for ADS/Tech FM Radio Receiver verified
 *		  (thanks to Guillaume RAMOUSSE)
 * 2008-01-27	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.5
 *		- number of seek_retries changed to tune_timeout
 *		- fixed problem with incomplete tune operations by own buffers
 *		- optimization of variables and printf types
 *		- improved error logging
 * 2008-01-31	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.6
 *		- fixed coverity checker warnings in *_usb_driver_disconnect
 *		- probe()/open() race by correct ordering in probe()
 *		- DMA coherency rules by separate allocation of all buffers
 *		- use of endianness macros
 *		- abuse of spinlock, replaced by mutex
 *		- racy handling of timer in disconnect,
 *		  replaced by delayed_work
 *		- racy interruptible_sleep_on(),
 *		  replaced with wait_event_interruptible()
 *		- handle signals in read()
 * 2008-02-08	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.7
 *		- usb autosuspend support
 *		- unplugging fixed
 * 2008-05-07	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.8
 *		- hardware frequency seek support
 *		- afc indication
 *		- more safety checks, let si470x_get_freq return errno
 *		- vidioc behavior corrected according to v4l2 spec
 * 2008-10-20	Alexey Klimov <klimov.linux@gmail.com>
 * 		- add support for KWorld USB FM Radio FM700
 * 		- blacklisted KWorld radio in hid-core.c and hid-ids.h
 * 2008-12-03	Mark Lord <mlord@pobox.com>
 *		- add support for DealExtreme USB Radio
 * 2009-01-31	Bob Ross <pigiron@gmx.com>
 *		- correction of stereo detection/setting
 *		- correction of signal strength indicator scaling
 * 2009-01-31	Rick Bronson <rick@efn.org>
 *		Tobias Lorenz <tobias.lorenz@gmx.net>
 *		- add LED status output
 *		- get HW/SW version from scratchpad
 * 2009-06-16   Edouard Lafargue <edouard@lafargue.name>
 *		Version 1.0.10
 *		- add support for interrupt mode for RDS endpoint,
 *                instead of polling.
 *                Improves RDS reception significantly
 */


/* kernel includes */
#include "radio-si470x.h"

#include <linux/delay.h>
#include <linux/gpio.h>


/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Spacing (kHz) */
/* 0: 200 kHz (USA, Australia) */
/* 1: 100 kHz (Europe, Japan) */
/* 2:  50 kHz */
static unsigned short space = 0;	
module_param(space, ushort, 0444);
MODULE_PARM_DESC(space, "Spacing: 0=200kHz 1=100kHz *2=50kHz*");

/* Bottom of Band (MHz) */
/* 0: 87.5 - 108 MHz (USA, Europe)*/
/* 1: 76   - 108 MHz (Japan wide band) */
/* 2: 76   -  90 MHz (Japan) */
static unsigned short band = 0;
module_param(band, ushort, 0444);
MODULE_PARM_DESC(band, "Band: 0=87.5..108MHz *1=76..108MHz* 2=76..90MHz");

/* De-emphasis */
/* 0: 75 us (USA) */
/* 1: 50 us (Europe, Australia, Japan) */
static unsigned short de = 1;
module_param(de, ushort, 0444);
MODULE_PARM_DESC(de, "De-emphasis: 0=75us *1=50us*");

/* Tune timeout */
static unsigned int tune_timeout = 3000;
module_param(tune_timeout, uint, 0644);
MODULE_PARM_DESC(tune_timeout, "Tune timeout: *3000*");

/* Seek timeout */
static unsigned int seek_timeout = 5000;
module_param(seek_timeout, uint, 0644);
MODULE_PARM_DESC(seek_timeout, "Seek timeout: *5000*");

// Determines if the basic or advanced PS/RT RDS display functions are used
// The basic functions are closely tied to the recommendations in the RBDS
// specification and are faster to update but more error prone. The advanced
// functions attempt to detect errors and only display complete messages
bool rdsBasic = 0;

// RDS Program Identifier
//unsigned short piDisplay;              // Displayed Program Identifier

// RDS Program Type
//unsigned char ptyDisplay;             // Displayed Program Type

unsigned char rdsBlerMax[4];

// RDS Radio Text
char rtDisplay[64];   // Displayed Radio Text
//char rtSimple[64];    // Simple Displayed Radio Text
char rtCheck[64];   
char rtTmp0[64];      // Temporary Radio Text (high probability)
char rtTmp1[64];      // Temporary radio text (low probability)
char rtCnt[64];       // Hit count of high probabiltiy radio text
bool       rtFlag;          // Radio Text A/B flag
bool       rtFlagValid;     // Radio Text A/B flag is valid
//static bool       rtsFlag;         // Radio Text A/B flag
//static bool       rtsFlagValid;    // Radio Text A/B flag is valid

// RDS Program Service
unsigned char psDisplay[13];    // Displayed Program Service text
unsigned char psCheck[8];
unsigned char psTmp0[8];       // Temporary PS text (high probability)
unsigned char psTmp1[8];       // Temporary PS text (low probability)
unsigned char psCnt[8];        // Hit count of high probability PS text

// RDS Clock Time and Date
bool       ctDayHigh;
unsigned short ctDayLow;        // Modified Julian Day low 16 bits
unsigned char ctHour;          // Hour
unsigned char ctMinute;        // Minute
signed char ctOffset;        // Local Time Offset from UTC
bool       rdsIgnoreAB;

// RDS Alternate Frequency List
unsigned char afCount;
unsigned short afList[25];

// RDS flags and counters
unsigned char RdsDataAvailable = 0;  // Count of unprocessed RDS Groups
unsigned char RdsIndicator     = 0;  // If true, RDS was recently detected
unsigned short RdsDataLost      = 0;  // Number of Groups lost
unsigned short  RdsBlocksValid   = 0;  // Number of valid blocks received
unsigned short  RdsBlocksTotal   = 0;  // Total number of blocks expected
//static unsigned short  RdsBlockTimer    = 0;  // Total number of blocks expected
unsigned short  RdsGroupCounters[32];  // Number of each kind of group received

unsigned short  RdsSynchValid  = 0;
unsigned short  RdsSynchTotal  = 0;
unsigned short  RdsGroupsTotal = 0;
unsigned short  RdsGroups      = 0;
 unsigned short  RdsValid[4];


/**************************************************************************
 * Generic Functions
 **************************************************************************/
 unsigned char si470x_evt = -1;
void si470x_q_event(struct si470x_device *radio,
				enum si470x_evt_t event) 
{

	struct kfifo *data_b = &radio->data_buf[SI470X_BUF_EVENTS];
	si470x_evt = event;
//	if(radio->event_lock == 0)
//	{
//		radio->event_lock =1;
//		radio->event_value= si470x_evt;

		radio->event_value[radio->event_number] = si470x_evt;
		radio->event_number=radio->event_number+1;
		if(radio->event_number>20)
		printk("!!!!!!!!!!!!!!!!!!!!!si470x_q_event buffer overflow!!!!!!!!!!!");
//	}
	printk(KERN_INFO "updating event_q with event %x userptr = %x\n", event,(unsigned int)data_b); 
	if (kfifo_in_locked(data_b, &si470x_evt, 1, &radio->buf_lock[SI470X_BUF_EVENTS]))
		wake_up_interruptible(&radio->event_queue);
}
static int copy_from_rdsData(struct si470x_device *radio,
		enum si470x_buf_t buf_type, unsigned int n){

	struct kfifo *data_fifo = &radio->data_buf[buf_type];
	unsigned char *rds_data = "";
	//int i=0;
	unsigned int retval =0;

	printk(KERN_INFO "copy_from_rdsData %d size = %d\n", buf_type, n);

	radio->buf_size = n;

	if(buf_type == SI470X_BUF_RT_RDS)
	{
		rds_data = &rtDisplay[0];
		retval = copy_to_user(radio->buffer, &rtDisplay[0],n);
		retval = copy_to_user(&rtCheck[0], &rtDisplay[0+5],n);
		if (retval > 0) {
			printk(KERN_INFO "Failed to RT data copy %d bytes of data\n", retval);
			return -EAGAIN;
		}	
	}
	else if(buf_type == SI470X_BUF_PS_RDS)
	{
		rds_data = &psDisplay[0];
		retval = copy_to_user(radio->buffer, &psDisplay[0],n);
		retval = copy_to_user(&psCheck[0], &psDisplay[0+5],n);
		if (retval > 0) {
			printk(KERN_INFO "Failed to PS data copy %d bytes of data\n", retval);
			return -EAGAIN;
		}	
	}
	kfifo_in_locked(data_fifo, rds_data, n, &radio->buf_lock[buf_type]);

	//for(i=0;i<n;i++)
	{
		printk(KERN_INFO "copy from rdsData %s\n", radio->buffer); 

	}
	return 0;
}
//-----------------------------------------------------------------------------
// This routine adds an additional level of error checking on the PI code.
// A PI code is only considered valid when it has been identical for several
// groups.
//-----------------------------------------------------------------------------
#define RDS_PI_VALIDATE_LIMIT  4

static void update_pi(u16 current_pi)
{
    static unsigned char rds_pi_validate_count = 0;
    static unsigned short rds_pi_nonvalidated;

    printk(KERN_INFO "RDS update_pi%x\n", current_pi); 

    // if the pi value is the same for a certain number of times, update
    // a validated pi variable
    if (rds_pi_nonvalidated != current_pi)
    {
        rds_pi_nonvalidated =  current_pi;
        rds_pi_validate_count = 1;
    }
    else
    {
        rds_pi_validate_count++;
    }

    if (rds_pi_validate_count > RDS_PI_VALIDATE_LIMIT)
    {
       // piDisplay = rds_pi_nonvalidated;
         rtDisplay[2]= (rds_pi_nonvalidated & 0xFF00) >> 8;
         rtDisplay[3]= (rds_pi_nonvalidated & 0x00FF);
         psDisplay[2]= (rds_pi_nonvalidated & 0xFF00) >> 8;
         psDisplay[3]= (rds_pi_nonvalidated & 0x00FF);
    }
}


//-----------------------------------------------------------------------------
// This routine adds an additional level of error checking on the PTY code.
// A PTY code is only considered valid when it has been identical for several
// groups.
//-----------------------------------------------------------------------------
#define RDS_PTY_VALIDATE_LIMIT 4

static void update_pty(u8 current_pty)
{
    static unsigned char rds_pty_validate_count = 0;
    static unsigned char rds_pty_nonvalidated;

    printk(KERN_INFO "RDS update_pty %x\n",current_pty);	

    // if the pty value is the same for a certain number of times, update
    // a validated pty variable
    if (rds_pty_nonvalidated != current_pty)
    {
        rds_pty_nonvalidated =  current_pty;
        rds_pty_validate_count = 1;
    }
    else
    {
        rds_pty_validate_count++;
    }

    if (rds_pty_validate_count > RDS_PTY_VALIDATE_LIMIT)
    {
        //ptyDisplay = rds_pty_nonvalidated;
        rtDisplay[1]= rds_pty_nonvalidated;
        psDisplay[1]= rds_pty_nonvalidated;
    }
}


//-----------------------------------------------------------------------------
// The basic implementation of the Program Service update displays data
// immediately but does no additional error detection.
//-----------------------------------------------------------------------------
static void update_ps_basic(u8 current_ps_addr, u8 current_ps_byte, struct si470x_device *radio)
{
    printk(KERN_INFO "RDS update_ps_basic %x\n",current_ps_addr);	

	psDisplay[current_ps_addr] = current_ps_byte;
	//q_event send
	 if(strcmp(&psCheck[0],&psDisplay[0]))
 	{
	 	copy_from_rdsData(radio, SI470X_BUF_PS_RDS,sizeof(psDisplay));
	       si470x_q_event(radio,SI470X_EVT_NEW_PS_RDS);	
	 }
}

//-----------------------------------------------------------------------------
// This implelentation of the Program Service update attempts to display
// only complete messages for stations who rotate text through the PS field
// in violation of the RBDS standard as well as providing enhanced error
// detection.
//-----------------------------------------------------------------------------
#define PS_VALIDATE_LIMIT 2
static void update_ps(u8 addr, u8 byte, struct si470x_device *radio)
{
    unsigned char i;
    bool textChange = 0; // indicates if the PS text is in transition
    bool psComplete = 1; // indicates that the PS text is ready to be displayed
    unsigned char msgcheck = 0;

    printk(KERN_INFO "RDS update_ps psTmp0[addr] %x psTmp1[addr] %x byte %x addr %x \n",psTmp0[addr],psTmp1[addr], byte, addr);	
    if(psTmp0[addr] == byte)
    {
        // The new byte matches the high probability byte
        if(psCnt[addr] < PS_VALIDATE_LIMIT)
        {
            psCnt[addr]++;
        }
        else
        {
            // we have recieved this byte enough to max out our counter
            // and push it into the low probability array as well
            psCnt[addr] = PS_VALIDATE_LIMIT;
            psTmp1[addr] = byte;
        }
    }
    else if(psTmp1[addr] == byte)
    {
        // The new byte is a match with the low probability byte. Swap
        // them, reset the counter and flag the text as in transition.
        // Note that the counter for this character goes higher than
        // the validation limit because it will get knocked down later
        if(psCnt[addr] >= PS_VALIDATE_LIMIT)
        {
            textChange = 1;
            psCnt[addr] = PS_VALIDATE_LIMIT + 1;
        }
        else
        {
            psCnt[addr] = PS_VALIDATE_LIMIT;
        }
        psTmp1[addr] = psTmp0[addr];
        psTmp0[addr] = byte;
    }
    else if(!psCnt[addr])
    {
        // The new byte is replacing an empty byte in the high
        // proability array
        psTmp0[addr] = byte;
        psCnt[addr] = 1;
    }
    else
    {
        // The new byte doesn't match anything, put it in the
        // low probablity array.
        psTmp1[addr] = byte;
    }
    printk(KERN_INFO "RDS update_ps textChange: %x psTmp0 %s\n",textChange,psTmp0);	
    if(textChange)
    {
        // When the text is changing, decrement the count for all
        // characters to prevent displaying part of a message
        // that is in transition.
        for(i=0;i<sizeof(psCnt);i++)
        {
            if(psCnt[i] > 1)
            {
                psCnt[i]--;
            }
        }
    }

    // The PS text is incomplete if any character in the high
    // probability array has been seen fewer times than the
    // validation limit.
    for (i=0;i<sizeof(psCnt);i++)
    {
        if(psCnt[i] < PS_VALIDATE_LIMIT)
        {
            psComplete = 0;
            break;
        }
    }
    printk(KERN_INFO "RDS update_ps psComplete: %x\n",psComplete);	
    // If the PS text in the high probability array is complete
    // copy it to the display array
    if (psComplete)
    {
        for (i=0;i<8; i++)
        {

	     if(!strcmp(&psTmp0[i],"\n"))
	     {
      	  	  printk(KERN_INFO "RDS strcmp result end\n");	

	     	  break;
	     }
            if(psTmp0[i] == 0x20)
            {
                break;
            }

            psDisplay[i+5] = psTmp0[i];
	     printk(KERN_INFO "RDS update_ps psDisplay %x psDisplay %x\n",psDisplay[i+5],psTmp0[i]);	
        }
	 printk(KERN_INFO "RDS update_ps_advance pscheck %s psDisplay %s\n",psCheck,psDisplay);	

	 for(i=0;i<8;i++)
 	{
 		if(psCheck[i] != psDisplay[i+5])
 			msgcheck = 1;
 	}
	 if(msgcheck)
 	{ 	
 		 psDisplay[0] = 1;//sizeof(psDisplay);;
 		 si470x_q_event(radio,SI470X_EVT_RDS_AVAIL);	
		 copy_from_rdsData(radio, SI470X_BUF_PS_RDS,sizeof(psDisplay));
	        si470x_q_event(radio,SI470X_EVT_NEW_PS_RDS);	

	        for (i=0;i<sizeof(psCnt);i++)
	        {
	            psCnt[i]     = 0;
	            psTmp0[i]    = 0;
	            psTmp1[i]    = 0;
	        }
		 for(i=0;i<sizeof(psDisplay);i++)
	 	{
	 		psDisplay[i] = 0;
	 	}
	 }
    }
}
#if 0
//-----------------------------------------------------------------------------
// The basic implementation of the Radio Text update displays data
// immediately but does no additional error detection.
//-----------------------------------------------------------------------------
static void update_rt_simple(bool abFlag, u8 count, u8 addr, u8 * chars, u8 errorFlags, struct si470x_device *radio)
{
    unsigned char errCount;
    unsigned char blerMax;
    unsigned char i,j;
    unsigned char temp[64]="";   
    unsigned char msgcheck = 0;
    printk(KERN_INFO "RDS update_rt_simple %x\n",addr);	
	
    // If the A/B flag changes, wipe out the rest of the text
    if ((abFlag != rtsFlag) && rtsFlagValid)
    {
        for (i=addr;i<sizeof(rtDisplay);i++)
        {
            rtSimple[i] = 0;
        }
    }
    rtsFlag = abFlag;    // Save the A/B flag
    rtsFlagValid = 1;    // Now the A/B flag is valid

    for (i=0; i<count; i++)
    {
        // Choose the appropriate block. Count > 2 check is necessary for 2B groups
        if ((i < 2) && (count > 2))
        {
            errCount = ERRORS_CORRECTED(errorFlags,BLOCK_C);
            blerMax = rdsBlerMax[2];
        }
        else
        {
            errCount = ERRORS_CORRECTED(errorFlags,BLOCK_D);
            blerMax = rdsBlerMax[3];
        }

        if(errCount <= blerMax)
        {
            // Store the data in our temporary array
	     printk(KERN_INFO "RDS update_rt_simple chars %x \n",chars[i]);	
            if(chars[i] == 0x0d)
            {
                // The end of message character has been received.
                // Wipe out the rest of the text.
               /* for (j=addr+i+1;j<sizeof(rtSimple);j++)
                {
                    rtSimple[j] = 0;
                }*/

		 for(j=0;i<i-1;j++)
		 {
		 	if(j%2 == 0)
	 		{
	 			temp[j+1] =  rtSimple[j];
		 	}
			else
			{
	 			temp[i-1] =  rtSimple[j];		
			}
		 }

	        for (j=0;j<i-1;j++)
	        {
	            rtSimple[j] = temp[j];
		     rtDisplay[j] = rtSimple[j];
	        }
		 for(j=0;j<i-1;j++)
	 	{
	 		if(rtCheck[j] != rtDisplay[j])
	 			msgcheck = 1;
	 	}
		 if(msgcheck)
	 	{
			 //copy_from_rdsData(radio, SI470X_BUF_RT_RDS,i-1);
		       // si470x_q_event(radio,SI470X_EVT_NEW_RT_RDS);	
		 }
                break;
            }
            rtSimple[addr+i] = chars[i];
        }
    }

    // Any null character before this should become a space
    for (i=0;i<addr;i++)
    {
        if(!rtSimple[i])
        {
            rtSimple[i] = ' ';
        }
    }
}
#endif
//-----------------------------------------------------------------------------
// This implelentation of the Radio Text update attempts to display
// only complete messages even if the A/B flag does not toggle as well
// as provide additional error detection. Note that many radio stations
// do not implement the A/B flag properly. In some cases, it is best left
// ignored.
//-----------------------------------------------------------------------------
#define RT_VALIDATE_LIMIT 2
static void display_rt(struct si470x_device *radio)
{
    bool rtComplete = 1;
    unsigned char i, j;
    char *temp;  
    unsigned char msgcheck = 0;

    printk(KERN_INFO "RDS display_rt \n");	
    // The Radio Text is incomplete if any character in the high
    // probability array has been seen fewer times than the
    // validation limit.
    for (i=0; i<sizeof(rtTmp0);i++)
    {
        if(rtCnt[i] < RT_VALIDATE_LIMIT)
        {
            rtComplete = 0;
            break;
        }
        if(rtTmp0[i] == 0x0d)
        {
            // The array is shorter than the maximum allowed
            break;
        }
    }

    // If the Radio Text in the high probability array is complete
    // copy it to the display array
    if (rtComplete)
    {
    	 temp = kmalloc(sizeof(rtTmp0), GFP_KERNEL);
        for (i=0;i<sizeof(rtDisplay); i++)
        {
      	     // printk(KERN_INFO "RDS update_rt_simple chars %x \n",rtTmp0[i]);	

//	     if(!strcmp(&rtTmp0[i],"\n"))
//	     {
//      	  	  printk(KERN_INFO "RDS strcmp result end\n");	

	     	  //break;
//	     	  continue;
//	     }
		 
            if(rtTmp0[i] == 0x0d)
            {
                break;
            }
            //rtDisplay[i+5] = rtTmp0[i];

        }

	//printk(KERN_INFO "!!!!!!!before swtiching rtDisplay : %s rtTmp0 : %s i : %d \n", rtDisplay, rtTmp0, i);	

	 for(j=0;j<i+2;j++)
	 {
	 	if(j%2 == 0)
 		{
 			temp[j+1] =  rtTmp0[j];//rtDisplay[j+5];
			//printk(KERN_INFO "!!!!!!!after swtiching temp mod 0 : %s rtDisplay : %s\n", temp, rtDisplay);	
	 	}
		else
		{
 			temp[j-1] =  rtTmp0[j];//rtDisplay[j+5];		
			//printk(KERN_INFO "!!!!!!!after swtiching temp mod 1 : %s rtDisplay : %s\n", temp, rtDisplay);	
		}
	 }

        for (j=0;j<i+1; j++)
        {
            rtDisplay[j+5] = temp[j];
        }
	 printk(KERN_INFO "RDS update_rt_advance rtcheck %s rtDisplay %s\n",rtCheck,rtDisplay);	

	 for(j=0;j<i+1;j++)
 	{
 		if(rtCheck[j] != rtDisplay[j+5])
 			msgcheck = 1;
 	}
	 if(msgcheck)
 	{
 		 rtDisplay[0] = sizeof(rtDisplay);
 		 si470x_q_event(radio,SI470X_EVT_RDS_AVAIL);	
		 copy_from_rdsData(radio, SI470X_BUF_RT_RDS, sizeof(rtDisplay));
       	 si470x_q_event(radio,SI470X_EVT_NEW_RT_RDS);	
	 }
	
        // Wipe out everything after the end-of-message marker
        for (i=0;i<sizeof(rtDisplay);i++)
        {
            rtDisplay[i] = 0;
            rtCnt[i]     = 0;
            rtTmp0[i]    = 0;
            rtTmp1[i]    = 0;
        }
	kfree(temp);	
    }
}

//-----------------------------------------------------------------------------
// This implementation of the Radio Text update attempts to further error
// correct the data by making sure that the data has been identical for
// multiple receptions of each byte.
//-----------------------------------------------------------------------------
static void
update_rt_advance(bool abFlag, u8 count, u8 addr, u8 * byte, u8 errorFlags,struct si470x_device *radio)
{
    unsigned char i;
    bool textChange = 0; // indicates if the Radio Text is changing
    printk(KERN_INFO "RDS update_rt_advance %d \n",abFlag);	
    if (abFlag != rtFlag && rtFlagValid && !rdsIgnoreAB)
    {
        // If the A/B message flag changes, try to force a display
        // by increasing the validation count of each byte
        // and stuffing a space in place of every NUL char
        for (i=0;i<sizeof(rtCnt);i++)
        {
            if (!rtTmp0[i])
            {
                rtTmp0[i] = ' ';
                rtCnt[i]++;
            }
        }
        for (i=0;i<sizeof(rtCnt);i++)
        {
            rtCnt[i]++;
        }
        display_rt(radio);

        // Wipe out the cached text
        for (i=0;i<sizeof(rtCnt);i++)
        {
            rtCnt[i]  = 0;
            rtTmp0[i] = 0;
            rtTmp1[i] = 0;
        }
    }

    rtFlag = abFlag;    // Save the A/B flag
    rtFlagValid = 1;    // Our copy of the A/B flag is now valid

    for(i=0;i<count;i++)
    {
        unsigned char errCount;
        unsigned char blerMax;
        // Choose the appropriate block. Count > 2 check is necessary for 2B groups
        if ((i < 2) && (count > 2))
        {
            errCount = ERRORS_CORRECTED(errorFlags,BLOCK_C);
            blerMax = rdsBlerMax[2];
        }
        else
        {
            errCount = ERRORS_CORRECTED(errorFlags,BLOCK_D);
            blerMax = rdsBlerMax[3];
        }
        if (errCount <= blerMax)
        {
            if(!byte[i])
            {
                byte[i] = ' '; // translate nulls to spaces
            }

            // The new byte matches the high probability byte
            if(rtTmp0[addr+i] == byte[i])
            {
                if(rtCnt[addr+i] < RT_VALIDATE_LIMIT)
                {
                    rtCnt[addr+i]++;
                }
                else
                {
                    // we have recieved this byte enough to max out our counter
                    // and push it into the low probability array as well
                    rtCnt[addr+i] = RT_VALIDATE_LIMIT;
                    rtTmp1[addr+i] = byte[i];
                }
            }
            else if(rtTmp1[addr+i] == byte[i])
            {
                // The new byte is a match with the low probability byte. Swap
                // them, reset the counter and flag the text as in transition.
                // Note that the counter for this character goes higher than
                // the validation limit because it will get knocked down later
                if(rtCnt[addr+i] >= RT_VALIDATE_LIMIT)
                {
                    textChange = 1;
                    rtCnt[addr+i] = RT_VALIDATE_LIMIT + 1;
                }
                else
                {
                    rtCnt[addr+i] = RT_VALIDATE_LIMIT;
                }
                rtTmp1[addr+i] = rtTmp0[addr+i];
                rtTmp0[addr+i] = byte[i];
            }
            else if(!rtCnt[addr+i])
            {
                // The new byte is replacing an empty byte in the high
                // proability array
                rtTmp0[addr+i] = byte[i];
                rtCnt[addr+i] = 1;
            }
            else
            {
                // The new byte doesn't match anything, put it in the
                // low probablity array.
                rtTmp1[addr+i] = byte[i];
            }
        }
    }

    if(textChange)
    {
        // When the text is changing, decrement the count for all
        // characters to prevent displaying part of a message
        // that is in transition.
        for(i=0;i<sizeof(rtCnt);i++)
        {
            if(rtCnt[i] > 1)
            {
                rtCnt[i]--;
            }
        }
    }

    // Display the Radio Text
    display_rt(radio);
}
//-----------------------------------------------------------------------------
// When tracking alternate frequencies, it's important to clear the list
// after tuning to a new station. (unless you are tuning to check an alternate
// frequency.
//-----------------------------------------------------------------------------
static void init_alt_freq(void)
{
    unsigned char i;
    afCount = 0;
    for (i = 0; i < sizeof(afList)/sizeof(afList[0]); i++)
    {
        afList[i] = 0;
    }
}
//-----------------------------------------------------------------------------
// Decode the RDS AF data into an array of AF frequencies.
//-----------------------------------------------------------------------------
#define AF_COUNT_MIN 225
#define AF_COUNT_MAX (AF_COUNT_MIN + 25)
#define AF_FREQ_MIN 0
#define AF_FREQ_MAX 204
#define AF_FREQ_TO_U16F(freq) (8750+((freq-AF_FREQ_MIN)*10))
static void update_alt_freq(u16 current_alt_freq)
{
    // Currently this only works well for AF method A, though AF method B
    // data will still be captured.
    unsigned char dat;
    unsigned char i;
    unsigned short freq;

    // the top 8 bits is either the AF Count or AF Data
    dat = (unsigned char)(current_alt_freq >> 8);
    // look for the AF Count indicator
    if ((dat >= AF_COUNT_MIN) && (dat <= AF_COUNT_MAX) && ((dat - AF_COUNT_MIN) != afCount))
    {
        init_alt_freq();  // clear the alternalte frequency list
        afCount = (dat - AF_COUNT_MIN); // set the count
        dat = (unsigned char)current_alt_freq;
        if (dat >= AF_FREQ_MIN && dat <= AF_FREQ_MAX)
        {
            freq = AF_FREQ_TO_U16F(dat);
            afList[0]= freq;
        }
    }
    // look for the AF Data
    else if (afCount && dat >= AF_FREQ_MIN && dat <= AF_FREQ_MAX)
    {
        bool foundSlot = 0;
        static unsigned char clobber=1;  // index to clobber if no empty slot is found
        freq = AF_FREQ_TO_U16F(dat);
        for (i=1; i < afCount; i+=2)
        {
            // look for either an empty slot or a match
            if ((!afList[i]) || (afList[i] = freq))
            {
                afList[i] = freq;
                dat = (unsigned char)current_alt_freq;
                freq = AF_FREQ_TO_U16F(dat);
                afList[i+1] = freq;
                foundSlot = 1;
                break;
            }
        }
        // If no empty slot or match was found, overwrite a 'random' slot.
        if (!foundSlot)
        {
            clobber += (clobber&1) + 1; // this ensures that an odd slot is always chosen.
            clobber %= (afCount);       // keep from overshooting the array
            afList[clobber] = freq;
            dat = (unsigned char)current_alt_freq;
            freq = AF_FREQ_TO_U16F(dat);
            afList[clobber+1] = freq;
        }
    }

}

//-----------------------------------------------------------------------------
// Decode the RDS time data into its individual parts.
//-----------------------------------------------------------------------------
static void
update_clock(u16 b, u16 c, u16 d, u8 errFlags)
{

    unsigned char errorb = ERRORS_CORRECTED(errFlags,BLOCK_B);
    unsigned char errorc = ERRORS_CORRECTED(errFlags,BLOCK_C);
    unsigned char errord = ERRORS_CORRECTED(errFlags,BLOCK_D);

    if (errorb <= rdsBlerMax[1] &&
        errorc <= rdsBlerMax[2] &&
        errord <= rdsBlerMax[3] &&
        (errorb + errorc + errord) <= rdsBlerMax[1]) {

        ctDayHigh = (b >> 1) & 1;
        ctDayLow  = (b << 15) | (c >> 1);
        ctHour    = ((c&1) << 4) | (d >> 12);
        ctMinute  = (d>>6) & 0x3F;
        ctOffset  = d & 0x1F;
        if (d & (1<<5))
        {
            ctOffset = -ctOffset;
        }
    }
}

//-----------------------------------------------------------------------------
// After an RDS interrupt, read back the RDS registers and process the data.
//-----------------------------------------------------------------------------
void updateRds(struct si470x_device *radio)
{
    unsigned char group_type = 0;      // bits 4:1 = type,  bit 0 = version
    unsigned char addr = 0;
    unsigned char errorCount = 0;
    unsigned char errorFlags = 0;
    bool abflag = 0;
    bool rdsv = 1;

    printk(KERN_INFO "updateRds\n");	
    RdsDataAvailable = 0;

    // read rds registers
    
    rdsv = radio->registers[POWERCFG] & POWERCFG_RDSM;

    if (rdsv)
    {
        unsigned char bler[4];
        unsigned char field ;

        printk(KERN_INFO "RDS verbose mode\n");	

        // Gather the latest BLER info
        bler[0] = (radio->registers[STATUSRSSI] & STATUSRSSI_BLERA) >> 9;
        bler[1] = (radio->registers[READCHAN] & READCHAN_BLERB) >> 14;
        bler[2] = (radio->registers[READCHAN] & READCHAN_BLERC) >> 12;
        bler[3] = (radio->registers[READCHAN] & READCHAN_BLERD) >> 10;

        errorCount = 0;
        RdsGroups++;
        for (field = 0; field <= 3; field++)
        {
            if (bler[field] == UNCORRECTABLE)
            {
                errorCount++;
            }
            else
            {
                RdsValid[field]++;
            }
            errorFlags = errorFlags << 2 | bler[field];
        }
    }
    else
    {
        errorCount = (radio->registers[STATUSRSSI] & 0x0e00) >> 9;  // This will only be non-zero in fw<15
        errorFlags = 0;
        printk(KERN_INFO "RDS standard mode %x\n", errorCount);	
    }

    if (errorCount < 4)
    {
        RdsBlocksValid += (4 - errorCount);
	printk(KERN_INFO "RdsBlocksValid %d\n",RdsBlocksValid );			
	
    }

    // Drop the data if there are any errors and not in verbose mode
    if (errorCount && !rdsv)
    {
        printk(KERN_INFO "RDS Drop the data if there are any errors and not in verbose mode %x\n", errorCount);	

        return;
    }

    // Update pi code.
    if (ERRORS_CORRECTED(errorFlags, BLOCK_A) < rdsBlerMax[0])
    {
        update_pi(radio->registers[RDSA]);
    }

    if (ERRORS_CORRECTED(errorFlags, BLOCK_B) <= rdsBlerMax[1])
    {
        group_type = radio->registers[RDSB] >> 11;  // upper five bits are the group type and version
        // Check for group counter overflow and divide all by 2
        if((RdsGroupCounters[group_type] + 1) == 0)
        {
            unsigned char i;
            for (i=0;i<32;i++)
            {
                RdsGroupCounters[i] >>= 1;
            }
        }
        RdsGroupCounters[group_type] += 1;
    }
    else
    {
        // Drop the data if more than two errors were corrected in block B
        printk(KERN_INFO "RDS Drop the data if more than two errors were corrected in block B\n");	
        return;
    }


    // Update pi code.  Version B formats always have the pi code in words A and C
    if (group_type & 0x01)
    {
        update_pi(radio->registers[RDSC]);
    }


    // update pty code.
    update_pty((radio->registers[RDSB] >> 5) & 0x1f);  // gdc:  fix this so all 16 bits are passed and other bits are also updated here?

    switch (group_type) {
	printk(KERN_INFO "RDS group_type %d\n",group_type );			
        case RDS_TYPE_0A:
            if (ERRORS_CORRECTED(errorFlags, BLOCK_C) <= rdsBlerMax[2])
            {
                update_alt_freq(radio->registers[RDSC]);
            }
            // fallthrough
        case RDS_TYPE_0B:
            addr = (radio->registers[RDSB] & 0x3) * 2;
            if (ERRORS_CORRECTED(errorFlags, BLOCK_D) <= rdsBlerMax[3])
            {
                if(rdsBasic)
                {
                    update_ps_basic(addr+0, radio->registers[RDSD] >> 8  , radio);
                    update_ps_basic(addr+1, radio->registers[RDSD] & 0xff, radio);
                }
                else
                {
                    update_ps(addr+0, radio->registers[RDSD] >> 8 ,radio );
                    update_ps(addr+1, radio->registers[RDSD] & 0xff,radio);
                }
            }
            break;

        case RDS_TYPE_2A:
        {
            unsigned char * rtptr = (unsigned char*)&(radio->registers[RDSC]);
            addr = (radio->registers[RDSB]  & 0xf) * 4;
            abflag = (radio->registers[RDSB]  & 0x0010) >> 4;
            //update_rt_simple(abflag, 4, addr, rtptr, errorFlags,radio);
            update_rt_advance(abflag, 4, addr, rtptr, errorFlags,radio);
            break;
        }

        case RDS_TYPE_2B:
        {
            unsigned char * rtptr = (u8*)&(radio->registers[RDSD] );
            addr = (radio->registers[RDSB]  & 0xf) * 2;
            abflag = (radio->registers[RDSB]  & 0x0010) >> 4;
            // The last 32 bytes are unused in this format
//            rtSimple[32]  = 0x0d;
            rtTmp0[32]    = 0x0d;
            rtTmp1[32]    = 0x0d;
            rtCnt[32]     = RT_VALIDATE_LIMIT;
            //update_rt_simple (abflag, 2, addr, rtptr, errorFlags,radio);
            update_rt_advance(abflag, 2, addr, rtptr, errorFlags,radio);
            break;
        }
        case RDS_TYPE_4A:
            // Clock Time and Date
            update_clock(radio->registers[RDSB] , radio->registers[RDSC] , radio->registers[RDSD] , errorFlags);
            break;
        default:
            break;
    }

}
/*
 * si470x_set_chan - set the channel
 */
int si470x_set_chan(struct si470x_device *radio, unsigned short chan)
{
	int retval;
	unsigned long timeout;
	bool timed_out = 0;

	printk(KERN_INFO "si470x_set_chan chan %d \n",chan);	
	/* initiallizing */	
	radio->registers[CHANNEL] &= ~CHANNEL_TUNE;
	retval = si470x_set_register(radio, CHANNEL);

	msleep(10);
	/* start tuning */
	radio->registers[CHANNEL] &= ~CHANNEL_CHAN;
	radio->registers[CHANNEL] |= CHANNEL_TUNE | chan;
	retval = si470x_set_register(radio, CHANNEL);
	msleep(10);	
	
	if (retval < 0){
		printk("si470x_set_chan error %x",retval);	
		goto stop;
}
	/* currently I2C driver only uses interrupt way to tune */
	if (radio->stci_enabled) {
		INIT_COMPLETION(radio->completion);

		/* wait till tune operation has completed */
		retval = wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(tune_timeout));
		printk("si470x_set_chan wait_for_completion_timeout retval %x\n ",retval);
		if (!retval){
			timed_out = true;
			printk("si470x_set_chan debug time out retval %x\n",retval);
			}
	} else {
		/* wait till tune operation has completed */
		timeout = jiffies + msecs_to_jiffies(tune_timeout);
		do {
			printk("!!!!!!!!!!!!!!!don't use polling!!!!!!!!!!!!!!!");

			retval = si470x_get_register(radio, STATUSRSSI);
			if (retval < 0){
				printk("don't use polling");
				goto stop;
				}
			timed_out = time_after(jiffies, timeout);
		} while (((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
				&& (!timed_out));
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
		printk("tune does not complete\n");
	if (timed_out)
		printk("tune timed out after %u ms\n", tune_timeout);

stop:
	/* stop tuning */
	printk("si470x_set_chan Stop lable retval %x \n",retval);
	printk("si470x_set_chan Stop [STATUSRSSI] = %x\n",radio->registers[STATUSRSSI]);

//done:
	radio->registers[CHANNEL] &= ~CHANNEL_TUNE;
	si470x_set_register(radio, CHANNEL);
	do {
		si470x_get_register(radio, STATUSRSSI);
	} while ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) != 0);
	
	printk("si470x_set_chan Done lable retval %x \n",retval);

	return 0;
}


/*
 * si470x_get_freq - get the frequency
 */
int si470x_get_freq(struct si470x_device *radio, unsigned int *freq)
{
	unsigned int spacing, band_bottom;
	unsigned short chan;
	int retval;

	printk(KERN_INFO "si470x_get_freq\n");	
	/* Spacing (kHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4) {
	/* 0: 200 kHz (USA, Australia) */
	case 0:
		spacing = 0.200 * FREQ_MUL; break;
	/* 1: 100 kHz (Europe, Japan) */
	case 1:
		spacing = 0.100 * FREQ_MUL; break;
	/* 2:  50 kHz */
	default:
		spacing = 0.050 * FREQ_MUL; break;
	};

	/* Bottom of Band (MHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	/* 0: 87.5 - 108 MHz (USA, Europe) */
	case 0:
		band_bottom = 87.5 * FREQ_MUL; break;
	/* 1: 76   - 108 MHz (Japan wide band) */
	default:
		band_bottom = 76   * FREQ_MUL; break;
	/* 2: 76   -  90 MHz (Japan) */
	case 2:
		band_bottom = 76   * FREQ_MUL; break;
	};

	/* read channel */
	retval = si470x_get_register(radio, READCHAN);
	chan = radio->registers[READCHAN] & READCHAN_READCHAN;

	/* Frequency (MHz) = Spacing (kHz) x Channel + Bottom of Band (MHz) */
	*freq = chan * spacing + band_bottom;

	printk("si470x_get_freq chan=%x\n, freq = %d",chan,*freq);
	si470x_q_event(radio,SI470X_EVT_ABOVE_TH);
	si470x_q_event(radio,SI470X_EVT_STEREO);

	return retval;
}


/*
 * si470x_set_freq - set the frequency
 */
int si470x_set_freq(struct si470x_device *radio, unsigned int freq)
{
	unsigned int spacing, band_bottom;
	unsigned short chan;


	printk("si470x_set_freq freq %d, freq - 1400000 = %d \n ",freq,freq-1400000);
	printk("you can calculate setting Frequency (freq - 1400000)/16000. ");

	/* Spacing (kHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4) {
	/* 0: 200 kHz (USA, Australia) */
	case 0:
		spacing = 0.200 * FREQ_MUL; break;
	/* 1: 100 kHz (Europe, Japan) */
	case 1:
		spacing = 0.100 * FREQ_MUL; break;
	/* 2:  50 kHz */
	default:
		spacing = 0.050 * FREQ_MUL; break;
	};

	/* Bottom of Band (MHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	/* 0: 87.5 - 108 MHz (USA, Europe) */
	case 0:
		band_bottom = 87.5 * FREQ_MUL; break;
	/* 1: 76   - 108 MHz (Japan wide band) */
	case 2:
		band_bottom = 76   * FREQ_MUL; break;
	/* 2: 76   -  90 MHz (Japan) */
	default:
		band_bottom = 76   * FREQ_MUL; break;
	};

	/* Chan = [ Freq (Mhz) - Bottom of Band (MHz) ] / Spacing (kHz) */
	chan = (freq - band_bottom) / spacing;

	printk(KERN_INFO "si470x_set_freq chan %d, freq %d band_bottom %d spacing %d\n", chan, freq,band_bottom,spacing);	
	return si470x_set_chan(radio, chan);
}


static void si470x_regional_cfg(struct si470x_device *radio, country_enum country)
{
	printk(KERN_INFO "si470x_regional_cfg \n");	
	printk(KERN_INFO "si470x_regional_cfg regional setting is temporary blocked country =%d \n",country);	// temp block lsw
//return;
//    band = 0;  // Clear BAND field		87.5 ~ 108MHz
//    space= 0; // Clear SPACE field
    switch (country) {
        case USA :
            de = 0;   // 75us de-emphasis
            break;
        case JAPAN :
            de = 1;           // 50us de-emphasis
            band = 2;		//76 ~ 90MHz
            space = 1;	//100kHz
            break;
        case EUROPE :
#if 0
            de = 1;   // 50us de-emphasis
            space = 1; 	//100kHz
            band = 0;		//76 ~ 90MHz
#endif
            de = 1;   // 50us de-emphasis
            space = 0; 	//200kHz	
            band = 0;		//87.5 ~ 108MHz
            break;

        default:
            de = 1;   // 50us de-emphasis
            space = 1; 	//100kHz
            break;
    }

//20120824 for setting country value
    	radio->registers[SYSCONFIG1] |=	(de << 11) & SYSCONFIG1_DE;		/* DE*/

	radio->registers[SYSCONFIG2] |=
		((band  << 6) & SYSCONFIG2_BAND)  |	/* BAND */
		((space << 4) & SYSCONFIG2_SPACE) ;	/* SPACE */

	si470x_set_register(radio, SYSCONFIG1);
	si470x_set_register(radio, SYSCONFIG2);

}

int si470x_mute(struct si470x_device *radio, u8 mute)
{
	int retval = 0;
	printk(KERN_INFO "si470x_mute %d\n", mute);	

	if(radio->mute != mute)
	{
		if (mute == 1)
			radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
		else
			radio->registers[POWERCFG] |= POWERCFG_DMUTE;
		retval = si470x_set_register(radio, POWERCFG);
		radio->mute = mute;
	}
	return retval;
}
int si470x_set_volume(struct si470x_device *radio, u8 volume)
{
	int retval = 0;
	printk(KERN_INFO "si470x_set_volume  volume %x \n", volume);	
	si470x_mute(radio, 0);
	radio->registers[SYSCONFIG2] &= ~SYSCONFIG2_VOLUME;
	radio->registers[SYSCONFIG2] |= volume;
	radio->volume = volume;
	retval = si470x_set_register(radio, SYSCONFIG2);
	return retval;
}
void si470x_set_extvolume(u8 volume)
{
#if 0
	si470x_mute(radio, 0);
    if (volume <= 15) {
        si470x_shadow[0x6] |= SI47_VOLEXT;
    } else {
        volume++;  // Have to account for 0 (16) being mute
        si470x_shadow[0x6] &= ~SI47_VOLEXT;
    }
    si470x_shadow[0x5] &= ~SI47_VOLUME;
    si470x_shadow[0x5] |= volume & SI47_VOLUME;
    si470x_reg_write(0x2, 0x6);
#endif
}
/*
 * si470x_set_seek - set seek
 */
int si470x_set_seek(struct si470x_device *radio,
		unsigned int wrap_around, unsigned int seek_upward)
{
	int retval = 0;
	unsigned long timeout;
	bool timed_out = 0;
// [AUDIO_BSP]_START, 20120802, hanna.oh@lge.com, for SI4709 FM RADIO AUTO SCAN & PATH SWITCHING

	printk(KERN_INFO "in si470x_set_seek %d\n",seek_upward);	

	/* start seeking */
	radio->registers[POWERCFG] |= POWERCFG_SEEK;
	if (wrap_around == 1)	
		radio->registers[POWERCFG] &= ~POWERCFG_SKMODE;
	else
		radio->registers[POWERCFG] |= POWERCFG_SKMODE;
	if (seek_upward == 1)
		radio->registers[POWERCFG] |= POWERCFG_SEEKUP;
	else
		radio->registers[POWERCFG] &= ~POWERCFG_SEEKUP;
	retval = si470x_set_register(radio, POWERCFG);
	if (retval < 0)
		goto done;
	
	//radio->seek_onoff = 1;
	/* currently I2C driver only uses interrupt way to seek */
	if (radio->stci_enabled) {
		INIT_COMPLETION(radio->completion);

		/* wait till seek operation has completed */
		retval = wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(seek_timeout));

		if (!retval)
			timed_out = true;
	} else {
		/* wait till seek operation has completed */
		timeout = jiffies + msecs_to_jiffies(seek_timeout);
		do {
			msleep(60);
			retval = si470x_get_register(radio, STATUSRSSI);
			retval = si470x_get_register(radio, READCHAN);
			printk(KERN_INFO "in si470x_set_seek check READCHAN %x\n", radio->registers[READCHAN]);	
			if (retval < 0)
				goto stop;
			timed_out = time_after(jiffies, timeout);
		} while (((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
				&& (!timed_out));
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
	{
		dev_warn(&radio->videodev->dev, "seek does not complete\n");
		//return 1;
	}
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_SF) != 0)
	{
		printk(KERN_INFO "in si470x_set_seek check SF %d\n", radio->registers[STATUSRSSI]);	

		dev_warn(&radio->videodev->dev,
			"seek failed / band limit reached\n");
		//return 1;
		//timed_out = 1;	//20120924 hanna.oh@lge.com FM Radio seek fail patch
	}
	if (timed_out)
	{
		dev_warn(&radio->videodev->dev,
			"seek timed out after %u ms\n", seek_timeout);
		//return 1;
	}

	//printk(KERN_INFO "si470x_set_seek %d\n", radio->registers[STATUSRSSI]);	

	//si470x_q_event(radio,SI470X_EVT_SCAN_NEXT);


stop:
	/* stop seeking */
	//radio->seek_onoff = 0;
	printk(KERN_INFO "in si470x_set_seek stop\n");	

	radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
	retval = si470x_set_register(radio, POWERCFG);

done:
	/* try again, if timed out */
	if ((retval == 0) && timed_out)
		retval = -EAGAIN;

// [AUDIO_BSP]_END, 20120802, hanna.oh@lge.com, for SI4709 FM RADIO AUTO SCAN & PATH SWITCHING
	return retval;
}

#if 0
static int si470x_wakeup(void)
{
	int rc;

	rc = gpio_direction_output(GPIO_SI470X_RESET_PIN, 1);
	if (rc < 0) {
		printk(KERN_INFO "si470x_wakeup: Failed to gpio_direction_output si470X_reset\n");
	}
	return rc;
}
#endif
void initRdsBuffers(void)
{
    u8 i;
	
    for (i=0;i<sizeof(psDisplay);i++)
    {
	 psCheck[i] = 0;
    }
	
    for (i=0; i<sizeof(rtDisplay); i++)
    {
	 rtCheck[i] = 0;
    }
}
void initRdsVars(void)
{
    u8 i;
    static bool firstInitDone = 0;

    // Set the maximum allowable errors in a block considered acceptable
    // It's critical that block B be correct since it determines what's
    // contained in the latter blocks. For this reason, a stricter tolerance
    // is placed on block B
    if (!firstInitDone)
    {
        rdsBlerMax[0] = CORRECTED_THREE_TO_FIVE; // Block A
        rdsBlerMax[1] = CORRECTED_ONE_TO_TWO;    // Block B
        rdsBlerMax[2] = CORRECTED_THREE_TO_FIVE; // Block C
        rdsBlerMax[3] = CORRECTED_THREE_TO_FIVE; // Block D
        firstInitDone = 1;
    }

    // Reset RDS variables
    RdsDataAvailable = 0;
    RdsDataLost      = 0;
    RdsIndicator     = 0;
    RdsBlocksValid   = 0;
    RdsBlocksTotal   = 0;

    // Clear RDS Fifo
    //RdsFifoEmpty = 1;
   // RdsReadPtr = 0;
  //  RdsWritePtr = 0;

    // Clear Radio Text
    rtFlagValid     = 0;
    //rtsFlagValid    = 0;
    for (i=0; i<sizeof(rtDisplay); i++)
    {
//        rtSimple[i]  = 0;
        rtDisplay[i] = 0;
        rtTmp0[i]    = 0;
        rtTmp1[i]    = 0;
        rtCnt[i]     = 0;
	 rtCheck[i] = 0;
    }

    // Clear Program Service
    for (i=0;i<sizeof(psDisplay);i++)
    {
        psDisplay[i] = 0;
        psTmp0[i]    = 0;
        psTmp1[i]    = 0;
        psCnt[i]     = 0;
	 psCheck[i] = 0;
    }

    // Reset Debug Group counters
    for (i=0; i<32; i++)
    {
        RdsGroupCounters[i] = 0;
    }

    // Reset alternate frequency tables
    init_alt_freq();

    RdsSynchValid  = 0;
    RdsSynchTotal  = 0;
    RdsValid[0]    = 0;
    RdsValid[1]    = 0;
    RdsValid[2]    = 0;
    RdsValid[3]    = 0;
    RdsGroups      = 0;
    RdsGroupsTotal = 0;
//    ptyDisplay     = 0;
//    piDisplay      = 0;
    ctDayHigh      = 0;
    ctDayLow       = 0;
    ctHour         = 0;
    ctMinute       = 0;
    ctOffset       = 0;
}
/*
 * si470x_start - switch on radio
 */
int si470x_start(struct si470x_device *radio)
{
	int retval = 0;

	printk(KERN_INFO "\n\nsi470x_start radio\n");

	/* power up : need 110ms */
	radio->registers[POWERCFG] =POWERCFG_DMUTE | POWERCFG_ENABLE | POWERCFG_DSMUTE;// | POWERCFG_RDSM;
	radio->registers[POWERCFG]=radio->registers[POWERCFG]&(~POWERCFG_DISABLE);
	printk(KERN_INFO "si470x_start[POWERCFG]= %x",radio->registers[POWERCFG]);			

	retval = si470x_set_register(radio, POWERCFG);
	msleep(110);

	if (retval < 0){
		printk("si470x_start POWERCFG Setting Error");
		goto done;
	}

	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] |=	(de << 11) & SYSCONFIG1_DE;		/* DE*/
	//radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDSIEN;
	radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDSIEN;
	radio->registers[SYSCONFIG1] |= SYSCONFIG1_STCIEN;
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_GPIO2;
	radio->registers[SYSCONFIG1] |= 0x1 << 2;
	retval = si470x_set_register(radio, SYSCONFIG1);
	printk(KERN_INFO "si470x_set_register return value = %d \n",retval);	
	if (retval < 0){
	printk("si470x_start SYSCONFIG1 Setting Error");
		goto done;
	}

	/* sysconfig 2 */
	radio->registers[SYSCONFIG2] =
		(SEEKTH_VAL  << 8) |				/* SEEKTH */
		((band  << 6) & SYSCONFIG2_BAND)  |	/* BAND */
		((space << 4) & SYSCONFIG2_SPACE) |	/* SPACE */
		SYSCONFIG2_VOLUME;
		// radio->volume;					/* VOLUME (max) */
	retval = si470x_set_register(radio, SYSCONFIG2);
	if (retval < 0){
		printk("si470x_start SYSCONFIG2 Setting Error");
		goto done;
	}
	
	radio->registers[SYSCONFIG3] =
		((8  << 4) &  SYSCONFIG3_SKSNR) |		/* SKSNR */
		(4 & SYSCONFIG3_SKCNT); 				/* SKCNT */
	retval = si470x_set_register(radio, SYSCONFIG3);
	if (retval < 0)	{
		printk("si470x_start SYSCONFIG3 Setting Error");
		goto done;
	}
	printk(KERN_INFO "si470x_start [POWERCFG]= %x",radio->registers[POWERCFG]);			
	printk(KERN_INFO "si470x_start [SYSCONFIG1]= %x",radio->registers[SYSCONFIG1]);			
	printk(KERN_INFO "si470x_start [SYSCONFIG2]= %x",radio->registers[SYSCONFIG2]);
	printk(KERN_INFO "si470x_start [SYSCONFIG3]= %x",radio->registers[SYSCONFIG3]);			

done:
	return retval;
}


/*
 * si470x_stop - switch off radio
 */
int si470x_stop(struct si470x_device *radio)
{
	int retval;
	printk(KERN_INFO "si470x_stop\n");	

	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		goto done;

	/* powercfg */
	radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
	/* POWERCFG_ENABLE has to automatically go low */
	radio->registers[POWERCFG] |= POWERCFG_ENABLE |	POWERCFG_DISABLE;
	retval = si470x_set_register(radio, POWERCFG);

	fmradio_power(0);

done:
	return retval;
}


/*
 * si470x_rds_on - switch on rds reception
 */
static int si470x_rds_on(struct si470x_device *radio, int onoff)
{
	int retval;
	printk(KERN_INFO "si470x_rds_onoff\n");	

	/* sysconfig 1 */
	if(onoff)
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDS;
	else
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;
	
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
	{
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;
		retval = si470x_set_register(radio, SYSCONFIG1);
	}
	return retval;
}



/**************************************************************************
 * File Operations Interface
 **************************************************************************/

/*
 * si470x_fops_read - read RDS data
 */
static ssize_t si470x_fops_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	unsigned int block_count = 0;

	printk(KERN_INFO "si470x_fops_read\n");	

	/* switch on rds reception */
	mutex_lock(&radio->lock);
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		si470x_rds_on(radio, 1);

	/* block if no new data available */
	while (radio->wr_index == radio->rd_index) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EWOULDBLOCK;
			goto done;
		}
		if (wait_event_interruptible(radio->read_queue,
			radio->wr_index != radio->rd_index) < 0) {
			retval = -EINTR;
			goto done;
		}
	}

	/* calculate block count from byte count */
	count /= 3;

	/* copy RDS block out of internal buffer and to user buffer */
	while (block_count < count) {
		if (radio->rd_index == radio->wr_index)
			break;

		/* always transfer rds complete blocks */
		if (copy_to_user(buf, &radio->buffer[radio->rd_index], 3))
			/* retval = -EFAULT; */
			break;

		/* increment and wrap read pointer */
		radio->rd_index += 3;
		if (radio->rd_index >= radio->buf_size)
			radio->rd_index = 0;

		/* increment counters */
		block_count++;
		buf += 3;
		retval += 3;
	}

done:
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_fops_poll - poll RDS data
 */
static unsigned int si470x_fops_poll(struct file *file,
		struct poll_table_struct *pts)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	/* switch on rds reception */
	printk(KERN_INFO "si470x_fops_poll\n");	
	
	mutex_lock(&radio->lock);
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		si470x_rds_on(radio, 1);
	mutex_unlock(&radio->lock);

	poll_wait(file, &radio->read_queue, pts);

	if (radio->rd_index != radio->wr_index)
		retval = POLLIN | POLLRDNORM;

	return retval;
}


/*
 * si470x_fops - file operations interface
 */
static const struct v4l2_file_operations si470x_fops = {
	.owner			= THIS_MODULE,
	.read			= si470x_fops_read,
	.poll			= si470x_fops_poll,
	.unlocked_ioctl		= video_ioctl2,
	.open			= si470x_fops_open,
	.release		= si470x_fops_release,
};



/**************************************************************************
 * Video4Linux Interface
 **************************************************************************/

/*
 * si470x_vidioc_queryctrl - enumerate control items
 */
static int si470x_vidioc_queryctrl(struct file *file, void *priv,
		struct v4l2_queryctrl *qc)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = -EINVAL;

	/* abort if qc->id is below V4L2_CID_BASE */
	if (qc->id < V4L2_CID_BASE)
		goto done;

	/* search video control */
	switch (qc->id) {
	case V4L2_CID_AUDIO_VOLUME:
		return v4l2_ctrl_query_fill(qc, 0, 15, 1, 15);
	case V4L2_CID_AUDIO_MUTE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	}

	/* disable unsupported base controls */
	/* to satisfy kradio and such apps */
	if ((retval == -EINVAL) && (qc->id < V4L2_CID_LASTP1)) {
		qc->flags = V4L2_CTRL_FLAG_DISABLED;
		retval = 0;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"query controls failed with %d\n", retval);
	return retval;
}


/*
 * si470x_vidioc_g_ctrl - get the value of a control
 */
static int si470x_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	printk(KERN_INFO "si470x_vidioc_g_ctrl %x\n",ctrl->id);	
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		ctrl->value = radio->registers[SYSCONFIG2] &
				SYSCONFIG2_VOLUME;
		break;
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = ((radio->registers[POWERCFG] &
				POWERCFG_DMUTE) == 0) ? 1 : 0;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCHMODE:
		ctrl->value = radio->registers[POWERCFG] & POWERCFG_SKMODE;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SCANDWELL:	//???
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCHON:
		ctrl->value = (radio->registers[POWERCFG] & POWERCFG_SEEK);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_STATE:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_IOVERC:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_INTDET:
		#if 0
		size = 0x1;
		xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
		xfr_buf[1] = INTDET_PEEK_MSB;
		xfr_buf[2] = INTDET_PEEK_LSB;
		retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
		if (retval < 0) {
			FMDBG("Failed to write\n");
			return retval;
		}
		FMDBG("INT_DET:Sync write success\n");
		msleep(TAVARUA_DELAY*10);
		retval = tavarua_read_registers(radio, XFRDAT0, 3);
		if (retval < 0) {
			FMDBG("INT_DET: Read failure\n");
			return retval;
		}
		ctrl->value = radio->registers[XFRDAT0];
		#endif
		break;
	case V4L2_CID_PRIVATE_TAVARUA_MPX_DCC:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_REGION:
		ctrl->value = de;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH:
	#if 0
		retval = sync_read_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0) {
			FMDBG("[G IOCTL=V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH]\n");
			FMDBG("sync_read_xfr error: [retval=%d]\n", retval);
			break;
		}
		cRmssiThreshold = (signed char)xfr_buf[0];
		ctrl->value  = cRmssiThreshold;
		FMDBG("cRmssiThreshold: %d\n", cRmssiThreshold);
	#endif
		ctrl->value =  radio->registers[SYSCONFIG2] & SYSCONFIG2_SEEKTH;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PI:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_EMPHASIS:
		ctrl->value = de;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDS_STD:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SPACING:
		ctrl->value = space;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSON:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_PSALL:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_LP_MODE:
		break;
	case V4L2_CID_PRIVATE_TAVARUA_ANTENNA:
		break;
	case V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD:
#if 0
		size = 0x04;
		xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
		xfr_buf[1] = ON_CHANNEL_TH_MSB;
		xfr_buf[2] = ON_CHANNEL_TH_LSB;
		retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		msleep(TAVARUA_DELAY*10);
		retval = tavarua_read_registers(radio, XFRDAT0, 4);
		if (retval < 0) {
			pr_err("%s: On Ch. DET: Read failure\n", __func__);
			return retval;
		}
		for (cnt = 0; cnt < 4; cnt++)
			FMDBG("On-Channel data set is : 0x%x\t",
				(int)radio->registers[XFRDAT0+cnt]);
		ctrl->value =	LSH_DATA(radio->registers[XFRDAT0],   24) |
				LSH_DATA(radio->registers[XFRDAT0+1], 16) |
				LSH_DATA(radio->registers[XFRDAT0+2],  8) |
				(radio->registers[XFRDAT0+3]);
		FMDBG("The On Channel Threshold value is : 0x%x", ctrl->value);
#endif
		break;
	case V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD:
#if 0
		size = 0x04;
		xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
		xfr_buf[1] = OFF_CHANNEL_TH_MSB;
		xfr_buf[2] = OFF_CHANNEL_TH_LSB;
		retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		msleep(TAVARUA_DELAY*10);
		retval = tavarua_read_registers(radio, XFRDAT0, 4);
		if (retval < 0) {
			pr_err("%s: Off Ch. DET: Read failure\n", __func__);
			return retval;
		}
		for (cnt = 0; cnt < 4; cnt++)
			FMDBG("Off-channel data set is : 0x%x\t",
				(int)radio->registers[XFRDAT0+cnt]);
		ctrl->value =	LSH_DATA(radio->registers[XFRDAT0],   24) |
				LSH_DATA(radio->registers[XFRDAT0+1], 16) |
				LSH_DATA(radio->registers[XFRDAT0+2],  8) |
				(radio->registers[XFRDAT0+3]);
		FMDBG("The Off Channel Threshold value is : 0x%x", ctrl->value);
#endif
		break;
	case V4L2_CID_PRIVATE_SINR_THRESHOLD:
	case V4L2_CID_PRIVATE_SINR_SAMPLES:
	case V4L2_CID_PRIVATE_IRIS_GET_SINR:
	case V4L2_CID_PRIVATE_TAVARUA_DO_CALIBRATION:	//20120913 hanna.oh FM Radio On/Off crash patch
		retval = 0;
		break;
	default:
		retval = -EINVAL;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get control failed with %d\n", retval);

	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_ctrl - set the value of a control
 */
static int si470x_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	dev_err(&radio->videodev->dev,
			"si470x_vidioc_s_ctrl %x \n", ctrl->id);
	switch (ctrl->id) {
	case V4L2_CID_SI470X_AUDIO_VOLUME:
		printk("si470x_vidioc_s_ctrl V4L2_CID_AUDIO_VOLUME value = %d", ctrl->value);
		retval = si470x_set_volume(radio, ctrl->value);
		break;
	case V4L2_CID_SI470X_AUDIO_MUTE:
		printk("si470x_vidioc_s_ctrl V4L2_CID_AUDIO_MUTE value = %d", ctrl->value);
		retval = si470x_mute(radio, ctrl->value);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCHMODE:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SRCHMODE ctrl->value%x \n",ctrl->value);
		printk("si470x is not used V4L2_CID_PRIVATE_TAVARUA_SRCHMODE");
		//retval = (radio->registers[POWERCFG] & ~POWERCFG_SKMODE) |
		//					ctrl->value;
		//radio->registers[POWERCFG] = retval;
		if(ctrl->value == 1)
			radio->seek_onoff = 1;
		else
			radio->seek_onoff = 0;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SCANDWELL:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SCANDWELL \n");
		printk("V4L2_CID_PRIVATE_TAVARUA_SCANDWELL ctrl->value %x\n ",ctrl->value);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCHON:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SRCHON ctrl->value=%x\n",ctrl->value);
		if(ctrl->value == 1)
		{
			 si470x_set_seek(radio, 1, SRCH_DIR_UP);
		}
		else
		{
			radio->seek_onoff = 0;
			radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
			retval = si470x_set_register(radio, POWERCFG);
		}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_STATE:
		#if 1
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_STATE value = %x",ctrl->value);
		if(ctrl->value == FM_RECV)
		{
			si470x_q_event(radio,SI470X_EVT_RADIO_READY);
		}
		else
		{
			//si470x_stop(radio);	//20120911 hanna.oh FM Radio OFF kernel panic patch
			si470x_q_event(radio, SI470X_EVT_RADIO_DISABLED);
		}
		#else
		if (((ctrl->value == FM_RECV) || (ctrl->value == FM_TRANS))
				    && !(radio->registers[RDCTRL] &
							ctrl->value)) {
			FMDBG("clearing flags\n");
			init_completion(&radio->sync_xfr_start);
			init_completion(&radio->sync_req_done);
			radio->xfr_in_progress = 0;
			radio->xfr_bytes_left = 0;
			FMDBG("turning on ..\n");
			retval = tavarua_start(radio, ctrl->value);
			if (retval >= 0) {
				value = (radio->registers[IOCTRL] |
				    IOC_SFT_MUTE | IOC_SIG_BLND);
				retval = tavarua_write_register(radio,
					IOCTRL, value);
				if (retval < 0)
					FMDBG("SMute and SBlending"
						"not enabled\n");
			}
		}
		else if ((ctrl->value == FM_OFF) && radio->registers[RDCTRL]) {
			FMDBG("%s: turning off...\n", __func__);
			tavarua_write_register(radio, RDCTRL, ctrl->value);
			kfifo_reset(&radio->data_buf[TAVARUA_BUF_EVENTS]);
			flush_workqueue(radio->wqueue);
			tavarua_q_event(radio, TAVARUA_EVT_RADIO_DISABLED);
			FMDBG("%s, Disable All Interrupts\n", __func__);
			dis_buf[STATUS_REG1] = 0x00;
			dis_buf[STATUS_REG2] = 0x00;
			dis_buf[STATUS_REG3] = TRANSFER;
			retval = sync_write_xfr(radio, INT_CTRL, dis_buf);
			if (retval < 0) {
				pr_err("%s: failed to disable"
						"Interrupts\n", __func__);
				return retval;
			}
		}
		#endif
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH value = %x\n",ctrl->value);
		printk("V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH is not used ");

		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_ALGORITHM:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SRCH_ALGORITHM\n");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_REGION:
	{
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_REGION value = %x\n",ctrl->value);
		printk("V4L2_CID_PRIVATE_TAVARUA_REGION is temporary blocked");
//		si470x_regional_cfg(ctrl->value);
		si470x_regional_cfg(radio, ctrl->value);
//		radio->registers[SYSCONFIG1] |= (de << 11) & SYSCONFIG1_DE;		/* DE*/
		//printk("V4L2_CID_PRIVATE_TAVARUA_REGION radio->registers[SYSCONFIG1] = %x, de=%x",radio->registers[SYSCONFIG1],de);
		//retval = si470x_set_register(radio, SYSCONFIG1);
#if 0
 		radio->registers[SYSCONFIG2] = (0x3f  << 8) |				/* SEEKTH */
		((band  << 6) & SYSCONFIG2_BAND)  |	/* BAND */
		((space << 4) & SYSCONFIG2_SPACE);	/* SPACE */					
		retval = si470x_set_register(radio, SYSCONFIG2);
#endif

	}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH");
		retval = si470x_get_register(radio, STATUSRSSI);
		if( SEEKTH_VAL>(radio->registers[STATUSRSSI] & STATUSRSSI_RSSI))
		si470x_q_event(radio, SI470X_EVT_BELOW_TH);
		else
		si470x_q_event(radio, SI470X_EVT_ABOVE_TH);		

#if 0
		retval = sync_read_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0)	{
			FMDERR("V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH]\n");
			FMDERR("sync_read_xfr [retval=%d]\n", retval);
			break;
		}
		xfr_buf[0] = (unsigned char)ctrl->value;
		xfr_buf[1] = (unsigned char)ctrl->value;
		retval = sync_write_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0) {
			FMDERR("V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH]\n");
			FMDERR("sync_write_xfr [retval=%d]\n", retval);
			break;
		}
#endif
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PI:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SRCH_PI");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_EMPHASIS:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_EMPHASIS de ctrl->value =%x",ctrl->value);
//		de = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDS_STD:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_RDS_STD");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SPACING:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_SPACING %x",ctrl->value);
		space = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSON:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_RDSON = %x", ctrl->value);
		retval = si470x_rds_on(radio, ctrl->value);		
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_AF_JUMP:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_AF_JUMP");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA");
		break;
	case V4L2_CID_PRIVATE_TAVARUA_HLSI:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_HLSI");		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF");		break;
	case V4L2_CID_PRIVATE_TAVARUA_PSALL:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_PSALL");		break;
	case V4L2_CID_PRIVATE_TAVARUA_LP_MODE:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_LP_MODE");		break;
	case V4L2_CID_PRIVATE_TAVARUA_ANTENNA:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_ANTENNA");		break;
	case V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD");
		#if 0
		size = 0x04;
		xfr_buf[0] = (XFR_POKE_MODE | (size << 1));
		xfr_buf[1] = ON_CHANNEL_TH_MSB;
		xfr_buf[2] = ON_CHANNEL_TH_LSB;
		xfr_buf[3] = (ctrl->value & 0xFF000000) >> 24;
		xfr_buf[4] = (ctrl->value & 0x00FF0000) >> 16;
		xfr_buf[5] = (ctrl->value & 0x0000FF00) >> 8;
		xfr_buf[6] = (ctrl->value & 0x000000FF);
		for (cnt = 3; cnt < 7; cnt++) {
			FMDBG("On-channel data to be poked is : %d",
				(int)xfr_buf[cnt]);
		}
		retval = tavarua_write_registers(radio, XFRCTRL,
				xfr_buf, size+3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		msleep(TAVARUA_DELAY*10);
		#endif
		break;
	case V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD:
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD");
		#if 0
		size = 0x04;
		xfr_buf[0] = (XFR_POKE_MODE | (size << 1));
		xfr_buf[1] = OFF_CHANNEL_TH_MSB;
		xfr_buf[2] = OFF_CHANNEL_TH_LSB;
		xfr_buf[3] = (ctrl->value & 0xFF000000) >> 24;
		xfr_buf[4] = (ctrl->value & 0x00FF0000) >> 16;
		xfr_buf[5] = (ctrl->value & 0x0000FF00) >> 8;
		xfr_buf[6] = (ctrl->value & 0x000000FF);
		for (cnt = 3; cnt < 7; cnt++) {
			FMDBG("Off-channel data to be poked is : %d",
				(int)xfr_buf[cnt]);
		}
		retval = tavarua_write_registers(radio, XFRCTRL,
				xfr_buf, size+3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		msleep(TAVARUA_DELAY*10);
		#endif
		break;
	case V4L2_CID_RDS_TX_PTY: {
		printk("si470x_vidioc_s_ctrl V4L2_CID_RDS_TX_PTY");
		} break;
	case V4L2_CID_RDS_TX_PI: {
		printk("si470x_vidioc_s_ctrl V4L2_CID_RDS_TX_PI");
		} break;
	case V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME: {
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME");
		#if 0
			FMDBG("In STOP_RDS_TX_PS_NAME\n");
			memset(tx_data, '0', XFR_REG_NUM);
			FMDBG("Writing PS header\n");
			retval = sync_write_xfr(radio, RDS_PS_0, tx_data);
			FMDBG("retval of PS Header write: %d", retval);
		#endif
		} break;
	case V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT: {
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT");
		#if 0
			memset(tx_data, '0', XFR_REG_NUM);
			FMDBG("Writing RT header\n");
			retval = sync_write_xfr(radio, RDS_RT_0, tx_data);
			FMDBG("retval of Header write: %d", retval);
		#endif
		} break;
	case V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT: {
		printk("si470x_vidioc_s_ctrl V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT");
		} break;
	case V4L2_CID_TUNE_POWER_LEVEL: {
		printk("si470x_vidioc_s_ctrl V4L2_CID_TUNE_POWER_LEVEL");
		#if 0
		unsigned char tx_power_lvl_config[FM_TX_PWR_LVL_MAX+1] = {
			0x85, /* tx_da<5:3> = 0  lpf<2:0> = 5*/
			0x95, /* tx_da<5:3> = 2  lpf<2:0> = 5*/
			0x9D, /* tx_da<5:3> = 3  lpf<2:0> = 5*/
			0xA5, /* tx_da<5:3> = 4  lpf<2:0> = 5*/
			0xAD, /* tx_da<5:3> = 5  lpf<2:0> = 5*/
			0xB5, /* tx_da<5:3> = 6  lpf<2:0> = 5*/
			0xBD, /* tx_da<5:3> = 7  lpf<2:0> = 5*/
			0xBF  /* tx_da<5:3> = 7  lpf<2:0> = 7*/
		};
		if (ctrl->value > FM_TX_PWR_LVL_MAX)
			ctrl->value = FM_TX_PWR_LVL_MAX;
		if (ctrl->value < FM_TX_PWR_LVL_0)
			ctrl->value = FM_TX_PWR_LVL_0;
		retval = sync_read_xfr(radio, PHY_TXGAIN, xfr_buf);
		FMDBG("return for PHY_TXGAIN is %d", retval);
		if (retval < 0) {
			FMDBG("read failed");
			break;
		}
		xfr_buf[2] = tx_power_lvl_config[ctrl->value];
		retval = sync_write_xfr(radio, PHY_TXGAIN, xfr_buf);
		FMDBG("return for write PHY_TXGAIN is %d", retval);
		if (retval < 0)
			FMDBG("write failed");
		#endif
	} break;
	case V4L2_CID_PRIVATE_SOFT_MUTE:
	case V4L2_CID_PRIVATE_RIVA_ACCS_ADDR:
	case V4L2_CID_PRIVATE_RIVA_ACCS_LEN:
	case V4L2_CID_PRIVATE_RIVA_PEEK:
	case V4L2_CID_PRIVATE_RIVA_POKE:
	case V4L2_CID_PRIVATE_SSBI_ACCS_ADDR:
	case V4L2_CID_PRIVATE_SSBI_PEEK:
	case V4L2_CID_PRIVATE_SSBI_POKE:
	case V4L2_CID_PRIVATE_TX_TONE:
	case V4L2_CID_PRIVATE_RDS_GRP_COUNTERS:
	case V4L2_CID_PRIVATE_SET_NOTCH_FILTER:
	case V4L2_CID_PRIVATE_TAVARUA_DO_CALIBRATION:
	case V4L2_CID_PRIVATE_SINR_THRESHOLD:
	case V4L2_CID_PRIVATE_SINR_SAMPLES:
		retval = 0;
		break;
	default:
		retval = -EINVAL;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set control failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_g_audio - get audio attributes
 */
static int si470x_vidioc_g_audio(struct file *file, void *priv,
		struct v4l2_audio *audio)
{
	/* driver constants */
	audio->index = 0;
	strcpy(audio->name, "Radio");
	audio->capability = V4L2_AUDCAP_STEREO;
	audio->mode = 0;

	return 0;
}


/*
 * si470x_vidioc_g_tuner - get tuner attributes
 */
static int si470x_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	printk(KERN_INFO "si470x_vidioc_g_tuner \n");	
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (tuner->index != 0) {
		retval = -EINVAL;
		goto done;
	}

	retval = si470x_get_register(radio, STATUSRSSI);
	if (retval < 0)
		goto done;

	/* driver constants */
	strcpy(tuner->name, "FM");
	printk("si470x_vidioc_g_tuner radio->registers[SYSCONFIG2]%x",radio->registers[SYSCONFIG2]);
	
	tuner->type = V4L2_TUNER_RADIO;
	tuner->capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
			    V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO;

	/* range limits */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	/* 0: 87.5 - 108 MHz (USA, Europe, default) */
	default:
	case 0:
		tuner->rangelow  =  87.5 * FREQ_MUL;
		tuner->rangehigh = 108   * FREQ_MUL;
		break;
	/* 1: 76   - 108 MHz (Japan wide band) */
	case 1:
		tuner->rangelow  =  76   * FREQ_MUL;
		tuner->rangehigh = 108   * FREQ_MUL;
		break;
	/* 2: 76   -  90 MHz (Japan) */
	case 2:
		tuner->rangelow  =  76   * FREQ_MUL;
		tuner->rangehigh =  90   * FREQ_MUL;
		break;
	};

	/* stereo indicator == stereo (instead of mono) */
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_ST) == 0)
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO;
	else
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	/* If there is a reliable method of detecting an RDS channel,
	   then this code should check for that before setting this
	   RDS subchannel. */
	tuner->rxsubchans |= V4L2_TUNER_SUB_RDS;

	/* mono/stereo selector */
	if ((radio->registers[POWERCFG] & POWERCFG_MONO) == 0)
		tuner->audmode = V4L2_TUNER_MODE_STEREO;
	else
		tuner->audmode = V4L2_TUNER_MODE_MONO;

	/* min is worst, max is best; signal:0..0xffff; rssi: 0..0xff */
	/* measured in units of dbµV in 1 db increments (max at ~75 dbµV) */
	tuner->signal = (radio->registers[STATUSRSSI] & STATUSRSSI_RSSI);
	/* the ideal factor is 0xffff/75 = 873,8 */
	tuner->signal = (tuner->signal * 873) + (8 * tuner->signal / 10);

	/* automatic frequency control: -1: freq to low, 1 freq to high */
	/* AFCRL does only indicate that freq. differs, not if too low/high */
	tuner->afc = (radio->registers[STATUSRSSI] & STATUSRSSI_AFCRL) ? 1 : 0;

	si470x_q_event(radio,SI470X_EVT_ABOVE_TH);  // need check point.


done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get tuner failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_tuner - set tuner attributes
 */
static int si470x_vidioc_s_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	printk(KERN_INFO "si470x_vidioc_s_tuner %x\n", tuner->audmode);	
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (tuner->index != 0)
		goto done;

	/* mono/stereo selector */
	//switch (tuner->audmode) {
	//case V4L2_TUNER_MODE_MONO:
	if (tuner->audmode == V4L2_TUNER_MODE_MONO)
	{
		radio->registers[POWERCFG] |= POWERCFG_MONO;  /* force mono */
		//break;
	}
	//case V4L2_TUNER_MODE_STEREO:
	else
	{
		radio->registers[POWERCFG] &= ~POWERCFG_MONO; /* try stereo */
		//break;
	//default:
		//goto done;
	}

	retval = si470x_set_register(radio, POWERCFG);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set tuner failed with %d\n", retval);
//	si470x_q_event(radio,SI470X_EVT_TUNE_SUCC);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_g_frequency - get tuner or modulator radio frequency
 */
static int si470x_vidioc_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	printk("si470x_vidioc_g_frequency\n");
	/* safety checks */
	mutex_lock(&radio->lock);
	retval = si470x_disconnect_check(radio);

	if (retval)
		goto done;

	if (freq->tuner != 0) {
		retval = -EINVAL;
		goto done;
	}

	freq->type = V4L2_TUNER_RADIO;
	retval = si470x_get_freq(radio, &freq->frequency);
	goto done;

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get frequency failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_frequency - set tuner or modulator radio frequency
 */
static int si470x_vidioc_s_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	printk(KERN_INFO "si470x_vidioc_s_frequency freq->frequency %x\n",freq->frequency);	
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	/*if (freq->tuner != 0) {
		retval = -EINVAL;
		goto done;
	}*/

	retval = si470x_set_freq(radio, freq->frequency);


done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set frequency failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_hw_freq_seek - set hardware frequency seek
 */
static int si470x_vidioc_s_hw_freq_seek(struct file *file, void *priv,
		struct v4l2_hw_freq_seek *seek)
{
	int retval = 0;
	struct si470x_device *radio = video_drvdata(file);
	printk("si470x_vidioc_s_hw_freq_seek tuner %d, wrap_around=%x, seek_upward=%x spacing %d\n",seek->tuner,seek->wrap_around,seek->seek_upward,seek->spacing);

	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (seek->tuner != 0) {
		retval = -EINVAL;
		printk("si470x_vidioc_s_hw_freq_seek return EINVAL\n");
		goto done;
	}

	retval = si470x_set_seek(radio, seek->wrap_around, seek->seek_upward);
	
done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,	"set hardware frequency seek failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	printk("si470x_vidioc_s_hw_freq_seek return %x\n",retval);

	return retval;
}

static int si470x_vidioc_g_fmt_type_private(struct file *file, void *priv,
						struct v4l2_format *f)
{
	printk("si470x_vidioc_g_fmt_type_private return 0");
	return 0;

}
static int si470x_vidioc_dqbuf(struct file *file, void *priv,
				struct v4l2_buffer *buffer)
{
#if 0
	struct si470x_device  *radio = video_drvdata(file);
	enum si470x_buf_t buf_type = -1;
	unsigned char buf_fifo[STD_BUF_SIZE] = {0};
	unsigned char *buf = NULL;
	unsigned int len = 0, retval = -1;
	if ((radio == NULL) || (buffer == NULL)) {
		printk(KERN_INFO "radio/buffer is NULL\n");
		return -ENXIO;
	}
	buf_type = buffer->index;
	buf = (unsigned char *)buffer->m.userptr;
	len = buffer->length;
	printk(KERN_INFO "%s: requesting buffer %d \n", __func__, buf_type);
	if ((buf_type < SI470X_BUF_MAX) && (buf_type >= 0)) {
		data_fifo = &radio->data_buf[buf_type];
		if (buf_type == SI470X_BUF_EVENTS) {
			printk(KERN_INFO "userptr %x\n",(unsigned int)data_fifo);
			if (wait_event_interruptible(radio->event_queue,
				kfifo_len(data_fifo)) < 0) {
				return -EINTR;
			}
		}
	} else {
		printk(KERN_INFO "invalid buffer type\n");
		return -EINVAL;
	}
	if (len <= STD_BUF_SIZE) {
		if(radio->buf_lock[SI470X_BUF_EVENTS] == 1)
		{
			buf = radio->data_buf[SI470X_BUF_EVENTS];
			buffer->bytesused = 1;
			radio->buf_lock[SI470X_BUF_EVENTS] = 0;
		}
	} else {
		printk(KERN_INFO "kfifo_out_locked can not use len more than 128\n");
		return -EINVAL;
	}
	printk(KERN_INFO "after memcpy %s, %x\n",buf, buffer->bytesused);
	if (retval > 0) {
		printk(KERN_INFO "Failed to copy %d bytes of data\n", retval);
		return -EAGAIN;
	}
	return retval;
#else
	struct si470x_device  *radio = video_drvdata(file);
	enum si470x_buf_t buf_type = buffer->index;
	struct kfifo *data_fifo;
	unsigned char *buf = (unsigned char *)buffer->m.userptr;
	unsigned int len = buffer->length;
	int i=0;

	printk(KERN_INFO "si470x_vidioc_dqbuf ENTRY buf_type %d\n",buf_type);

	if (!access_ok(VERIFY_WRITE, buf, len))
		return -EFAULT;
	if ((buf_type < SI470X_BUF_MAX) && (buf_type >= 0)) {
		data_fifo = &radio->data_buf[buf_type];
		if (buf_type == SI470X_BUF_EVENTS)
			if (wait_event_interruptible(radio->event_queue,
				kfifo_len(data_fifo)) < 0)
				return -EINTR;
	} else {
		printk(KERN_INFO "invalid buffer type\n");
		return -EINVAL;
	}
	buffer->bytesused = kfifo_out_locked(data_fifo, buf, len,
					&radio->buf_lock[buf_type]);
	if(buf_type == SI470X_BUF_EVENTS)
	{
		printk(KERN_INFO "si470x_vidioc_dqbuf OUT radio->event_value = %x\n",radio->event_value[i]);
//		radio->event_lock =0;
		for(i=0;i<radio->event_number;i++){
		buf[i] = radio->event_value[i];//radio->event_value = si470x_evt;
//		buf[i] = radio->event_value;//radio->event_value = si470x_evt;

		printk(KERN_INFO "si470x_vidioc_dqbuf OUT buf[%d] = %s\n",i,buf);
		}
		buffer->bytesused = radio->event_number;
		radio->event_number=0;
	}
	else if((buf_type == SI470X_BUF_RT_RDS))
	{
		printk(KERN_INFO "si470x_vidioc_dqbuf RDS RT OUT radio->buf_type = %d\n",buf_type);
//		radio->event_lock =0;
		for(i=0;i<radio->buf_size;i++){
		buf[i] = radio->buffer[i];//radio->event_value = si470x_evt;
//		buf[i] = radio->event_value;//radio->event_value = si470x_evt;

		printk(KERN_INFO "si470x_vidioc_dqbuf RT OUT buf[%d] = %s\n",i,buf);
		}
		buffer->bytesused =radio->buf_size;
		//radio->event_number=0;
	}
	else if(buf_type == SI470X_BUF_PS_RDS)
	{
		printk(KERN_INFO "si470x_vidioc_dqbuf RDS PS OUT radio->buf_type = %d\n",buf_type);
//		radio->event_lock =0;
		for(i=0;i<radio->buf_size;i++){
		buf[i] = radio->buffer[i];//radio->event_value = si470x_evt;
//		buf[i] = radio->event_value;//radio->event_value = si470x_evt;

		printk(KERN_INFO "si470x_vidioc_dqbuf PS OUT buf[%d] = %s\n",i,buf);
		}
		buffer->bytesused =radio->buf_size;
		//buffer->bytesused =1;
		//radio->event_number=0;
	}
	return 0;
#endif
}
/*
 * si470x_ioctl_ops - video device ioctl operations
 */
static const struct v4l2_ioctl_ops si470x_ioctl_ops = {
	.vidioc_querycap	= si470x_vidioc_querycap,
	.vidioc_dqbuf                 = si470x_vidioc_dqbuf,
	.vidioc_g_fmt_type_private    = si470x_vidioc_g_fmt_type_private,
	.vidioc_queryctrl	= si470x_vidioc_queryctrl,
	.vidioc_g_ctrl		= si470x_vidioc_g_ctrl,
	.vidioc_s_ctrl		= si470x_vidioc_s_ctrl,
	.vidioc_g_audio		= si470x_vidioc_g_audio,
	.vidioc_g_tuner		= si470x_vidioc_g_tuner,
	.vidioc_s_tuner		= si470x_vidioc_s_tuner,
	.vidioc_g_frequency	= si470x_vidioc_g_frequency,
	.vidioc_s_frequency	= si470x_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek	= si470x_vidioc_s_hw_freq_seek,
};


/*
 * si470x_viddev_template - video device interface
 */
struct video_device si470x_viddev_template = {
	.fops			= &si470x_fops,
	.name			= DRIVER_NAME,
	.release		= video_device_release,
	.ioctl_ops		= &si470x_ioctl_ops,
};
