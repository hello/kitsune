#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "pn_stream.h"
#include "pn.h"
#include "audio_types.h"
#include "hlo_audio.h"
#include "hlo_audio_tools.h"
#include <string.h>
#include <stdint.h>
#include "uart_logger.h"
#include "i2c_cmd.h"

static hlo_stream_t * _pstream;
static hlo_stream_vftbl_t _tbl;
static uint32_t _read_bytes_count = 0;
static volatile uint8_t stop_flag;
static uint32_t _write_bytes_count = 0;

#define WRITE_BYTES_MAX (32768 * 3)

typedef struct {
	hlo_stream_t * input;
	hlo_stream_t * output;
	void * ctx;
	hlo_stream_signal signal;
} hlo_stream_transfer_t;

typedef struct {
	uint32_t buf[4096 * 3];
	uint32_t idx;
} write_buf_context_t;



static int close (void * context) {
	/*
	DISP("closing pn stream\r\n");
	vTaskDelay(10);
	if (_pstream) {

		DISP("doing the close\r\n");
		vTaskDelay(10);
		hlo_stream_close(_pstream);
	}

	_pstream = NULL;

	DISP("done closing pn stream\r\n");
	vTaskDelay(10);
	*/
	return HLO_STREAM_NO_IMPL;
}

static int read(void * ctx, void * buf, size_t size) {
	//gets bit from the PN sequence
	uint32_t i;
	uint8_t bit;
	int16_t * p16 = (int16_t *)buf;

	for (i = 0; i < size / sizeof(int16_t); i++) {
		bit =  pn_get_next_bit();
		if (bit) {
			p16[i] = 1024;
		}
		else {
			p16[i] = -1024;
		}
	}


	_read_bytes_count += size;

	if (_read_bytes_count > WRITE_BYTES_MAX) {
		_read_bytes_count = 0;
		//DISP("done with read\r\n");
		return HLO_STREAM_EOF;
	}

	//DISP("d\r\n");
	return size;
}

static int write(void * ctx, const void * buf, size_t size) {
	write_buf_context_t * pctx = (write_buf_context_t *) ctx;
	uint32_t space_left = sizeof(pctx->buf) - sizeof(uint32_t) * pctx->idx;
	if (size % 4 != 0) {
		return HLO_STREAM_ERROR;
	}

	//if what you want to write is greater than what's there, then only write to what's there.
	if (size > space_left) {
		size = space_left;
	}

	//copy over
	memcpy(&pctx->buf[pctx->idx],buf,size);
	pctx->idx += size/sizeof(uint32_t);

	//if size written is equal to the space that was left, then that was the last copy.
	if (size >= space_left) {
		return HLO_STREAM_EOF;
	}

	return size;

}


void pn_stream_init(void) {
	memset(&_tbl,0,sizeof(_tbl));

	DISP("init!\r\n");
	_tbl.close = close;
	_tbl.read = read;
	_tbl.write = write;

	_pstream = hlo_stream_new(&_tbl,NULL,HLO_STREAM_READ_WRITE);


	pn_init_with_mask_12();

	_read_bytes_count = 0;
}

hlo_stream_t * pn_stream_open(void){
	if (!_pstream) {
		pn_stream_init();
	}

	return _pstream;
}


static uint8_t stop_signal(void) {
	return stop_flag;
}




void pn_write_task( void * params ) {
	write_buf_context_t ctx;

	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	//git decision bits not raw audio
	hlo_audio_set_read_type(quad_decision_bits_from_quad);

	hlo_filter_data_transfer(p->input,p->output,&ctx,p->signal);

	//set back to normal
	hlo_audio_set_read_type(mono_from_quad_by_channel);

	//TODO CORRELATE STUFF HERE

	pn_init_with_mask_12();

	//pn_correlate_with_xor()

	vTaskDelete(NULL);
	return;
}

void pn_read_task( void * params ) {
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	hlo_filter_data_transfer(p->input,p->output,p->ctx,p->signal);

	stop_flag = 1;

	vTaskDelete(NULL);
	return;
}

int cmd_audio_self_test(int argc, char* argv[]) {
	//give me my bits!
	hlo_stream_transfer_t outgoing_path; //out to speaker
	hlo_stream_transfer_t incoming_path; //from mics

	memset(&outgoing_path,0,sizeof(outgoing_path));
	memset(&incoming_path,0,sizeof(incoming_path));

	outgoing_path.input = pn_stream_open();
	outgoing_path.output =  hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_PLAYBACK);
	set_volume(64, portMAX_DELAY);

	incoming_path.input  = hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_RECORD);
	incoming_path.output  = NULL; //TODO implement this
	incoming_path.signal = stop_signal;

	stop_flag = 0;

	DISP("launching task\r\n");
	if (xTaskCreate(pn_read_task, "pn_read_task", 1024 * 4 / 4, &outgoing_path, 5, NULL) == NULL) {
		DISP("problem creating pn_read_task\r\n");
	}


	if (xTaskCreate(pn_write_task, "pn_write_task", 20000 / 4, &incoming_path, 4, NULL) == NULL) {
		DISP("problem creating pn_write_task\r\n");
	}


	//go back to to normal
	return 0;
}
