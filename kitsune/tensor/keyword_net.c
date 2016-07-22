#include "keyword_net.h"
#include "model_may25_lstm_small_okay_sense_tiny.c"
#include "tinytensor_features.h"
#include "tinytensor_memory.h"
#include "uartstdio.h"

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

} KeywordNetContext_t;

static KeywordNetContext_t _context;


static void feats_callback(void * p, int8_t * feats) {
	KeywordNetContext_t * context = (KeywordNetContext_t *)p;
	Tensor_t * out;
	Tensor_t temp_tensor;
	uint32_t i;

	temp_tensor.dims[0] = 1;
	temp_tensor.dims[1] = 1;
	temp_tensor.dims[2] = 1;
	temp_tensor.dims[3] = NUM_MEL_BINS;

	temp_tensor.x = feats;
	temp_tensor.scale = 0;
	temp_tensor.delete_me = 0;


	out = tinytensor_eval_stateful_net(&context->net, &context->state, &temp_tensor);

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



}

void keyword_net_initialize(void) {
	MEMSET(&_context,0,sizeof(_context));

	_context.net = initialize_network();

    tinytensor_allocate_states(&_context.state, &_context.net);

	tinytensor_features_initialize(&_context,feats_callback);
}

void keyword_net_deinitialize(void) {
	tinytensor_features_deinitialize();

	tinytensor_free_states(&_context.state,&_context.net);
}

void keyword_net_register_callback(void * target_context, Keyword_t keyword, int8_t threshold,KeywordCallback_t on_start, KeywordCallback_t on_end) {

	_context.callbacks[keyword].on_start = on_start;
	_context.callbacks[keyword].on_end = on_end;
	_context.callbacks[keyword].context = target_context;
	_context.callbacks[keyword].activation_threshold = threshold;

}

void keyword_net_add_audio_samples(const int16_t * samples, uint32_t nsamples) {
	tinytensor_features_add_samples(samples,nsamples);
}

#include "FreeRTOS.h"
#include "task.h"

int cmd_test_neural_net(int argc, char * argv[]) {
	int16_t samples[256];
	uint32_t start = xTaskGetTickCount();
	int i;

	memset(samples,0,sizeof(samples));
	keyword_net_initialize();

	keyword_net_register_callback(0,okay_sense,20,0,0);

	UARTprintf("start test\n\n");

	for (i = 0; i < 1024; i++) {
		keyword_net_add_audio_samples(samples,256);
	}

	UARTprintf("\n\nstop test %d\n\n", xTaskGetTickCount() - start);

	keyword_net_deinitialize();

	return 0;

}
