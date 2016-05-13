#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "kit_assert.h"
#include <stdint.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "i2c_if.h"
#include "uartstdio.h"
#include "i2c_cmd.h"

#include "hw_ver.h"

#include "stdbool.h"

#define MAX_MEASURE_TIME		10

#define FAILURE                 -1
#define SUCCESS                 0

#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}
#define BUF_SIZE 2

#define Codec_addr 0x1A
#define DELAY_CODEC 1

extern xSemaphoreHandle i2c_smphr;

#include "stdbool.h"
static bool old_light_sensor;

//*****************************************************************************
//
//! Display the buffer contents over I2C
//!
//! \param  pucDataBuf is the pointer to the data store to be displayed
//! \param  ucLen is the length of the data to be displayed
//!
//! \return none
//!
//*****************************************************************************
static void DisplayBuffer(unsigned char *pucDataBuf, unsigned char ucLen) {
	unsigned char ucBufIndx = 0;
	LOGI("Read contents");
	LOGI("\n\r");
	while (ucBufIndx < ucLen) {
		LOGI(" 0x%x, ", pucDataBuf[ucBufIndx]);
		ucBufIndx++;
		if ((ucBufIndx % 8) == 0) {
			LOGI("\n\r");
		}
	}
	LOGI("\n\r");
}

int Cmd_i2c_read(int argc, char *argv[]) {
	unsigned char ucDevAddr, ucLen;
	unsigned char aucDataBuf[256];
	char *pcErrPtr;
	int iRetVal;

	if (argc != 3) {
		LOGI(
				"read  <dev_addr> <rdlen> \n\r\t - Read data frpm the specified i2c device\n\r");
		return FAILURE;
	}

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));
	// Get the device address
	//
	ucDevAddr = (unsigned char) strtoul(argv[2], &pcErrPtr, 16);
	//
	// Get the length of data to be read
	//
	ucLen = (unsigned char) strtoul(argv[3], &pcErrPtr, 10);

	// Read the specified length of data
	//
	iRetVal = I2C_IF_Read(ucDevAddr, aucDataBuf, ucLen);

	xSemaphoreGiveRecursive(i2c_smphr);

	if (iRetVal == SUCCESS) {
		LOGI("I2C_IF_ Read complete\n\r");
		//
		// Display the buffer over UART on successful write
		//
		DisplayBuffer(aucDataBuf, ucLen);
	} else {
		LOGI("I2C_IF_ Read failed\n\r");
		return FAILURE;
	}

	return SUCCESS;
}

int Cmd_i2c_writereg(int argc, char *argv[]) {
	unsigned char ucDevAddr, ucRegOffset, ucWrLen;
	unsigned char aucDataBuf[256];
	char *pcErrPtr;
	int iLoopCnt = 0;

	if (argc != 5) {
		LOGI(
				"writereg <dev_addr> <reg_offset> <wrlen> <<byte0> [<byte1> ... ]> \n\r");
		return FAILURE;
	}

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	// Get the device address
	//
	ucDevAddr = (unsigned char) strtoul(argv[2], &pcErrPtr, 16);

	//get the register offset
	ucRegOffset = (unsigned char) strtoul(argv[3], &pcErrPtr, 10);
	aucDataBuf[iLoopCnt] = ucRegOffset;
	iLoopCnt++;
	//
	// Get the length of data to be written
	//
	ucWrLen = (unsigned char) strtoul(argv[4], &pcErrPtr, 10);
	//
	// Get the bytes to be written
	//
	for (; iLoopCnt < ucWrLen + 1; iLoopCnt++) {
		//
		// Store the data to be written
		//
		aucDataBuf[iLoopCnt] = (unsigned char) strtoul(argv[4], &pcErrPtr, 16);

		++argv[4];
	}
	//
	// Write the data values.
	//
	RET_IF_ERR(I2C_IF_Write(ucDevAddr, &aucDataBuf[0], ucWrLen + 1, 1));

	xSemaphoreGiveRecursive(i2c_smphr);

	LOGI("I2C_IF_ Write To address complete\n\r");

	return SUCCESS;
}
int Cmd_i2c_readreg(int argc, char *argv[]) {
	unsigned char ucDevAddr, ucRegOffset, ucRdLen;
	unsigned char aucRdDataBuf[256];
	char *pcErrPtr;

	if (argc != 4) {
		LOGI("readreg <dev_addr> <reg_offset> <rdlen> \n\r");
		return FAILURE;
	}

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));
	//
	// Get the device address
	//
	ucDevAddr = (unsigned char) strtoul(argv[1], &pcErrPtr, 16);
	//
	// Get the register offset address
	//
	ucRegOffset = (unsigned char) strtoul(argv[2], &pcErrPtr, 16);

	//
	// Get the length of data to be read
	//
	ucRdLen = (unsigned char) strtoul(argv[3], &pcErrPtr, 10);

	//
	// Write the register address to be read from.
	// Stop bit implicitly assumed to be 0.
	//
	RET_IF_ERR(I2C_IF_Write(ucDevAddr, &ucRegOffset, 1, 0));

	vTaskDelay(0);
	//
	// Read the specified length of data
	//
	RET_IF_ERR(I2C_IF_Read(ucDevAddr, &aucRdDataBuf[0], ucRdLen));

	xSemaphoreGiveRecursive(i2c_smphr);

	LOGI("I2C_IF_ Read From address complete\n");
	//
	// Display the buffer over UART on successful readreg
	//
	DisplayBuffer(aucRdDataBuf, ucRdLen);

	return SUCCESS;
}
int Cmd_i2c_write(int argc, char *argv[]) {
	unsigned char ucDevAddr, ucStopBit, ucLen;
	unsigned char aucDataBuf[256];
	char *pcErrPtr;
	int iRetVal, iLoopCnt;

	if (argc != 4) {
		LOGI("write <dev_addr> <wrlen> <<byte0>[<byte1> ... ]>\n\r");
		return FAILURE;
	}

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));
	//
	// Get the device address
	//
	ucDevAddr = (unsigned char) strtoul(argv[1], &pcErrPtr, 16);
	//
	// Get the length of data to be written
	//
	ucLen = (unsigned char) strtoul(argv[2], &pcErrPtr, 10);
	//RETERR_IF_TRUE(ucLen > sizeof(aucDataBuf));

	for (iLoopCnt = 0; iLoopCnt < ucLen; iLoopCnt++) {
		//
		// Store the data to be written
		//
		aucDataBuf[iLoopCnt] = (unsigned char) strtoul(argv[3], &pcErrPtr, 16);
		++argv[3];
	}
	//
	// Get the stop bit
	//
	ucStopBit = (unsigned char) strtoul(argv[3], &pcErrPtr, 10);
	//
	// Write the data to the specified address
	//
	iRetVal = I2C_IF_Write(ucDevAddr, aucDataBuf, ucLen, ucStopBit);

	xSemaphoreGiveRecursive(i2c_smphr);

	if (iRetVal == SUCCESS) {
		LOGI("I2C_IF_ Write complete\n\r");
	} else {
		LOGI("I2C_IF_ Write failed\n\r");
		return FAILURE;
	}

	return SUCCESS;

}

static int get_temp() {
	unsigned char cmd = 0xe3;
	int temp_raw;
	int temp;

	unsigned char aucDataBuf[2];

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	vTaskDelay(5);
	(I2C_IF_Write(0x40, &cmd, 1, 1));

	xSemaphoreGiveRecursive(i2c_smphr);
	vTaskDelay(50);
	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	vTaskDelay(5);
	(I2C_IF_Read(0x40, aucDataBuf, 2));
	temp_raw = (aucDataBuf[0] << 8) | ((aucDataBuf[1] & 0xfc));
	
	temp = 17572 * temp_raw / 65536 - 4685;

	xSemaphoreGiveRecursive(i2c_smphr);

	return temp;
}

int init_temp_sensor()
{
	unsigned char cmd = 0xfe;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));
	(I2C_IF_Write(0x40, &cmd, 1, 1));    // reset

	get_temp();
	xSemaphoreGiveRecursive(i2c_smphr);

	return SUCCESS;
}

static int get_humid() {
	unsigned char aucDataBuf[2];
	unsigned char cmd = 0xe5;
	int humid_raw;
	int humid;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	vTaskDelay(5);

	(I2C_IF_Write(0x40, &cmd, 1, 1));

	xSemaphoreGiveRecursive(i2c_smphr);
	vTaskDelay(50);
	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	vTaskDelay(5);
	(I2C_IF_Read(0x40, aucDataBuf, 2));
	humid_raw = (aucDataBuf[0] << 8) | ((aucDataBuf[1] & 0xfc));

	xSemaphoreGiveRecursive(i2c_smphr);

	humid = 12500 * humid_raw / 65536 - 600;
	return humid;
}

int init_humid_sensor()
{
	unsigned char cmd = 0xfe;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	(I2C_IF_Write(0x40, &cmd, 1, 1));    // reset

	// Dummy read the 1st value.
	get_humid();

	xSemaphoreGiveRecursive(i2c_smphr);

	return SUCCESS;
}


void get_temp_humid( int * temp, int * humid ) {
	*temp = get_temp();
	*humid = get_humid();
	*humid += (2500-*temp)*-15/100;
}

int Cmd_readhumid(int argc, char *argv[]) {
	int temp,humid;
	get_temp_humid(&temp, &humid);
	LOGF("%d\n", humid);
	return SUCCESS;
}

int Cmd_readtemp(int argc, char *argv[]) {
	int temp,humid;
	get_temp_humid(&temp, &humid);
	LOGF("%d\n", temp);
	return SUCCESS;
}

int init_light_sensor()
{
	old_light_sensor = get_hw_ver()==EVT2;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	if (old_light_sensor) {
		unsigned char cmd_init[2];

		cmd_init[0] = 0x80; // Command register - 8'b1000_0000
		cmd_init[1] = 0x03; // Control register - 8'b0000_0011
		(I2C_IF_Write(0x29, cmd_init, 2, 1)); // setup normal mode

		cmd_init[0] = 0x81; // Command register - 8'b1000_0000
		cmd_init[1] = 0x02; // Control register - 8'b0000_0010 // 100ms due to page 9 of http://media.digikey.com/pdf/Data%20Sheets/Austriamicrosystems%20PDFs/TSL4531.pdf
		(I2C_IF_Write(0x29, cmd_init, 2, 1)); //  );// change integration
	} else {
		unsigned char aucDataBuf[2] = { 0, 0 };
		aucDataBuf[0] = 0;
		aucDataBuf[1] = 0xA0;
		(I2C_IF_Write(0x44, aucDataBuf, 2, 1));
	}
	xSemaphoreGiveRecursive(i2c_smphr);
	return SUCCESS;
}

static int _read_als(){
	unsigned char cmd;
	unsigned char aucDataBuf[2] = { 0, 0 };

	cmd = 0x2;
	(I2C_IF_Write(0x44, &cmd, 1, 1));
	(I2C_IF_Read(0x44, aucDataBuf, 2));

	return aucDataBuf[0] | (aucDataBuf[1] << 8);
}

int get_light() {
	unsigned char cmd;
	unsigned char aucDataBuf[2] = { 0, 0 };

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	if (old_light_sensor) {
		unsigned char aucDataBuf_LOW[2];
		unsigned char aucDataBuf_HIGH[2];
		int light_lux;

		cmd = 0x84; // Command register - 0x04
		(I2C_IF_Write(0x29, &cmd, 1, 1));
		(I2C_IF_Read(0x29, aucDataBuf_LOW, 1)); //could read 2 here, but we don't use the other one...

		cmd = 0x85; // Command register - 0x05
		(I2C_IF_Write(0x29, &cmd, 1, 1));
		(I2C_IF_Read(0x29, aucDataBuf_HIGH, 1));

		// We are using 100ms mode, multipler is 4
		// formula based on page 6 of http://media.digikey.com/pdf/Data%20Sheets/Austriamicrosystems%20PDFs/TSL4531.pdf
		light_lux = ((aucDataBuf_HIGH[0] << 8) | aucDataBuf_LOW[0]) << 2;

		xSemaphoreGiveRecursive(i2c_smphr);
		return light_lux;
	} else {
		int light = 0;
		static int scaling = 0;
		int prev_scaling = scaling;

		for(;;) {
			#define MAX_RETRIES 10
			int switch_cnt = 0;
			light = _read_als();
			if( light == 65535 ) {
				if( scaling < 3 ) {
					LOGI("increase scaling %d\r\n", scaling);
					aucDataBuf[0] = 1;
					aucDataBuf[1] = ++scaling;
					(I2C_IF_Write(0x44, aucDataBuf, 2, 1));
					while (_read_als() == 65535 && ++switch_cnt < MAX_RETRIES) {
						vTaskDelay(100);
					}
					continue;
				} else {
					break;
				}
			}
			if( light < 16384 ) {
				if( scaling != 0 ) {
					LOGI("decrease scaling %d\r\n", scaling);
					aucDataBuf[0] = 1;
					aucDataBuf[1] = --scaling;
					(I2C_IF_Write(0x44, aucDataBuf, 2, 1));
					while( _read_als() < 16384 && ++switch_cnt < MAX_RETRIES ) {
						vTaskDelay(100);
					}
					continue;
				} else {
					break;
				}
			}
			break;
		}

		light *= (1<<(scaling*2));

		if( scaling != prev_scaling ) {
			aucDataBuf[0] = 1;
			aucDataBuf[1] = scaling;
			(I2C_IF_Write(0x44, aucDataBuf, 2, 1));
		}
		xSemaphoreGiveRecursive(i2c_smphr);
		return light;
	}
}

int Cmd_readlight(int argc, char *argv[]) {
	LOGF("%d\n", get_light());
	return SUCCESS;
}

int init_prox_sensor()
{
	unsigned char prx_cmd_init[2];
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	prx_cmd_init[0] = 0x8f;
	//                  ---++--- delay, frequency, dead time
	prx_cmd_init[1] = 0b10000001;
	(I2C_IF_Write(0x13, prx_cmd_init, 2, 1) );

	prx_cmd_init[0] = 0x83; // Current setting register
	prx_cmd_init[1] = 14; // Value * 10mA
	( I2C_IF_Write(0x13, prx_cmd_init, 2, 1) );

	xSemaphoreGiveRecursive(i2c_smphr);

	return SUCCESS;
}

uint32_t get_prox() {
	unsigned char data[2];
	static uint64_t proximity_raw = 0;
	unsigned char prx_cmd;
	unsigned char prx_cmd_init[2];
	unsigned char cmd_reg = 0;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	prx_cmd_init[0] = 0x80; // Command register - 8'b1000_0000
	prx_cmd_init[1] = 0x08; // one shot measurements

	(I2C_IF_Write(0x13, prx_cmd_init, 2, 1) );

	vTaskDelay(1);
	(I2C_IF_Read(0x13, &cmd_reg,  1 ) );

	if( cmd_reg & 0b00100000 ) {
		prx_cmd = 0x87; // Command register - 0x87
		( I2C_IF_Write(0x13, &prx_cmd, 1, 1) );
		( I2C_IF_Read(0x13, data, 2) );

		proximity_raw = (data[0] << 8) | data[1];
	}

	xSemaphoreGiveRecursive(i2c_smphr);

	return 200000 - proximity_raw * 200000 / 65536;

}

int Cmd_readproximity(int argc, char *argv[]) {
	LOGF("%u\n", get_prox());

	return SUCCESS;
}

bool set_volume(int v, unsigned int dly) {
	unsigned char cmd_init[2];

	cmd_init[0] = 0x6c;
	cmd_init[1] = v;

	if( xSemaphoreTakeRecursive(i2c_smphr, dly) ) {
		I2C_IF_Write(Codec_addr, cmd_init, 2, 1);
		xSemaphoreGiveRecursive(i2c_smphr);
		return true;
	} else {
		return false;
	}
}
int get_codec_io_NAU(void){
	unsigned char cmd_init[2] = {0};
	int i;
	static const char reg[][2] = {//gl
			{ 0x00 ,  0x00   },
			            { 0x03 ,  0x7d   },
			            { 0x04 ,  0x15   },
			            { 0x06 ,  0xfd   },
			            { 0x09 ,  0x90   },//note
			            { 0x0a ,  0x00   },
			            { 0x0d ,  0x08   },//0x48
			            { 0x0e ,  0x00   },
			            { 0x10 ,  0x00   },
			            { 0x12 ,  0x00   },
			            { 0x14 ,  0x08   },
			            { 0x17 , 0xff   },
			            { 0x18 ,  0x00   },
			            { 0x1a ,  0x00   },
			            { 0x1d , 0x08   },
			            { 0x1f , 0xff   },
			            { 0x25 , 0x2c   },
			            { 0x26 ,  0x2c   },
			            { 0x28 ,  0x2c   },
			            { 0x2a ,  0x2c   },
			            { 0x2c ,  0x2c   },
			            { 0x30 ,  0x32   },
			            { 0x32 ,  0x00   },
			            { 0x36 ,  0x00   },
			            { 0x38 ,  0x00   },
			            { 0x3a ,  0x00   },
			            { 0x3c ,  0x00   },
			            { 0x40 ,  0x38   },
			            { 0x42 ,  0x0b   },
			            { 0x44 ,  0x32   },
			            { 0x46 ,  0x00   },
			            { 0x48 ,  0x18   },//048 08
			            { 0x4a ,  0x0c   },
			            { 0x4c ,  0x93   },
			            { 0x4e ,  0xe9   },
			            { 0x50 ,  0x01   },
			            { 0x59 ,  0x82   },//0x58, 0x02
			            { 0x5a ,  0x10   },
			            { 0x5c ,  0x00   },
			            { 0x5e , 0x50   },//0x5f -> 5e, 0x00 -> 0x50
			            { 0x60 ,  0x00   },
			            { 0x62 ,  0x02   },
			            { 0x64 ,  0x01   },
			            { 0x66 ,  0x00   },
			            { 0x68 ,  0x40   },
			            { 0x6a ,  0x40   },
			            { 0x6c ,  0xb9   },
			            { 0x6e ,  0x40   },
			            { 0x70 ,  0x41   }, // set DACMOUT = 1
			            { 0x72 ,  0x40   },
			            { 0x74 ,  0x10   },
		};
		for( i=0;i<sizeof(reg)/2;++i) {
			cmd_init[0] = reg[i][0];
			cmd_init[1] = reg[i][1];
			I2C_IF_Write(Codec_addr, cmd_init, 2, 1);
			DISP("%u : %u \r\n", reg[i][0], reg[i][1]);
			vTaskDelay(DELAY_CODEC);
		}
		return SUCCESS;
}
int get_codec_mic_NAU(int argc, char *argv[]) {
	unsigned char cmd_init[2];
	int i;

	static const char reg[50][2] = {
			{0x00,0x00},
			{0x03,0x3d},
			// Addr D8 		D7 D6    D5    D4        D3      D2     D1,D0
			// 0x01 DCBUFEN 0  AUXEN PLLEN MICBIASEN ABIASEN IOBUFEN REFIMP[1:0]
			// set  1       0  0     1     1         1       0      1  1
			{0x04,0x15},
			// Addr D8  D7 D6    D5    D4      D3   D2     D1 D0
			// 0x02 0   0  0     0     BSTEN   0    PGAEN  0  ADCEN
			// set  0   0  0     0     1       0    1      0  1
			{0x06,0x00},
			// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
			// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
			// set  0  0      0      0      0       0        0       0  0
			{0x09,0x90}, // Can be BCLKP or BCLKP_BAR {0x09 0x90}
			// Addr D8    D7   D6    D5    D4 D3         D2      D1      D0  Default
			// 0x04 BCLKP FSP  WLEN[1:0]   AIFMT[1:0]    DACPHS  ADCPHS  0   0x050
			// set  1     1    0     0     1  0           0       0       0
			{0x0a,0x00},
			// Addr D8    D7   D6    D5    D4 D3         D2   D1      D0     Default
			// 0x05 0     0    0     CMB8  DACCM[1:0]    ADCCM[1:0]   ADDA
			// set  0     0    0     1     1  0           0    0      0
			{0x0d,0x08},
			//Addr D8   D7 D6 D5     D4 D3 D2     D1 D0
			//0x06 CLKM MCLKSEL[2:0] BCLKSEL[2:0] 0  CLKIOEN
			//set  1    0  0  0       0  1 0      0  0
			{0x0e,0x00},
			// Addr D8    D7 D6 D5 D4 D3 D2 D1   D0
			// 0x07 SPIEN 0  0  0  0  SMPLR[2:0] SCLKEN
			// set  0     0  0  0  0  0  1  1    0
			{0x10,0x04},
			// Addr D8    D7 D6 D5 D4 			D3     D2 D1  D0
			//0x08  0     0  0  GPIOPLL[4:5]    GPIOPL GPIOSEL[2:0]
			//      0     0  0  0  0            0      1  0   0
			// General Purpose I/O Selection
			// GPIOSEL [2]  GPIOSEL [1]  GPIOSEL [0]   Mode (Hz)
			//	 0             0             0         CSb Input
			//   0     		   0   			 1         Jack Insert Detect
			//   0   		   1   			 0  	   Temperature OK
			//   0			   1		     1		   AMUTE Active
			//   1  		   0  			 0	       PLL CLK Output
			//   1			   0			 1         PLL Lock
			//   1             1             0         1
			//   1             1             1         0
			{0x12,0x00},
			{0x14,0x08},
			// Addr D8 D7  D6                  D5,D4      D3     D2      D1 D0
			// 0x0A 0  0   DACMT/0: Disable    DEEMP[1:0] DACOS  AUTOMT  0  DACPL
			// set  0  0   0                   0  0       1      0       0  0
			{0x17,0xff},
			{0x18,0x00},
			{0x1a,0x00},
			{0x1d,0xf8},
			//Addr D8    D7    D6 D5 D4 D3    D2 D1 D0      Default
			//0x0E HPFEN HPFAM HPF[2:0] ADCOS 0  0  ADCPL   0x100
			//     1     1     0  0  0  1     0  0  0
			{0x1e,0xff},
			{0x25,0x2c},
			{0x26,0x2c},
			{0x28,0x2c},
			{0x2a,0x2c},
			{0x2c,0x2c},
			{0x30,0x32},
			{0x32,0x00},
			{0x37,0x40},//{0x37,0xc0} Notch filter is on; {0x37,0x40} Notch filter is off
			{0x39,0x15},//Notch 2
			{0x3b,0x3f},//Notch 3
			{0x3d,0x75},//Notch 4
			{0x40,0x38},
			{0x42,0x0b},
			{0x44,0x32},
			{0x46,0x00},
			{0x48,0x18},
			// Addr D8    D7 D6 D5 D4       D3 D2 D1 D0 Default
			// 0x24 0     0  0  0  PLLMCLK  PLLN[3:0]
			//  set 0     0  0  0  1        1  0  0  0
			{0x4a,0x0c},
			{0x4c,0x93},
			{0x4e,0xe9},
			{0x50,0x00},
			{0x59,0x82},
			// Addr       D8    D7 D6  D5 D4   D3     D2       D1       D0      Default
			// 0x2C       MICBIASV 0   0  0    AUXM   AUXPGA   NMICPGA  PMICPGA
			// set        1     1  0   0  0    0      0        1        0
			{0x5a,0x08},
			{0x5c,0x00},
			{0x5e,0x50},
			{0x60,0x00},
			{0x62,0x02},
			{0x64,0x00},
			// Address D8    D7  D6   D5     D4 D3 D2 D1       D0
			// 0x32    0     0   0    AUXSPK 0  0  0  BYPSPK   DACSPK
			// set     0     0   0    0      0  0  0  0        0
			{0x66,0x00},
			{0x68,0x40},
			{0x6a,0x40},
			{0x6c,0x79},
			// Address D8    D7      D6       D5 D4 D3 D2 D1 D0
			// 0x36    0     SPKZC   SPKMT    SPKGAIN[5:0]
			// set     0     0       1        1  1  1  1  1  1
			// 								  1  1  1  0  0  1  0dB
			// 								  1  1  1  0  1  0 +1.0
			// 								  1  1  1  1  1  1 +6.0
			{0x6e,0x40},
			{0x70,0x40},
			// Address D8    D7      D6       D5 D4 D3 D2      D1      D0
			// 0x38    0     0       MOUTMXMT 0  0  0  AUXMOUT BYPMOUT DACMOUT
			// set     0     0       1        0  0  0  0       0       0
			{0x72,0x40},
			//{0x74,0x10}, //Power Management 4
			//Addr  D8      D7    D6     D5    D4        D3   D2     D1 D0
			// 0x3A LPIPBST LPADC LPSPKD LPDAC MICBIASM TRIMREG[3:2] IBADJ[1:0]
			// set  0       0     0      0     1         0    0      0  0
			//{0x78,0xa8}, // 0xa8
			//Addr  D8      D7    D6     D5    D4        D3   D2     D1     D0
			// 0x3C PCMTSEN TRI PCM8BIT PUDOEN PUDPE    PUDPS LOUTR  PCMB TSLOT
			// set  0       1     0      1     0         1    0      0      0
	};
	if( xSemaphoreTakeRecursive(i2c_smphr, 300000) ) {
		vTaskDelay(DELAY_CODEC);
		for( i=0;i<sizeof(reg)/2;++i) {
			cmd_init[0] = reg[i][0];
			cmd_init[1] = reg[i][1];
			I2C_IF_Write(Codec_addr, cmd_init, 2, 1);
			vTaskDelay(DELAY_CODEC);
		}
		xSemaphoreGiveRecursive(i2c_smphr);
	} else {
		LOGW("failed to get i2c %d\n", __LINE__);
	}
	return SUCCESS;
}


int get_codec_NAU(int vol_codec) {
	unsigned char cmd_init[2];
	int i;

	static const char reg[46][2] = {
			{0x00,0x00},
			// Do sequencing for avoid pop and click sounds
			/////////////// 1. Power supplies VDDA, VDDB, VDDC, and VDDSPK /////////////////////
			/////////////// 2. Mode SPKBST and MOUTBST /////////////////////////////////////////
			{0x62,0x00},
			//Addr D8 D7 D6 D5 D4 D3      D2     D1   D0
			//0x31 0  0  0  0  0  MOUTBST SPKBST TSEN AOUTIMP
			//set  0  0  0  0  0  0       0      0    0
			//////////////// 3. Power management ///////////////////////////////////////////////
			{0x02,0x0b},
			// Addr D8 		D7 D6    D5    D4        D3      D2     D1,D0
			// 0x01 DCBUFEN 0  AUXEN PLLEN MICBIASEN ABIASEN IOBUFEN REFIMP[1:0]
			// set  0       0  0     0     0         1       0      1  1
			{0x04,0x00},
			// Addr D8  D7 D6    D5    D4      D3   D2     D1 D0
			// 0x02 0   0  0     0     BSTEN   0    PGAEN  0  ADCEN
			// set  0   0  0     0     0       0    0      0  0
			{0x06,0x10},
			// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
			// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
			// set  0  0      0      0      1       0        0       0  0
    		//////////////// 4. Clock divider //////////////////////////////////////////////////
			{0x0d,0x0c},
			//Addr D8   D7 D6 D5     D4 D3 D2     D1 D0
			//0x06 CLKM MCLKSEL[2:0] BCLKSEL[2:0] 0  CLKIOEN
			//set  1    0  0  0       0  1  1      0  0
			//cmd_init[0] = 0x0e ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(DELAY_CODEC);
			{0x0E,0x00},
			// Addr D8    D7 D6 D5 D4 D3 D2 D1   D0
			// 0x07 SPIEN 0  0  0  0  SMPLR[2:0] SCLKEN
			// set  0     0  0  0  0  0  1  1    0
			//////////////// 5. PLL ////////////////////////////////////////////////////////////
			{0x02,0x2b},
			// Addr D8 		D7 D6    D5    D4        D3      D2     D1,D0
			// 0x01 DCBUFEN 0  AUXEN PLLEN MICBIASEN ABIASEN IOBUFEN REFIMP[1:0]
			// set  0       0  0     1     0         1       0      1  1
			// pre  1       0  0     1     0         1       1      1  1
			//////////////// 6. DAC, ADC ////////////////////////////////////////////////////////
			{0x06,0x11},
			// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
			// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
			// set  0  0      0      0      1       0        0       0  1
			//////////////// 7. SPK MIXER ENABLED ////////////////////////////////////////////////////////
			{0x06,0x15},
			// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
			// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
			// set  0  0      0      0      1       0        1       0  1
			//////////////// 8. Output stages ////////////////////////////////////////////////////////
			{0x06,0x75},
			// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
			// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
			// set  0  0      1      1      1       0        1       0  1
			//////////////// 9. Un-mute DAC ////////////////////////////////////////////////////////
			{0x14,0x0c},
			// Addr D8 D7  D6                  D5,D4      D3     D2      D1 D0
			// 0x0A 0  0   DACMT/0: Disable    DEEMP[1:0] DACOS  AUTOMT  0  DACPL
			// set  0  0   0                   0  0       1      1       0  0
			{0x17,0xff},
			// Addr D8 D7 D6 D5 D4 D3 D2 D1 D0
			// 0x0B 0  DACGAIN
			// set  0  1  1  1  1  1  1  1  1
			//
			//      0  0  0  0  0  0  0  1  1   -126 dB
			//
			//      0  1  1  1  1  1  0  0  1   -3.0 dB
			//      0  1  1  1  1  1  0  1  0   -2.5 dB
			//      0  1  1  1  1  1  0  1  1   -2.0 dB
			// 	    0  1  1  1  1  1  1  0  0   -1.5 dB
			// 		0  1  1  1  1  1  1  1  1      0 dB
			{0x09,0x10},
			// Addr D8    D7   D6    D5    D4 D3         D2      D1      D0  Default
			// 0x04 BCLKP FSP  WLEN[1:0]   AIFMT[1:0]    DACPHS  ADCPHS  0   0x050
			// set  0     0    0     0     1  0          0       0       0
			{0x0a,0x00},
			// Addr D8    D7   D6    D5    D4 D3         D2   D1      D0     Default
			// 0x05 0     0    0     CMB8  DACCM[1:0]    ADCCM[1:0]   ADDA
			// set  0     0    0     1     1  0           0    0      0
			{0x10,0x04},
			// Addr D8    D7 D6 D5 D4 			D3     D2 D1  D0
			//0x08  0     0  0  GPIOPLL[4:5]    GPIOPL GPIOSEL[2:0]
			//      0     0  0  0  0            0      1  0   0
			// General Purpose I/O Selection
			// GPIOSEL [2]  GPIOSEL [1]  GPIOSEL [0]   Mode (Hz)
			//	 0             0             0         CSb Input
			//   0     		   0   			 1         Jack Insert Detect
			//   0   		   1   			 0  	   Temperature OK
			//   0			   1		     1		   AMUTE Active
			//   1  		   0  			 0	       PLL CLK Output
			//   1			   0			 1         PLL Lock
			//   1             1             0         1
			//   1             1             1         0
			{0x1c,0x00},
			//Addr D8    D7    D6 D5 D4 D3    D2 D1 D0      Default
			//0x0E HPFEN HPFAM HPF[2:0] ADCOS 0  0  ADCPL   0x100
			//     0     0     0  0  0  1     0  0  0
			{0x1e,0xff},
			{0x25,0x0a}, // 0x25 -> EQ on/ 0x24 -> EQ off
			// Address D8  D7 D6   D5     D4 D3 D2 D1 D0
			// 0x12    EQM 0  EQ1CF[1:0]  EQ1GC[4:0]
			// set     1   0  1    1      0  0  0  0  1
			{0x27,0x4a},
			// Address D8    D7 D6   D5     D4 D3 D2 D1 D0
			// 0x13    EQ2BW 0  EQ2CF[1:0]  EQ2GC[4:0]
			// set     1     0  1    0      0  0  0  1  0
			{0x29,0x6c},
			// Address D8    D7 D6   D5     D4 D3 D2 D1 D0
			// 0x14    EQ3BW 0  EQ3CF[1:0]  EQ3GC[4:0]
			// set     1     0  1    0      0  1  0  0  0
			{0x2b,0x6c},
			// Address D8    D7 D6   D5     D4 D3 D2 D1 D0
			// 0x15    EQ4BW 0  EQ4CF[1:0]  EQ4GC[4:0]
			// set     1     0  1    0      0  0  1  0  1
			{0x2c,0x6c},
			// Address D8    D7  D6   D5     D4 D3 D2 D1 D0
			// 0x16    0     0   EQ5CF[1:0]  EQ5GC[4:0]
			// set     0     0   0    0      1  1  0  0  0
			{0x30,0x32},
			//Addr D8          D7 D6 D5 D4    D3 D2 D1 D0
			//0x18 DACLIMEN    DACLIMDCY[3:0] DACLIMATK[3:0]
			//set  0           0  0  1  1     0  0  1  0
			{0x32,0x00},
			//0x19 0    0 DACLIMTHL[2:0] DACLIMBST[3:0]
			// set 0    0   0  0  0      0 0 0 0
			{0x36,0xC0},
			{0x38,0x6B},
			{0x3a,0x3F},
			{0x3d,0x05},
			{0x40,0x38},
			// Addr D8    D7 D6 D5 D4 D3        D2 D1 D0 Default
			// 0x20 ALCEN 0  0  ALCMXGAIN[2:0]  ALCMNGAIN[2:0]
			//      0     0  0  1  1  1         0  0  0
			{0x42,0x0b},
			// Addr D8    D7 D6 D5 D4   D3 D2 D1 D0 Default
			// 0x21 ALCZC ALCHT[3:0]    ALCSL[3:0]
			//      0     0  0  0  0    1  0  1  1
			{0x44,0x32},
			// Addr D8    D7 D6 D5 D4   D3 D2 D1 D0 Default
			//0x22  ALCM  ALCDCY[3:0]   ALCATK[3:0]
			// set  0     0  0  1  1    0  0  1  0
			{0x46,0x00},
			// Addr D8    D7 D6 D5 D4   D3      D2 D1 D0 Default
			// 0x23	0     0  0  0  0    ALCNEN  ALCNTH[2:0]
			//      0     0  0  0  0    0       0  0  0
			{0x48,0x18},
			// Addr D8    D7 D6 D5 D4       D3 D2 D1 D0 Default
			// 0x24 0     0  0  0  PLLMCLK  PLLN[3:0]
			//  set 0     0  0  0  1        1  0  0  0
			{0x4a,0x0c},
			// Addr D8    D7 D6  D5 D4 D3 D2 D1 D0 Default
			// 0x25 0     0  0   PLLK[23:18]
			// set  0     0  0   0  0  1  1  0  0
			{0x4c,0x93},
			// Addr       D8    D7 D6  D5 D4 D3 D2 D1 D0 Default
			// 0x26       PLLK[17:9]
			// set        0     1  0   0  1  0  0  1  1
			{0x4e,0xe9},
			// Addr       D8    D7 D6  D5 D4 D3 D2 D1 D0 Default
			// 0x27       PLLK[8:0]
			// set        0     1  1   1  0  1  0  0  1
			{0x50,0x02},
			// Addr       D8    D7 D6  D5 D4 D3 D2       D1      D0 Default
			// 0x28       0     0  0   0  0  0  MOUTATT  SPKATT  0
			//
			{0x58,0x00},
			// Addr       D8    D7 D6  D5 D4   D3     D2       D1       D0      Default
			// 0x2C       MICBIASV 0   0  0    AUXM   AUXPGA   NMICPGA  PMICPGA
			// set
			{0x5a,0x50},
			// Addr       D8    D7    D6     D5 D4 D3 D2 D1 D0      Default
			// 0x2D       0     PGAZC PGAMT  PGAGAIN[5:0]
			// set        0     0     1      0  1  0  0  0  0
			{0x5f,0x00},
			// Addr       D8     D7    D6   D5 D4  D3 D2 D1 D0      Default
			// 0x2F       PGABST 0     PMICBSTGAIN 0  AUXBSTGAIN
			//
			{0x64,0x01},
			// Address D8    D7  D6   D5     D4 D3 D2 D1       D0
			// 0x32    0     0   0    AUXSPK 0  0  0  BYPSPK   DACSPK
			// set     0     0   0    0      0  0  0  0        1
			{0x6c,60},
			// Address D8    D7      D6       D5 D4 D3 D2 D1 D0
			// 0x36    0     SPKZC   SPKMT    SPKGAIN[5:0]
			// set     0     1       0        1  1  1  1  1  1
			// 								  1  1  1  0  0  1  0dB
			// 								  1  1  1  0  1  0 +1.0
			// 								  1  1  1  1  1  1 +6.0
			{0x70,0x40},
			// Address D8    D7      D6       D5 D4 D3 D2      D1      D0
			// 0x38    0     0       MOUTMXMT 0  0  0  AUXMOUT BYPMOUT DACMOUT
			// set     0     0       1        0  0  0  0       0       0
			{0x74,0x00},
			//Addr  D8      D7    D6     D5    D4        D3   D2     D1 D0
			// 0x3A LPIPBST LPADC LPSPKD LPDAC MICBIASM TRIMREG[3:2] IBADJ[1:0]
			// set  0       0     0      0     0         0    0      0  0
			{0x92,0xc1},
			//Addr D8     D7 D6            D5    D4      D3    D2      D1       D0
			//0x49 SPIEN FSERRVAL[1:0] FSERFLSH FSERRENA NFDLY DACINMT PLLLOCKP DACOS256
			//     0      1  1             0     0       0     0       0        1
	};
	if (xSemaphoreTakeRecursive(i2c_smphr, 300000)) {
		vTaskDelay(DELAY_CODEC);
		for (i = 0; i < sizeof(reg)/2; ++i) {
			cmd_init[0] = reg[i][0];
			cmd_init[1] = reg[i][1];
			if (cmd_init[0] == 0x6c) {
				cmd_init[1] = vol_codec;
			}
			I2C_IF_Write(Codec_addr, cmd_init, 2, 1);
			vTaskDelay(DELAY_CODEC);
		}
		xSemaphoreGiveRecursive(i2c_smphr);
	} else {
		LOGW("failed to get i2c %d\n", __LINE__);
	}
	LOGI(" codec is testing \n\r");
	return SUCCESS;
}


int close_codec_NAU(int argc, char *argv[]) {
	unsigned char cmd_init[2];
	int i;

	static const char reg[3][2] = {
			//////// 1.  Un-mute DAC DACMT[6] = 1
			{0x14,0x4c},
			// Addr D8 D7  D6                  D5,D4      D3     D2      D1 D0
			// 0x0A 0  0   DACMT/0: Disable    DEEMP[1:0] DACOS  AUTOMT  0  DACPL
			// set  0  0   1                   0  0       1      1       0  0
			//////// 2.  Power Management PWRM1 = 0x000
			{0x02,0x00},// Power Management 1
			// Addr D8 		D7 D6    D5    D4        D3      D2     D1,D0
			// 0x01 DCBUFEN 0  AUXEN PLLEN MICBIASEN ABIASEN IOBUFEN REFIMP[1:0]
			// set  0       0  0     0     0         0       0      0  0
			//////// 3.  Output stages MOUTEN[7] NSPKEN PSPKEN
			{0x06,0x15}, // Power Management 3
			// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
			// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
			// set  0  0      0      0      1       0        1       0  1
			//////// 4.  Power supplies Analog VDDA VDDB VDDC VDDSPK
	};
	if (xSemaphoreTakeRecursive(i2c_smphr, 300000)) {
		vTaskDelay(DELAY_CODEC);
		for (i = 0; i < sizeof(reg)/2; ++i) {
			cmd_init[0] = reg[i][0];
			cmd_init[1] = reg[i][1];
			I2C_IF_Write(Codec_addr, cmd_init, 2, 1);
			vTaskDelay(DELAY_CODEC);
		}
		xSemaphoreGiveRecursive(i2c_smphr);
	} else {
		LOGW("failed to get i2c %d\n", __LINE__);
	}

	return 0;
}
