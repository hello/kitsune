#include "codec_runtime_update.h"
#include "audio_codec_pps_driver.h"
#include "i2c_if.h"
#include "i2c_cmd.h"

// The miniDSP has 24-bit coefficients
#define NUMBER_BYTES_IN_DSP_COEFFICIENT 	3
// I2C write for each coefficient will have 1 byte for starting register and
// data bytes depending on coefficient size
#define I2C_COEFF_WRITE_LENGTH				(NUMBER_BYTES_IN_DSP_COEFFICIENT)+1

#if (I2C_COEFF_WRITE_LENGTH != 4)
#error "Function codec_update_minidsp_coeff assumes size 4, modify it"
#endif

typedef struct {
            uint8_t book;           //coefficient book location
            uint8_t page;           //coefficient page location
            uint8_t reg;            //coefficient register location
            uint8_t length;         // coefficient length in 32-bit words
} control_t;

static const control_t control[MAX_CONTROL_BLOCKS] = {

		{ 40, 2, 104, 1},
		{ 80, 2 , 24, 1},
		{ 80, 1 , 108, 1},
		{ 40, 2, 108, 1}
};

int32_t codec_update_minidsp_mux(control_blocks_t type, uint32_t data)
{
	return codec_update_minidsp_coeff(type,&data);

}

int32_t codec_update_minidsp_coeff(control_blocks_t type, uint32_t* data)
{
	char send_stop = 1;
	unsigned char cmd[I2C_COEFF_WRITE_LENGTH];
	uint8_t index;

	if( (type >= MAX_CONTROL_BLOCKS ) || (!data)) return -1;

	codec_set_book(control[type].book);

	codec_set_page(control[type].page);

	cmd[0] = control[type].reg;

	for(index=0;index<control[type].length;index++){

		cmd[1] = (data[index] & 0xFF0000UL) >> 16;
		cmd[2] = (data[index] & 0xFF00UL) >> 8;
		cmd[3] = (data[index] & 0xFFUL) >> 0;

		I2C_IF_Write(Codec_addr, cmd, 4, send_stop);

	}

	return 0;
}


