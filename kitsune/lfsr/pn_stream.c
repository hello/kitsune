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
#include "hellofilesystem.h"
#include "rom_map.h"
#include "hw_memmap.h"
#include "hellomath.h"
#include "network.h"

static hlo_stream_t * _p_read_stream = NULL;
static hlo_stream_t * _p_write_stream = NULL;

static hlo_stream_vftbl_t _read_tbl;
static hlo_stream_vftbl_t _write_tbl;

static uint32_t _read_bytes_count = 0;
static uint32_t _write_bytes_count = 0;

/*  TEST CRITERIA  */
#define NUM_SAMPLES_SEPARTED_FAIL_THRESHOLD (6)
#define LOG2_THRESHOLD_Q10_FOR_FAILURE (1024)


#define AEC_CHANNEL (0)
#define PN_LEN_SAMPLES PN_LEN_12
#define PN_INIT pn_init_with_mask_12
#define PN_AMPLITUDE (1024)

#define ABS(x)  ( (x) < 0 ? -(x) : (x) )

#define NUM_READ_PERIODS (20)
#define WRITE_BYTES_MAX (PN_LEN_SAMPLES * NUM_READ_PERIODS * sizeof(int16_t))

//delay just to just before the start of the second sequence
#define CIRCULAR_BUFFER_FILL_TIME_MS (( TX_BUFFER_SIZE / 4 / sizeof(int16_t) * 1000  / AUDIO_CAPTURE_PLAYBACK_RATE))
#define PN_PERIOD_TIME_MS (PN_LEN_SAMPLES * 1000 / AUDIO_CAPTURE_PLAYBACK_RATE)
#define NUM_PN_PERIODS_TO_WAIT   (CIRCULAR_BUFFER_FILL_TIME_MS / PN_PERIOD_TIME_MS + 1)
#define WRITE_TASK_STARTUP_DELAY  (NUM_PN_PERIODS_TO_WAIT * PN_PERIOD_TIME_MS  +  20)

#define TIME_TO_COMPLETE_TASKS (10000)

//1.1x the PN sequence length, from bits to bytes
#define WRITE_LEN_SAMPLES (2 * PN_LEN_SAMPLES)

//this takes a long time if you don't find a peak
#define CORR_SEARCH_WINDOW    (PN_LEN_SAMPLES)
#define CORR_SEARCH_START_IDX (0)
#define DETECTION_THRESHOLD   (3e5)
#define NUM_DETECT            (2)

typedef struct {
	hlo_stream_t * input;
	hlo_stream_t * output;
	void * ctx;
	hlo_stream_signal signal;
	uint8_t debug;
	uint8_t test_impulse;
} hlo_stream_transfer_t;

typedef struct {
	int16_t samples[WRITE_LEN_SAMPLES]; //each uint32_t has space for 4 channels
	uint32_t idx;
} write_buf_context_t;



#define REQUIRED_WRITE_STACK_SIZE       (sizeof(write_buf_context_t) + CORR_SEARCH_WINDOW * sizeof(int32_t) + PN_LEN_SAMPLES * sizeof(int16_t) + 3000)


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

	//upsamples at 2x
	for (i = 0; i < size / sizeof(int16_t); i++) {
		bit =  pn_get_next_bit();
		if (bit) {
			p16[i] = PN_AMPLITUDE;
		}
		else {
			p16[i] = -PN_AMPLITUDE;
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

	const uint32_t space_left = sizeof(pctx->samples) - sizeof(int16_t) * pctx->idx;



	//if what you want to write is greater than what's there, then only write to what's there.
	if (size > space_left) {
		size = space_left;
	}


	//copy over
	memcpy(&pctx->samples[pctx->idx],buf,size);
	pctx->idx += size/sizeof(int16_t);

	//if size written is equal to the space that was left, then that was the last copy.
	if (size >= space_left) {
		DISP("completed write to buffer\r\n");
		return HLO_STREAM_EOF;
	}

	_write_bytes_count += size;

	return size;

}


void pn_stream_init(void) {
	memset(&_read_tbl,0,sizeof(_read_tbl));
	memset(&_write_tbl,0,sizeof(_write_tbl));

	DISP("pn_stream_init\r\n");
	vTaskDelay(10);
	_read_tbl.close = close;
	_read_tbl.read = read;

	_write_tbl.close = close;
	_write_tbl.write = write;

	_p_read_stream = hlo_stream_new(&_read_tbl,NULL,HLO_STREAM_READ);

	_p_write_stream = hlo_stream_new(&_write_tbl,NULL,HLO_STREAM_WRITE);

	PN_INIT();

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


void pn_write_task( void * params ) {
	int32_t j,i;
	int32_t corrnumber;

	uint32_t ndetect = 0;
	uint8_t found_peak = 0;

	int32_t istart;
	int32_t iend;

	write_buf_context_t ctx;
	TickType_t tick_count_start;
	TickType_t tick_count_end;
	int16_t pn_sequence[PN_LEN_SAMPLES];
	int32_t corr_result[CORR_SEARCH_WINDOW];
	uint32_t corridx;
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;


	ctx.idx = 0;



	p->output->ctx = &ctx;
	tick_count_start = xTaskGetTickCount();

	_write_bytes_count = 0;

	DISP("pn_write_task started\r\n");

	//hlo_audio_set_channel(3);

	if (p->debug) {
		hlo_stream_t * filestream = fs_stream_open("DBGBYTE.DAT",HLO_STREAM_WRITE);
		hlo_filter_data_transfer(p->input,filestream,NULL,NULL);
	}
	else if (p->test_impulse) {
		uint32_t i;
		write_buf_context_t * pctx = (write_buf_context_t *)p->output->ctx;

		PN_INIT();
		for (i = 0; i < PN_LEN_SAMPLES/2; i++) {
			pn_get_next_bit();
		}

		for (i = 0; i < WRITE_LEN_SAMPLES; i++) {
			if (pn_get_next_bit()) {
				pctx->samples[i] = 1;
			}
			else {
				pctx->samples[i] = -1;
			}
		}

		_write_bytes_count += WRITE_LEN_SAMPLES * sizeof(int16_t);
	}
	else {
		hlo_filter_data_transfer(p->input,p->output,NULL,p->signal);
	}


	tick_count_end = xTaskGetTickCount();

	DISP("got %d bytes in %d ticks, starting correlation...\r\n",_write_bytes_count,tick_count_end - tick_count_start);
	vTaskDelay(20);

	PN_INIT();
	get_pn_sequence(pn_sequence,PN_LEN_SAMPLES);

	corridx = 0;
	for (corrnumber = CORR_SEARCH_START_IDX; corrnumber < CORR_SEARCH_WINDOW + CORR_SEARCH_START_IDX; corrnumber++,corridx++) {

		//DO THE CORRELATION
		corr_result[corridx] = pn_correlate_1x_soft(&ctx.samples[corrnumber],pn_sequence,PN_LEN_SAMPLES);

		if (p->test_impulse && ABS(corr_result[corridx]) > 500) {
			DISP("%d\r\n",corr_result[corridx]);
		}


		if (corr_result[corridx] >= DETECTION_THRESHOLD && !found_peak) {

			ndetect++;

			if (ndetect >= NUM_DETECT) {
				DISP("FOUND PEAK AT c=%d\r\n",corrnumber);
				istart = corridx - 16;
				found_peak = 1;
			}
		}
		else {
			ndetect = 0;
		}

		if (found_peak && corridx >= istart + 256) {
			DISP("exiting correlation at c=%d\r\n",corridx);
			break;
		}


		MAP_WatchdogIntClear(WDT_BASE);
	}

	if (istart < 0) {
		istart = 0;
	}


	if (!found_peak) {
		DISP("NO PEAKS FOUND\r\n");
		vTaskDelete(NULL);
		return;
	}

	iend = istart + 256;

	if (iend >= CORR_SEARCH_WINDOW) {
		iend = CORR_SEARCH_WINDOW;
	}

	DISP("istart=%d, iend=%d\r\n");

	for (i = istart; i < iend; i++) {
		DISP("%d\r\n",corr_result[i]);
		vTaskDelay(5);

	}
	DISP("\r\n");



/*
	for (j = 0; j < 4; j++) {
		if (j == AEC_CHANNEL) {
			continue;
		}

		for (i = j + 1; i < 4; i++) {
			if (i == AEC_CHANNEL) {
				continue;
			}

#define NOISE_FLOOR (300)
			a1 = ABS(sums_history[j][max_indices[j]]);
			a1 -= NOISE_FLOOR;
			a1 = a1 < NOISE_FLOOR ? NOISE_FLOOR : a1;
			a1 = FixedPointLog2Q10(a1);

			a2 = ABS(sums_history[i][max_indices[i]]);
			a2 -= NOISE_FLOOR;
			a2 = a2 < NOISE_FLOOR ? NOISE_FLOOR : a2;
			a2 = FixedPointLog2Q10(a2);

			logdiff = ABS(a2-a1);
			DISP("{chA=%d,chB=%d,log2A=%d,log2B=%d,idxA=%d,idxB=%d}\r\n",j,i,a1,a2,max_indices[j],max_indices[i]);
			if (  logdiff > LOG2_THRESHOLD_Q10_FOR_FAILURE  ) {
				DISP("SELF-TEST FAILED: LOG2 AMPLITUDE MISMATCH=%d between channels %d and %d\r\n",ABS(a1-a2),j,i);
				fail = 1;
			}

			idx = ABS(max_indices[j] - max_indices[i]);

			if (idx > NUM_SAMPLES_SEPARTED_FAIL_THRESHOLD) {
				DISP("SELF-TEST FAILED: INDICES TOO FAR APART at %d counts between chanels %d and %d\r\n",idx,j,i);
				fail = 1;
			}
		}
	}

	if (!fail) {
		DISP("TEST SUCCESS\r\n");
	}

	//make sure the peaks are within three samples (3cm or so) of each other
	*/
	DISP("pn_write_task completed\r\n");

	vTaskDelete(NULL);
	return;
}

void pn_read_task( void * params ) {
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	DISP("pn_read_task started\r\n");

	hlo_filter_data_transfer(p->input,p->output,p->ctx,p->signal);

	DISP("pn_read_task completed\r\n");

	vTaskDelete(NULL);
	return;
}

int cmd_audio_self_test(int argc, char* argv[]) {
	//give me my bits!
	hlo_stream_transfer_t outgoing_path; //out to speaker
	hlo_stream_transfer_t incoming_path; //from mics
	uint8_t debug = 0;
	uint8_t disable_playback = 0;
	uint8_t test_impulse = 0;
	pn_stream_init();

	if (argc > 1)  {
		if (strcmp(argv[1],"d") == 0) {
			debug = 1;
			DISP("setting debug\r\n");
		}

		if (strcmp(argv[1],"n") == 0) {
			disable_playback = 1;
			DISP("setting no playaback\r\n");
		}

		if (strcmp(argv[1],"i") == 0) {
			disable_playback = 1;
			test_impulse = 1;
			DISP("testing impulse\r\n");
		}
	}



	//get decision bits not raw audio

	memset(&outgoing_path,0,sizeof(outgoing_path));
	memset(&incoming_path,0,sizeof(incoming_path));


	outgoing_path.input = pn_read_stream_open();
	outgoing_path.output =  hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_PLAYBACK);
	outgoing_path.debug = debug;

	incoming_path.input  = hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE,HLO_AUDIO_RECORD);

	incoming_path.output  = pn_write_stream_open();
	incoming_path.debug = debug;
	incoming_path.test_impulse = test_impulse;

	set_volume(64, portMAX_DELAY);

	if (!disable_playback) {
		DISP("launching pn_read_task\r\n");
		if (xTaskCreate(pn_read_task, "pn_read_task", 1024 * 4 / 4, &outgoing_path, 9, NULL) == NULL) {
			DISP("problem creating pn_read_task\r\n");
		}
	}

	DISP("waiting %d ms\r\n",WRITE_TASK_STARTUP_DELAY);
	vTaskDelay(WRITE_TASK_STARTUP_DELAY);

	DISP("launching pn_write_task, stack=%d bytes\r\n",REQUIRED_WRITE_STACK_SIZE);

	if (xTaskCreate(pn_write_task, "pn_write_task", REQUIRED_WRITE_STACK_SIZE / 4, &incoming_path, 5, NULL) == NULL) {
		DISP("problem creating pn_write_task\r\n");
	}

	vTaskDelay(TIME_TO_COMPLETE_TASKS);


	return 0;
}
