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

static hlo_stream_t * _p_read_stream;
static hlo_stream_t * _p_write_stream;

static hlo_stream_vftbl_t _read_tbl;
static hlo_stream_vftbl_t _write_tbl;

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
	uint32_t buf[2048 * 8];
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
			p16[i] = 2048;
		}
		else {
			p16[i] = -2048;
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

	const uint32_t space_left = sizeof(pctx->buf) - sizeof(uint32_t) * pctx->idx;

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
	_write_bytes_count += size;

	if (size >= space_left) {
		DISP("pn_write_task write complete\r\n");
		return HLO_STREAM_EOF;
	}

	return size;

}


void pn_stream_init(void) {
	memset(&_read_tbl,0,sizeof(_read_tbl));
	memset(&_write_tbl,0,sizeof(_write_tbl));

	DISP("init!\r\n");
	_read_tbl.close = close;
	_read_tbl.read = read;

	_write_tbl.close = close;
	_write_tbl.write = write;

	_p_read_stream = hlo_stream_new(&_read_tbl,NULL,HLO_STREAM_READ);

	_p_write_stream = hlo_stream_new(&_read_tbl,NULL,HLO_STREAM_WRITE);

	pn_init_with_mask_12();

	_read_bytes_count = 0;
	_write_bytes_count = 0;
}

hlo_stream_t * pn_read_stream_open(void){
	if (!_p_read_stream) {
		pn_stream_init();
	}

	return _p_read_stream;
}

hlo_stream_t * pn_write_stream_open(void){
	if (!_p_write_stream) {
		pn_stream_init();
	}

	return _p_write_stream;
}

static uint8_t stop_signal(void) {
	return stop_flag;
}



#define PN_LEN_12             ((1<<12) - 1)
#define CORR_LEN              ((PN_LEN_12 / 8) * 4)
#define CORR_SEARCH_WINDOW    (PN_LEN_12 / 4)
#define CORR_SEARCH_START_IDX (0)
#define DETECTION_THRESHOLD   (1000)

void pn_write_task( void * params ) {

	int i;
	int j;
	int maxindices[4];
	int maxidx;
	int corrnumber;
	uint8_t found_peak = 0;
	uint8_t the_byte = 0x00;
	int32_t max;
	write_buf_context_t ctx;
	int32_t corr[CORR_SEARCH_WINDOW][4];

	ctx.idx = 0;

	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	_write_bytes_count = 0;

	DISP("pn_write_task started\r\n");

	//git decision bits not raw audio

	hlo_filter_data_transfer(p->input,p->output,&ctx,p->signal);

	//set back to normal
	hlo_audio_set_read_type(mono_from_quad_by_channel);



	DISP("got %d bytes, starting correlation...\r\n",_write_bytes_count);
	vTaskDelay(1000);

	for (corrnumber = 0; corrnumber < CORR_SEARCH_WINDOW; corrnumber++) {
		pn_init_with_mask_12();
		int16_t sums[4][8];
		the_byte = 0x00;

		memset(&sums[0][0],0,sizeof(sums));

		//DO THE CORRELATION
		for (i = 0; i < CORR_LEN; i++) {
			pn_correlate_4x(ctx.buf[i + corrnumber],sums,&the_byte);
		}

		//CHECK THE SUMS TO SEE IF THERE'S ANY PEAKS IN THERE
		for (j = 0; j < 4; j++) {
			for (i = 7; i >= 0; i--) {
				if (sums[0][i] >= DETECTION_THRESHOLD) {
					found_peak = 1;
					break;
				}
			}
		}

		if (found_peak) {
			DISP("FOUND PEAK AT c=%d\r\n",corrnumber);
			break;
		}

	}

	if (!found_peak) {
		DISP("NO PEAKS FOUND\r\n");
	}


	/*
	for (j = 0; j < 4; j++) {
		max = 0;
		for (i = 0; i < CORR_SEARCH_WINDOW; i++) {
			if (corr[i][0] > max) {
				max = corr[i][0];
				maxidx = i;
			}

			if (corr[i][0] < -max) {
				max = -corr[i][0];
				maxidx = i;
			}
		}

		maxindices[j] = maxidx;
		DISP("maxidx=%d\r\n",maxidx);
	}


	for (i = -10; i < 64; i++ ) {
		DISP("%d\r\n",corr[i+maxindices[0]][0]);
	}
	*/
	DISP("pn_write_task completed\r\n");

	vTaskDelete(NULL);
	return;
}

void pn_read_task( void * params ) {
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	DISP("pn_read_task started\r\n");


	hlo_filter_data_transfer(p->input,p->output,p->ctx,p->signal);

	stop_flag = 1;

	DISP("pn_read_task completed\r\n");

	vTaskDelete(NULL);
	return;
}

int cmd_audio_self_test(int argc, char* argv[]) {
	//give me my bits!
	hlo_stream_transfer_t outgoing_path; //out to speaker
	hlo_stream_transfer_t incoming_path; //from mics

	memset(&outgoing_path,0,sizeof(outgoing_path));
	memset(&incoming_path,0,sizeof(incoming_path));

	hlo_audio_set_read_type(quad_decision_bits_from_quad);

	outgoing_path.input = pn_read_stream_open();
	outgoing_path.output =  hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_PLAYBACK);

	incoming_path.input  = hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_RECORD);
	incoming_path.output  = pn_write_stream_open;

	set_volume(64, portMAX_DELAY);

	//incoming_path.signal = stop_signal;


	stop_flag = 0;

	DISP("launching pn_read_task\r\n");
	if (xTaskCreate(pn_read_task, "pn_read_task", 1024 * 4 / 4, &outgoing_path, 5, NULL) == NULL) {
		DISP("problem creating pn_read_task\r\n");
	}

	vTaskDelay(124); //don't change this

	DISP("launching pn_write_task\r\n");

	if (xTaskCreate(pn_write_task, "pn_write_task", 60000 / 4, &incoming_path, 4, NULL) == NULL) {
		DISP("problem creating pn_write_task\r\n");
	}


	//go back to to normal
	return 0;
}
