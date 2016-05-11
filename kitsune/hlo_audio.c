#include "hlo_audio.h"
#include "bigint.h"
#include "circ_buff.h"
#include "network.h"
#include "mcasp_if.h"
#include "uart_logger.h"
#include "kit_assert.h"
#include "hw_types.h"

extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

#define MODESWITCH_TIMEOUT_MS 500

static xSemaphoreHandle lock;
static hlo_stream_t * master;
static unsigned char * audio_mem;
typedef enum{
	CLOSED = 0,
	PLAYBACK,
	RECORD,
}codec_state_t;
static codec_state_t mode;
static unsigned long record_sr;
static unsigned long playback_sr;
static unsigned int initial_vol;
static uint8_t audio_playback_started;
xSemaphoreHandle isr_sem;;

static TickType_t last_playback_time;

#define LOCK() xSemaphoreTakeRecursive(lock,portMAX_DELAY)
#define UNLOCK() xSemaphoreGiveRecursive(lock)
#define ERROR_IF_CLOSED() if(mode == CLOSED) {return HLO_STREAM_ERROR;}
////------------------------------
// Audio Helpers
#include "pcm_handler.h"
#include "udma.h"
#include "i2s.h"
#include "i2c_cmd.h"
#include "rom.h"
#include "rom_map.h"
#include "hw_memmap.h"
#include "udma_if.h"
static uint8_t InitAudioCapture(uint32_t rate) {

	if(pTxBuffer == NULL) {
		pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE, audio_mem);
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

	return 1;
}

static uint8_t InitAudioPlayback(int32_t vol, uint32_t rate ) {

	//create circular buffer
	if (!pRxBuffer) {
		pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE, audio_mem);
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
static void DeinitAudio(void){
	Audio_Stop();
	McASPDeInit();
	MAP_uDMAChannelDisable(UDMA_CH4_I2S_RX);
	MAP_uDMAChannelDisable(UDMA_CH5_I2S_TX);
	if (pTxBuffer) {
		DestroyCircularBuffer(pTxBuffer);
		pTxBuffer = NULL;
	}
	if (pRxBuffer) {
		DestroyCircularBuffer(pRxBuffer);
		pRxBuffer = NULL;
	}
	audio_playback_started = 0;
	mode = CLOSED;
}
////------------------------------
// playback stream driver
static int _playback_done(void){
	return (int)( (xTaskGetTickCount() - last_playback_time) > MODESWITCH_TIMEOUT_MS);
}
static int _open_playback(uint32_t sr, uint8_t vol){
	if(!InitAudioPlayback(vol, sr)){
		return -1;
	}else{
		mode = PLAYBACK;
		DISP("Codec in SPKR mode\r\n");
		return 0;
	}
}
static int _reinit_playback(sr, initial_vol){
	DeinitAudio();
	_open_playback(sr, initial_vol);
	return 0;
}
static int _write_playback_mono(void * ctx, const void * buf, size_t size){
	ERROR_IF_CLOSED();
	last_playback_time = xTaskGetTickCount();
	if( mode == RECORD ){
		//playback has priority, always swap to playback state
		return _reinit_playback(playback_sr, initial_vol);
	}
	int bytes_consumed = min(PING_PONG_CHUNK_SIZE, size);
	if(IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE){
		if(!audio_playback_started){
			audio_playback_started = 1;
			Audio_Start();
		}
		return 0;
	}else if(bytes_consumed %2){
		bytes_consumed--;//if there's an odd number of empty space available, adjust it to be even.
	}
	if(bytes_consumed > 0){
		bytes_consumed = FillBuffer(pRxBuffer, (unsigned char*) (buf), bytes_consumed);
		return bytes_consumed;
	}else if( !xSemaphoreTake(isr_sem,5000) ){
		return _reinit_playback(playback_sr, initial_vol);
	}
	return 0;
}

////------------------------------
//record stream driver
static int _open_record(uint32_t sr){
	if(!InitAudioCapture(sr)){
		return -1;
	}
	mode = RECORD;
	Audio_Start();
	DISP("Codec in MIC mode\r\n");
	return 0;
}
static int _reinit_record(sr){
	DeinitAudio();
	_open_record(sr);
	return 0;
}
static int _read_record_mono(void * ctx, void * buf, size_t size){
	ERROR_IF_CLOSED();
	if( mode == PLAYBACK ){
		//swap mode back to record iff playback buffer is empty
		if( _playback_done() ){
			return _reinit_record(record_sr);
		}else{
			return HLO_STREAM_EOF;
		}
	}else if( !xSemaphoreTake(isr_sem,5000) ){
		return _reinit_record(record_sr);
	}
	int raw_buff_size =  GetBufferSize(pTxBuffer);
	int fill_size = min(raw_buff_size, size);
	if(raw_buff_size < 1*PING_PONG_CHUNK_SIZE) {
		//todo remove and completely empty buffer on demand
		return 0;
	}else{
		//need to discard half of the buffer
		ReadBuffer(pTxBuffer, buf, fill_size);
		return fill_size;
	}
}
////------------------------------
//  Public API
void hlo_audio_init(void){
	lock = xSemaphoreCreateRecursiveMutex();
	assert(lock);
	hlo_stream_vftbl_t tbl = { 0 };
	tbl.write = _write_playback_mono;
	tbl.read = _read_record_mono;
	tbl.close = NULL;
	master = hlo_stream_new(&tbl, NULL, HLO_AUDIO_RECORD|HLO_AUDIO_PLAYBACK);
	mode = CLOSED;
	audio_mem = pvPortMalloc(AUD_BUFFER_SIZE);
	assert(audio_mem);
	isr_sem = xSemaphoreCreateBinary();
	assert(isr_sem);
}

hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint8_t vol, uint32_t direction){
	hlo_stream_t * ret = master;
	LOCK();
	if(direction == HLO_AUDIO_PLAYBACK){
		playback_sr = sr;
		initial_vol = vol;
	}else if(direction == HLO_AUDIO_RECORD){
		record_sr = sr;
	}else{
		LOGW("Unsupported Audio Mode, returning default stream\r\n");
	}
	if(mode == CLOSED){
		if( 0 != _open_record(sr) ){
			ret = NULL;
		}
	}
	UNLOCK();
	return ret;
}
