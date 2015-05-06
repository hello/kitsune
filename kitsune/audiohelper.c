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


/* externs */
extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

#define AUDIO_PLAYBACK_RATE_HZ (48000)

static const unsigned int CPU_XDATA_CAPTURE = 0; //1: enabled CPU interrupt triggerred

static const unsigned int CPU_XDATA_PLAYBACK = 0; //1: enabled CPU interrupt triggerred, 0 for DMA

uint8_t InitAudioDuplex(uint32_t rate){
	pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE);
	pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);
	if(!pTxBuffer || !pRxBuffer){
		return 0;
	}
	get_codec_io_NAU();
	AudioCapturerInit(0, rate); //always dma
	UDMAInit();
	UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);
	UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);
	SetupPingPongDMATransferTx();
	SetupPingPongDMATransferRx();
	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure(I2S_PORT_DMA, rate);
	return 1;
}
void DeInitAudioDuplex(){
	Audio_Stop();
	McASPDeInit();
	if(pTxBuffer){
		DestroyCircularBuffer(pTxBuffer);
	}
	if(pRxBuffer){
		DestroyCircularBuffer(pRxBuffer);
	}
}
uint8_t InitAudioCapture(uint32_t rate) {

	if(pTxBuffer == NULL) {
		pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE);
	}

	if(pTxBuffer == NULL) {
		return 0;
	}

	get_codec_mic_NAU();
	// Initialize the Audio(I2S) Module
	AudioCapturerInit(CPU_XDATA_CAPTURE, rate);

	// Initialize the DMA Module
	UDMAInit();

	UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);

	// Setup the DMA Mode
	SetupPingPongDMATransferTx();

	// Setup the Audio In/Out
	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure(I2S_PORT_DMA, rate);

	// Start Audio Tx/Rx
	Audio_Start();

	return 1;
}

void DeinitAudioCapture(void) {
	Audio_Stop();

	McASPDeInit();

	if (pTxBuffer) {
		DestroyCircularBuffer(pTxBuffer);
		pTxBuffer = NULL;
	}
}

uint8_t InitAudioPlayback(int32_t vol, uint32_t rate ) {

	//create circular buffer
	pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);

	if (!pRxBuffer) {
		return 0;
	}

	//zero out RX byffer
	memset(pRxBuffer->pucBufferStartPtr,0,RX_BUFFER_SIZE);

	//////
	// SET UP AUDIO PLAYBACK
	get_codec_NAU(vol);
	// Initialize the Audio(I2S) Module
	AudioCapturerInit(CPU_XDATA_PLAYBACK, rate);

	// Initialize the DMA Module
	UDMAInit();

	UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);

	// Setup the DMA Mode
	SetupPingPongDMATransferRx();

	// Setup the Audio In/Out
	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure(I2S_PORT_DMA, rate);

	return 1;

}

void DeinitAudioPlayback(void) {
	close_codec_NAU();

	McASPDeInit();

	if (pRxBuffer) {
		DestroyCircularBuffer(pRxBuffer);
	}
}


///// FILE STUFF/////


uint8_t InitFile(Filedata_t * pfiledata) {
	FRESULT res;

	/*  If we got here, then the file should already be closed */
	uint8_t ret = 1;
	/* open file */
	res = hello_fs_open(&pfiledata->file_obj, pfiledata->file_name, FA_WRITE|FA_CREATE_ALWAYS);

	/*  Did something horrible happen?  */
	if(res != FR_OK) {
		ret = 0;
	}

	return ret;

}

uint8_t WriteToFile(Filedata_t * pfiledata,const WORD bytes_to_write,const uint8_t * const_ptr_samples_bytes) {
	WORD bytes = 0;
	WORD bytes_written = 0;
	FRESULT res;
	uint8_t ret = 1;


	/* write until we cannot write anymore.  This does take a finite amount of time, by the way.  */
	do {
		res = hello_fs_write(&pfiledata->file_obj, const_ptr_samples_bytes +  bytes_written , bytes_to_write-bytes_written, &bytes );

		bytes_written+=bytes;

		if (res != FR_OK) {
			ret = 0;
			break;
		}

	} while( bytes_written < bytes_to_write );

	return ret;

}

void CloseFile(Filedata_t * pfiledata) {
	hello_fs_close(&pfiledata->file_obj)	;
	memset(&pfiledata->file_obj, 0, sizeof(file_obj));

}

void CloseAndDeleteFile(Filedata_t * pfiledata) {
	hello_fs_close(&pfiledata->file_obj);
	hello_fs_unlink(pfiledata->file_name);
	memset(&pfiledata->file_obj, 0, sizeof(file_obj));

}

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

