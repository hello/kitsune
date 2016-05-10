#include "audiohelper.h"

#include "network.h"
#include "audioprocessingtask.h"
#include "mcasp_if.h"
#include "hw_types.h"
#include "udma.h"
#include "pcm_handler.h"
#include "i2c_cmd.h"
#include "i2s.h"
#include "udma_if.h"
#include "circ_buff.h"
#include "simplelink.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "hw_ints.h"
// common interface includes
#include "common.h"
#include "hw_memmap.h"
#include "hellofilesystem.h"
#include "fatfs_cmd.h"
#include "uart_logger.h"
#include <ustdlib.h>

#include "FreeRTOS.h"
#include "kit_assert.h"

/* externs */
extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

#define AUDIO_PLAYBACK_RATE_HZ (48000)

uint8_t InitAudioCapture(uint32_t rate) {

	if(pTxBuffer == NULL) {
		pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE);
	}
	get_codec_mic_NAU();
	// Initialize the Audio(I2S) Module
	McASPInit(rate);

	UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);

	// Setup the DMA Mode
	SetupPingPongDMATransferTx();

	// Setup the Audio In/Out
    MAP_I2SIntEnable(I2S_BASE, I2S_INT_RDMA );
	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure( rate);

	// Start Audio Tx/Rx
	Audio_Start();

	return 1;
}

void DeinitAudioCapture(void) {
	Audio_Stop();

	McASPDeInit();

	MAP_uDMAChannelDisable(UDMA_CH4_I2S_RX);

	if (pTxBuffer) {
		DestroyCircularBuffer(pTxBuffer);
		pTxBuffer = NULL;
	}
}

uint8_t InitAudioPlayback(int32_t vol, uint32_t rate ) {

	//create circular buffer
	if (!pRxBuffer) {
		pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);
	}

	//////
	// SET UP AUDIO PLAYBACK
	get_codec_NAU(vol);
	// Initialize the Audio(I2S) Module
	McASPInit(rate);


	UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);

	// Setup the DMA Mode
	SetupPingPongDMATransferRx();

	// Setup the Audio In/Out

    MAP_I2SIntEnable(I2S_BASE,I2S_INT_XDMA  );
	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure( rate);

	return 1;

}

void DeinitAudioPlayback(void) {
	close_codec_NAU();

	McASPDeInit();

	MAP_uDMAChannelDisable(UDMA_CH5_I2S_TX);
	if (pRxBuffer) {
		DestroyCircularBuffer(pRxBuffer);
		pRxBuffer = NULL;
	}
}


///// FILE STUFF/////
int deleteFilesInDir(const char* dir)
{
	DIR dirObject = {0};
	FILINFO fileInfo = {0};
    FRESULT res;
    char path[64] = {0};

    res = hello_fs_opendir(&dirObject, dir);

    if(res != FR_OK)
    {
        return 0;
    }


    for(;;)
    {
        res = hello_fs_readdir(&dirObject, &fileInfo);
        if(res != FR_OK)
        {
            return 0;
        }

        // If the file name is blank, then this is the end of the listing.
        if(!fileInfo.fname[0])
        {
            break;
        }

        if(fileInfo.fattrib & AM_DIR)  // directory
        {
            continue;
        } else {
        	memset(path, 0, sizeof(path));
        	usnprintf(path, sizeof(fileInfo.fname) + 5, "/usr/%s", fileInfo.fname);
            res = hello_fs_unlink(path);
            if(res == FR_OK)
            {
            	LOGI("User file deleted %s\n", path);
            }else{
            	LOGE("Delete user file %s failed, err %d\n", path, res);
            }
        }
    }

    return(0);
}

