#include "hlo_audio.h"
#include "audiohelper.h"
#include "bigint.h"
#include "circ_buff.h"
#include "network.h"
#include "mcasp_if.h"
extern tCircularBuffer * pRxBuffer;
extern tCircularBuffer * pTxBuffer;
static uint8_t zeroes[2];
extern unsigned int g_uiPlayWaterMark;

////------------------------------
// playback stream driver
static int _close_playback(void * ctx){
	Audio_Stop();
	DeinitAudioPlayback();
}
static int _write_playback_mono(void * ctx, const void * buf, size_t size){
	//incoming data is in mono bytes, need to convert to uint16, then to uint32 with high bytes zeroed
	int empty_bytes = GetBufferEmptySize(pRxBuffer);
	int bytes_to_fill = min(size, empty_bytes);
	int written = 0;
	if(IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE){
		//do not need to fill any more
		return 0;
	}
	while(bytes_to_fill >= 4){
		FillBuffer(pRxBuffer, (uint8_t*)buf + written, 2);
		//FillBuffer(pRxBuffer, (uint8_t*)buf + written, 2);
		FillBuffer(pRxBuffer, zeroes, 2);
		bytes_to_fill -= 4;
		written += 2;
	}
	if (g_uiPlayWaterMark == 0) {
		if (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE) {
			g_uiPlayWaterMark = 1;
		}
	}
	return written;
}
static int _open_playback(uint32_t sr, uint8_t vol){
	if(!InitAudioPlayback(vol, sr)){
		return -1;
	}else{
		g_uiPlayWaterMark = 1;
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
			Audio_Start();
			return hlo_stream_new(&tbl,NULL,HLO_STREAM_WRITE);
		}
	}
	return NULL;
}
