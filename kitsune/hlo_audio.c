#include "hlo_audio.h"
#include "audiohelper.h"
#include "bigint.h"
#include "circ_buff.h"
#include "network.h"
#include "mcasp_if.h"
#include "uart_logger.h"


extern tCircularBuffer * pRxBuffer;
extern tCircularBuffer * pTxBuffer;

typedef struct{

}hlo_audio_t;

////------------------------------
// playback stream driver
static int _close_playback(void * ctx){
	Audio_Stop();
	DeinitAudioPlayback();
	return 0;
}
static int _write_playback_mono(void * ctx, const void * buf, size_t size){
	int bytes_consumed = min(512, size);
	if(IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE){
		return 0;
	}else if(bytes_consumed %2){
		return HLO_STREAM_ERROR;
	}
	if(bytes_consumed > 0){
		bytes_consumed = FillBuffer(pRxBuffer, (unsigned char*) (buf), bytes_consumed);
		return bytes_consumed;
	}
	return 0;
}
static int _open_playback(uint32_t sr, uint8_t vol){
	if(!InitAudioPlayback(vol, sr)){
		return -1;
	}else{
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
			Audio_Start();
			return hlo_stream_new(&tbl,NULL,HLO_STREAM_WRITE);
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
			Audio_Start();
			return hlo_stream_new(&tbl,NULL,HLO_STREAM_READ|HLO_STREAM_WRITE);
		}else{
			DISP("1");
		}
	}
	return NULL;
}
