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

#include "hw_memmap.h"
#include "rom_map.h"
#include "wdt.h"

#include "stdbool.h"

#include "codec_debug_config.h"

#define MAX_MEASURE_TIME			10

#define FAILURE                 	-1
#define SUCCESS                 	0

#define RETERR_IF_TRUE(condition) 	{if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          	{int iRetVal = (Func); \
								  	  if (SUCCESS != iRetVal) \
									  	  return  iRetVal;}
#define BUF_SIZE 					2

#define Codec_addr 					(0x18U)

#define DELAY_CODEC 				5 // TODO set arbitrarily, might need to be adjusted

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
#define CODEC_USE_MINIDSP 			1 // Set to 1 if using miniDSP, else 0
#endif

#define CODEC_DIG_MIC1_EN			1

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
	#define CODEC_DIG_MIC2_EN 			1
#else
	#define CODEC_DIG_MIC2_EN 			0
#endif

#define CODEC_BEEP_GENERATOR		0

#if (CODEC_USE_MINIDSP == 1)
// If 0-> PPS code is loaded from flash, 1-> PPS code is laoded from array
#define CODEC_PPS_FROM_ARRAY 1
#endif

#if (CODEC_USE_MINIDSP == 1)
#define codec_init_with_dsp codec_init
#else
#define codec_init_no_dsp codec_init
#endif

extern xSemaphoreHandle i2c_smphr;

#include "stdbool.h"

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


static int get_temp_old() {
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
static int get_humid_old() {
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
int Cmd_read_temp_humid_old(int argc, char *argv[]) {
	unsigned char cmd = 0xfe;
	int temp,humid;
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));
	(I2C_IF_Write(0x40, &cmd, 1, 1));    // reset
	get_temp_old();
	get_humid_old();
	xSemaphoreGiveRecursive(i2c_smphr);
	vTaskDelay(500);

	temp = get_temp_old();
	humid = get_humid_old();
	humid += (2500-temp)*-15/100;

	LOGF("%d,%d\n", temp, humid);
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
#define DBG_BME(...)
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

		DBG_BME("%x %x %x\n", b[0],b[1],b[2] );

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
    DBG_BME("%x %x %x %d %d\n", b[0],b[1],b[2], press_raw, *press);

	temp_raw = (b[3] << 16) | (b[4]<<8) | (b[5]);
	temp_raw >>= 4;
    *temp = BME280_compensate_T_int32(temp_raw);
    DBG_BME("%x %x %x %d %d\n", b[0],b[1],b[2], temp_raw, *temp);

	hum_raw = (b[6]<<8) | (b[7]);
    *hum = bme280_compensate_H_int32(hum_raw);

    DBG_BME("%x %x %d %d\n", b[6],b[7], hum_raw, *hum);

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
	DBG_BME("found %x\n", id);

	return id == 0x60;
}

int Cmd_read_temp_hum_press(int argc, char *argv[]) {
	int32_t temp;
	uint32_t hum,press;
	get_temp_press_hum(&temp, &press, &hum);
	LOGF("%d,%d,%d\n", temp, hum, press);
	return SUCCESS;
}

int init_tvoc() {
	unsigned char b[2];
	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	b[0] = 0;
	(I2C_IF_Write(0x5a, b, 1, 1));
	(I2C_IF_Read(0x5a, b, 1));

	if( !(b[0] & 0x10) ) {
		LOGE("no valid fw for TVOC\n");
		xSemaphoreGiveRecursive(i2c_smphr);
		return -1;
	}
	//boot
	b[0] = 0xf4;
	(I2C_IF_Write(0x5a, b, 1, 1));
	vTaskDelay(100);
	b[0] = 0;
	(I2C_IF_Write(0x5a, b, 1, 1));
	(I2C_IF_Read(0x5a, b, 1));
	if( !(b[0] & 0x90) ) {
		LOGE("fail to boot TVOC\n");
		xSemaphoreGiveRecursive(i2c_smphr);
		return -1;
	}
	b[0] = 1;
	b[1] = 0x20; //60sec
	(I2C_IF_Write(0x5a, b, 2, 1));

	xSemaphoreGiveRecursive(i2c_smphr);

	return 0;
}
#define DBG_TVOC LOGI
int get_tvoc(int * tvoc, int * eco2, int * current, int * voltage, int temp, unsigned int humid ) {
	unsigned char b[8];
	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	vTaskDelay(10);
	//environmental
	b[0] = 0x05;
	temp = (temp + 2500)/50;
	humid /= 50;
	b[1] = humid>>8;
	b[2] = (humid&0xff);;
	b[3] = temp>>8;
	b[4] = (temp&0xff);
	(I2C_IF_Write(0x5a, b, 5, 1));

	b[0] = 2;
	(I2C_IF_Write(0x5a, b, 1, 1));
	(I2C_IF_Read(0x5a, b, 8));

	DBG_TVOC("%x:%x:%x:%x:%x:%x:%x:%x\n",
			b[0],b[1],b[2],b[3],b[4],b[5],
		    b[6],b[7]);

	if( !(b[5] & 0x90) ) {
		LOGE("TVOC error %x ", b[5] );
		b[0] = 0xe0;
		(I2C_IF_Write(0x5a, b, 1, 1));
		(I2C_IF_Read(0x5a, b, 1));
		LOGE("%x\n", b[0]);
		xSemaphoreGiveRecursive(i2c_smphr);
		return -1;
	}

	*eco2 = (b[1] | (b[0]<<8));
	*tvoc = (b[3] | (b[2]<<8));
	*current = (b[6]>>2);
	*voltage = (((b[6]&3)<<8) | (b[7]));

	vTaskDelay(10);
	xSemaphoreGiveRecursive(i2c_smphr);
	return 0;
}

int Cmd_meas_TVOC(int argc, char *argv[]) {
	int32_t temp;
	uint32_t hum,press;
	int tvoc, eco2, current, voltage;
	if( 0 == get_temp_press_hum(&temp, &press, &hum) &&
	    0 == get_tvoc( &tvoc, &eco2, &current, &voltage, temp, hum) ) {
		LOGF("voc %d eco2 %d %duA %dmv %d %d\n", tvoc, eco2, current, voltage, temp, hum);
		return 0;
	}
	return -1;
}
static bool haz_tmg4903() {
	unsigned char b[2]={0};
	b[0] = 0x92;
	(I2C_IF_Write(0x39, b, 1, 1));
	(I2C_IF_Read(0x39, b, 1));

	if( b[0] != 0xb8 ) {
		LOGE("can't find TMG4903\n");
		xSemaphoreGiveRecursive(i2c_smphr);
		return false;
	}
	return true;
}

int init_light_sensor()
{
	unsigned char b[5];

	if( !haz_tmg4903() ) {
		LOGE("can't find TMG4903\n");
		xSemaphoreGiveRecursive(i2c_smphr);
		return FAILURE;
	}
	b[0] = 0x80;
	b[1] = 0b1000111; //enable gesture/prox/als/power
	b[2] = 249; //20ms integration
	b[3] = 35; //100ms prox time
	b[4] = 249; //20ms als time
	(I2C_IF_Write(0x39, b, 5, 1));
	b[0] = 0x8d;
	b[1] = 0x0;
	(I2C_IF_Write(0x39, b, 2, 1));
	b[0] = 0x90;
	b[1] = 0x3;
	(I2C_IF_Write(0x39, b, 2, 1));

	vTaskDelay(50);
	return SUCCESS;
}


static int get_le_short( uint8_t * b ) {
	return (b[0] | (b[1]<<8));
}
#define DBG_TMG(...)
int get_rgb_prox( int * w, int * r, int * g, int * bl, int * p ) {
	unsigned char b[10];
	int i;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));
	/*Red, green, blue, and clear data are stored as 16-bit values.
	The read sequence must read byte pairs (low followed by high)
	 starting on an even address boundary (0x94, 0x96, 0x98, or 0x9A)
	  inside the CRGB Data Register block. In addition, reading the
	  Clear channel data low byte (0x94) latches all 8 data bytes.
	   Reading these 8 bytes consecutively (0x94 - 0x9A) ensures that
	    the data is concurrent.
	*/
start_rgb:
	if( !haz_tmg4903() ) {
		LOGE("can't find TMG4903\n");
		xSemaphoreGiveRecursive(i2c_smphr);
		return FAILURE;
	}

	b[0] = 0x94;
	(I2C_IF_Write(0x39, b, 1, 1));
	(I2C_IF_Read(0x39, b, 10));
	for(i=0;i<10;++i) {
		DBG_TMG("%x,",b[i]);
	}DBG_TMG("\n");

	for( i=0;i<10;++i) {
		if( b[i] != 0 ) {
			break;
		}
	}
	if( i == 10  ) {
		init_light_sensor();
		LOGE("Fail to read TMG\n");
		return 0;
	}

	*w = get_le_short(b);
	*r = get_le_short(b+2);
	*g = get_le_short(b+4);
	*bl = get_le_short(b+6);
	*p = get_le_short(b+8);

	xSemaphoreGiveRecursive(i2c_smphr);
	return SUCCESS;
}

int Cmd_readlight(int argc, char *argv[]) {
	int r,g,b,w,p;
	if( SUCCESS == get_rgb_prox( &w, &r, &g, &b, &p ) ) {
		LOGF("%d,%d,%d,%d,%d\n", w,r,g,b,p );
	}
	return SUCCESS;
}

int get_ir( int * ir ) {
	unsigned char b[2];
	int w,r,g,bl,p;
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	b[0] = 0xAB;
	b[1] = 0x40;
	(I2C_IF_Write(0x39, b, 2, 1));
	vTaskDelay(110);
	get_rgb_prox( &w, &r, &g, &bl, &p );
	*ir = w+r+g+bl;

	b[0] = 0xAB;
	b[1] = 0x00;
	(I2C_IF_Write(0x39, b, 2, 1));
	vTaskDelay(110);

	xSemaphoreGiveRecursive(i2c_smphr);

	return 0;
}
int init_uv(bool als) {
	unsigned char b[2];
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

#if 0 // checking the part id seems to make the mode setting fail
	//check the part id
	b[0] = 0x6;
	(I2C_IF_Write(0x53, b, 1, 1));
	(I2C_IF_Read(0x53, b, 1));
	if( b[1] != 0xb2 ) {
		xSemaphoreGiveRecursive(i2c_smphr);
		return -1;
	}
#endif
	//set mode
	b[0] = 0;
	if( als ) {
		b[1] = 2;
	} else {
		b[1] = 0xa;
	}
	(I2C_IF_Write(0x53, b, 2, 1));

	//set gain to 18
	b[0] = 0x5;
	b[1] = 0b100;
	(I2C_IF_Write(0x53, b, 2, 1));

	//set rate to 2 hz
	b[0] = 0x4;
	b[1] = 0b110100;
	(I2C_IF_Write(0x53, b, 2, 1));

	xSemaphoreGiveRecursive(i2c_smphr);

	return 0;
}

int Cmd_read_uv(int argc, char *argv[]) {
	static int use_als;

	if( use_als != (atoi(argv[1]) == 1) ) {
		use_als  = (atoi(argv[1]) == 1);
		if( init_uv( use_als ) ) {
			LOGF("UV FAIL\n");
			return -1;
		}
	}

	int32_t v = 0;
	unsigned char b[2];
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	b[0] = use_als ? 0xd : 0x10;
	(I2C_IF_Write(0x53, b, 1, 1));
	(I2C_IF_Read(0x53, (uint8_t*)&v, 3));
	xSemaphoreGiveRecursive(i2c_smphr);

	LOGF("%d\n", v);

	return 0;
}

int Cmd_uvr(int argc, char *argv[]) {
	int i;
	int addr = strtol(argv[1], NULL, 16);
	int len = strtol(argv[2], NULL, 16);
	unsigned char b[2];
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	b[0] = addr;
	(I2C_IF_Write(0x53, b, 1, 1));
	(I2C_IF_Read(0x53, b, len));

	for(i=0;i<len;++i) {
		LOGF("%x:", b[i] );
	}
	LOGF("\n");
	xSemaphoreGiveRecursive(i2c_smphr);
	return SUCCESS;
}
int Cmd_uvw(int argc, char *argv[]) {
	int addr = strtol(argv[1], NULL, 16);
	int data = strtol(argv[2], NULL, 16);

	unsigned char b[2];
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));
	b[0] = addr;
	b[1] = data;
	(I2C_IF_Write(0x53, b, 2, 1));
	xSemaphoreGiveRecursive(i2c_smphr);

	return SUCCESS;
}

#include "simplelink.h"
#include "sl_sync_include_after_simplelink_header.h"


#if (CODEC_USE_MINIDSP == 0)
static void codec_fifo_config(void);
static void codec_power_config(void);
static void codec_clock_config(void);
static void codec_gpio_config(void);
static void codec_asi_config(void);
static void codec_signal_processing_config(void);
static void codec_mic_config(void);
static void codec_speaker_config(void);
#endif

static void codec_sw_reset(void);
static void codec_set_page(uint32_t page);
static void codec_set_book(uint32_t book);

#if (CODEC_BEEP_GENERATOR==1)
static void beep_gen(void);
#endif

/*
 * This function updates the gain on the LOL output to the speaker driver
 * On the codec, this gain has 117 levels between 0db to -78.3db
 * The input v to the function varies from 0-64.
 */
bool set_volume(int v, unsigned int dly) {

	char send_stop = 1;
	unsigned char cmd[2];

	if(v < 0) v = 0;
	if(v >64) v = 64;

	v = 64-v;
	v << 10;
	v /= 560;

	if( xSemaphoreTakeRecursive(i2c_smphr, dly)) {

		//	w 30 00 00 # Select Page 0
		codec_set_page(1);

		codec_set_book(0);

		cmd[0] = 46;
		cmd[0] = v;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
		xSemaphoreGiveRecursive(i2c_smphr);
		return true;
	} else {
		return false;
	}

}

static void codec_sw_reset(void)
{
	char send_stop = 1;
	unsigned char cmd[2];
	const TickType_t delay = 10 / portTICK_PERIOD_MS;
	int ret;

	// w 30 00 00 # Initialize to Page 0
	codec_set_page(0);

	//w 30 7f 00 # Initialize to Book 0
	codec_set_book(0);

	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {

		//w 30 01 01 # Software Reset
		cmd[0] = 0x01;
		cmd[1] = 0x01;
		if((ret=I2C_IF_Write(Codec_addr, cmd, 2, send_stop)))
		{
			UARTprintf("Codec sw reset fail:%d\n",ret);
		}
		xSemaphoreGiveRecursive(i2c_smphr);
	}
	vTaskDelay(delay);
}

static void codec_set_page(uint32_t page)
{
	char send_stop = 1;
	unsigned char cmd[2];

	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {

		//	w 30 00 00 # Select Page 0
		cmd[0] = 0;
		cmd[1] = page;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

		xSemaphoreGiveRecursive(i2c_smphr);
	}
}

static void codec_set_book(uint32_t book)
{
	char send_stop = 1;
	unsigned char cmd[2];

	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {

		//	# Select Book 0
		cmd[0] = 0x7F;
		cmd[1] = book;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

		xSemaphoreGiveRecursive(i2c_smphr);
	}
}

// FOR board bring up
#ifdef CODEC_1P5_TEST
int32_t codec_test_commands(void)
{
	unsigned char cmd[2] = {0};
	char send_stop = 1;

	// Send Software reset
	codec_sw_reset();

	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {

		// Read register in [0][0][00]
		cmd[0] = 0;
		cmd[1] = 0;

		I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
		I2C_IF_Read(Codec_addr, &cmd[1], 1);

		UARTprintf("Codec Test read 1 %s: [0][0][%u]: %X \n", (cmd[1]==0)?"Pass":"Fail", cmd[0], cmd[1]);

		// Read register in [0][0][06]
		cmd[0] = 0x06;
		cmd[1] = 0;

		I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
		I2C_IF_Read(Codec_addr, &cmd[1], 1);

		UARTprintf("Codec Test read 2 %s: [0][0][%u]: %X  \r\n", (cmd[1]==0x11)?"Pass":"Fail", cmd[0], cmd[1]);

		// Read register in [0][0][0A]
		cmd[0] = 0x0A;
		cmd[1] = 0;

		I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
		I2C_IF_Read(Codec_addr, &cmd[1], 1);

		UARTprintf("Codec Test read 3 %s: [0][0][%u]: %X  \r\n", (cmd[1]==0x01)?"Pass":"Fail", cmd[0], cmd[1]);


		xSemaphoreGiveRecursive(i2c_smphr);
	}

	return 0;

}
#endif

typedef enum{
	SPK_VOLUME_MUTE = 0,
	SPK_VOLUME_6dB = 1,
	SPK_VOLUME_12dB = 2,
	SPK_VOLUME_18dB = 3,
	SPK_VOLUME_24dB = 4,
	SPK_VOLUME_30dB = 5,
}spk_volume_t;

void codec_mute_spkr(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 00 # Select Page 0
	codec_set_page(1);

	codec_set_book(0);

	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {
		cmd[0] = 48;
		cmd[1] = 0x00;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

		xSemaphoreGiveRecursive(i2c_smphr);
	}

}

void codec_unmute_spkr(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	set_volume(0, portMAX_DELAY);
	//	w 30 00 00 # Select Page 0
	codec_set_page(1);

	codec_set_book(0);

	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {
		cmd[0] = 48;
		cmd[1] = (SPK_VOLUME_12dB << 4) | (1 << 0);
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

		xSemaphoreGiveRecursive(i2c_smphr);
	}

}

#if (CODEC_USE_MINIDSP == 1)

#if (CODEC_PPS_FROM_ARRAY == 1)
#include "audio_codec_pps_driver.h"
int32_t codec_init_with_dsp(void)
{
	uint32_t i;
	char send_stop = 1;
	unsigned char cmd[2];

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	/********************************************************************************
	 * Update clock config
	 ********************************************************************************
	 */
	uint32_t reg_array_size = sizeof(REG_Section_program)/2;

	// Write the registers
	for(i=0;i<reg_array_size;i++)
	{
		//	# Select Book 0
		cmd[0] = REG_Section_program[i].reg_off;
		cmd[1] = REG_Section_program[i].reg_val;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	}

	xSemaphoreGiveRecursive(i2c_smphr);

	vTaskDelay(20);

	/********************************************************************************
	 * Update miniDSP A instructions and coefficients
	 ********************************************************************************
	 */
	reg_array_size = sizeof(miniDSP_A_reg_values)/2;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	// Write the registers
	for(i=0;i<reg_array_size;i++)
	{
		//	# Select Book 0
		cmd[0] = miniDSP_A_reg_values[i].reg_off;
		cmd[1] = miniDSP_A_reg_values[i].reg_val;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	}
	xSemaphoreGiveRecursive(i2c_smphr);

	vTaskDelay(20);

	/********************************************************************************
	 * Update miniDSP D instructions and coefficients
	 ********************************************************************************
	 */
	reg_array_size = sizeof(miniDSP_D_reg_values)/2;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	// Write the registers
	for(i=0;i<reg_array_size;i++)
	{
		//	# Select Book 0
		cmd[0] = miniDSP_D_reg_values[i].reg_off;
		cmd[1] = miniDSP_D_reg_values[i].reg_val;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	}
	xSemaphoreGiveRecursive(i2c_smphr);

	vTaskDelay(20);

	/********************************************************************************
	 * Power Up ADC, DAC and other modules
	 * IMPORTANT: REG_Section_program2 has to be written after minidsp coefficients
	 * and instructions have been written in order for playback to work.
	 ********************************************************************************
	 */
	reg_array_size = sizeof(REG_Section_program2)/2;

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	// Write the registers
	for(i=0;i<reg_array_size;i++)
	{
		//	# Select Book 0
		cmd[0] = REG_Section_program2[i].reg_off;
		cmd[1] = REG_Section_program2[i].reg_val;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	}
	xSemaphoreGiveRecursive(i2c_smphr);

	vTaskDelay(100);

#if 0

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

	codec_set_book(0);

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

	// Read register in [0][0][36]
	cmd[0] = 0x24;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("ADC Flag read [0][0][%u]: %X \n",cmd[0], cmd[1]);

	// Read register in [0][0][37]
	cmd[0] = 0x25;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("DAC Flag read [0][0][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][0][38]
	cmd[0] = 0x26;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("DAC Flag read [0][0][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][0][38]
	cmd[0] = 0x3C;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("[0][0][%u]: %X  \r\n", cmd[0], cmd[1]);

	cmd[0] = 0x3D;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("[0][0][%u]: %X  \r\n", cmd[0], cmd[1]);

	codec_set_book(0);

	//	w 30 00 00 # Select Page 1
	codec_set_page(1);

	// Read register in [0][1][51]
	cmd[0] = 51;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("Mic Bias Control [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][64]
	cmd[0] = 64;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("DAC Analog Gain [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][66]
	cmd[0] = 66;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("Driver power up [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][52]
	cmd[0] = 31;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][54]
	cmd[0] = 32;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][55]
	cmd[0] = 33;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][57]
	cmd[0] = 34;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][57]
	cmd[0] = 47;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	xSemaphoreGiveRecursive(i2c_smphr);

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

#endif
	return 1;

}

#else

#define CODEC_DRIVER_FILE "/sys/codec_driver"
#define FILE_READ_BLOCK 128
int32_t codec_init_with_dsp(void)
{
    _i32 hndl;
    unsigned long ulToken = 0;
    uint32_t codec_file_header = 0;
    uint32_t codec_file_version=0;
    uint8_t* codec_program = NULL;
    uint32_t offset;
    uint32_t i;
    uint32_t bytes_read = 0;

    codec_program = pvPortMalloc(FILE_READ_BLOCK);
    assert(codec_program);

	// Read codec file from flash
	if (sl_FsOpen((unsigned char *)CODEC_DRIVER_FILE, FS_MODE_OPEN_READ, &ulToken, &hndl))
	{
		LOGI("Codec: error opening file\n");
		return -1;
	}

	offset=0;
	// Check if header is valid
	if(sl_FsRead(hndl, offset, (_u8 *)codec_file_header, sizeof(codec_file_header)) <= 0)
	{
		sl_FsClose(hndl, 0, 0, 0);
		return -1;
	}
	if(codec_file_header != 0xA508A509)
	{
		LOGE("Codec: Driver header incorrect %d\n", codec_file_header);
		sl_FsClose(hndl, 0, 0, 0);
		return -1;
	}

	offset +=4;
	if(sl_FsRead(hndl, offset, (_u8 *)codec_file_version, sizeof(codec_file_version)) <= 0)
	{
		sl_FsClose(hndl, 0, 0, 0);
		return -1;
	}

	// Display codec version
	LOGI("Codec: Driver version %d\n", codec_file_version);

	// Read data into buffer and program codec

	/*
	 * NOTE: block I2c write is not possible since the registers are not contiguous in the codec code.
	 * Two bytes need to be taken at a time and then written
	 */


	offset += 4;

	while((bytes_read=sl_FsRead(hndl, offset, (_u8 *)codec_program, sizeof(FILE_READ_BLOCK))) > 0)
	{
		// The number of bytes read must always be even. If it is not, we have a problem!
		if(bytes_read%2)
		{
			sl_FsClose(hndl, 0, 0, 0);
			return -1;
		}

		for(i=0;i<bytes_read/2;i++)
		{
			uint8_t cmd[2];
			uint8_t send_stop = 1;

			uint32_t index = i*2;

			cmd[0] = codec_program[index];
			cmd[1] = codec_program[index+1];
			I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

			vTaskDelay(5);

#ifdef CODEC_1P5_TEST

			uint8_t data;
			I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
			I2C_IF_Read(Codec_addr, &data, 1);

			if(data != cmd[1]){
				UARTprintf("Codec read back fail for reg %d: %d vs %d \r\n",cmd[0], cmd[1], data);
			}

			vTaskDelay(5);
#endif
		}
		offset += bytes_read;

		MAP_WatchdogIntClear(WDT_BASE); //clear wdt

	}

	LOGI("Codec: Bytes read: %d\n", offset);

	// Close file
	sl_FsClose(hndl, 0, 0, 0);

	return 0;

}
#endif

#else

int32_t codec_init_no_dsp(void)
{
	// Softwarre Reset
	codec_sw_reset();

	// FIFO Configuration
	codec_fifo_config();

	// Power and Analog Configuration
	codec_power_config();

	// Clock configuration
	codec_clock_config();

	// Config GPIO pins
	codec_gpio_config();

	// Audio Serial Interface Configuration (ASI1 with multi-channel on single pin I2S setup)
	codec_asi_config();



	/*
	 * 		Setting the coeffients for the signal processing blocks must be done
	 *     	before the ADC and DAC are powered up. (non-adaptive mode. The ADC
	 *     	is powered up in codec_mic_config()
	 *
	 */
	// Signal Processing Settings (Select signal processing blocks for record and playback)
	codec_signal_processing_config();

	// Configure GPIO for digital mic input (TODO maybe power off analog section in input)
	// ADC Input Channel Configuration
	codec_mic_config();

	// Output Channel Configuration
	codec_speaker_config();


#ifdef CODEC_1P5_TEST
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

	// Read register in [0][0][36]
	cmd[0] = 0x24;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("ADC Flag read [0][0][%u]: %X \n",cmd[0], cmd[1]);

	// Read register in [0][0][37]
	cmd[0] = 0x25;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("DAC Flag read [0][0][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][0][38]
	cmd[0] = 0x26;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("DAC Flag read [0][0][%u]: %X  \r\n", cmd[0], cmd[1]);

	//	w 30 00 00 # Select Page 1
	codec_set_page(1);

	// Read register in [0][1][66]
	cmd[0] = 66;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf("Driver power up [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][52]
	cmd[0] = 31;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][54]
	cmd[0] = 32;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][55]
	cmd[0] = 33;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][57]
	cmd[0] = 34;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	// Read register in [0][1][57]
	cmd[0] = 35;
	cmd[1] = 0;

	I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
	I2C_IF_Read(Codec_addr, &cmd[1], 1);

	UARTprintf(" [0][1][%u]: %X  \r\n", cmd[0], cmd[1]);

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

#endif

#if (CODEC_BEEP_GENERATOR == 1)
	beep_gen();
#endif

	return 0;
}


// Codec FIFO config
static void codec_fifo_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

	//	w 30 7f 64 # Select Book 100
	codec_set_book(0x64);

#if (CODEC_DIG_MIC1_EN == 1)
	//	w 30 32 80 # Enable ADC (CIC output) FIFO
	// TODO is auto clear necessary?, currently disbaled
	cmd[0] = 0x32;
	cmd[1] = ( 1<<7 ) | ( 0 << 6 ) | (0 << 5) | (4 << 0); // EnABLE CIC, Auto normal, decimation ratio=4
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#endif

#if (CODEC_DIG_MIC2_EN==1)
	// CIC2 FIFO not bypassed
	cmd[0] = 0x3C;
	cmd[1] = ( 1<<7 ) | ( 0 << 6 );
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#endif

	//	w 30 7f 00 # Select Book 0
	codec_set_book(0);

}

//Codec power and analog config
static void codec_power_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 01 # Select Page 1
	codec_set_page(1);

	//	w 30 01 00 # Disable weak AVDD to DVDD connection // TODO verify with hardware
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);


	//	w 30 7a 01 # REF charging time = 40ms
	cmd[0] = 0x7A;//122
	cmd[1] = 0x01; // Recommended settings as per datasheet
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 79 33 # Quick charge of analog inputs caps (1.1ms Left and 0.5ms right),
	// as per MIC example, may not be needed for digital mic? TODO // This is the reset value
	cmd[0] = 0x79;//121
	cmd[1] = 0x33;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// Update PowerTune
	cmd[0] = 61;
	cmd[1] = (1 << 6);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// Update Mic Bias
	cmd[0] = 51;
	cmd[1] = (4 << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 00 01 # Select Page 0
	codec_set_page(0);
}

// Codec Clock config
static void codec_clock_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	// ADC CLOCK - 16kHz
	// DAC CLOCK - 48kHz
	// Codec Control Clock calculator used to obtain settings

	/*
	* Clock configuration
	* -----------------------------------------------------
	* - MCLK = 12MHz ( All codec PLL calculations are based off this clock)
	* - FS = 48kHz
	* - I2S Slave
	*/
	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

#if (CODEC_ADC_16KHZ == 1)
	// *********************************************
	// ********* ADC Fs = 16kHz != DAC Fs = 48k Hz *********
	// *********************************************

#define PLL_P 1
#define PLL_R 1
#define PLL_J 6
#define PLL_D 9120UL
#define NDAC 3
#define MDAC 9
#define NADC 3
#define MADC 27
#define DOSR 64UL
#define AOSR 64UL

	//	w 30 04 00 # Set ADC_CLKIN = PLL_CLK and DAC_CLK = PLL_CLK
	cmd[0] = 0x04;
	cmd[1] = (3 << 4) | (3 << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// --------- PLL -------------
	cmd[0] = 0x05;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x06;
	cmd[1] = (1 << 7) | (PLL_P << 4) | (PLL_R << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x07;
	cmd[1] = PLL_J;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x08; // MSB
	cmd[1] = (PLL_D & 0xFF00) >> 8;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x09; // LSB
	cmd[1] = (PLL_D & 0xFF) >> 0;;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// --------- DAC -------------

	//	# NDAC = 3
	cmd[0] = 0x0B; // 11
	cmd[1] = (1 << 7) | (NDAC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	# MDAC = 9
	cmd[0] = 0x0C; // 12
	cmd[1] = (1 << 7) | (MDAC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// DOSR = 128
	//	w 30 0d 00 # DOSR (MSB)
	cmd[0] = 0x0D; // 13
	cmd[1] = (DOSR & 0x0300) >> 8;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 0e 80 # DOSR (LSB)
	cmd[0] = 0x0E; // 14
	cmd[1] = (DOSR & 0x00FF) >> 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// --------- ADC -------------

	// NADC and MADC same as DAC

	//	w 30 12 81 # NADC = 3
	cmd[0] = 0x12;  // 18
	cmd[1] = (1 << 7) | (NADC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 13 84 # MADC = 15
	cmd[0] = 0x13; // 19
	cmd[1] = (1 << 7) | (MADC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 14 40 # AOSR = 128
	// TODO As per datasheet, if AOSR=128, Use with PRB_R1 to PRB_R6, ADC Filter Type A
	cmd[0] = 0x14;  //20
	cmd[1] = AOSR;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

#else
	// *********************************************
	// ********* ADC Fs = DAC Fs = 48k Hz *********
	// *********************************************

#define PLL_P 1
#define PLL_R 1
#define PLL_J 6
#define PLL_D 9120UL
#define NDAC 3
#define MDAC 9
#define NADC 3
#define MADC 9
#define DOSR 64UL
#define AOSR 64UL

	//	w 30 04 00 # Set ADC_CLKIN = PLL_CLK and DAC_CLK = PLL_CLK
	cmd[0] = 0x04;
	cmd[1] = (3 << 4) | (3 << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// --------- PLL -------------
	cmd[0] = 0x05;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x06;
	cmd[1] = (1 << 7) | (PLL_P << 4) | (PLL_R << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x07;
	cmd[1] = PLL_J;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x08; // MSB
	cmd[1] = (PLL_D & 0xFF00) >> 8;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x09; // LSB
	cmd[1] = (PLL_D & 0xFF) >> 0;;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// --------- DAC -------------

	//	# NDAC = 3
	cmd[0] = 0x0B; // 11
	cmd[1] = (1 << 7) | (NDAC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	# MDAC = 9
	cmd[0] = 0x0C; // 12
	cmd[1] = (1 << 7) | (MDAC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// DOSR = 128
	//	w 30 0d 00 # DOSR (MSB)
	cmd[0] = 0x0D; // 13
	cmd[1] = (DOSR & 0x0300) >> 8;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 0e 80 # DOSR (LSB)
	cmd[0] = 0x0E; // 14
	cmd[1] = (DOSR & 0x00FF) >> 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// --------- ADC -------------

	// NADC and MADC same as DAC

	//	w 30 12 81 # NADC = 3
	cmd[0] = 0x12;  // 18
	cmd[1] = (1 << 7) | (NADC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 13 84 # MADC = 15
	cmd[0] = 0x13; // 19
	cmd[1] = (1 << 7) | (MADC << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 14 40 # AOSR = 128
	// TODO As per datasheet, if AOSR=128, Use with PRB_R1 to PRB_R6, ADC Filter Type A
	cmd[0] = 0x14;  //20
	cmd[1] = AOSR;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

#endif


}

// Codec audio serial interface settings
static void codec_asi_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 04 # Select Page 4
	codec_set_page(4);

	//	w 30 01 00 # ASI1 set to I2S mode, 16-bit
	cmd[0] = 0x01;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

#if (CODEC_ENABLE_MULTI_CHANNEL == 1)
	// w 30 04 40 # Enable 4 channel for ASI1 bus
	cmd[0] = 0x04;
	cmd[1] = (1 << 6); // 2-pair of left and right channel (i.e. 4-channel) is enabled for the ASI1 bus
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#else
	// w 30 04 40 # Enable 2 channel for ASI1 bus
	cmd[0] = 0x04;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#endif

	//	w 30 0a 00 # ASI1 WCLK/BCLK to WCLK1 pin/BCLK1 pin
	cmd[0] = 0x0A;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	ASI1 Left DAC Datapath = Left Data, ASI1 Right DAC Datapath = Right Data
	cmd[0] = 8;
	cmd[1] = 0x50;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x10;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

#if 0
	// 0x05 - ASI2 digital audio output data is sourced from ADC miniDSP Data Output 2
	cmd[0] = 23;
	cmd[1] = 0x05;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// Enable ASI2 left and right datapath
	cmd[0] = 24;
	cmd[1] = 0x50;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//0x06: ASI3 digital audio output data is sourced from ADC miniDSP Data Output 3
	cmd[0] = 39;
	cmd[1] = 0x06;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// Enable ASI3 left and right datapath
	cmd[0] = 40;
	cmd[1] = 0x50;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#endif

	cmd[0] = 0x47;
	cmd[1] = ( 1 << 5 ) ;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x48;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x49;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x4A;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x4B;
	cmd[1] = ( 1 << 5 ) ;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x4C;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x77;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

}

static void codec_gpio_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 00 # Select Page 4
	codec_set_page(4);

	// # GPIO1 as clock input (will be used as ADC WCLK if DAC Fs != ADC Fs)
	cmd[0] = 0x56;//86
	cmd[1] = (0x01 << 2);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # GPIO2 as clock output (ADC_MOD_CLK for digital mic)
	cmd[0] = 0x57;//87
	cmd[1] = (0x0A << 2);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # GPIO3 as clock input (will be used as ADC BCLK)
	cmd[0] = 0x58;//88
	cmd[1] = (0x01 << 2);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # GPIO4 as digital mic input, MIC1_DAT, sampled on rising edge, hence left by default (DIG MIC PAIR 1)
	cmd[0] = 0x59;//89
	cmd[1] = (0x01 << 2);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # GPIO5 as digital mic input, MIC2_DAT, sampled on falling edge, hence right by default (DIG MIC PAIR 1)
	cmd[0] = 0x5A;//90
	cmd[1] = (0x01 << 2);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # GPIO6 as digital mic input,  MIC3_DAT, sampled on rising edge, hence left by default (DIG MIC PAIR 2)
	cmd[0] = 0x5B;//91
	cmd[1] = (0x01 << 2);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # GPIO1 pin output disabled
	cmd[0] = 0x60;//96
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

}

// signal processing settings
// TODO Change later
static void codec_signal_processing_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	// Filter coefficients
	const uint64_t n0_l = 0x7C73E5;
	const uint64_t n1_l = 0x838C1B;
	const uint64_t d1_l = 0x78E7CC;

	const uint64_t n0_r = 0x7C73E5;
	const uint64_t n1_r = 0x838C1B;
	const uint64_t d1_r = 0x78E7CC;

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

	// Decimation filter must be A
	//	w 30 3d 0a # Set the ADC Mode to PRB_R1
	cmd[0] = 0x3D;
	cmd[1] = 0x01; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// w 30 3c 01 # Set the DAC PRB Mode to PRB_P1
	cmd[0] = 0x3C;
#if (CODEC_BEEP_GENERATOR==1)
	cmd[1] = 0x1B;
#else
	cmd[1] = 0x01;
#endif
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 00 00 # Select Page 1
	codec_set_page(1);

	//	# Select Book 120
	codec_set_book(40);

	// Filter coefficients------------ Start
	cmd[0] = 24;
	cmd[1] = (n0_l & 0xFF0000UL) >> 16; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 25;
	cmd[1] = (n0_l & 0xFF00UL) >> 8; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 26;
	cmd[1] = (n0_l & 0xFFUL) >> 0; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 28;
	cmd[1] = (n1_l & 0xFF0000UL) >> 16; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 29;
	cmd[1] = (n1_l & 0xFF00UL) >> 8; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 30;
	cmd[1] = (n1_l & 0xFFUL) >> 0; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 32;
	cmd[1] = (d1_l & 0xFF0000UL) >> 16; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 33;
	cmd[1] = (d1_l & 0xFF00UL) >> 8; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 34;
	cmd[1] = (d1_l & 0xFFUL) >> 0; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	// Filter coefficients------------ end

	//	w 30 00 00 # Select Page 1
	codec_set_page(2);

	// Filter coefficients------------ Start
	cmd[0] = 32;
	cmd[1] = (n0_r & 0xFF0000UL) >> 16; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 33;
	cmd[1] = (n0_r & 0xFF00UL) >> 8; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 34;
	cmd[1] = (n0_r & 0xFFUL) >> 0; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 36;
	cmd[1] = (n1_r & 0xFF0000UL) >> 16; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 37;
	cmd[1] = (n1_r & 0xFF00UL) >> 8; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 38;
	cmd[1] = (n1_r & 0xFFUL) >> 0; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 40;
	cmd[1] = (d1_r & 0xFF0000UL) >> 16; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 41;
	cmd[1] = (d1_r & 0xFF00UL) >> 8; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 42;
	cmd[1] = (d1_r & 0xFFUL) >> 0; //
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	// Filter coefficients------------ end

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

	//	# Select Book 0
	codec_set_book(0);

}



#if (CODEC_BEEP_GENERATOR==1)
static void beep_gen(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	codec_set_page(0);

	UARTprintf("Beep\n");

	// Set beep time to be 2 seconds=0x01770

	uint32_t delay=0x01770;
	cmd[0] = 73;
	cmd[1] = (delay & 0xFF0000) >> 16;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	cmd[0] = 74;
	cmd[1] = (delay & 0xFF00) >> 8;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	cmd[0] = 75;
	cmd[1] = (delay & 0xFF) >> 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 0x47;
	cmd[1] = (1 << 7);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);


}
#endif



static void codec_mic_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	// Enable ADC_MOD_CLK for mic B0_P4

	//	w 30 00 01 # Select Page 4
	codec_set_page(4);

#if (CODEC_LEFT_LATCH_FALLING == 1)
	cmd[0] = 0x64;
	cmd[1] = (1 << 7) | (0 << 6) | (1 << 3) | (0 << 2);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#endif

	// # Digital Mic 1 Input Pin Control (GPIO4 -Left, GPIO5 - Right) - Stereo
	cmd[0] = 0x65;
	cmd[1] = (3 << 4) | (4 << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # Digital Mix 2 Input Pin Control (GPIO6 -Left) - Left-only
	cmd[0] = 0x66;
	cmd[1] = (5 << 4);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	# Select Page 0
	codec_set_page(0);

	// # ADC channel power control - Left and right channel ADC configured for Digital Mic
	cmd[0] = 0x51;
	cmd[1] = (1 << 4) | (1 << 2) |  	// Configure left and right channel ADC for digital microphone
				(2 << 0) 				// ADC Volume control soft-stepping disabled
			| (1 << 6) | (1 << 7);		// Enable left and right ADC
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # ADC Fine Gain Volume Control, Unmute Left and Right ADC channel
	cmd[0] = 0x52;
	cmd[1] = 0; //(4 << 4) | (4 << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// # Left ADC Volume Control
	// TODO play with volume
	cmd[0] = 0x53;
	cmd[1] = 0x28; //0;// 0x68;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// TODO play with volume
	cmd[0] = 0x54;
	cmd[1] = 0x28; //0; //0x68;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

#if (CODEC_DIG_MIC2_EN==1)
	// # Digital mic 2 control - Enable CIC2 Left channel, and digital mic to left channel
	cmd[0] = 0x70;
	cmd[1] = (1 << 7) | (1 << 4);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#endif

	//	Disable headsest detection
	cmd[0] = 0x77;
	cmd[1] = 1 << 2;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	# Select Page 0
	codec_set_page(0);

	vTaskDelay(5);
}

// Enable Class-D Speaker playback
static void codec_speaker_config(void)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

	//	w 30 3f c0 # Power up the Left and Right DAC Channels
	cmd[0] = 0x3F;
	cmd[1] = 0xC0; // TODO soft stepping disabled
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 40 00 # Unmute the DAC digital volume control
	cmd[0] = 0x40;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 00 01 # Select Page 1
	codec_set_page(1);

	//	w 30 03 00 # Set PTM mode for Left DAC to PTM_P3
	cmd[0] = 0x03;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 04 00 # Set PTM mode for Right DAC to PTM_P3
	cmd[0] = 0x04;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 16 c3 # DAC to LOL/R routing, power-up LOL/R
	cmd[0] = 0x16;
	cmd[1] = (1 << 7) | (1 << 6) | (1 << 1) | (1 << 0); //0xC3;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 2E 0c # Route LOL to SPK @ -6dB
	cmd[0] = 0x2E;
	cmd[1] = 0x0C;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 2F 0c # Route LOR to SPK_RIGHT_CH_IN @ -6dB
	cmd[0] = 0x2F;
	cmd[1] = 0x0C;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 30 21 # SPK Gain = 12dB, unmute SPK_RIGHT_CH_IN
	cmd[0] = 0x30;
	cmd[1] = (2 << 4) | (1 << 0); //0x21;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	// ENable PGA, might not be needed //TODO
#if 1
	cmd[0] = 59;
	cmd[1] = 0x80;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	cmd[0] = 60;
	cmd[1] = 0x80;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
#endif

	//	w 30 2D 06 # Power-up SPK, route SPK_RIGHT_CH_IN to SPK
	cmd[0] = 0x2D;
	cmd[1] = (1 << 2) | (1 << 1); //0x06;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);



	// ENable DRC hold
	cmd[0] = 0x45;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);


	vTaskDelay(20);
}
#endif

