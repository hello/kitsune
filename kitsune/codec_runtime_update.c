#include "codec_runtime_update.h"
#include "audio_codec_pps_driver.h"

typedef struct {
            uint8_t book;           //coefficient book location
            uint8_t page;           //coefficient page location
            uint8_t reg;            //coefficient register location
            uint8_t length;         // coefficient length
} control_t;

static const control_t control[MAX_CONTROL_BLOCKS] = {

		{ 40, 2, 104, 1},
		{ 80, 2 , 24, 1},
		{ 80, 1 , 108, 1},
		{ 40, 2, 108, 1}
};

int32_t codec_update_minidsp_coeff(control_blocks_t type, uint8_t* data)
{

	return 0;
}
