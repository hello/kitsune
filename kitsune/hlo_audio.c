#include "hlo_audio.h"
#include "audiohelper.h"
#include "bigint.h"
#include "circ_buff.h"
#include "network.h"
#include "mcasp_if.h"
#include "uart_logger.h"
extern tCircularBuffer * pRxBuffer;
extern tCircularBuffer * pTxBuffer;
#define PADDED_STREAM_SIZE 1024
extern unsigned int g_uiPlayWaterMark;

////------------------------------
// playback stream driver
static int _close_playback(void * ctx){
	vPortFree(ctx);
	Audio_Stop();
	DeinitAudioPlayback();
	return 0;
}
static int _write_playback_mono(void * ctx, const void * buf, size_t size){
	uint16_t * speaker_data_padded = (uint16_t *)ctx;
	uint16_t * buf16 = (uint16_t*)buf;
	int bytes_to_fill = min(PADDED_STREAM_SIZE, size);

	if(IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE){
		return 0;
	}else if(bytes_to_fill %2){
		return HLO_STREAM_ERROR;
	}
	if(bytes_to_fill > 0){
		unsigned int i;
		for (i = 0; i != (bytes_to_fill/2); ++i) {
			//the odd ones are zeroed already
			speaker_data_padded[i<<1] = buf16[i];
		}
		FillBuffer(pRxBuffer, (unsigned char*) (speaker_data_padded), bytes_to_fill * 2);
		if (g_uiPlayWaterMark == 0) {
			if (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE) {
				g_uiPlayWaterMark = 1;
			}
		}
		return bytes_to_fill;
	}

	return 0;
}
static int _open_playback(uint32_t sr, uint8_t vol){
	if(!InitAudioPlayback(vol, sr)){
		return -1;
	}else{
		g_uiPlayWaterMark = 0;
		return 0;
	}
}

////------------------------------
//  Public API
hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint8_t vol, uint32_t direction){
	hlo_stream_vftbl_t tbl = { 0 };
	if(direction == HLO_AUDIO_PLAYBACK){
		tbl.write = _write_playback_mono;
		tbl.close = _close_playback;
		if(0 == _open_playback(sr,vol)){
			uint16_t * speaker_data_padded = pvPortMalloc(PADDED_STREAM_SIZE);
			memset(speaker_data_padded,0,PADDED_STREAM_SIZE);
			Audio_Start();
			return hlo_stream_new(&tbl,speaker_data_padded,HLO_STREAM_WRITE);
		}
	}
	return NULL;
}
