#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "audiocapturetask.h"

#include "network.h"
#include "circ_buff.h"
#include "simplelink.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "hw_ints.h"
// common interface includes
#include "common.h"
#include "hw_memmap.h"
#include "fatfs_cmd.h"
#include "ff.h"

#include "audiofeatures.h"
#include "audioprocessingtask.h"
#include "mcasp_if.h"
#include "hw_types.h"
#include "udma.h"
#include "pcm_handler.h"
#include "i2c_cmd.h"
#include "i2s.h"



#define AUDIO_DEBUG_MESSAGES

#define SWAP_ENDIAN

#define INBOX_QUEUE_LENGTH (5)
#define LOOP_DELAY_WHILE_PROCESSING_IN_TICKS (1)

#define AUDIO_RATE 16000
static const unsigned int CPU_XDATA = 1; //1: enabled CPU interrupt triggerred


/* externs */
extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;



/* static variables  */
static xQueueHandle _queue = NULL;
static uint32_t _captureCounter;
static portTickType _readMaxDelay = portMAX_DELAY;
static uint8_t _isCapturing = 0;
static int64_t _callCounter;
static CommandCompleteNotification _fpLastCommandComplete;

static void DataCallback(const AudioFeatures_t * pfeats) {
	//AudioProcessingTask_AddMessageToQueue(pfeats);
}


static void CreateTxBuffer(void) {
	// Create TX Buffer
	if (!pTxBuffer) {

#ifdef AUDIO_DEBUG_MESSAGES
		UARTprintf("tx buffer created\n");
		vTaskDelay(5);
#endif

		pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE);
		if(pTxBuffer == NULL)
		{
			UARTprintf("Unable to Allocate Memory for Tx Buffer\n\r");
		}
	}
}

static void DeleteTxBuffer(void) {
	if (pTxBuffer) {
		DestroyCircularBuffer(pTxBuffer);
		pTxBuffer = NULL;

#ifdef AUDIO_DEBUG_MESSAGES
		UARTprintf("tx buffer destroyed\n");
		vTaskDelay(5);
#endif
	}
}

static void InitAudio(void) {
	_readMaxDelay = portMAX_DELAY;
	_isCapturing = 0;
	_callCounter = 0;
	_fpLastCommandComplete = NULL;

	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioCaptureMessage_t));
	}

	CreateTxBuffer();
	vTaskDelay(20);

	get_codec_mic_NAU();
	vTaskDelay(20);

	// Initialize the Audio(I2S) Module
	AudioCapturerInit(CPU_XDATA, AUDIO_RATE);
	vTaskDelay(20);

	// Initialize the DMA Module
	UDMAInit();
	vTaskDelay(20);

	UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);
	vTaskDelay(20);

	// Setup the DMA Mode
	SetupPingPongDMATransferTx();
	vTaskDelay(20);


	// Setup the Audio In/Out
	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	vTaskDelay(20);

	AudioCaptureRendererConfigure(I2S_PORT_DMA, AUDIO_RATE);
	vTaskDelay(20);

	// Start Audio Tx/Rx
	Audio_Start();
	vTaskDelay(20);


#ifdef AUDIO_DEBUG_MESSAGES
	UARTprintf("INIT AUDIO\n");
	vTaskDelay(5);
#endif


}

static void DeinitAudio(void) {
	Audio_Stop();
	vTaskDelay(20);

	McASPDeInit();
	vTaskDelay(20);

	DeleteTxBuffer();
	vTaskDelay(20);


#ifdef AUDIO_DEBUG_MESSAGES
	UARTprintf("DEINITIALIZE AUDIO\n");
	vTaskDelay(5);
#endif
}

void AudioCaptureTask_Thread(void * data) {
	int16_t samples[PACKET_SIZE/4];

	WORD bytes = 0;
	WORD bytes_written = 0;
	const WORD bytes_to_write = PACKET_SIZE/4 * sizeof(int16_t);
	const char* file_name = "/POD101";
	FIL file_obj;
	FILINFO file_info;
	memset(&file_obj, 0, sizeof(file_obj));
	FIL* file_ptr = &file_obj;
	uint16_t i;
	uint16_t * pu16;
	unsigned long t0,t1,t2,dt;
	FRESULT res;
	int iBufferFilled = 0;
	uint8_t * ptr_samples_bytes = (uint8_t *)&samples[0];
	const uint8_t * const_ptr_samples_bytes = (const uint8_t *)&samples[0];

#ifdef AUDIO_DEBUG_MESSAGES
	UARTprintf("INITIALIZING AUDIO THREAD\n");
	vTaskDelay(5);
#endif

	vTaskDelay(100);
	//for some reason I must have this here, or AudioFeatures_Init causes a crash.  THIS MAKES NO SENSE.
	InitAudio();
	DeinitAudio();

	//Initialize the audio features
	AudioFeatures_Init(DataCallback);


	for (; ;) {
		AudioCaptureMessage_t  message;
		memset(&message,0,sizeof(message));

		/* Wait until we get a message, UNLESS _readMaxDelay is set to zero (i.e. we are sampling audio) */
		if (xQueueReceive( _queue,(void *) &message, _readMaxDelay )) {


			/* PROCESS COMMANDS */
			switch (message.command) {

			case eAudioCaptureTurnOn:
			{
#ifdef AUDIO_DEBUG_MESSAGES
				UARTprintf("turn on audio\n");
				vTaskDelay(5);
#endif
				InitAudio();
				vTaskDelay(5);

				_readMaxDelay = LOOP_DELAY_WHILE_PROCESSING_IN_TICKS; //ensure that our queue no longer blocks
				_isCapturing = 1;

				break;

			}

			case eAudioCaptureTurnOff:
			{
#ifdef AUDIO_DEBUG_MESSAGES
				UARTprintf("turn off audio\n");
				vTaskDelay(5);
#endif
				DeinitAudio();
				vTaskDelay(5);

				_readMaxDelay = portMAX_DELAY; //make queue block again
				_isCapturing = 0;
				_captureCounter = 0;

				if (file_ptr) {
					f_close(file_ptr);
					memset(&file_obj, 0, sizeof(file_obj));
				}

				break;
			}

			case eAudioCaptureSaveToDisk:
			{
				uint8_t isNewRequest = 0;
				_fpLastCommandComplete = message.fpCommandComplete;
				UARTprintf("saving %d samples of audio....\n",message.captureduration);

				if (_captureCounter == 0) {
					isNewRequest = 1;
				}

				_captureCounter = message.captureduration;

				if (isNewRequest) {
					/*  If we got here, then the file should already be closed */

					/* open file */
					res = f_open(&file_obj, file_name, FA_WRITE|FA_OPEN_ALWAYS);

					/*  Did something horrible happen?  */
					if(res != FR_OK && res != FR_EXIST){
						UARTprintf("File open %s failed: %d\n", file_name, res);
						_captureCounter = 0;
					}
					else {
						//append to file if it already exists
						memset(&file_info, 0, sizeof(file_info));

						f_stat(file_name, &file_info);

						if( file_info.fsize != 0 ){
							res = f_lseek(&file_obj, file_info.fsize);
						}
					}

				}
				break;
			}

			default:
			{
				//do nothing for default case
				break;
			}
			}
		}

		if (_isCapturing) {

			iBufferFilled = GetBufferSize(pTxBuffer);
			//UARTprintf("iBufferFilled %d\r\n" , iBufferFilled);


			if(iBufferFilled >= (sizeof(int16_t)*PACKET_SIZE)) {

				//deinterleave
				for (i = 0; i < PACKET_SIZE/2; i++ ) {
					ptr_samples_bytes[i] = pTxBuffer->pucReadPtr[2*i];
				}

				UpdateReadPtr(pTxBuffer, PACKET_SIZE);


				t1 = xTaskGetTickCount(); dt = t1 - t0; t0 = t1;

#ifdef  SWAP_ENDIAN
				//swap endian, so we output little endian (it comes in as big endian)
				pu16 = (uint16_t *)samples;
				for (i = 0; i < PACKET_SIZE/4; i ++) {
					*pu16 = ((*pu16) << 8) | ((*pu16) >> 8);
					pu16++;
				}
#endif

				/* process audio to get features.  This takes a finite amount of time, by the way. */
				t1 = xTaskGetTickCount();
				//do audio feature processing
				AudioFeatures_SetAudioData(samples,_callCounter++);
				t2 = xTaskGetTickCount();

			  //	UARTprintf("dt = %d, compute=%d\n",dt,t2-t1); //vTaskDelay(5);


				if (_captureCounter > 0) {
					bytes = 0;
					bytes_written = 0;

					/* write until we cannot write anymore.  This does take a finite amount of time, by the way.  */
					do {
						res = f_write(file_ptr, const_ptr_samples_bytes +  bytes_written , bytes_to_write-bytes_written, &bytes );

						bytes_written+=bytes;

						if (res != FR_OK) {
							UARTprintf("Write fail %d\n",res);
							break;
						}

					} while( bytes_written < bytes_to_write );


					_captureCounter--;

#ifdef AUDIO_DEBUG_MESSAGES
					if (_captureCounter % 20 == 0) {
						UARTprintf("%d more frames to capture\n",_captureCounter);
					}
#endif

					if (_captureCounter == 0) {
						f_close(file_ptr);
						memset(&file_obj, 0, sizeof(file_obj));
#ifdef AUDIO_DEBUG_MESSAGES
						UARTprintf("done capturing\n");
#endif
						if (_fpLastCommandComplete) {
							_fpLastCommandComplete();
							_fpLastCommandComplete = NULL;
						}


					}
				}
			}

		}
	}
}


void AudioCaptureTask_AddMessageToQueue(const AudioCaptureMessage_t * message) {
	xQueueSend(_queue,message,0);
}

