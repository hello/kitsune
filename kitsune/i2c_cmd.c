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
	b[1] = 0x30; //60sec
	(I2C_IF_Write(0x5a, b, 2, 1));

	xSemaphoreGiveRecursive(i2c_smphr);

	return 0;
}
#define DBG_TVOC(...)
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

	*w = get_le_short(b);
	*r = get_le_short(b+2);
	*g = get_le_short(b+4);
	*bl = get_le_short(b+6);
	*p = get_le_short(b+8);

	xSemaphoreGiveRecursive(i2c_smphr);
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

int Cmd_readlight(int argc, char *argv[]) {
	int r,g,b,w,p,ir;
	if( SUCCESS == get_rgb_prox( &w, &r, &g, &b, &p ) ) {
		LOGF("%d,%d,%d,%d,%d\n", w,r,g,b,p );
	}
	if( 0 == get_ir(&ir) ) {
		LOGF("%d\n", ir );
	}
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
bool set_mic_gain(int v, unsigned int dly) {
	unsigned char cmd_init[2];

	cmd_init[0] = 0x5f;
	cmd_init[1] = (v & 0x1F) | 0x20;

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
