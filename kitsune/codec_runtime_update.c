#include "codec_runtime_update.h"
#include "audio_codec_pps_driver.h"
#include "i2c_if.h"
#include "i2c_cmd.h"

#include "kit_assert.h"
#include "uartstdio.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "stdint.h"

/*
 * Best resource for changing coefficients on the fly:
 * http://www.ti.com/lit/an/slaa425d/slaa425d.pdf
 */

// The miniDSP has 24-bit coefficients
#define NUMBER_BYTES_IN_DSP_COEFFICIENT 	3
// I2C write for each coefficient will have 1 byte for starting register and
// data bytes depending on coefficient size
#define I2C_COEFF_WRITE_LENGTH				(NUMBER_BYTES_IN_DSP_COEFFICIENT)+1

#if (I2C_COEFF_WRITE_LENGTH != 4)
#error "Function codec_write_cram assumes size 4, modify it"
#endif

#define ADC_ADAPTIVE_COEFFICIENT_BANK   	40
#define DAC_ADAPTIVE_COEFFICIENT_BANK_1 	80
#define DAC_ADAPTIVE_COEFFICIENT_BANK_2 	82

extern xSemaphoreHandle i2c_smphr;

typedef int32_t cram_rw_t(control_blocks_t type, uint32_t* data);

typedef struct {
            uint8_t book;           //coefficient book location
            uint8_t page;           //coefficient page location
            uint8_t reg;            //coefficient register location
            uint8_t length;         // coefficient length in 32-bit words
} control_t;

typedef enum{
	codec_cram_write = 0,
	codec_cram_read = 1
}codec_cram_rw_t;

/********************************************************************************
 *                        IMPORTANT: PLEASE READ
 *
 * Note: If the PurePath Process flow changes, the following array and all the
 * coresponding I2C addresses need to be updated by comparing with them with the
 * Component Interface Overview (Under Tools) in PurePath Studio
 *
 * Also, verify that the I2C addresses here match the array indices given by
 * control_blocks_t (in codec_runtime_update.h)
 ********************************************************************************
 */
static const control_t control[MAX_CONTROL_BLOCKS] = {

		{ADC_ADAPTIVE_COEFFICIENT_BANK,    6, 20, 1},   	// MUX_LOOPBACK_SELECTOR


};


/*
 * IMPORTANT: From AN-SLAA425d
 * 		"the audio bus must be active (externally or internally); otherwise, the buffers
 * 		do not switch until the bus is activated"
 */
static int32_t codec_write_cram(control_blocks_t type, uint32_t* data){
	char send_stop = 1;
	unsigned char cmd[I2C_COEFF_WRITE_LENGTH];
	uint8_t index;

	if( (type >= MAX_CONTROL_BLOCKS ) || (!data)) return -1;

	codec_set_book(control[type].book);

	codec_set_page(control[type].page);

	cmd[0] = control[type].reg;

	for(index=0;index<control[type].length;index++){

		assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));

		cmd[1] = (data[index] & 0xFF0000UL) >> 16;
		cmd[2] = (data[index] & 0xFF00UL) >> 8;
		cmd[3] = (data[index] & 0xFFUL) >> 0;
		I2C_IF_Write(Codec_addr, cmd, 4, send_stop);

		xSemaphoreGiveRecursive(i2c_smphr);

		UARTprintf("CRAM Write: [%d][%d][%d]:%x\n", \
						control[type].book,control[type].page,cmd[0],data[index]);

		cmd[0] += 4;

		vTaskDelay(5);
	}

	return 0;
}

static int32_t codec_switch_buffer(uint8_t bank){

	unsigned char cmd[2];
	char send_stop = 1;

	codec_set_book(bank);

	codec_set_page(0);

	assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
	// Write one to switch buffer
	cmd[0] = 0x01;
	cmd[1] = (1 << 2) | (1 << 0);
	I2C_IF_Write(Codec_addr, cmd, 2, send_stop);
	xSemaphoreGiveRecursive(i2c_smphr);

	// Wait till bit is cleared
	do{
		assert(xSemaphoreTakeRecursive(i2c_smphr, 30000));
		I2C_IF_Write(Codec_addr, &cmd[0],1,send_stop);
		I2C_IF_Read(Codec_addr, &cmd[1], 1);
		xSemaphoreGiveRecursive(i2c_smphr);

		vTaskDelay(5);

#if 0
		UARTprintf("Switch: [%d][%d][%d]: %x\n",bank, 0, cmd[0], cmd[1]);
#endif
	}while(cmd[1] & 0x1);


	return 0;

}

static int32_t codec_read_cram(control_blocks_t type, uint32_t* data){

	char send_stop = 1;
	unsigned char cmd[I2C_COEFF_WRITE_LENGTH];
	uint8_t index;

	if( (type >= MAX_CONTROL_BLOCKS ) || (!data)) return -1;

	codec_set_book(control[type].book);

	codec_set_page(control[type].page);

	cmd[0] = control[type].reg;

	for(index=0;index<control[type].length;index++){

		assert(xSemaphoreTakeRecursive(i2c_smphr, 30000))

		I2C_IF_Write(Codec_addr, &cmd[0], 1, send_stop);
		I2C_IF_Read(Codec_addr, &cmd[1],3 );
		xSemaphoreGiveRecursive(i2c_smphr);

		data[index] = ((uint32_t)cmd[1] << 16) | ((uint32_t)cmd[2] << 8) | ((uint32_t)cmd[3] << 0);

#if 0
		UARTprintf("CRAM Read: [%d][%d][%d]:%x\n", \
				control[type].book,control[type].page,cmd[0],data[index]);
#endif

	}
	return 0;
}

/*
 * Note: CRAM write is done twice (before and after switching buffers).
 * According to the source http://www.ti.com/lit/an/slaa404c/slaa404c.pdf,
 * 		"This step ensures that both buffers are synchronized."
 */
int32_t codec_update_cram(control_blocks_t type, uint32_t* data, codec_cram_rw_t rw){

	int32_t ret = 0;

	if( (type >= MAX_CONTROL_BLOCKS ) || (!data)) return -1;

	cram_rw_t *func = (rw == codec_cram_read) ? codec_read_cram : codec_write_cram;

	if(rw == codec_cram_write){
		ret = func(type, data); // Only for write
		if(ret) return ret;
	}

	ret = codec_switch_buffer(control[type].book);
	if(ret) return ret;

	return func(type, data);

}

int32_t codec_update_minidsp_mux(control_blocks_t type, uint32_t data){
	return codec_update_cram(type,&data, codec_cram_write);

}

// Only for testing
int32_t codec_runtime_prop_update(control_blocks_t type, uint32_t value){
	uint32_t read_data[10];
	uint32_t test_data[1] = {(uint32_t)value};

	codec_update_minidsp_mux(type, test_data[0]);
	codec_update_cram(type, read_data, codec_cram_read);

	if(test_data[0] != read_data[0]){
		UARTprintf("Runtime Update Error: Test %x, Read %x\n", test_data[0],read_data[0]);
		return -1;
	}

	UARTprintf("Runtime Update Pass: Test %x, Read %x\n", test_data[0],read_data[0]);

	return 0;
}


int cmd_codec_runtime_update(int argc, char *argv[]) {
	codec_runtime_prop_update(atoi(argv[1]), atoi(argv[2]));
	return 0;
}

