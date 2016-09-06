#include "hlo_audio.h"
#include "bigint.h"
#include "circ_buff.h"
#include "network.h"
#include "mcasp_if.h"
#include "uart_logger.h"
#include "kit_assert.h"
#include "hw_types.h"

#include "audiohelper.h"
#include "hw_memmap.h"
#include "rom.h"
#include "rom_map.h"
#include "audio_types.h"
#include "i2c_cmd.h"

extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

#define MODESWITCH_TIMEOUT_MS 500

static xSemaphoreHandle lock;
static hlo_stream_t * master_plbk;
static hlo_stream_t * master_rec;

static uint8_t audio_started = 0;
xSemaphoreHandle record_isr_sem;
xSemaphoreHandle playback_isr_sem;;

static volatile uint32_t last_play;

#define LOCK() xSemaphoreTakeRecursive(lock,portMAX_DELAY)
#define UNLOCK() xSemaphoreGiveRecursive(lock)
extern bool is_playback_active(void);
extern void set_isr_playback(bool active);

////------------------------------
//codec routines
int32_t codec_test_commands(void);
int32_t codec_init(void);
static void _reset_codec(void){
	// Reset codec
	MAP_GPIOPinWrite(GPIOA3_BASE, 0x4, 0);
	vTaskDelay(20);
	MAP_GPIOPinWrite(GPIOA3_BASE, 0x4, 0x4);
	vTaskDelay(30);

	//codec_test_commands();

	// Program codec
	codec_init();
}
////------------------------------
// playback stream driver

static int _open_playback(){
	if(InitAudioPlayback()){
		return -1;
	}
	DISP("Open playback\r\n");
	return 0;

}
int32_t set_volume(int v, unsigned int dly);
extern volatile int sys_volume;

static int _write_playback_mono(void * ctx, const void * buf, size_t size){
	if(IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE){
		if(!is_playback_active()){
			set_isr_playback(true);
		}
		if(!xSemaphoreTake(playback_isr_sem,1000)){
			LOGI("ISR Failed\r\n");
#if 1
			Audio_Stop();
			_reset_codec();
			InitAudioTxRx(AUDIO_SAMPLE_RATE);
			InitAudioPlayback();
			set_volume(sys_volume, portMAX_DELAY);
			Audio_Start();
			return 0;
#else
			return HLO_STREAM_ERROR;
#endif
		}
	}else if(IsBufferVacant(pRxBuffer, PLAY_UNDERRUN_WATERMARK) && is_playback_active()){
		static TickType_t last_warn;
		if(xTaskGetTickCount() - last_warn > 1000){
			LOGW("SPKR BUFFER UNDERRUN\r\n");
			last_warn = xTaskGetTickCount();
		}
		set_isr_playback(false);
		return HLO_STREAM_ERROR;
	}
	last_play = xTaskGetTickCount();
	int written = min(PING_PONG_CHUNK_SIZE, size);
	if(written > 0){
		return FillBuffer(pRxBuffer, (unsigned char*) (buf), written);
	}
	return 0;
}

////------------------------------
//record stream driver
//bool set_mic_gain(int v, unsigned int dly) ;
static int _open_record(){
	if(InitAudioCapture()){
		return -1;
	}
	DISP("Open record\r\n");
	return 0;
}
static int _read_record_mono(void * ctx, void * buf, size_t size){
	if( !IsBufferSizeFilled(pTxBuffer, LISTEN_WATERMARK) ){
		if(!xSemaphoreTake(record_isr_sem,1000)){
			LOGI("ISR Failed\r\n");
#if 1
			Audio_Stop();
			_reset_codec();
			InitAudioTxRx(AUDIO_SAMPLE_RATE);
			InitAudioCapture();
			Audio_Start();
			return 0;
#else
//			mcu_reset();
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
// TODO might need two functions for close of capture and playback?
static int _close(void * ctx){
	return HLO_STREAM_NO_IMPL;
}


////------------------------------
//  Public API
void hlo_audio_init(void){
	lock = xSemaphoreCreateRecursiveMutex();
	assert(lock);
	hlo_stream_vftbl_t tbl = { 0 };

	tbl.close = _close;
	tbl.write = NULL;
	tbl.read = _read_record_mono;
	master_rec = hlo_stream_new(&tbl, NULL, HLO_AUDIO_RECORD);

	tbl.read = NULL;
	tbl.write = _write_playback_mono;
	master_plbk = hlo_stream_new(&tbl, NULL, HLO_AUDIO_PLAYBACK);

	record_isr_sem = xSemaphoreCreateBinary();
	assert(record_isr_sem);
	playback_isr_sem = xSemaphoreCreateBinary();
	assert(playback_isr_sem);

	_reset_codec();

	get_system_volume();

	// McASP and DMA init
	InitAudioTxRx(AUDIO_SAMPLE_RATE);

	InitAudioHelper_p();
	InitAudioHelper();

}

hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint32_t direction){
	hlo_stream_t * ret;
	LOCK();
	if(direction == HLO_AUDIO_PLAYBACK) {
		flush_audio_playback_buffer();
		set_isr_playback(false);
		ret = master_plbk;
	} else if(direction == HLO_AUDIO_RECORD){
		ret = master_rec;
	}else{
		LOGW("Unsupported Audio Mode, returning default stream\r\n");
	}

	if(!audio_started){
		_open_record();
		_open_playback();

		set_volume(0, portMAX_DELAY);
		Audio_Start();
		audio_started = 1;
	}
	UNLOCK();

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
	uint32_t off;
	uint32_t begin;
	bool close_lights;
}light_stream_t;

static void _do_lights(void * ctx, const void * buf, size_t size) {
	light_stream_t * stream = (light_stream_t*)ctx;
	int i;

	int16_t * samples = (int16_t *)buf;
	size /= sizeof(int16_t);

	for(i = 0; i < size; i++){
		stream->eng += abs(samples[i]);

		if( ++stream->ctr > NSAMPLES ) {
			stream->eng = fxd_sqrt(stream->eng/NSAMPLES);

			stream->reduced = 3 * stream->reduced >> 2;
			stream->reduced += abs(stream->eng - stream->last_eng);

			stream->lp += ( stream->reduced - stream->lp ) >> 2;
			//DISP("%d\n", stream->lp) ;

			stream->last_eng = stream->eng;

			uint32_t light = stream->lp + stream->off;

			if(light > 253){
				light = 253;
			}
			if( light < 20 ){
				light = 20;
			}

			uint32_t d =  xTaskGetTickCount() - stream->begin;
			if( d < 150 ) {
				light = (light*d/150);
			}

			set_modulation_intensity( light );
			stream->ctr = 0;
			stream->eng = 0;
		}
	}
 }

static int _write_light(void * ctx, const void * buf, size_t size) {
	light_stream_t * stream = (light_stream_t*)ctx;
	_do_lights(ctx, buf,size);
	return hlo_stream_write(stream->base, buf, size);
}
static int _read_light(void * ctx, void * buf, size_t size){
	light_stream_t * stream = (light_stream_t*)ctx;
	int rv = hlo_stream_read(stream->base, buf, size);
	_do_lights(ctx, buf,size);
	return rv;
}
static int _close_light(void * ctx){
	light_stream_t * stream = (light_stream_t*)ctx;
	if( stream->close_lights ) {
		stop_led_animation( 0, 33 );
	}
	DISP("close light\n") ;
	hlo_stream_close(stream->base);
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_light_stream( hlo_stream_t * base, bool start, uint32_t offset){
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
	stream->off = offset;

	stream->close_lights = true;

	if( start )
	{
		DISP("open light\n") ;
		set_modulation_intensity( 0 );
		play_modulation(140,29,237,30,0);
		stream->close_lights = false;
		stream->begin = xTaskGetTickCount();
	}

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ_WRITE);
}

//------------- 32->16 stream------------------//

#include "hlo_pipe.h"

typedef struct{
	hlo_stream_t * base;
	sr_snv_dir dir;
}sr_cnv_stream_t;


//assumes array s is 2n+1 elements long
static void _upsample( int16_t * s, int n) {
	int i;
	for(i=n-1;i!=-1;--i) {
		s[i*2]   = s[i];// i == 0 ? s[i] : (s[i-1]+s[i])/2;
		s[i*2+1] = s[i];//(s[i]+s[i+1])/2;;
	}
}

//assumes array s is n elements long
static void _downsample( int16_t * s, int n) {
	int i;
	for(i=0;i<n/2;++i) {
		s[i] = ((int32_t)s[i*2]+s[i*2+1])/2;
	}
}

static int _read_sr_cnv(void * ctx, void * buf, size_t size){
	sr_cnv_stream_t * stream = (sr_cnv_stream_t*)ctx;
	int16_t * i16buf = (int16_t*)buf;

	if( stream->dir == DOWNSAMPLE ) {
		if( size == 1 ) {
			size = 2;
		}
		size /=  sizeof(int16_t);
		if( size % 2 ) {
			size += 1;
		}
		int ret = hlo_stream_transfer_all(FROM_STREAM, stream->base, (uint8_t*)buf, 2*size, 4);
		if( ret < 0 ) {
			return ret;
		}

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
hlo_stream_t * hlo_stream_sr_cnv( hlo_stream_t * base, sr_snv_dir dir ){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = NULL,
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
	stream->dir = dir;
	stream->base = base;
	DISP("open cnv\n" ) ;

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ);
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

			stream->reduced = 15 * stream->reduced >> 4;
			stream->reduced += abs(stream->eng - stream->last_eng)<<1;

			stream->lp += ( stream->reduced - stream->lp ) >> 1;
			DISP("%d\t\t\r", stream->eng);

			stream->last_eng = stream->eng;

			if( (stream->ctr_tot > NSAMPLES*100 && stream->lp <= 50)
					|| stream->ctr_tot > NSAMPLES*800 ){
				return HLO_STREAM_EOF;
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
hlo_stream_t * hlo_stream_en( hlo_stream_t * base ){
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
	stream->lp = 200;
	stream->base = base;
	DISP("open en\n") ;

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ_WRITE);
}



//-------------bw stream------------------//

typedef struct{
	hlo_stream_t * base;
	uint32_t start;
	uint32_t startup;
	uint32_t bw;
	int bwr;
	int brd;
}bw_stream_t;

static int _get_bw( bw_stream_t * s, size_t t ) {
	TickType_t tdelta = xTaskGetTickCount() - s->start;
	return ( 1000 * t / tdelta ) / 1024;
}
static int _check_bw(bw_stream_t * s, size_t t, int rv) {
	TickType_t tdelta = xTaskGetTickCount() - s->start;
	if( tdelta > s->startup && _get_bw(s, t) < s->bw ) {
		LOGE("BW too low %d\n", _get_bw(s, t) );
		stop_led_animation( 0, 33 );
		play_led_animation_solid(LED_MAX, LED_MAX, 0, 0, 1,18, 1);
		return HLO_STREAM_EOF;
	}
	return rv;
}

static int _write_bw(void * ctx, const void * buf, size_t size){
	bw_stream_t * stream = (bw_stream_t*)ctx;
	int rv = hlo_stream_write(stream->base, buf, size);
	if( rv > 0 ) { stream->bwr += rv; }
	return _check_bw(stream, stream->bwr, rv);
}
static int _read_bw(void * ctx, void * buf, size_t size){
	bw_stream_t * stream = (bw_stream_t*)ctx;
	int rv = hlo_stream_read(stream->base, buf, size);

	if( rv > 0 ) { stream->brd += rv; }
	return _check_bw(stream, stream->brd, rv);
}
static int _close_bw(void * ctx){
	bw_stream_t * stream = (bw_stream_t*)ctx;
	LOGI("sthr < %d kbps", _get_bw(stream, stream->brd));
	LOGI(" > %d kbps\n", _get_bw(stream, stream->bwr));

	hlo_stream_close(stream->base);
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_stream_bw_limited( hlo_stream_t * base, uint32_t bw, uint32_t startup ){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _write_bw,
		.read = _read_bw,
		.close = _close_bw,
	};
	if( !base ) return NULL;

	bw_stream_t * stream = pvPortMalloc(sizeof(*stream));
	if( !stream ){
		hlo_stream_close(base);
		return NULL;
	}
	memset(stream, 0, sizeof(*stream) );
	stream->start = xTaskGetTickCount();
	stream->bw = bw;
	stream->base = base;
	stream->startup = startup;
	LOGI("open bw\n") ;

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


