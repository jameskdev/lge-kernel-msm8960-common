/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/* Variables for F54 functionality */
unsigned short F54_Query_Base; /*0x019E*/
unsigned short F54_Command_Base; /*0x019D*/
unsigned short F54_Control_Base; /*0x010B*/
unsigned short F54_Data_Base; /*0x0100*/
unsigned short F54_Data_LowIndex; /*0x0101*/
unsigned short F54_Data_HighIndex; /*0x0102*/
unsigned short F54_Data_Buffer; /*0x0103*/
unsigned short F54_PhysicalTx_Addr; /*0x0130*/
unsigned short F54_PhysicalRx_Addr; /*0x011D*/
unsigned short F01_RMI_Ctrl0; /*0x005C*/
unsigned short F11_Query_Base; /*0x00C6*/
unsigned short F11_MaxNumberOfTx_Addr; /*0x01C8*/
unsigned short F11_MaxNumberOfRx_Addr; /*0x01C9*/

#define NoiseMitigation 0x96 /*page 0x01*/

#define CFG_F54_TXCOUNT 50
#define CFG_F54_RXCOUNT 50

unsigned char TxChannelUsed[CFG_F54_TXCOUNT];
unsigned char RxChannelUsed[CFG_F54_TXCOUNT];

unsigned char MaxNumberTx; /*=0x0B(x)*/
unsigned char MaxNumberRx; /*=0x13(y)*/
unsigned char numberOfTx = 0; //Tx
unsigned char numberOfRx = 0; //Rx
