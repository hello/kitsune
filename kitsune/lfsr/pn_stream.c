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
#include "pcm_handler.h"


/*
 *
 * DEFINES
 *
 */

/*  TEST CRITERIA  */
#define NUM_SAMPLES_SEPARTED_FAIL_THRESHOLD (6)
#define LOG2_THRESHOLD_Q10_FOR_FAILURE (1024)

/* PN choices  */
#define AEC_CHANNEL (0)
#define NUM_CHANNELS (4)
#define PN_LEN_SAMPLES PN_LEN_10
#define PN_INIT pn_init_with_mask_10
#define PN_AMPLITUDE (2*1024)

#define ABS(x)  ( (x) < 0 ? -(x) : (x) )


//delay just to just before the start of the second sequence
#define PLAY_BUFFER_FILL_TIME_MS (RX_BUFFER_SIZE * 1000 / sizeof(int16_t) / AUDIO_SAMPLE_RATE)
#define CIRCULAR_BUFFER_FILL_TIME_MS (( TX_BUFFER_SIZE / 4 / sizeof(int16_t) * 1000  / AUDIO_SAMPLE_RATE))
#define PN_PERIOD_TIME_MS (PN_LEN_SAMPLES * 1000 / AUDIO_SAMPLE_RATE)
#define NUM_PN_PERIODS_TO_WAIT   (CIRCULAR_BUFFER_FILL_TIME_MS / PN_PERIOD_TIME_MS + PLAY_BUFFER_FILL_TIME_MS / PN_PERIOD_TIME_MS +  1)
#define WRITE_TASK_STARTUP_DELAY  (NUM_PN_PERIODS_TO_WAIT * PN_PERIOD_TIME_MS  +  PLAY_BUFFER_FILL_TIME_MS + 0)

#define NUM_READ_PERIODS (NUM_PN_PERIODS_TO_WAIT + 60)
#define READ_BYTES_MAX (PN_LEN_SAMPLES * NUM_READ_PERIODS * sizeof(int16_t))


#define TIME_TO_COMPLETE_TASKS (4000)

//2x the PN sequence length
#define WRITE_LEN_SAMPLES (2 * PN_LEN_SAMPLES)

//this takes a long time if you don't find a peak
#define IMPULSE_LENGTH         (50)
#define CORR_SEARCH_WINDOW    (PN_LEN_SAMPLES + IMPULSE_LENGTH)
#define CORR_SEARCH_START_IDX (0)
#define DETECTION_THRESHOLD   (2e5)
#define INDEX_DIFFERENCE_FAIL_CRITERIA  (6)
#define NUM_DETECT            (1)

#define WRITE_BUF_SKIP_BYTES (1 * TX_BUFFER_SIZE)

/*
 *
 *  LOCAL TYPE DEFINITIONS
 *
 */
typedef struct {
	hlo_stream_t * input;
	hlo_stream_t * output;
	void * ctx;
	hlo_stream_signal signal;
	uint8_t debug;
	uint8_t test_impulse;
	uint8_t print_correlation;
} hlo_stream_transfer_t;

typedef struct {
	int16_t samples[WRITE_LEN_SAMPLES * NUM_CHANNELS];
	uint32_t idx;
} write_buf_context_t;

typedef struct {
	uint32_t peak_indices[4];
	int32_t peak_values[4];
} TestResult_t;

/*
 *
 * STATIC VARIABLES
 */
static hlo_stream_t * _p_read_stream = NULL;
static hlo_stream_t * _p_write_stream = NULL;

static hlo_stream_vftbl_t _read_tbl;
static hlo_stream_vftbl_t _write_tbl;

static uint32_t _read_bytes_count = 0;
static uint32_t _write_bytes_count = 0;

static int32_t _index_fail_threshold = 0;
static int32_t _threshold_for_starting_peak_detection = 0;

#define REQUIRED_WRITE_STACK_SIZE       (sizeof(write_buf_context_t) + CORR_SEARCH_WINDOW * sizeof(int32_t) + PN_LEN_SAMPLES * sizeof(int16_t) + 10000)


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

	_read_bytes_count += size;

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



	if (_read_bytes_count > READ_BYTES_MAX) {
		_read_bytes_count = 0;
		//DISP("done with read\r\n");
		return HLO_STREAM_EOF;
	}

	//DISP("d\r\n");
	return size;
}

static int write(void * ctx, const void * buf, size_t size) {

	write_buf_context_t * pctx = (write_buf_context_t *) ctx;

	const uint32_t space_left = sizeof(pctx->samples) - sizeof(int16_t) * pctx->idx * NUM_CHANNELS;

	_write_bytes_count += size;

	if (_write_bytes_count < WRITE_BUF_SKIP_BYTES) {
		return size;
	}

	//if what you want to write is greater than what's there, then only write to what's there.
	if (size > space_left) {
		size = space_left;
	}


	//copy over
	memcpy(&pctx->samples[pctx->idx*NUM_CHANNELS],buf,size);
	pctx->idx += size/(NUM_CHANNELS*sizeof(int16_t));

	//if size written is equal to the space that was left, then that was the last copy.
	if (size >= space_left) {
		DISP("completed write to buffer\r\n");
		return HLO_STREAM_EOF;
	}


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

uint8_t correlate(TestResult_t * result,const int16_t * samples, const int16_t * pn_sequence,uint32_t print_correlation,uint32_t channel) {
	int32_t i;
	int32_t corrnumber;

	int32_t istart;
	int32_t iend;
	int32_t current_value;
	int32_t max_value;
	uint32_t corridx;
	uint32_t ifoundrise = 0;
	uint8_t last_sign;
	uint8_t sign;

	int32_t corr_result[CORR_SEARCH_WINDOW];

	int32_t max = INT32_MIN;
	bool found_peak = false;

	for (corridx = 0,corrnumber = CORR_SEARCH_START_IDX; corrnumber < CORR_SEARCH_WINDOW + CORR_SEARCH_START_IDX; corrnumber++,corridx++) {
		int64_t temp64;
		//DO THE CORRELATION
		temp64 = pn_correlate_1x_soft(&samples[corrnumber],pn_sequence,PN_LEN_SAMPLES);

		if (temp64 > (int64_t)INT32_MAX) {
			corr_result[corridx] = INT32_MAX;
		}
		else if (temp64 <  (int64_t)INT32_MIN) {
			corr_result[corridx] = INT32_MIN;
		}
		else {
			corr_result[corridx] = (int32_t)temp64;
		}

		if (ABS(corr_result[corridx]) >= _threshold_for_starting_peak_detection ) {
			if (ABS(corr_result[corridx]) > max ) {
				if (corridx < IMPULSE_LENGTH/2) {
						 //skip ahead
						 corridx = IMPULSE_LENGTH;
						 corrnumber = CORR_SEARCH_START_IDX + IMPULSE_LENGTH;
						continue;
				}
				max = ABS(corr_result[corridx]);

				istart = corridx - 16;
				ifoundrise = corridx;
			}
			found_peak = true;
		}
	}

	if (istart < 0) {
		istart = 0;
	}
	//if no peaks were found, then quit
	if (!found_peak ) {
		DISP("{CHANNEL=%d,TEST_STATUS : FAIL, REASON= NO_PEAK_FOUND}\r\n", channel);
		//return 0;
	}
	//DISP("found above threshold at i=%d\r\n",corrnumber);
	DISP("{CHANNEL=%d,MAX %d, IDX %d}\r\n", channel, max, corridx);

	iend = istart + 256;

	if (iend >= CORR_SEARCH_WINDOW) {
		iend = CORR_SEARCH_WINDOW;
	}

	int print_start = istart;


	//find peak magnitude and index
	max_value = ABS(corr_result[ifoundrise]);
	sign = corr_result[ifoundrise] >= 0;
	last_sign = sign;
	istart = ifoundrise;

	for (i = istart + 1; i < iend; i++) {
		current_value = ABS(corr_result[i]);
		sign = corr_result[i] >= 0;
		if (current_value > max_value) {
			max_value = current_value;
			istart = i;
		}
		if (last_sign != sign) {
			break;
		}

		last_sign = sign;

	}

	//debug print correlation if requested
	if (print_correlation) {

		iend = istart + 64;

		DISP("istart=%d, iend=%d\r\n",print_start,iend);


		for (i = print_start; i < iend; i++) {
			int d = corr_result[i];
			d+=5e6;
			d/=2e5;
			DISP("%d ", d);
			if( d < 0 ) {
				d = 0;
			} else if( d > 50 ) {
				d = 50;
			}
			int s;
			for( s = 0; s < 50; s++ ) {
				if( s == d ) {
					DISP("x");
				} else {
					DISP(" ");
				}
			}

			vTaskDelay(5);
			DISP("\t%d",corr_result[i]);

			if( i == istart ) {
				DISP("\t<--");
			}
			DISP("\n");
		}
		int s;
		for( s = 0; s < 51; ++s ) {
			DISP("=");
		}
		DISP("\n");
	}
	if( istart > CORR_SEARCH_WINDOW ) {
		DISP("{CHANNEL=%d,TEST_STATUS : FAIL, REASON= LATE_PEAK}\r\n", channel);
	}

	result->peak_indices[channel] = istart;
	result->peak_values[channel] = max_value;

	return 1;
}


void pn_write_task( void * params ) {
	uint32_t i;
	write_buf_context_t ctx;
	TickType_t tick_count_start;
	TickType_t tick_count_end;
	int16_t pn_sequence[PN_LEN_SAMPLES];
	int16_t samples[WRITE_LEN_SAMPLES];
	uint32_t ichannel;
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;
	uint8_t ret = 1;
	TestResult_t results;

	memset(&results,0,sizeof(results));

	ctx.idx = 0;
	p->output->ctx = &ctx;
	_write_bytes_count = 0;

	tick_count_start = xTaskGetTickCount();
	DISP("pn_write_task started\r\n");

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
		//DEFAULT
		hlo_filter_data_transfer(p->input,p->output,NULL,NULL);
	}


	tick_count_end = xTaskGetTickCount();

	DISP("got %d bytes in %d ticks, starting correlation...\r\n",_write_bytes_count,tick_count_end - tick_count_start);
	vTaskDelay(20);

	PN_INIT();
	get_pn_sequence(pn_sequence,PN_LEN_SAMPLES);


	for (ichannel = 0; ichannel < NUM_CHANNELS; ichannel++) {



		for (i = 0; i < WRITE_LEN_SAMPLES; i++) {
			samples[i] = ctx.samples[i*NUM_CHANNELS + ichannel];
		}

		ret = correlate(&results,samples,pn_sequence,p->print_correlation,ichannel);

		if (!ret) {
			DISP("channel %d failed to correlate\n",ichannel);
		}

	}

	uint32_t maxidx = 0;
	uint32_t minidx = UINT32_MAX;
	//take care of indices that came from something ahead one period
	for (ichannel = 0; ichannel < NUM_CHANNELS; ichannel++) {
		uint32_t channel_index =  results.peak_indices[ichannel];


		if( results.peak_indices[ichannel] > maxidx ) {
			maxidx = results.peak_indices[ichannel];
		}
		if( results.peak_indices[ichannel] < minidx ) {
			minidx = results.peak_indices[ichannel];
		}


		DISP("{CHANNEL=%d,FIRST_PEAK_ABS_MAGNITUDE : %d, FIRST_PEAK_INDEX : %d}\r\n",ichannel,results.peak_values[ichannel],channel_index);

	}

	if( (maxidx - minidx) % PN_LEN_SAMPLES >= _index_fail_threshold) {
		DISP("FAIL: range of peak indices too large.  Range is %d, fail threshold is %d\r\n", (maxidx - minidx) % PN_LEN_SAMPLES, _index_fail_threshold);
	}

	DISP("pn_write_task completed\r\n");

	vTaskDelete(NULL);
	return;
}

void pn_read_task( void * params ) {
	hlo_stream_transfer_t * p = (hlo_stream_transfer_t *)params;

	hlo_filter_data_transfer(p->input,p->output,p->ctx,NULL);

	vTaskDelete(NULL);
	return;
}

extern void reset_audio();

int cmd_audio_self_test(int argc, char* argv[]) {
	//give me my bits!
	hlo_stream_transfer_t outgoing_path; //out to speaker
	hlo_stream_transfer_t incoming_path; //from mics
	uint8_t debug = 0;
	uint8_t disable_playback = 0;
	uint8_t test_impulse = 0;
	uint8_t print_correlation = 0;

	static bool first = true;
	if( !first ) {
		reset_audio();
	}
	first = false;

	pn_stream_init();

	pcm_set_ping_pong_incoming_stream_mode(PCM_PING_PONG_MODE_ALL_CHANNELS_FULL_RATE);

	_index_fail_threshold = INDEX_DIFFERENCE_FAIL_CRITERIA;
	_threshold_for_starting_peak_detection = DETECTION_THRESHOLD;

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

		if (strcmp(argv[1],"p") == 0) {
			print_correlation = 1;
			DISP("printing correlation\r\n");
		}


		// so if I do "pn d p" I will print my correlation from an external source
		if (argc > 2) {
			if (strcmp(argv[2],"p") == 0) {
				print_correlation = 1;
				DISP("printing correlation\r\n");
			}

			if (strcmp(argv[1],"t") == 0) {
				_threshold_for_starting_peak_detection = atoi(argv[2]);
				DISP("setting detection threshold set to %d\r\n",_threshold_for_starting_peak_detection);

				if (argc > 3) {
					_index_fail_threshold = atoi(argv[3]);
					DISP("index fail threshold set to %d\r\n",_index_fail_threshold);
				}
			}
		}
	}



	//get decision bits not raw audio

	memset(&outgoing_path,0,sizeof(outgoing_path));
	memset(&incoming_path,0,sizeof(incoming_path));


	outgoing_path.input = pn_read_stream_open();
	outgoing_path.output =  hlo_audio_open_mono(AUDIO_SAMPLE_RATE,HLO_AUDIO_PLAYBACK);
	outgoing_path.debug = debug;

	incoming_path.input  = hlo_audio_open_mono(AUDIO_SAMPLE_RATE,HLO_AUDIO_RECORD);

	incoming_path.output  = pn_write_stream_open();
	incoming_path.debug = debug;
	incoming_path.test_impulse = test_impulse;
	incoming_path.print_correlation = print_correlation;

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

	pcm_set_ping_pong_incoming_stream_mode(PCM_PING_PONG_MODE_SINGLE_CHANNEL_HALF_RATE);


	return 0;
}
