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

static hlo_stream_t * _pstream;
static hlo_stream_vftbl_t _tbl;
static xSemaphoreHandle _wait_semaphore = NULL;
static uint32_t _write_bytes_count = 0;

#define WRITE_BYTES_MAX (32768 * 2)

static int close (void * context) {
	if (_pstream) {
		hlo_stream_close(_pstream);
	}

	_pstream = NULL;

	return 0;
}

static int read(void * ctx, void * buf, size_t size) {
	//gets bit from the PN sequence
	uint32_t i;
	uint8_t bit;
	int16_t * p16 = (int16_t *)buf;
	for (i = 0; i < size / sizeof(int16_t); i++) {
		bit =  pn_get_next_bit();
		if (bit) {
			p16[i] = 4096;
		}
		else {
			p16[i] = -4096;
		}
	}

	_write_bytes_count += size;

	if (_write_bytes_count > WRITE_BYTES_MAX) {
		_write_bytes_count = 0;
		return HLO_STREAM_EOF;
	}

	return size;
}

void pn_stream_init(void) {
	memset(&_tbl,0,sizeof(_tbl));

	_tbl.close = close;
	_tbl.read = read;

	_pstream = hlo_stream_new(&_tbl,NULL,HLO_STREAM_READ);


	pn_init_with_mask_12();

	_wait_semaphore = xSemaphoreCreateBinary();
	_write_bytes_count = 0;
}

hlo_stream_t * pn_stream_open(void){
	if (!_pstream) {
		pn_stream_init();
	}

	return _pstream;
}


static uint8_t stop_signal(void) {
	return 0;
}


typedef struct {
	hlo_stream_t * input;
	hlo_stream_t * output;
	void * ctx;
	hlo_stream_signal signal;
} hlo_stream_transfer_t;

void pn_write_task( void * params ) {
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	hlo_filter_data_transfer(p->input,p->output,p->ctx,p->signal);

	xSemaphoreGive(_wait_semaphore);
}

void pn_read_task( void * params ) {
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	hlo_filter_data_transfer(p->input,p->output,p->ctx,p->signal);
}

int cmd_audio_self_test(int argc, char* argv[]) {
	//give me my bits!
	hlo_stream_transfer_t outgoing_path; //out to speaker
	hlo_stream_transfer_t incoming_path; //from mics

	hlo_filter xfer_to_speaker = hlo_filter_data_transfer;
	hlo_filter xfer_from_mics  = hlo_filter_data_transfer;

	hlo_audio_set_read_type(quad_decision_bits_from_quad);

	outgoing_path.input = pn_stream_open();
	outgoing_path.output =  hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_PLAYBACK);

	incoming_path.input  = hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_RECORD);
	incoming_path.output  = NULL; //TODO implement this


	xSemaphoreTake(_wait_semaphore,portMAX_DELAY);


	if (xTaskCreate(pn_read_task, "pn_out_task", 1024 / 4, &outgoing_path, 3, NULL) == NULL) {

	}

	//wait for 10 seconds or until done
	if (!xSemaphoreTake(_wait_semaphore,10000)) {
		xSemaphoreGive(_wait_semaphore);
	}


	/*
	if (xTaskCreate(pn_write_task, "pn_in_task", 1024 / 4, &incoming_path, 4, NULL) == NULL) {

	}
	*/

	//AND... pause here until done


	//go back to to normal
	hlo_audio_set_read_type(mono_from_quad_by_channel);
	return 0;
}
