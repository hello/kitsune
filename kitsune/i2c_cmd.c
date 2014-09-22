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



#define FAILURE                 -1
#define SUCCESS                 0

#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}
#define BUF_SIZE 2

#define Codec_addr 0x1A
#define delay_codec 50

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

	UARTprintf("I2C_IF_ Read From address complete\n\r");
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

int get_temp() {
#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %d\n\r", __FILE__, __LINE__ ); return FAILURE;}
	unsigned char aucDataBuf[2];

	unsigned char cmd = 0xfe;
	int temp_raw;
	int temp;

	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));    // reset

	vTaskDelay(10);

	cmd = 0xe3;
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));

	vTaskDelay(50);
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x40, aucDataBuf, 2));
	temp_raw = (aucDataBuf[0] << 8) | ((aucDataBuf[1] & 0xfc));
	temp = temp_raw;

	temp *= 175;
	temp /= (65536/100);
	temp -= 47*100;

	return temp;
}

int Cmd_readtemp(int argc, char *argv[]) {
	UARTprintf("temp is %d\n\rç", get_temp());
	return SUCCESS;
}

int get_humid() {
#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %d\n\r", __FILE__, __LINE__ ); return FAILURE;}
	unsigned char aucDataBuf[2];
	unsigned char cmd = 0xfe;
	int humid_raw;
	int humid;

	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));    // reset

	vTaskDelay(10);

	cmd = 0xe5;
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x40, &cmd, 1, 1));

	vTaskDelay(50);
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x40, aucDataBuf, 2));
	humid_raw = (aucDataBuf[0] << 8) | ((aucDataBuf[1] & 0xfc));
	humid = humid_raw;

	humid *= 125;
	humid /= 65536/100;
	humid -= 6*100;

	return humid;
}

int Cmd_readhumid(int argc, char *argv[]) {

	UARTprintf("humid is %d\n\rç", get_humid());
	return SUCCESS;
}

int get_light() {
	#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %d\n\r", __FILE__, __LINE__ ); return FAILURE;}
	unsigned char aucDataBuf_LOW[2];
	unsigned char aucDataBuf_HIGH[2];
	unsigned char setup_config;
	unsigned char cmd_init[2];
	int light_raw;

	unsigned char cmd = 0x81; // Command register - 0x01

	cmd_init[0] = 0x80; // Command register - 8'b1000_0000
	cmd_init[1] = 0x0F; // Control register - 8'b0000_1111
//RET_IF_ERR(
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x39, cmd_init, 2, 1)); //  );// reset
//RET_IF_ERR( I2C_IF_Write(ucDevAddr,&aucDataBuf[0],ucWrLen+1,1));
//vTaskDelay(10);

	cmd_init[0] = 0x81; // Command register - 8'b1000_0000
	cmd_init[1] = 0x00; // Control register - 8'b0000_0000 // 400ms
//RET_IF_ERR(
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x39, cmd_init, 2, 1)); //  );// change integration
	vTaskDelay(50);

	TRY_OR_GOTOFAIL(I2C_IF_Write(0x39, &cmd, 1, 1));
	vTaskDelay(50);
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x39, &setup_config, 1)); // configure

	cmd = 0x84; // Command register - 0x04
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x39, &cmd, 1, 1));
//vTaskDelay(50);
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x39, aucDataBuf_LOW, 2));

	cmd = 0x85; // Command register - 0x05
	TRY_OR_GOTOFAIL(I2C_IF_Write(0x39, &cmd, 1, 1));
//vTaskDelay(50);
	TRY_OR_GOTOFAIL(I2C_IF_Read(0x39, aucDataBuf_HIGH, 2));

//light_raw = aucDataBuf[0];
	light_raw = ((aucDataBuf_HIGH[0] << 8) | aucDataBuf_LOW[0]) << 0;

	return light_raw;
}

int Cmd_readlight(int argc, char *argv[]) {

	UARTprintf(" light is %d\n\r", get_light());

	return SUCCESS;
}

int get_prox() {
	unsigned char prx_aucDataBuf_LOW[2];
	unsigned char prx_aucDataBuf_HIGH[2];
	unsigned char prx_cmd_init[2];
	int proximity_raw;
	unsigned char prx_cmd = 0x88; // Command register - 0x88

	//int proximity;
	prx_cmd_init[0] = 0x80; // Command register - 8'b1000_0000
	prx_cmd_init[1] = 0x08; // Control register - 8'b0000_1000
	//RET_IF_ERR(

	I2C_IF_Write(0x13, prx_cmd_init, 2, 1);	//  );// reset
	//RET_IF_ERR( I2C_IF_Write(ucDevAddr,&aucDataBuf[0],ucWrLen+1,1));
	//vTaskDelay(10);

	unsigned char prx_cmd_current[2];
	//int proximity;
	prx_cmd_current[0] = 0x83; // Current setting register
	prx_cmd_current[1] = 0x14; // Value * 10mA
	//RET_IF_ERR(
	I2C_IF_Write(0x13, prx_cmd_current, 2, 1);

	I2C_IF_Write(0x13, &prx_cmd, 1, 1);    // );
	//vTaskDelay(50);
	I2C_IF_Read(0x13, prx_aucDataBuf_LOW, 2);    // );

	prx_cmd = 0x87; // Command register - 0x87
	I2C_IF_Write(0x13, &prx_cmd, 1, 1); // );
	//vTaskDelay(50);
	I2C_IF_Read(0x13, prx_aucDataBuf_HIGH, 2);    // );

	//light_raw = aucDataBuf[0];
	proximity_raw = (prx_aucDataBuf_HIGH[0] << 8) | prx_aucDataBuf_LOW[0];
	//proximity = proximity_raw;
	return proximity_raw;
}

int Cmd_readproximity(int argc, char *argv[]) {

	UARTprintf(" proximity is %d\n\r", get_prox());
	return SUCCESS;
}

int get_codec_mic_NAU(int argc, char *argv[]) {
#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %s\n\r", __FILE__, __LINE__ ); return FAILURE;}
unsigned char cmd_init[2];
cmd_init[0] = 0x00 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // Software reset
cmd_init[0] = 0x03 ; cmd_init[1] = 0x7d ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PWR 1
cmd_init[0] = 0x04 ; cmd_init[1] = 0x15 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PWR 2
cmd_init[0] = 0x06 ; cmd_init[1] = 0xfd ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // PWR 3
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
cmd_init[0] = 0x64 ; cmd_init[1] = 0x01 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec); // MixerCTRL
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
	#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %s\n\r", __FILE__, __LINE__ ); return FAILURE;}
	unsigned char cmd_init[2];
	//int light_raw;

	//unsigned char cmd = 0x81; // Command register - 0x01
/*
	cmd_init[0] = 0x00; // Contro register - 8'b0000_000_0
	cmd_init[1] = 0x00; // Control register - 8'b0000_0000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x03; // Control register - 8'b0000_001_1
	cmd_init[1] = 0x6D; // Control register - 8'b0110_1101
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x04; // Control register - 8'b0000_010_0
	cmd_init[1] = 0x15; // Control register -
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x06; // Control register - 8'b0000_011_0
	cmd_init[1] = 0xFD; // Control register - 1111_1101
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x09; // Control register 4 - 8'b0000_100_1
	cmd_init[1] = 0x00; // 0001_0000 // I2S, Left justified
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x0A; // Control register 5 - 8'b0000_101_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x0C; // Control register 6 - 8'b0000_110_0/ 1: MCLK (PLL Output)  /0: MCLK (PLL Bypassed)
	cmd_init[1] = 0x48;    // 01001000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x0E; // Control register 7 - 8'b0000_111_0
	cmd_init[1] = 0x00; // 0000_0000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x10; // Control register 8 - 8'b0001_000_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x14; // Control register 10 - 8'b0001_010_0
	cmd_init[1] = 0x08; // 00001000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x17; // Control register 11 - 8'b0001_011_1
	cmd_init[1] = 0xFF; // 1111_1111
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x1D; // Control register 14 - 8'b0001_110_1
	cmd_init[1] = 0x08; //00001000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x1F; // Control register 15- 8'b0001_111_1
	cmd_init[1] = 0xFF; // 1111_1111
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x25; // Control register 18 - 8'b0010_010_1
	cmd_init[1] = 0x2C;// 00101100
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x26; // Control register 19- 8'b001_0011_0
	cmd_init[1] = 0x2C;// 00101100
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x28; // Control register 20- 8'b0010100_0
	cmd_init[1] = 0x2C;  // 00101100
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x2A; // Control register 21- 8'b0010101_0
	cmd_init[1] = 0x2C;  // 00101100
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x2C; // Control register 22- 8'b0010110_0
	cmd_init[1] = 0x2C;  // 00101100
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x30; // Control register 24- 8'b0011000_0
	cmd_init[1] = 0x32;// 00110010
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x32; // Control register 25- 8'b0011001_0
	cmd_init[1] = 0x00;//00000000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x36; // Control register 27- 8'b0011011_0
	cmd_init[1] = 0x00;//0000_0000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x38; // Control register 28- 8'b0011100_0
	cmd_init[1] = 0x00;// 0000_0000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x3A; // Control register 29- 8'b0011101_0
	cmd_init[1] = 0x00;//0000_0000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x3C; // Control register 30- 8'b0011110_0
	cmd_init[1] = 0x00; // 0000_0000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x40; // Control register 32- 8'b0100000_0
	cmd_init[1] = 0x38; // 00111000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x42; // Control register 33- 8'b0100001_0
	cmd_init[1] = 0x0B;// 00001011
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x44; // Control register 34- 8'b0100010_0
	cmd_init[1] = 0x32; //00110010
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x46; // Control register 35- 8'b0100011_0
	cmd_init[1] = 0x00; //00000000
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x48; // Control register 36- 8'b0100100_0
	cmd_init[1] = 0x08;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x4A; // Control register 37- 8'b0100101_0
	cmd_init[1] = 0x0C;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x4C; // Control register 38- 8'b0100110_0
	cmd_init[1] = 0x93;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x4E; // Control register 39- 8'b0100111_0
	cmd_init[1] = 0xE9;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x50; // Control register 40- 8'b0101000_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x58; // Control register 44- 8'b0101100_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x5A; // Control register 45- 8'b0101101_0
	cmd_init[1] = 0x50;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x5F; // Control register 47- 8'b0101111_1
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x62; // Control register 49- 8'b0110001_0
	cmd_init[1] = 0x02; //0000_0010
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x64; // Control register 50- 8'b0110010_0
	cmd_init[1] = 0x01;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x6C; // Control register 54- 8'b0110110_0
	cmd_init[1] = 0xB9;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x70; // Control register 56- 8'b0111000_0
	cmd_init[1] = 0x40;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x74; // Control register 58- 8'b0111010_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x76; // Control register 59- 8'b0111011_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x78; // Control register 60- 8'b0111100_0
	cmd_init[1] = 0x20; // 0010_0000;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x7C; // Control register 62- 8'b0111110_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x7E; // Control register 63- 8'b0111111_0
	cmd_init[1] = 0x1A;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x80; // Control register 64- 8'b1000000_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x82; // Control register 65- 8'b1000001_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x8A; // Control register 69- 8'b1000101_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x8C; // Control register 70- 8'b1000110_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x8E; // Control register 71- 8'b1000111_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x92; // Control register 73- 8'b1001001_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x96; // Control register 75- 8'b1001011_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x98; // Control register 76- 8'b1001100_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x9A; // Control register 77- 8'b1001101_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x9C; // Control register 78- 8'b1001110_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	cmd_init[0] = 0x9E; // Control register 79- 8'b1001111_0
	cmd_init[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd_init, 2, 1); //
	vTaskDelay(delay_codec);
	*/
	cmd_init[0] = 0x00 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x03 ; cmd_init[1] = 0x6d ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x04 ; cmd_init[1] = 0x15 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x06 ; cmd_init[1] = 0xfd ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x08 ; cmd_init[1] = 0x10 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x0a ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x0D ; cmd_init[1] = 0x48 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x0E ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x10 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x12 ; cmd_init[1] = 0x00 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x14 ; cmd_init[1] = 0x38 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
	cmd_init[0] = 0x17 ; cmd_init[1] = 0xff ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
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
	cmd_init[0] = 0x62 ; cmd_init[1] = 0x02 ; I2C_IF_Write(Codec_addr, cmd_init, 2, 1); vTaskDelay(delay_codec);
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


