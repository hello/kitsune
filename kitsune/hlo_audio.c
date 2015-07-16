#include "hlo_audio.h"
#include "audiohelper.h"
#include "bigint.h"
#include "circ_buff.h"
#include "network.h"
#include "mcasp_if.h"
#include "uart_logger.h"

/* do not change PADDED_STREAM_SIZE without checking what the watermark size is, otherwise it'll overflow */
#define PADDED_PLAYBACK_STREAM_SIZE 1024

extern unsigned int g_uiPlayWaterMark;
extern tCircularBuffer * pRxBuffer;
extern tCircularBuffer * pTxBuffer;

typedef struct{

}hlo_audio_t;

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
	int bytes_to_fill = min((PADDED_PLAYBACK_STREAM_SIZE/2), size);//we can only fill half of the padded stream

	if(IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE){
		//TODO remove this once i understand how that fifo lib works, seems to bug out when kept full vs dma interrupt
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
//record stream driver
static int _close_record(void * ctx){
	Audio_Stop();
	DeinitAudioCapture();
	return 0;
}
//assumes to have at least 4 bytes
static inline int16_t i2s_to_i16(const uint8_t * buf){
	int16_t * buf16 = (int16_t *)buf;
	return ((buf16[1] << 8) | (buf16[1] >> 8));
}
static int _read_record_mono(void * ctx, void * buf, size_t size){
	int raw_buff_size =  GetBufferSize(pTxBuffer);
	uint8_t tmp[4] = {0};
	if(raw_buff_size < 1*PING_PONG_CHUNK_SIZE) {
		//todo remove and completely empty buffer on demand
		return 0;
	}else{
		//need to discard half of the buffer
		int bytes_to_process = min(size * 2, raw_buff_size);
		int16_t * buf16 = (int16_t*)buf;
		while(bytes_to_process >= 4){
			ReadBuffer(pTxBuffer, tmp, 4);
			*buf16 = i2s_to_i16(tmp);
			buf16++;
			bytes_to_process -= 4;
		}
		return ((uint8_t*)buf16 - (uint8_t*)buf);
	}
}
static int _open_record(uint32_t sr){
	if(!InitAudioCapture(sr)){
		return -1;
	}
	return 0;
}
////------------------------------
//duplex stream driver
static int _open_duplex(uint32_t sr){
	if(InitAudioDuplex(48000)){
		return 0;
	}
	return 1;
}
static int _close_duplex(void * ctx){
	DeInitAudioDuplex();
	return 0;
}
////------------------------------
//  Public API
hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint8_t vol, uint32_t direction){
	hlo_stream_vftbl_t tbl = { 0 };
	if(direction == HLO_AUDIO_PLAYBACK){
		tbl.write = _write_playback_mono;
		tbl.close = _close_playback;
		if(0 == _open_playback(sr,vol)){
			uint16_t * speaker_data_padded = pvPortMalloc(PADDED_PLAYBACK_STREAM_SIZE);
			memset(speaker_data_padded,0,PADDED_PLAYBACK_STREAM_SIZE);
			Audio_Start();
			return hlo_stream_new(&tbl,speaker_data_padded,HLO_STREAM_WRITE);
		}
	}else if(direction == HLO_AUDIO_RECORD){
		tbl.read = _read_record_mono;
		tbl.close = _close_record;
		if(0 == _open_record(sr)){
			Audio_Start();
			return hlo_stream_new(&tbl,NULL,HLO_STREAM_READ);
		}
	}else if(direction == HLO_AUDIO_RECORD|HLO_AUDIO_PLAYBACK){
		tbl.read = _read_record_mono;
		tbl.write = _write_playback_mono;
		tbl.close = _close_duplex;
		if(0 == _open_duplex(sr)){
			uint16_t * speaker_data_padded = pvPortMalloc(PADDED_PLAYBACK_STREAM_SIZE);
			memset(speaker_data_padded,0,PADDED_PLAYBACK_STREAM_SIZE);
			if(speaker_data_padded){
				Audio_Start();
				return hlo_stream_new(&tbl,speaker_data_padded,HLO_STREAM_READ|HLO_STREAM_WRITE);
			}else{
				_close_duplex(NULL);
				DISP("2");
				return NULL;
			}
		}else{
			DISP("1");
		}
	}
	return NULL;
}
