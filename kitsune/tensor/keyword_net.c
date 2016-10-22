#include "keyword_net.h"
#include "tinytensor_features.h"
#include "tinytensor_memory.h"
#include "tinytensor_math_defs.h"
#include "uart_logger.h"
#include "audio_features_upload_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stdlib.h"

#include "arm_math.h"
#include "arm_const_structs.h"
#include "fft.h"

#define NEURAL_NET_MODEL "model_aug30_lstm_med_dist_okay_sense_stop_snooze_tiny_fa8_1014_ep105.c"
#include NEURAL_NET_MODEL
const static char * k_net_id = NEURAL_NET_MODEL;

static volatile int _is_net_running = 1;

typedef struct {
	KeywordCallback_t on_start;
	KeywordCallback_t on_end;
	void * context;
	int16_t activation_threshold;
	uint32_t active_count;
	int16_t max_value;
	uint32_t min_duration;
} CallbackItem_t;

typedef struct {
    ConstSequentialNetwork_t net;
    SequentialNetworkStates_t state;
    uint8_t keyword_on_states[NUM_KEYWORDS];

    CallbackItem_t callbacks[NUM_KEYWORDS];
    uint32_t counter;

    tinytensor_speech_detector_callback_t speech_callback;
    void * speech_callback_context;

    xSemaphoreHandle stats_mutex;
    NetStats_t stats;

} KeywordNetContext_t;

static KeywordNetContext_t _context;

const static char * k_okay_sense = "okay_sense";
const static char * k_stop = "stop";
const static char * k_snooze = "snooze";
const static char * k_okay = "okay";
const static char * k_unknown = "unkown";

static const char * keyword_enum_to_str(const Keyword_t keyword) {
	switch (keyword) {
	case okay_sense:
		return k_okay_sense;
	case stop:
		return k_stop;
	case snooze:
		return k_snooze;
	case okay:
		return k_okay;
	default:
		return k_unknown;
	}
}

static void speech_detect_callback(void * context, SpeechTransition_t transition) {
	KeywordNetContext_t * p = (KeywordNetContext_t *)context;

	if (p->speech_callback) {
		p->speech_callback(p->speech_callback_context,transition);
	}
}

uint8_t keyword_net_get_and_reset_stats(NetStats_t * stats) {
    //copy out stats, and zero it out
    if( xSemaphoreTake(_context.stats_mutex, ( TickType_t ) 5 ) == pdTRUE )  {
        memcpy(stats,&_context.stats,sizeof(NetStats_t));
        net_stats_reset(&_context.stats);
        xSemaphoreGive(_context.stats_mutex);
        return 1;
    }

    return 0;
}
extern volatile int idlecnt;

__attribute__((section(".ramcode")))
static void feats_callback(void * p, Weight_t * feats) {
	KeywordNetContext_t * context = (KeywordNetContext_t *)p;
	Tensor_t * out;
	Tensor_t temp_tensor;
	int8_t melfeats8[NUM_MEL_BINS];
	uint32_t i;
	int j;
	DECLCHKCYC

	if (!_is_net_running) {
		return;
	}


	audio_features_upload_task_buffer_bytes(melfeats8,NUM_MEL_BINS);

	temp_tensor.dims[0] = 1;
	temp_tensor.dims[1] = 1;
	temp_tensor.dims[2] = 1;
	temp_tensor.dims[3] = NUM_MEL_BINS;

	temp_tensor.x = feats;
	temp_tensor.scale = 0;
	temp_tensor.delete_me = 0;

	CHKCYC(" eval prep");
	out = tinytensor_eval_stateful_net(&context->net, &context->state, &temp_tensor,NET_FLAG_LSTM_DAMPING);
	CHKCYC("evalnet");

	if (context->counter++ < 20) {
		out->delete_me(out);
		return;
	}
	if(context->counter % 50 ==0){
		Weight_t max = out->x[1];
		for (j = 2; j < out->dims[3]; j++) {
			max = max > out->x[j] ? max : out->x[j];
		}

		for(j = 0 ; j < 10; j++){
			if(max >= j * TOFIX(0.1) ){
				DISP("X");
			}else{
				DISP("_");
			}
		}
		DISP("%03d %d\r", out->x[1], idlecnt);

		idlecnt = 0;
	}


    //update stats
    if( xSemaphoreTake(context->stats_mutex, ( TickType_t ) 5 ) == pdTRUE )  {
        
        net_stats_update_counts(&context->stats,out->x);

        xSemaphoreGive(context->stats_mutex);
    }

	//evaluate output
	for (i = 0; i < NUM_KEYWORDS; i++) {
		CallbackItem_t * callback_item = &context->callbacks[i];
		if (callback_item) {
			const int16_t val = (int16_t)out->x[i];

			//track max value of net output for this keyword
			if (callback_item->active_count && val > callback_item->max_value) {
				callback_item->max_value = val;
			}



			//activating?
			if (val >= callback_item->activation_threshold) {

				//did we just start?
				if (callback_item->active_count == 0 && callback_item->on_start) {
					callback_item->on_start(callback_item->context,(Keyword_t)i, val);
				}

				callback_item->active_count++;

				//did we reach the desired number of counts?
				if (callback_item->active_count == callback_item->min_duration && callback_item->on_end) {
					//do callback
					callback_item->on_end(callback_item->context,(Keyword_t)i, callback_item->max_value);
					
					//trigger feats asynchronous upload
					audio_features_upload_trigger_async_upload(NEURAL_NET_MODEL, keyword_enum_to_str((Keyword_t)i),NUM_MEL_BINS,feats_sint8);

					//log activation, has to be thread safe
                    if( xSemaphoreTake(context->stats_mutex, ( TickType_t ) 5 ) == pdTRUE )  {
						net_stats_record_activation(&context->stats,(Keyword_t)i,context->counter);
						xSemaphoreGive(context->stats_mutex);
					}
				}
			}

			//check if we went below threshold, if so, then reset
			if (val < callback_item->activation_threshold && callback_item->active_count) {
				//reset
				callback_item->active_count = 0;
				callback_item->max_value = 0;
			}
		}
	}

	//free
	out->delete_me(out);
	CHKCYC("eval cmplt");
}

void keyword_net_resume_net_operation(void) {
	_is_net_running = 1;
}
void keyword_net_pause_net_operation(void) {
	_is_net_running = 0;
}

__attribute__((section(".ramcode")))
void keyword_net_initialize(void) {
	MEMSET(&_context,0,sizeof(_context));
	_context.stats_mutex = xSemaphoreCreateMutex();

	_context.net = initialize_network();
	net_stats_init(&_context.stats,NUM_KEYWORDS,k_net_id);

    tinytensor_allocate_states(&_context.state, &_context.net);

	tinytensor_features_initialize(&_context,feats_callback, speech_detect_callback);
}

__attribute__((section(".ramcode")))
void keyword_net_deinitialize(void) {
	tinytensor_features_deinitialize();

	tinytensor_free_states(&_context.state,&_context.net);
}

__attribute__((section(".ramcode")))
void keyword_net_register_callback(void * target_context, Keyword_t keyword, int16_t threshold,uint32_t min_duration,KeywordCallback_t on_start, KeywordCallback_t on_end) {

	_context.callbacks[keyword].on_start = on_start;
	_context.callbacks[keyword].on_end = on_end;
	_context.callbacks[keyword].context = target_context;
	_context.callbacks[keyword].activation_threshold = threshold;
	_context.callbacks[keyword].min_duration = min_duration;

}

__attribute__((section(".ramcode")))
void keyword_net_register_speech_callback(void * context, tinytensor_speech_detector_callback_t callback) {
	_context.speech_callback = callback;
	_context.speech_callback_context = context;
}

__attribute__((section(".ramcode")))
void keyword_net_add_audio_samples(const int16_t * samples, uint32_t nsamples) {
	tinytensor_features_add_samples(samples,nsamples);
}

uint32_t __dwt_tot_CYC_cnt;
#if 0
int cmd_test_neural_net2(int argc, char * argv[])
    {

#define L 512
#define SR 8000
#define FREQ 697

	int i,k;
		q31_t fr[512];
		q31_t fi[512];


		q31_t f_int[1024];
		q31_t f_mag[512];

		memset(f_int, 0, sizeof(f_int));
		for (i = 0; i < L; i++) {
			fr[i] = 32768 * (0.5f * sinf(2.0f * PI * FREQ * i / SR));
			fi[i] = 0;
			f_int[2*i+1] = 0;
			f_int[2*i] = 0;
		}
        DISP("%d", fr[0]);
		for (i = 1; i < L; i++) {
			DISP(",%d", fr[i]);
			vTaskDelay(2);
		}DISP("\n");

		 q31_t hanning_window_q31[L];
		for (i = 0; i < L; i++) {
			hanning_window_q31[i] = (q31_t) (0.5f * 2147483647.0f
					* (1.0f - cosf(2.0f*PI*i / L)));
		}
        // Apply the window to the input buffer
        arm_mult_q31(hanning_window_q31, fr, fr, L);

        arm_rfft_instance_q31 fftr_inst;
        arm_rfft_init_q31(&fftr_inst, L, 0, 1);
		STARTCYC
        arm_rfft_q31(&fftr_inst, fr, f_int );
//		arm_cfft_q31( &arm_cfft_sR_q31_len64, f_int, 0, 1 );
	    CHKCYC("ARM FFT");

        arm_cmplx_mag_squared_q31(f_int, f_mag, L);

        DISP("%d,", f_mag[0]);
		for (i = 1; i < L; i++) {
			DISP(",%d", f_mag[0]);
			vTaskDelay(2);
		}DISP("\n");DISP("\n");

		CHKCYC("-");
		fft(fr,fi,9);
		CHKCYC("MY FFT");
	    STOPCYC

        DISP("%d", fr[i]*fr[i]+ fi[i]*fi[i]);
		for (i = 1; i < L; i++) {
			DISP(",%d", fr[i]*fr[i]+ fi[i]*fi[i]);
			vTaskDelay(2);
		}DISP("\n");DISP("\n");
        DISP("%d, %d", fr[i], fi[i]);
		for (i = 1; i < L; i++) {
			DISP(",%d, %d", fr[i], fi[i]);
			vTaskDelay(2);
		}DISP("\n");DISP("\n");
		return 0;
    }
#endif

int cmd_test_neural_net(int argc, char * argv[]) {
	int16_t samples[160];
	int i,k;

#if 1
	uint32_t start = xTaskGetTickCount();
	for (i = 0; i < 160; i++) {
		samples[i] = rand();
	}
	keyword_net_initialize();

	keyword_net_register_callback(0,okay_sense,70,1,0,0);

	DISP("start test\n\n");

	for (k = 0; k < 1024; k++) {
		STARTCYC
		CHKCYC("begin");
		keyword_net_add_audio_samples(samples,160);
		STOPCYC
	}

	DISP("\n\nstop test %d\n\n", xTaskGetTickCount() - start);

	keyword_net_deinitialize();
	return 0;
#endif
}

void keyword_net_reset_states(void) {

}
