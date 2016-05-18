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

#if 1
struct {
	uint16_t dig_T1;
	int16_t  dig_T2;
	int16_t  dig_T3;
	uint16_t dig_P1;
	int16_t  dig_P2;
	int16_t  dig_P3;
	int16_t  dig_P4;
	int16_t  dig_P5;
	int16_t  dig_P6;
	int16_t  dig_P7;
	int16_t  dig_P8;
	int16_t  dig_P9;
	uint8_t  dig_H1;
	int16_t  dig_H2;
	uint8_t  dig_H3;
	int16_t  dig_H4;
	int16_t  dig_H5;
	uint8_t  dig_H6;
} bme280_cal;

#define BME280_S32_t int32_t
#define BME280_U32_t uint32_t
#define BME280_S64_t int64_t
// Returns temperature in DegC, resolution is 0.01 DegC. Output value of “5123” equals 51.23 DegC. // t_fine carries fine temperature as global value
BME280_S32_t t_fine;
BME280_S32_t BME280_compensate_T_int32(BME280_S32_t adc_T) {
	BME280_S32_t var1, var2, T;
	var1 = ((((adc_T >> 3) -
			((BME280_S32_t)bme280_cal.dig_T1<<1))) *
			((BME280_S32_t)bme280_cal.dig_T2)) >> 11;
	var2 = (((((adc_T >> 4) -
			((BME280_S32_t)bme280_cal.dig_T1)) *
			((adc_T>>4) -
					((BME280_S32_t)bme280_cal.dig_T1))) >> 12) *
			((BME280_S32_t)bme280_cal.dig_T3)) >> 14;
	t_fine = var1 + var2;
	T = (t_fine * 5 + 128) >> 8;
	return T;
}
// Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits). // Output value of “24674867” represents 24674867/256 = 96386.2 Pa = 963.862 hPa
BME280_U32_t BME280_compensate_P_int64(BME280_S32_t adc_P) {
	BME280_S64_t var1, var2, p;
	var1 = ((BME280_S64_t) t_fine)- 128000;
	var2 = var1 * var1 * (BME280_S64_t) bme280_cal.dig_P6;
	var2 = var2 + ((var1 * (BME280_S64_t) bme280_cal.dig_P5) << 17);
	var2 = var2 + (((BME280_S64_t) bme280_cal.dig_P4) << 35);
	var1 = ((var1 * var1 * (BME280_S64_t) bme280_cal.dig_P3) >> 8)
			+ ((var1 * (BME280_S64_t) bme280_cal.dig_P2) << 12);
	var1 = (((((BME280_S64_t) 1) << 47) + var1))
			* ((BME280_S64_t) bme280_cal.dig_P1) >> 33;
	if (var1 == 0) {
		return 0; // avoid exception caused by division by zero
	}
	p = 1048576 - adc_P;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((BME280_S64_t) bme280_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((BME280_S64_t) bme280_cal.dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((BME280_S64_t) bme280_cal.dig_P7) << 4);
	return (BME280_U32_t) p;
}
// Returns humidity in %RH as unsigned 32 bit integer in Q22.10 format (22 integer and 10 fractional bits). // Output value of “47445” represents 47445/1024 = 46.333 %RH
// CAJ EDIT: returns 0.01 %RH
BME280_U32_t bme280_compensate_H_int32(BME280_S32_t adc_H) {
	BME280_S32_t v_x1_u32r;
	v_x1_u32r = (t_fine- ((BME280_S32_t)76800));
	v_x1_u32r = (((((adc_H << 14)- (((BME280_S32_t)bme280_cal.dig_H4) << 20) - (((BME280_S32_t)bme280_cal.dig_H5) * v_x1_u32r)) +
	((BME280_S32_t)16384)) >> 15) * (((((((v_x1_u32r * ((BME280_S32_t)bme280_cal.dig_H6)) >> 10) * (((v_x1_u32r *
											((BME280_S32_t)bme280_cal.dig_H3)) >> 11) + ((BME280_S32_t)32768))) >> 10) + ((BME280_S32_t)2097152)) *
	((BME280_S32_t)bme280_cal.dig_H2) + 8192) >> 14));
	v_x1_u32r = (v_x1_u32r- (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((BME280_S32_t)bme280_cal.dig_H1)) >> 4));
	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
	return (100*(BME280_U32_t)(v_x1_u32r >> 12))>>10;
}
#endif

int get_temp_press_hum(int32_t * temp, uint32_t * press, uint32_t * hum) {
	unsigned char cmd;
	int temp_raw, press_raw, hum_raw;

	unsigned char b[8] = {0};

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	//humid oversample
	b[0] = 0xf2; //must come before 0xf4
	b[1] = 0b00000101;
	(I2C_IF_Write(0x77, b, 2, 1));
	//temp/pressure oversample, force meas
	b[0] = 0xf4;
	b[1] = 0b10110101;
	(I2C_IF_Write(0x77, b, 2, 1));

	xSemaphoreGiveRecursive(i2c_smphr);
	vTaskDelay(95);
	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	b[0] = 1;
	b[2] = b[1] = 0;
	cmd = 0xf3;
	while (b[0] != 4) {
		(I2C_IF_Write(0x77, &cmd, 1, 1));
		(I2C_IF_Read(0x77, b, 1));

		DISP("%x %x %x\n", b[0],b[1],b[2] );

		xSemaphoreGiveRecursive(i2c_smphr);
		if( b[0] == 0 ) {
			return -1;
		}
		vTaskDelay(5);
		assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	}
	cmd = 0xf7;
	(I2C_IF_Write(0x77, &cmd, 1, 1));
	(I2C_IF_Read(0x77, b, 8));

	xSemaphoreGiveRecursive(i2c_smphr);

	press_raw = (b[0] << 16) | (b[1]<<8) | (b[2]);
	press_raw >>= 4;
    *press = BME280_compensate_P_int64(press_raw);
	DISP("%x %x %x %d %d\n", b[0],b[1],b[2], press_raw, *press);

	temp_raw = (b[3] << 16) | (b[4]<<8) | (b[5]);
	temp_raw >>= 4;
    *temp = BME280_compensate_T_int32(temp_raw);
	DISP("%x %x %x %d %d\n", b[0],b[1],b[2], temp_raw, *temp);

	hum_raw = (b[6]<<8) | (b[7]);
    *hum = bme280_compensate_H_int32(hum_raw);
    *hum += (2500-*temp)*-15/100;

	DISP("%x %x %d %d\n", b[6],b[7], hum_raw, *hum);

	return 0;
}

int init_temp_sensor()
{
	unsigned char id;
	unsigned char cmd = 0xd0;
	unsigned char b[25];

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	vTaskDelay(5);
	(I2C_IF_Write(0x77, &cmd, 1, 1));
	vTaskDelay(5);
	(I2C_IF_Read(0x77, &id, 1));

	cmd =0x88;
	(I2C_IF_Write(0x77, &cmd, 1, 1));
	(I2C_IF_Read(0x77, b, 25));

	bme280_cal.dig_T1 = b[0] | (b[1]<<8);
	bme280_cal.dig_T2 = b[2] | (b[3]<<8);
	bme280_cal.dig_T3 = b[4] | (b[5]<<8);
	bme280_cal.dig_P1 = b[6] | (b[7]<<8);
	bme280_cal.dig_P2 = b[8] | (b[9]<<8);
	bme280_cal.dig_P3 = b[10] | (b[11]<<8);
	bme280_cal.dig_P4 = b[12] | (b[13]<<8);
	bme280_cal.dig_P5 = b[14] | (b[15]<<8);
	bme280_cal.dig_P6 = b[16] | (b[17]<<8);
	bme280_cal.dig_P7 = b[18] | (b[19]<<8);
	bme280_cal.dig_P8 = b[20] | (b[21]<<8);
	bme280_cal.dig_P9 = b[22] | (b[23]<<8);
	bme280_cal.dig_H1 = b[24];

	cmd =0xE1;
	(I2C_IF_Write(0x77, &cmd, 1, 1));
	(I2C_IF_Read(0x77, b, 7));

	bme280_cal.dig_H2 = b[0] | (b[1]<<8);
	bme280_cal.dig_H3 = b[2];
	bme280_cal.dig_H4 = (b[3]<<4) | (b[4]&0xf);
	bme280_cal.dig_H5 = (b[5]<<4) | ((b[4]>>4)&0xf);
	bme280_cal.dig_H6 = b[7];

	xSemaphoreGiveRecursive(i2c_smphr);
	DISP("found %x\n", id);

	return id == 0x60;
}

int Cmd_read_temp_hum_press(int argc, char *argv[]) {
	int32_t temp;
	uint32_t hum,press;
	get_temp_press_hum(&temp, &press, &hum);
	LOGF("%d,%d,%d\n", temp, hum, press);
	return SUCCESS;
}

int Cmd_read_TVOC(int argc, char *argv[]) {
	int cmd;
	char b[2];

	cmd = 0x20;
	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	vTaskDelay(5);
	(I2C_IF_Write(0x5a, &cmd, 1, 1));
	vTaskDelay(5);
	(I2C_IF_Read(0x5a, b, 2));

	xSemaphoreGiveRecursive(i2c_smphr);
	LOGF("%x %x\n", b[0], b[1]);

	return SUCCESS;
}

