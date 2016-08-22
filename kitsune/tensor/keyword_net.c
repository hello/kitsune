#include "keyword_net.h"
#include "model_may25_lstm_small_okay_sense_tiny_727.c"
#include "tinytensor_features.h"
#include "tinytensor_memory.h"
#include "uart_logger.h"


#include "FreeRTOS.h"
#include "task.h"
#include "stdlib.h"

#include "arm_math.h"
#include "arm_const_structs.h"
#include "fft.h"

typedef struct {
	KeywordCallback_t on_start;
	KeywordCallback_t on_end;
	void * context;
	int8_t activation_threshold;
	uint8_t is_active;
	int8_t max_value;
} CallbackItem_t;

typedef struct {
    ConstSequentialNetwork_t net;
    SequentialNetworkStates_t state;
    uint8_t keyword_on_states[NUM_KEYWORDS];

    CallbackItem_t callbacks[NUM_KEYWORDS];
    uint32_t counter;

} KeywordNetContext_t;

static KeywordNetContext_t _context;

__attribute__((section(".ramcode")))
static void feats_callback(void * p, Weight_t * feats) {
	KeywordNetContext_t * context = (KeywordNetContext_t *)p;
	Tensor_t * out;
	Tensor_t temp_tensor;
	uint32_t i;
	int j;
	DECLCHKCYC

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
			if(max >= j * 12 ){
				DISP("X");
			}else{
				DISP("_");
			}
		}
		DISP("%03d\r", out->x[1]);
	}


	//evaluate output
	for (i = 0; i < NUM_KEYWORDS; i++) {
		CallbackItem_t * callback_item = &context->callbacks[i];
		if (callback_item) {
			const int8_t val = (int8_t)out->x[i];

			//track max value of net output for this keyword
			if (callback_item->is_active && val > callback_item->max_value) {
				callback_item->max_value = val;
			}
			else {
				callback_item->max_value = INT8_MIN;
			}


			//activating
			if (val >= callback_item->activation_threshold && !callback_item->is_active) {
				callback_item->is_active = 1;

				if (callback_item->on_start) {
					callback_item->on_start(callback_item->context,(Keyword_t)i, val);
				}
			}

			//deactivating
			if (val < callback_item->activation_threshold && callback_item->is_active) {
				callback_item->is_active = 0;

				//report max value
				if (callback_item->on_end) {
					callback_item->on_end(callback_item->context,(Keyword_t)i,callback_item->max_value);
				}

			}
		}
	}

	//free
	out->delete_me(out);
	CHKCYC("eval cmplt");
}

__attribute__((section(".ramcode")))
void keyword_net_initialize(void) {
	MEMSET(&_context,0,sizeof(_context));

	_context.net = initialize_network();

    tinytensor_allocate_states(&_context.state, &_context.net);

	tinytensor_features_initialize(&_context,feats_callback, NULL);
}

__attribute__((section(".ramcode")))
void keyword_net_deinitialize(void) {
	tinytensor_features_deinitialize();

	tinytensor_free_states(&_context.state,&_context.net);
}

__attribute__((section(".ramcode")))
void keyword_net_register_callback(void * target_context, Keyword_t keyword, int8_t threshold,KeywordCallback_t on_start, KeywordCallback_t on_end) {

	_context.callbacks[keyword].on_start = on_start;
	_context.callbacks[keyword].on_end = on_end;
	_context.callbacks[keyword].context = target_context;
	_context.callbacks[keyword].activation_threshold = threshold;

}
__attribute__((section(".ramcode")))
void keyword_net_add_audio_samples(const int16_t * samples, uint32_t nsamples) {
	tinytensor_features_add_samples(samples,nsamples);
}

uint32_t __dwt_tot_CYC_cnt;
#if 1
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

	keyword_net_register_callback(0,okay_sense,70,0,0);

	DISP("start test\n\n");

	for (k = 0; k < 10; k++) {
		STARTCYC
		CHKCYC("begin");
		keyword_net_add_audio_samples(samples,160);
		STOPCYC

		for (i = 0; i < 160; i++) {
			samples[i] = rand();
		}
	}

	DISP("\n\nstop test %d\n\n", xTaskGetTickCount() - start);

	keyword_net_deinitialize();
	return 0;
#endif
}
