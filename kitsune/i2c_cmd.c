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
#define CODEC_USE_MINIDSP 			1 // Set to 1 if using miniDSP, else 0
#define CODEC_ADC_16KHZ    			1 // Set to 1 if ADC sampling rate is 16k Hz. If not, ADC Fs = DAC Fs = 48k Hz

// Left mic data is latched on rising by default, to latch it on rising edge instead
// set this to be 1
#define CODEC_LEFT_LATCH_FALLING 	1
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
	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	if( get_hw_ver() < EVT1_1p5 ) {
		unsigned char aucDataBuf[2] = { 0, 0xA0 };
		(I2C_IF_Write(0x44, aucDataBuf, 2, 1));
	} else {
		unsigned char aucDataBuf[3] = { 1, 0b11001110, 0b00001000 };
		(I2C_IF_Write(0x44, aucDataBuf, 3, 1));
	}

	xSemaphoreGiveRecursive(i2c_smphr);
	return SUCCESS;
}

static int _read_als(){
	unsigned char cmd;
	unsigned char aucDataBuf[5] = { 0, 0 };

	cmd = 0x2;
	(I2C_IF_Write(0x44, &cmd, 1, 1));
	(I2C_IF_Read(0x44, aucDataBuf, 2));

	return aucDataBuf[0] | (aucDataBuf[1] << 8);
}

int get_light() {
	unsigned char aucDataBuf[2] = { 0, 0 };

	assert(xSemaphoreTakeRecursive(i2c_smphr, 1000));

	int light = 0;
	static int scaling = 0;
	int prev_scaling = scaling;

	if( get_hw_ver() < EVT1_1p5 ) {
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
	} else {
		uint8_t cmd = 0x0;
		(I2C_IF_Write(0x44, &cmd, 1, 1));
		(I2C_IF_Read(0x44, aucDataBuf, 2));

		uint16_t raw =  aucDataBuf[1] | (aucDataBuf[0] << 8);
		unsigned int exp = raw >> 12; //pull out exponent
		raw &= 0xFFF; //clear the exponent

		light = raw;
		light *= ((1<<exp) * 65536 ) / (125 * 100 );
		//LOGI("%x\t%x\t\t%d, %d, %d\n", aucDataBuf[0],aucDataBuf[1], raw, exp, light);
	}
	xSemaphoreGiveRecursive(i2c_smphr);
	return light;
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

#include "simplelink.h"
#include "sl_sync_include_after_simplelink_header.h"

static void codec_sw_reset(void);

#if (CODEC_USE_MINIDSP == 0)
static void codec_fifo_config(void);
static void codec_power_config(void);
static void codec_clock_config(void);
static void codec_gpio_config(void);
static void codec_asi_config(void);
static void codec_signal_processing_config(void);
#endif

static void codec_mic_config(void);
static void codec_speaker_config(void);
static void codec_set_page(uint32_t page);
static void codec_set_book(uint32_t book);

#if (CODEC_BEEP_GENERATOR==1)
static void beep_gen(void);
#endif



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

	//w 30 01 01 # Software Reset
	cmd[0] = 0x01;
	cmd[1] = 0x01;
	if((ret=I2C_IF_Write(Codec_addr, cmd, 2, send_stop)))
	{
		UARTprintf("Codec sw reset fail:%d\n",ret);
	}

	vTaskDelay(delay);
}

static void codec_set_page(uint32_t page)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	w 30 00 00 # Select Page 0
	cmd[0] = 0;
	cmd[1] = page;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
}

static void codec_set_book(uint32_t book)
{
	char send_stop = 1;
	unsigned char cmd[2];

	//	# Select Book 0
	cmd[0] = 0x7F;
	cmd[1] = book;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
}

// FOR board bring up
#ifdef CODEC_1P5_TEST
int32_t codec_test_commands(void)
{
	unsigned char cmd[2] = {0};
	char send_stop = 1;

	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {
		// Send Software reset
		codec_sw_reset();

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

// TODO use i2s semaphore


#if (CODEC_USE_MINIDSP == 1)

#if (CODEC_PPS_FROM_ARRAY == 1)
#include "audio_codec_pps_driver.h"
int32_t codec_init_with_dsp(void)
{
	uint32_t i;
	char send_stop = 1;
	unsigned char cmd[2];
	uint32_t reg_array_size = sizeof(REG_Section_program)/2;
	UARTprintf("Size of reg array = %u\n", reg_array_size);

	// Write the registers
	for(i=0;i<reg_array_size;i++)
	{
		//	# Select Book 0
		cmd[0] = REG_Section_program[i].reg_off;
		cmd[1] = REG_Section_program[i].reg_val;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	}



	// Update miniDSP A
	reg_array_size = sizeof(miniDSP_A_reg_values)/2;
	UARTprintf("Size of miniDSP_A array = %u vs %u\n", reg_array_size,miniDSP_A_reg_values_COEFF_SIZE+miniDSP_A_reg_values_INST_SIZE );

	// Write the registers
	for(i=0;i<reg_array_size;i++)
	{
		//	# Select Book 0
		cmd[0] = miniDSP_A_reg_values[i].reg_off;
		cmd[1] = miniDSP_A_reg_values[i].reg_val;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	}

	// Update miniDSP D
	reg_array_size = sizeof(miniDSP_D_reg_values)/2;
	UARTprintf("Size of miniDSP_D array = %u vs %u\n", reg_array_size, miniDSP_D_reg_values_COEFF_SIZE+miniDSP_D_reg_values_INST_SIZE  );

	// Write the registers
	for(i=0;i<reg_array_size;i++)
	{
		//	# Select Book 0
		cmd[0] = miniDSP_D_reg_values[i].reg_off;
		cmd[1] = miniDSP_D_reg_values[i].reg_val;
		I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	}

	//codec_set_page(0);

	//codec_set_book(0);

	//codec_mic_config();

	//codec_set_page(0);

	//codec_set_book(0);

	//codec_speaker_config();

	vTaskDelay(100);
#ifdef CODEC_1P5_TEST

	//	w 30 00 00 # Select Page 0
	codec_set_page(0);

	codec_set_book(0);

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

// Start playback

// Stop playback

// start capture

//stop capture

// update volume

// Software reset codec
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

	// Audio Serial Interface Configuration (ASI1 with 6 wire I2S setup)
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

	// Output Channel Configuration TODO verify
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
// TODO verify
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
	cmd[0] = 0x7A;
	cmd[1] = 0x01; // Recommended settings as per datasheet
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);

	//	w 30 79 33 # Quick charge of analog inputs caps (1.1ms Left and 0.5ms right),
	// as per MIC example, may not be needed for digital mic? TODO // This is the reset value
	cmd[0] = 0x79;
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

	// MULTI CHANNEL TODO DKH
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

	cmd[0] = 0x010;
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
	cmd[0] = 0x60;//92
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

	// ENable DRC hold
	cmd[0] = 0x45;
	cmd[1] = 0;
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);


	vTaskDelay(20);
}

