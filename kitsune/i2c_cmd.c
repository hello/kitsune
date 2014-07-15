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
#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %s\n\r", __FILE__, __LINE__ ); return FAILURE;}
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
	temp /= 65536;
	temp -= 47;

	return temp;
}

int Cmd_readtemp(int argc, char *argv[]) {
	UARTprintf("temp is %d\n\rç", get_temp());
	return SUCCESS;
}

int get_humid() {
#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %s\n\r", __FILE__, __LINE__ ); return FAILURE;}
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
	humid /= 65536;
	humid -= 6;

	return humid;
}

int Cmd_readhumid(int argc, char *argv[]) {

	UARTprintf("humid is %d\n\rç", get_humid());
	return SUCCESS;
}

int get_light() {
	#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %s\n\r", __FILE__, __LINE__ ); return FAILURE;}
	unsigned char aucDataBuf_LOW[2];
	unsigned char aucDataBuf_HIGH[2];
	unsigned char setup_config;
	unsigned char cmd_init[2];
	int light_raw;

	unsigned char cmd = 0x81; // Command register - 0x01

	cmd_init[0] = 0x80; // Command register - 8'b1000_0000
	cmd_init[1] = 0x0F; // Control register - 8'b0000_1111
//RET_IF_ERR(
	I2C_IF_Write(0x39, cmd_init, 2, 1); //  );// reset
//RET_IF_ERR( I2C_IF_Write(ucDevAddr,&aucDataBuf[0],ucWrLen+1,1));
//vTaskDelay(10);

	cmd_init[0] = 0x81; // Command register - 8'b1000_0000
	cmd_init[1] = 0x00; // Control register - 8'b0000_0000 // 400ms
//RET_IF_ERR(
	I2C_IF_Write(0x39, cmd_init, 2, 1); //  );// change integration
	vTaskDelay(50);

	I2C_IF_Write(0x39, &cmd, 1, 1); // );
	vTaskDelay(50);
	I2C_IF_Read(0x39, &setup_config, 1); // ); // configure

	cmd = 0x84; // Command register - 0x04
	I2C_IF_Write(0x39, &cmd, 1, 1); // );
//vTaskDelay(50);
	I2C_IF_Read(0x39, aucDataBuf_LOW, 2); // );

	cmd = 0x85; // Command register - 0x05
	I2C_IF_Write(0x39, &cmd, 1, 1); // );
//vTaskDelay(50);
	I2C_IF_Read(0x39, aucDataBuf_HIGH, 2); // );

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
