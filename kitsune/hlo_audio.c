#include "hlo_audio.h"
#include "bigint.h"
#include "circ_buff.h"
#include "network.h"
#include "mcasp_if.h"
#include "uart_logger.h"
#include "kit_assert.h"
#include "hw_types.h"

#include "audiohelper.h"

extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

#define MODESWITCH_TIMEOUT_MS 500

static xSemaphoreHandle lock;
static hlo_stream_t * master;

static unsigned long record_sr;
static unsigned long playback_sr;
static unsigned int initial_vol;
static unsigned int initial_gain;
static uint8_t audio_playback_reference=0;
static uint8_t audio_record_started=0;
xSemaphoreHandle record_isr_sem;
xSemaphoreHandle playback_isr_sem;;

#define LOCK() xSemaphoreTakeRecursive(lock,portMAX_DELAY)
#define UNLOCK() xSemaphoreGiveRecursive(lock)

////------------------------------
// playback stream driver

static int _open_playback(uint32_t sr, uint8_t vol){
	if(InitAudioPlayback(vol, sr)){
		return -1;
	}
	DISP("Open playback\r\n");
	return 0;

}

/*
static int _reinit_playback(unsigned int sr, unsigned int initial_vol){
	DeinitAudioPlayback();
	_open_playback(sr, initial_vol);
	return 0;
}
*/

static int _write_playback_mono(void * ctx, const void * buf, size_t size){
	if(IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE){
/*		if(audio_playback_started){ */
			if(!xSemaphoreTake(playback_isr_sem,5000)){
				LOGI("ISR Failed\r\n");
#if 0
				return _reinit_playback(playback_sr, initial_vol);
#else
				return HLO_STREAM_ERROR;
#endif
			}
/*		}else{
			audio_playback_started = 1;
			LOGI("Init playback\n");
			ret = _reinit_playback(playback_sr, initial_vol);
			if(ret) return ret;
			Audio_Start();
			return 0;
		}*/
	}
	int written = min(PING_PONG_CHUNK_SIZE, size);
	if(written > 0){
		return FillBuffer(pRxBuffer, (unsigned char*) (buf), written);
	}
	return 0;
}

////------------------------------
//record stream driver
//bool set_mic_gain(int v, unsigned int dly) ;
static int _open_record(uint32_t sr, uint32_t gain){
	if(InitAudioCapture(sr)){
		return -1;
	}
	DISP("Open record\r\n");
	return 0;
}
/*
static int _reinit_record(unsigned int sr, unsigned int vol){
	DeinitAudioCapture();
	_open_record(sr, initial_gain?initial_gain:16);
	return 0;
}*/
static int _read_record_mono(void * ctx, void * buf, size_t size){

	/*
	int ret;

	if(!audio_record_started){
		audio_record_started = 1;
		ret = _reinit_record(record_sr, initial_gain);
		Audio_Start();
		if(ret) return ret;
	}
	*/

	if( !IsBufferSizeFilled(pTxBuffer, LISTEN_WATERMARK) ){
		if(!xSemaphoreTake(record_isr_sem,5000)){
			LOGI("ISR Failed\r\n");
#if 0
			return _reinit_record(record_sr, initial_gain);
#else
			return HLO_STREAM_ERROR;
#endif
		}
	}
	int read = min(PING_PONG_CHUNK_SIZE, size);
	if(read > 0){
		return ReadBuffer(pTxBuffer, buf, read);
	}
	return 0;
}
static int16_t _select_channel(int16_t * samples, uint8_t ch){
	return samples[ch];
}
static int16_t _quad_to_mono(int16_t * samples){
	/*
	 * Word Order
	 * Left1	Left2	Right1	Right2
	 * Loopback	MIC1	MIC2	MIC3
	 *
	 */
	//naive approach to pick the strongest number between samples
	//probably causes a lot of distortion
	int n1 = abs(samples[1]);
	int n2 = abs(samples[2]);
	int n3 = abs(samples[3]);
	if( n1 >= n2 ){
		if(n1 >= n3){
			return samples[1];
		}else{
			return samples[3];
		}
	}else{
		if(n2 >= n3){
			return  samples[2];
		}else{
			return samples[3];
		}
	}
}
static int16_t _ez_lpf(int16_t now, int16_t prev){
	return (int16_t)(((int32_t)now + prev)/2);
}
int ch = 2;
static int _read_record_quad_to_mono(void * ctx, void * buf, size_t size){
	int i;
	static int16_t last;
	if(size % 2){//buffer must be in multiple of 2 bytes
		LOGE("audio buffer alignment error\r\n");
		return HLO_STREAM_ERROR;
	}
	int16_t * iter = (int16_t*)buf;
	for(i = 0; i < size/2; i++){
		uint8_t samples[2 * 4];
		int ret = _read_record_mono(ctx, samples, sizeof(samples));
		if(ret <= 0){
			return ret;
		}else if(ret != sizeof(samples)){
			return HLO_STREAM_ERROR;
		}
	//	*iter = _ez_lpf(_quad_to_mono((int16_t*)samples), last);
	//	*iter = _ez_lpf(_select_channel((int16_t*)samples, 3), last);
	//	*iter = _select_channel((int16_t*)samples, 3);
	//	*iter = _quad_to_mono((int16_t*)samples);
		*iter = _select_channel((int16_t*)samples, ch);
		last = *iter;
		iter++;
	}
	return (int)size;
}
// TODO might need two functions for close of capture and playback?
static int _close(void * ctx){
	DISP("Closing stream\n");

#if 0
	Audio_Stop();

	DeinitAudioPlayback();
	DeinitAudioCapture();


	audio_record_started = 0;
	audio_playback_started = 0;
#endif

	return HLO_STREAM_NO_IMPL;
}

////------------------------------
//  Public API
void hlo_audio_init(void){
	lock = xSemaphoreCreateRecursiveMutex();
	assert(lock);
	hlo_stream_vftbl_t tbl = { 0 };
	tbl.write = _write_playback_mono;
#if 0
	tbl.read = _read_record_mono;			//for 1p0 when return channel is mono
#else
	tbl.read = _read_record_quad_to_mono;	//for 1p5 when return channel is quad
#endif
	tbl.close = _close;
	master = hlo_stream_new(&tbl, NULL, HLO_AUDIO_RECORD|HLO_AUDIO_PLAYBACK);
	record_isr_sem = xSemaphoreCreateBinary();
	assert(record_isr_sem);
	playback_isr_sem = xSemaphoreCreateBinary();
	assert(playback_isr_sem);



}

bool set_volume(int v, unsigned int dly);
hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint8_t vol, uint32_t direction){
	hlo_stream_t * ret = master;
	LOCK();
	if(direction == HLO_AUDIO_PLAYBACK){
		playback_sr = sr;
		initial_vol = vol;
		if( !audio_playback_reference ) {
			_open_playback(playback_sr,0);
			audio_playback_reference  += 1;	//todo reference count playback stream to stop audio tx interrupt
			set_volume(vol, portMAX_DELAY);
		}
	}else if(direction == HLO_AUDIO_RECORD){
		record_sr = sr;
		initial_gain = vol;
		if(!audio_record_started){
			_open_record(playback_sr,0);
			audio_record_started = 1;
		}
	}else{
		LOGW("Unsupported Audio Mode, returning default stream\r\n");
	}
	UNLOCK();
	Audio_Start();
	return ret;
}

//------------------light stream-------------------//

#include "led_animations.h"
#include "hellomath.h"
#define NSAMPLES 512
typedef struct{
	hlo_stream_t * base;
	int32_t reduced;
	int32_t lp;
	int32_t last_eng;
	int32_t eng;
	int32_t ctr;
}light_stream_t;

static int _write_light(void * ctx, const void * buf, size_t size){
	light_stream_t * stream = (light_stream_t*)ctx;
	int rv = hlo_stream_write(stream->base, buf, size);
	return rv;
}
static int _read_light(void * ctx, void * buf, size_t size){
	light_stream_t * stream = (light_stream_t*)ctx;
	int rv = hlo_stream_read(stream->base, buf, size);
	int i;

	int16_t * samples = (int16_t *)buf;
	size /= sizeof(int16_t);

	for(i = 0; i < size; i++){
		stream->eng += abs(samples[i]);

		if( ++stream->ctr > NSAMPLES ) {
			stream->eng = fxd_sqrt(stream->eng/NSAMPLES);

			stream->reduced = 3 * stream->reduced >> 2;
			stream->reduced += abs(stream->eng - stream->last_eng)<<1;

			stream->lp += ( stream->reduced - stream->lp ) >> 3;
			//DISP("%d\n", stream->lp) ;

			stream->last_eng = stream->eng;

			if(stream->lp > 253){
				stream->lp = 253;
			}
			if( stream->lp < 20 ){
				stream->lp = 20;
			}
			set_modulation_intensity( stream->lp );
			stream->ctr = 0;
			stream->eng = 0;
		}
	}
	return rv;
}
static int _close_light(void * ctx){
	light_stream_t * stream = (light_stream_t*)ctx;
	DISP("close light\n") ;

	stop_led_animation( 0, 33 );
	hlo_stream_close(stream->base);
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_light_stream( hlo_stream_t * base){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _write_light,
		.read = _read_light,
		.close = _close_light,
	};
	if( !base ) return NULL;

	light_stream_t * stream = pvPortMalloc(sizeof(*stream));
	if( !stream ){
		hlo_stream_close(base);
		return NULL;
	}
	memset(stream, 0, sizeof(*stream) );
	stream->base = base;

	DISP("open light\n") ;
	play_modulation(253,253,253,30,0);

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ_WRITE);
}

//------------- 32->16 stream------------------//

#include "hlo_pipe.h"

typedef struct{
	hlo_stream_t * base;
	bool flip;
}sr_cnv_stream_t;


//assumes array s is 2n elements long
static void _upsample( int16_t * s, int n) {
	int i;
	for(i=n-1;i!=-1;--i) {
		s[i*2] = s[i];
		s[i*2+1] = s[i];
	}
}

//assumes array s is n elements long
static void _downsample( int16_t * s, int n) {
	int i;
	for(i=0;i<n/2;++i) {
		s[i] = ((int32_t)s[i*2]+s[i*2+1])/2;
	}
}

//write goes 32->16Khz so like so `x $a $i$c` (flip reverses so 16->32  `x $i $a$c` )
static int _write_sr_cnv(void * ctx, const void * buf, size_t size){
	sr_cnv_stream_t * stream = (sr_cnv_stream_t*)ctx;
	int16_t * i16buf = (int16_t*)buf;

	if( stream->flip ) {
		int isize = size / sizeof(int16_t);
		_upsample(i16buf, isize/2); //discard half...

		int ret = hlo_stream_transfer_all(INTO_STREAM, stream->base, (uint8_t*)buf, size, 4);
		if( ret < 0 ) return ret/2;
	} else {
		int isize = size / sizeof(int16_t);
		_downsample(i16buf, isize);
		//DISP("cnv writing %d\n", size/2 ) ;
		int ret = hlo_stream_transfer_all(INTO_STREAM, stream->base, (uint8_t*)i16buf, size/2, 4);
		if( ret > 0 ) {
			return ret*2;
		}
		return ret;
	}
}

//read goes 16->32khz so like so `x $i$c $a` (or with flip 32->16Khz `x $a$c $i` )
static int _read_sr_cnv(void * ctx, void * buf, size_t size){
	sr_cnv_stream_t * stream = (sr_cnv_stream_t*)ctx;
	int16_t * i16buf = (int16_t*)buf;

	if( stream->flip ) {
		if( size == 1 ) {
			return 1;
		}
		if( size % 2 ) {
			size -=1;
		}
		int ret = hlo_stream_transfer_all(FROM_STREAM, stream->base, (uint8_t*)buf, size, 4);
		if( ret < 0 ) return ret;

		int isize = ret / sizeof(int16_t);
		_downsample(i16buf, isize);
		return ret/2;
	} else {
		//read half
		int ret = hlo_stream_transfer_all(FROM_STREAM, stream->base, (uint8_t*)buf, size/2, 4);
		if( ret < 0 ) return ret;

		int isize = ret / sizeof(int16_t);
		_upsample(i16buf, isize);
		return 2*ret;
	}
}
static int _close_sr_cnv(void * ctx){
	sr_cnv_stream_t * stream = (sr_cnv_stream_t*)ctx;

	hlo_stream_close(stream->base);
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_stream_sr_cnv( hlo_stream_t * base, bool flip ){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _write_sr_cnv,
		.read = _read_sr_cnv,
		.close = _close_sr_cnv,
	};
	if( !base ) return NULL;

	sr_cnv_stream_t * stream = pvPortMalloc(sizeof(*stream));
	if( !stream ){
		hlo_stream_close(base);
		return NULL;
	}
	memset(stream, 0, sizeof(*stream) );
	stream->flip = flip;
	stream->base = base;
	DISP("open cnv\n" ) ;

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ_WRITE);
}


//-------------energy stream------------------//

#define NSAMPLES 512
typedef struct{
	hlo_stream_t * base;
	int32_t reduced;
	int32_t lp;
	int32_t last_eng;
	int32_t eng;
	int32_t ctr;
	int32_t ctr_tot;
	bool * brk;
}energy_stream_t;

static int _write_energy(void * ctx, const void * buf, size_t size){
	energy_stream_t * stream = (energy_stream_t*)ctx;
	int rv = hlo_stream_write(stream->base, buf, size);
	return rv;
}
static int _read_energy(void * ctx, void * buf, size_t size){
	energy_stream_t * stream = (energy_stream_t*)ctx;
	int rv = hlo_stream_read(stream->base, buf, size);
	int i;

	int16_t * samples = (int16_t *)buf;
	size /= sizeof(int16_t);

	for(i = 0; i < size; i++){
		stream->eng += abs(samples[i]);


		++stream->ctr_tot;

		if( ++stream->ctr > NSAMPLES ) {
			stream->eng = fxd_sqrt(stream->eng/NSAMPLES);

			stream->reduced = 7 * stream->reduced >> 3;
			stream->reduced += abs(stream->eng - stream->last_eng)<<1;

			stream->lp += ( stream->reduced - stream->lp ) >> 3;
			DISP("%d\t\t\r", stream->eng);

			stream->last_eng = stream->eng;

			if( stream->brk &&
					((stream->ctr_tot > NSAMPLES*100
							&& stream->lp <= 100)
							||stream->ctr_tot > NSAMPLES*800)  ){
				//DISP("\n") ;
				*stream->brk = true;
			}
			stream->ctr = 0;
			stream->eng = 0;
		}
	}
	return rv;
}
static int _close_energy(void * ctx){
	energy_stream_t * stream = (energy_stream_t*)ctx;
	DISP("close energy\n") ;

	hlo_stream_close(stream->base);
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_stream_en( hlo_stream_t * base, bool * brk ){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _write_energy,
		.read = _read_energy,
		.close = _close_energy,
	};
	if( !base ) return NULL;

	energy_stream_t * stream = pvPortMalloc(sizeof(*stream));
	if( !stream ){
		hlo_stream_close(base);
		return NULL;
	}
	memset(stream, 0, sizeof(*stream) );
	stream->base = base;
	stream->brk = brk;
	DISP("open en\n") ;

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ_WRITE);
}



//-------------tunes stream------------------//

typedef struct{
	int t;
}tunes_stream_t;

static int _read_tunes(void * ctx, void * buf, size_t size){
	tunes_stream_t * s = (tunes_stream_t*)ctx;
	int i;
	int16_t * samples = (int16_t *)buf;
	size /= sizeof(int16_t);

	for(i = 0; i < size; i++){
		samples[i] = (int16_t)(s->t*(((s->t>>24)|(s->t>>18))&(2047&(s->t>>7))));
		++s->t;
	}
	return size *  sizeof(int16_t);
}
static int _close_tunes(void * ctx){
	tunes_stream_t * stream = (tunes_stream_t*)ctx;
	DISP("close energy\n") ;
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_stream_tunes(){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = NULL,
		.read = _read_tunes,
		.close = _close_tunes,
	};
	tunes_stream_t * stream = pvPortMalloc(sizeof(*stream));
	if( !stream ){
		return NULL;
	}
	memset(stream, 0, sizeof(*stream) );
	return hlo_stream_new(&functions, stream, HLO_STREAM_READ);
}


