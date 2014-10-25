#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <assert.h>
#include <stdint.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "i2c_if.h"
#include "uartstdio.h"
#include "i2c_cmd.h"


#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}
#define BUF_SIZE 2

#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %d\n\r", __FILE__, __LINE__ ); return FAILURE;}

#define Codec_addr 0x1A
#define delay_codec 10

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
	UARTprintf("Read contents");
	UARTprintf("\n\r");
	while (ucBufIndx < ucLen) {
		UARTprintf(" 0x%x, ", pucDataBuf[ucBufIndx]);
		ucBufIndx++;
		if ((ucBufIndx % 8) == 0) {
			UARTprintf("\n\r");
		}
	}
	UARTprintf("\n\r");
}

int Cmd_i2c_read(int argc, char *argv[]) {
	unsigned char ucDevAddr, ucLen;
	unsigned char aucDataBuf[256];
	char *pcErrPtr;
	int iRetVal;

	if (argc != 3) {
		UARTprintf(
				"read  <dev_addr> <rdlen> \n\r\t - Read data frpm the specified i2c device\n\r");
		return FAILURE;
	}

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

	if (iRetVal == SUCCESS) {
		UARTprintf("I2C_IF_ Read complete\n\r");
		//
		// Display the buffer over UART on successful write
		//
		DisplayBuffer(aucDataBuf, ucLen);
	} else {
		UARTprintf("I2C_IF_ Read failed\n\r");
		return FAILURE;
	}

	return SUCCESS;
}

int Cmd_i2c_writereg(int argc, char *argv[]) {
	unsigned char ucDevAddr, ucRegOffset, ucWrLen;
	unsigned char aucDataBuf[256];
	char *pcErrPtr;
	int iLoopCnt = 0;

	if (argc != 4) {
		UARTprintf(
				"writereg <dev_addr> <reg_offset> <wrlen> <<byte0> [<byte1> ... ]> \n\r");
		return FAILURE;
	}
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

	UARTprintf("I2C_IF_ Write To address complete\n\r");

	return SUCCESS;
}
int Cmd_i2c_readreg(int argc, char *argv[]) {
	unsigned char ucDevAddr, ucRegOffset, ucRdLen;
	unsigned char aucRdDataBuf[256];
	char *pcErrPtr;

	if (argc != 4) {
		UARTprintf("readreg <dev_addr> <reg_offset> <rdlen> \n\r");
		return FAILURE;
	}
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

	UARTprintf("I2C_IF_ Read From address complete\n");
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
		UARTprintf("write <dev_addr> <wrlen> <<byte0>[<byte1> ... ]>\n\r");
		return FAILURE;
	}

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
	if (iRetVal == SUCCESS) {
		UARTprintf("I2C_IF_ Write complete\n\r");
	} else {
		UARTprintf("I2C_IF_ Write failed\n\r");
		return FAILURE;
	}

	return SUCCESS;

}

int init_temp_sensor()
{
	unsigned char cmd = 0xfe;
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));    // reset

	get_temp();

	return SUCCESS;
}


int get_temp() {
	unsigned char cmd = 0xe3;
	int temp_raw;
	int temp;

	unsigned char aucDataBuf[2];
	vTaskDelay(10);
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));

	vTaskDelay(50);
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x40, aucDataBuf, 2));
	temp_raw = (aucDataBuf[0] << 8) | ((aucDataBuf[1] & 0xfc));
	
	temp = 17572 * temp_raw / 65536 - 4685;

	return temp;
}

int Cmd_readtemp(int argc, char *argv[]) {
	UARTprintf("temp is %d\n", get_temp());
	return SUCCESS;
}

int init_humid_sensor()
{
	unsigned char cmd = 0xfe;
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));    // reset

	// Dummy read the 1st value.
	get_humid();

	return SUCCESS;
}

int get_humid() {
	unsigned char aucDataBuf[2];
	unsigned char cmd = 0xe5;
	int humid_raw;
	int humid;

	vTaskDelay(10);
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));

	vTaskDelay(50);
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x40, aucDataBuf, 2));
	humid_raw = (aucDataBuf[0] << 8) | ((aucDataBuf[1] & 0xfc));
	

	humid = 12500 * humid_raw / 65536 - 600;
	return humid;
}

int Cmd_readhumid(int argc, char *argv[]) {
	UARTprintf("humid is %d\n", get_humid());
	return SUCCESS;
}

int init_light_sensor()
{
	unsigned char cmd_init[2];

	cmd_init[0] = 0x80; // Command register - 8'b1000_0000
	cmd_init[1] = 0x03; // Control register - 8'b0000_0011
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x29, cmd_init, 2, 1)); // setup normal mode

	cmd_init[0] = 0x81; // Command register - 8'b1000_0000
	cmd_init[1] = 0x02; // Control register - 8'b0000_0010 // 100ms due to page 9 of http://media.digikey.com/pdf/Data%20Sheets/Austriamicrosystems%20PDFs/TSL4531.pdf
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x29, cmd_init, 2, 1)); //  );// change integration
	

	return SUCCESS;
}

int get_light() {
	unsigned char aucDataBuf_LOW[2];
	unsigned char aucDataBuf_HIGH[2];
	int light_lux;

	unsigned char cmd;
	
	cmd = 0x84; // Command register - 0x04
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x29, &cmd, 1, 1));
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x29, aucDataBuf_LOW, 1)); //could read 2 here, but we don't use the other one...

	cmd = 0x85; // Command register - 0x05
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x29, &cmd, 1, 1));
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x29, aucDataBuf_HIGH, 1));


	// We are using 100ms mode, multipler is 4
	// formula based on page 6 of http://media.digikey.com/pdf/Data%20Sheets/Austriamicrosystems%20PDFs/TSL4531.pdf
	light_lux = ((aucDataBuf_HIGH[0] << 8) | aucDataBuf_LOW[0]) << 2;

	return light_lux;
		
}

int Cmd_readlight(int argc, char *argv[]) {
	UARTprintf(" light is %d\n", get_light());

	return SUCCESS;
}

int init_prox_sensor()
{
	unsigned char prx_cmd_init[2];

//		prx_cmd_init[0] = 0x82;
//		prx_cmd_init[1] = 111; // max speed
//		TRY_OR_GOTOFAIL(I2C_IF_Write(0x13, prx_cmd_init, 2, 1) );

	prx_cmd_init[0] = 0x83; // Current setting register
	prx_cmd_init[1] = 20; // Value * 10mA
	TRY_OR_GOTOFAIL( I2C_IF_Write(0x13, prx_cmd_init, 2, 1) );


	return SUCCESS;
}

int get_prox() {
	unsigned char prx_aucDataBuf_LOW[2];
	unsigned char prx_aucDataBuf_HIGH[2];
	int proximity_raw;
	unsigned char prx_cmd;

	unsigned char prx_cmd_init[2];
	prx_cmd_init[0] = 0x80; // Command register - 8'b1000_0000
	prx_cmd_init[1] = 0x08; // one shot measurements
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x13, prx_cmd_init, 2, 1) );// reset


	prx_cmd = 0x88; // Command register - 0x87
	TRY_OR_GOTOFAIL( I2C_IF_Write(0x13, &prx_cmd, 1, 1) );
	TRY_OR_GOTOFAIL( I2C_IF_Read(0x13, prx_aucDataBuf_LOW, 1) );  //only using top byte...

	prx_cmd = 0x87; // Command register - 0x87
	TRY_OR_GOTOFAIL( I2C_IF_Write(0x13, &prx_cmd, 1, 1) );
	TRY_OR_GOTOFAIL( I2C_IF_Read(0x13, prx_aucDataBuf_HIGH, 1) );   //only using top byte...

	proximity_raw = (prx_aucDataBuf_HIGH[0] << 8) | prx_aucDataBuf_LOW[0];

	return 200000 - proximity_raw * 200000 / 65536;

}

int Cmd_readproximity(int argc, char *argv[]) {
	UARTprintf(" proximity is %d\n", get_prox());
	return SUCCESS;
}

int get_codec_mic_NAU(int argc, char *argv[]) {
unsigned char cmd_init[2];
cmd_init[0] = 0x00 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Software reset
cmd_init[0] = 0x03 ; cmd_init[1] = 0x2d ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PWR 1 // 7d
cmd_init[0] = 0x04 ; cmd_init[1] = 0x15 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PWR 2
cmd_init[0] = 0x06 ; cmd_init[1] = 0xf9 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PWR 3
cmd_init[0] = 0x09 ; cmd_init[1] = 0x18 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Audio interface
cmd_init[0] = 0x0a ; cmd_init[1] = 0x01 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Companding
cmd_init[0] = 0x0d ; cmd_init[1] = 0x48 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // CLK control 1
cmd_init[0] = 0x0e ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // CLK control 2
cmd_init[0] = 0x10 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // GPIO contrl
//cmd_init[0] = 0x12 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x14 ; cmd_init[1] = 0x08 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // DAC control
cmd_init[0] = 0x17 ; cmd_init[1] = 0xff ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // DAC volume
//cmd_init[0] = 0x18 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); //
//cmd_init[0] = 0x1a ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); //
cmd_init[0] = 0x1d ; cmd_init[1] = 0xf8 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // ADC control
cmd_init[0] = 0x1e ; cmd_init[1] = 0xff ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // ADC Volume
cmd_init[0] = 0x25 ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // EQ1-Low cutoff
cmd_init[0] = 0x26 ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // EQ2- Peak1
cmd_init[0] = 0x28 ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // EQ3- Peak2
cmd_init[0] = 0x2a ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // EQ4- Peak3
cmd_init[0] = 0x2c ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // EQ5- High cutoff
cmd_init[0] = 0x30 ; cmd_init[1] = 0x32 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // DAC Limiter 1
cmd_init[0] = 0x32 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // DAC Limiter 2
cmd_init[0] = 0x37 ; cmd_init[1] = 0xc0 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Notch filter 1B (0x36)
cmd_init[0] = 0x39 ; cmd_init[1] = 0xeb ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Notch filter 1C (0x38)
cmd_init[0] = 0x3b ; cmd_init[1] = 0xbf ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Notch filter 1D (0x3a)
cmd_init[0] = 0x3d ; cmd_init[1] = 0x85 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Notch filter 1E (0x3c)
cmd_init[0] = 0x40 ; cmd_init[1] = 0x38 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // ALC CTRL 1
cmd_init[0] = 0x42 ; cmd_init[1] = 0x0b ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // ALC CTRL 2
cmd_init[0] = 0x44 ; cmd_init[1] = 0x32 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // ALC CTRL 3
cmd_init[0] = 0x46 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Noise Gate
cmd_init[0] = 0x48 ; cmd_init[1] = 0x08 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PLL N CTRL
cmd_init[0] = 0x4a ; cmd_init[1] = 0x0c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PLL K 1
cmd_init[0] = 0x4c ; cmd_init[1] = 0x93 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PLL K 2
cmd_init[0] = 0x4e ; cmd_init[1] = 0xe9 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PLL K 3
cmd_init[0] = 0x50 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Attenuation CTRL
cmd_init[0] = 0x58 ; cmd_init[1] = 0x02 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Input CTRL MICBIASV
cmd_init[0] = 0x5a ; cmd_init[1] = 0x02 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PGA Gain //0x10 ->0dB
//cmd_init[0] = 0x5c ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x5f ; cmd_init[1] = 0x50 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // ADC Boost
//cmd_init[0] = 0x60 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x62 ; cmd_init[1] = 0x02 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Output CTRL
cmd_init[0] = 0x64 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // MixerCTRL
//cmd_init[0] = 0x66 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x68 ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x6a ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x6c ; cmd_init[1] = 0xb9 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // SPKOUT Volume
cmd_init[0] = 0x6e ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x70 ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // MONO Mixer Control
cmd_init[0] = 0x72 ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
cmd_init[0] = 0x74 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 4
cmd_init[0] = 0x78 ; cmd_init[1] = 0x8C ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);// 8C : high Z // ADCOUTDrive
	//UARTprintf(" Mic codec is testing \n\r");

	return SUCCESS;
}


int get_codec_NAU(int argc, char *argv[]) {
	unsigned char cmd_init[2];
	//int light_raw;

	cmd_init[0] = 0x00 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(200); // reset register
	// Do sequencing for avoid pop and click sounds
	/////////////// 1. Power supplies VDDA, VDDB, VDDC, and VDDSPK /////////////////////
	/////////////// 2. Mode SPKBST and MOUTBST /////////////////////////////////////////
	cmd_init[0] = 0x62 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Output Register
	//Addr D8 D7 D6 D5 D4 D3      D2     D1   D0
	//0x31 0  0  0  0  0  MOUTBST SPKBST TSEN AOUTIMP
	//set  0  0  0  0  0  0       0      0    0
	//////////////// 3. Power management ///////////////////////////////////////////////
	cmd_init[0] = 0x02 ; cmd_init[1] = 0x0b ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 1
	// Addr D8 		D7 D6    D5    D4        D3      D2     D1,D0
	// 0x01 DCBUFEN 0  AUXEN PLLEN MICBIASEN ABIASEN IOBUFEN REFIMP[1:0]
	// set  0       0  0     0     0         1       0      1  1
	cmd_init[0] = 0x04 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 2
	// Addr D8  D7 D6    D5    D4      D3   D2     D1 D0
	// 0x02 0   0  0     0     BSTEN   0    PGAEN  0  ADCEN
	// set  0   0  0     0     0       0    0      0  0
	cmd_init[0] = 0x06 ; cmd_init[1] = 0x10 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 3
	// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
	// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
	// set  0  0      0      0      1       0        0       0  0
	//////////////// 4. Clock divider //////////////////////////////////////////////////
	cmd_init[0] = 0x0D ; cmd_init[1] = 0x48 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); //Clock Control Register
	//Addr D8   D7 D6 D5     D4 D3 D2     D1 D0
	//0x06 CLKM MCLKSEL[2:0] BCLKSEL[2:0] 0  CLKIOEN
	//set  1    0  1  0      0  1  0      0  0
	cmd_init[0] = 0x0E ; cmd_init[1] = 0x06 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); //Audio Sample Rate Control Register 00 0x06:16kHz
	// Addr D8    D7 D6 D5 D4 D3 D2 D1   D0
	// 0x07 SPIEN 0  0  0  0  SMPLR[2:0] SCLKEN
	// set  0     0  0  0  0  0  1  1    0
	//////////////// 5. PLL ////////////////////////////////////////////////////////////
	cmd_init[0] = 0x02 ; cmd_init[1] = 0x2b ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 1
	// Addr D8 		D7 D6    D5    D4        D3      D2     D1,D0
	// 0x01 DCBUFEN 0  AUXEN PLLEN MICBIASEN ABIASEN IOBUFEN REFIMP[1:0]
	// set  0       0  0     1     0         1       0      1  1
	//////////////// 6. DAC, ADC ////////////////////////////////////////////////////////
	cmd_init[0] = 0x06 ; cmd_init[1] = 0x11 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 3
	// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
	// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
	// set  0  0      0      0      1       0        0       0  1
	//////////////// 7. SPK MIXER ENABLED ////////////////////////////////////////////////////////
	cmd_init[0] = 0x06 ; cmd_init[1] = 0x15 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 3
	// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
	// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
	// set  0  0      0      0      1       0        1       0  1
	//////////////// 8. Output stages ////////////////////////////////////////////////////////
	cmd_init[0] = 0x06 ; cmd_init[1] = 0x75 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 3
	// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
	// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
	// set  0  0      1      1      1       0        1       0  1
	//////////////// 9. Un-mute DAC ////////////////////////////////////////////////////////
	cmd_init[0] = 0x14 ; cmd_init[1] = 0x0C ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // DAC control
	// Addr D8 D7  D6                  D5,D4      D3     D2      D1 D0
	// 0x0A 0  0   DACMT/0: Disable    DEEMP[1:0] DACOS  AUTOMT  0  DACPL
	// set  0  0   0                   0  0       1      1       0  0
	cmd_init[0] = 0x17 ; cmd_init[1] = 0xff ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // DAC Gain Control Register
	// Addr D8 D7 D6 D5 D4 D3 D2 D1 D0
	// 0x0B 0  DACGAIN
	// set  0  1  1  1  1  1  1  1  1
	cmd_init[0] = 0x08 ; cmd_init[1] = 0x10 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);


	cmd_init[0] = 0x10 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x12 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);


	cmd_init[0] = 0x18 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x1a ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x1D ; cmd_init[1] = 0x08 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x1f ; cmd_init[1] = 0xff ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x25 ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x26 ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x28 ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x2a ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x2c ; cmd_init[1] = 0x2c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x30 ; cmd_init[1] = 0x32 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x32 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x36 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x38 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x3a ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x3c ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x40 ; cmd_init[1] = 0x38 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x42 ; cmd_init[1] = 0x0b ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x44 ; cmd_init[1] = 0x32 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x46 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x48 ; cmd_init[1] = 0x08 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x4a ; cmd_init[1] = 0x0c ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x4c ; cmd_init[1] = 0x93 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x4e ; cmd_init[1] = 0xe9 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x50 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x58 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x5a ; cmd_init[1] = 0x50 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x5c ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x5f ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x60 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);

	cmd_init[0] = 0x64 ; cmd_init[1] = 0x01 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x66 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x68 ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x6a ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x6c ; cmd_init[1] = 0xb9 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x6e ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x70 ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x72 ; cmd_init[1] = 0x40 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x74 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);

	UARTprintf(" codec is testing \n\r");




	return SUCCESS;
}


int close_codec_NAU(int argc, char *argv[]) {
	unsigned char cmd_init[2];
	//////// 1.  Un-mute DAC DACMT[6] = 1
	cmd_init[0] = 0x14 ; cmd_init[1] = 0x4C ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // DAC control
	// Addr D8 D7  D6                  D5,D4      D3     D2      D1 D0
	// 0x0A 0  0   DACMT/0: Disable    DEEMP[1:0] DACOS  AUTOMT  0  DACPL
	// set  0  0   1                   0  0       1      1       0  0
	//////// 2.  Power Management PWRM1 = 0x000
	cmd_init[0] = 0x02 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 1
	// Addr D8 		D7 D6    D5    D4        D3      D2     D1,D0
	// 0x01 DCBUFEN 0  AUXEN PLLEN MICBIASEN ABIASEN IOBUFEN REFIMP[1:0]
	// set  0       0  0     0     0         0       0      0  0
	//////// 3.  Output stages MOUTEN[7] NSPKEN PSPKEN
	cmd_init[0] = 0x06 ; cmd_init[1] = 0x15 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Power Management 3
	// Addr D8 D7     D6     D5     D4      D3       D2      D1 D0
	// 0x03 0  MOUTEN NSPKEN PSPKEN BIASGEN MOUTMXEN SPKMXEN 0  DACEN
	// set  0  0      0      0      1       0        1       0  1
	//////// 4.  Power supplies Analog VDDA VDDB VDDC VDDSPK
}
