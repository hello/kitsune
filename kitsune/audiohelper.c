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
#include "queue.h"
#include "semphr.h"

/* externs */
extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

static unsigned char * audio_mem;
static unsigned char * audio_mem_p;

// mutex to protect i2s enabled status
static xSemaphoreHandle _i2s_enabled_mutex = NULL;

typedef enum
{
	deinit_capture = 0,
	deinit_playback = 1,
	deinit_both = 2
}deinit_type;
void McASPDeInit_FullDuplex(deinit_type type);
void SetupDMAModeFullDuplex(void (*pfnAppCbHndlr)(void), deinit_type type);

void InitAudioHelper() {
	audio_mem = (unsigned char*)pvPortMalloc( AUD_BUFFER_SIZE );
}

void InitAudioHelper_p() {

	audio_mem_p = (unsigned char*)pvPortMalloc( AUD_BUFFER_SIZE );
}

void InitAudioTxRx(uint32_t rate)
{
	// Initialize the Audio(I2S) Module
	McASPInit(rate);

	MAP_I2SIntRegister(I2S_BASE,DMAPingPongCompleteAppCB_opt);

	// init mutex for file download status flag
	if(!_i2s_enabled_mutex)
	{
		_i2s_enabled_mutex = xSemaphoreCreateMutex();
	}

}

uint8_t InitAudioCapture(uint32_t rate) {

	if(pTxBuffer == NULL) {
		pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE, audio_mem);
	}
	memset( audio_mem, 0, AUD_BUFFER_SIZE);

	// Setup the Audio In/Out
	MAP_I2SIntEnable(I2S_BASE, I2S_INT_RDMA );

	UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);

	// Setup the DMA Mode
	SetupPingPongDMATransferTx();

	// Setup the Audio In/Out
    //MAP_I2SIntEnable(I2S_BASE, I2S_INT_RDMA | I2S_INT_XDMA );
	MAP_I2SRxFIFOEnable(I2S_BASE,8,1);

	//MAP_I2SIntRegister(I2S_BASE,DMAPingPongCompleteAppCB_opt);

    MAP_I2SSerializerConfig(I2S_BASE,I2S_DATA_LINE_1,I2S_SER_MODE_RX, I2S_INACT_LOW_LEVEL);

	// Start Audio Tx/Rx
	Audio_Start();

	return 1;
}

void McASPDeInit_FullDuplex(deinit_type type)
{

	switch(type)
	{
	case deinit_capture:
		MAP_I2SIntDisable(I2S_BASE, I2S_INT_RDMA);
		//MAP_I2SRxFIFODisable(I2S_BASE);
		break;

	case deinit_playback:
		MAP_I2SIntDisable(I2S_BASE,I2S_INT_XDMA);
		//MAP_I2STxFIFODisable(I2S_BASE);
		break;

	case deinit_both:
		MAP_I2SIntDisable(I2S_BASE,I2S_INT_XDMA | I2S_INT_RDMA);
		MAP_I2STxFIFODisable(I2S_BASE);
		MAP_I2SRxFIFODisable(I2S_BASE);
		I2SIntUnregister(I2S_BASE);
		break;

	default:
		break;


	}

}

//*****************************************************************************
//
//! Setup the Audio capturer callback routine and the interval of callback.
//! On the invocation the callback is expected to fill the AFIFO with enough
//! number of samples for one callback interval.
//!
//! \param pfnAppCbHndlr is the callback handler that is invoked when
//! \e ucCallbackEvtSamples space is available in the audio FIFO
//! \param ucCallbackEvtSamples is used to configure the callback interval
//!
//! This function initializes
//!        1. Initializes the interrupt callback routine
//!        2. Sets up the AFIFO to interrupt at the configured interval
//!
//! \return None.
//
//*****************************************************************************
void SetupDMAModeFullDuplex(void (*pfnAppCbHndlr)(void), deinit_type type)
{
	MAP_I2SIntRegister(I2S_BASE,DMAPingPongCompleteAppCB_opt);

	if(type == deinit_capture)
	{
		MAP_I2SRxFIFOEnable(I2S_BASE,8,1);
	}
	else if(type == deinit_playback)
	{
		MAP_I2STxFIFOEnable(I2S_BASE,8,1);
	}

}


void DeinitAudioCapture(void) {

	Audio_Stop();

	// Setup the Audio In/Out
	MAP_I2SIntDisable(I2S_BASE, I2S_INT_RDMA );
	//McASPDeInit_FullDuplex(deinit_capture);

	MAP_uDMAChannelDisable(UDMA_CH4_I2S_RX);

	MAP_I2SRxFIFODisable(I2S_BASE);

	if (pTxBuffer) {
		DestroyCircularBuffer(pTxBuffer);
		pTxBuffer = NULL;
	}
}

uint8_t InitAudioPlayback(int32_t vol, uint32_t rate ) {

	//create circular buffer
	if (!pRxBuffer) {
		pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE, audio_mem_p);
	}
	memset( audio_mem_p, 0, AUD_BUFFER_SIZE);

	// Initialize the Audio(I2S) Module
	//McASPInit(rate);

	// Unmute speaker
	codec_unmute_spkr();

	UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);

	// Setup the DMA Mode
	SetupPingPongDMATransferRx();

	// Setup the Audio In/Out
    //MAP_I2SIntEnable(I2S_BASE, I2S_INT_RDMA | I2S_INT_XDMA );
	MAP_I2STxFIFOEnable(I2S_BASE,8,1);

	//MAP_I2SIntRegister(I2S_BASE,DMAPingPongCompleteAppCB_opt);

    //MAP_I2SSerializerConfig(I2S_BASE,I2S_DATA_LINE_1,I2S_SER_MODE_RX, I2S_INACT_LOW_LEVEL);
    MAP_I2SSerializerConfig(I2S_BASE,I2S_DATA_LINE_0,I2S_SER_MODE_TX, I2S_INACT_LOW_LEVEL);

	// Setup the Audio In/Out
    MAP_I2SIntEnable(I2S_BASE,I2S_INT_XDMA);

	//////
	// SET UP AUDIO PLAYBACK
	return 1;

}

void DeinitAudioPlayback(void) {

	// Mute speaker
	codec_mute_spkr();

	//McASPDeInit_FullDuplex(deinit_playback);

	MAP_uDMAChannelDisable(UDMA_CH5_I2S_TX);

	MAP_I2STxFIFODisable(I2S_BASE);
	MAP_I2SIntDisable(I2S_BASE,I2S_INT_XDMA);

	if (pRxBuffer) {
		DestroyCircularBuffer(pRxBuffer);
		pRxBuffer = NULL;
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

uint8_t WriteToFile(Filedata_t * pfiledata,const UINT bytes_to_write,const uint8_t * const_ptr_samples_bytes) {
	UINT bytes = 0;
	UINT bytes_written = 0;
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
