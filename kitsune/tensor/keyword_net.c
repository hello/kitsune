#include "keyword_net.h"
#include "model_may25_lstm_large.c"
#include "tinytensor_features.h"
#include "tinytensor_memory.h"
#include "uartstdio.h"

#include "data\lstm1.c"
#include "data\lstm1_input.c"
#include "data\lstm1_ref.c"
#include "data\lstm3.c"
#include "data\lstm3_ref.c"

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

static void test1(void) {
	Tensor_t * tensor_in;
	Tensor_t * tensor_out;
	const uint32_t * d;
	int n;
	int i;
    uint32_t dims[4];

    ConstLayer_t lstm_layer = tinytensor_create_lstm_layer(&LSTM2_01);

    UARTprintf("---test1---\r\n");
    tensor_in = tinytensor_clone_new_tensor(&lstm1_input_zeros);

    lstm_layer.get_output_dims(lstm_layer.context,dims);

    tensor_out = tinytensor_create_new_tensor(dims);

    lstm_layer.eval(lstm_layer.context,NULL,tensor_out,tensor_in,input_layer);


    d = tensor_out->dims;
    n = d[0] * d[1] * d[2] * d[3];

    for (i = 0; i < n; i++) {

		UARTprintf("%d\r\n",tensor_out->x[i] );

    	if (abs(tensor_out->x[i]) > 1) {
    		UARTprintf("test1 FAIL at iter %d\r\n",i);
    	}
    }

    if (tensor_out->delete_me) {
    	tensor_out->delete_me(tensor_out);
    }

    if (tensor_in->delete_me) {
    	tensor_in->delete_me(tensor_in);
    }

}

static void test2(void) {
	Tensor_t * tensor_in;
	Tensor_t * tensor_out;
	const uint32_t * d;
	int n;
	int i;
	ConstLayer_t lstm_layer = tinytensor_create_lstm_layer(&LSTM2_01);

    UARTprintf("---test2---\r\n");

	tensor_in = tinytensor_clone_new_tensor(&lstm1_input);

	uint32_t dims[4];
	lstm_layer.get_output_dims(lstm_layer.context,dims);

	tensor_out = tinytensor_create_new_tensor(dims);

	lstm_layer.eval(lstm_layer.context,NULL,tensor_out,tensor_in,input_layer);


	d = lstm1_ref.dims;
	n = d[0] * d[1] * d[2] * d[3];

	for (i = 0; i < n; i++) {
		int x1 = tensor_out->x[i] >> tensor_out->scale;
		int x2 = lstm1_ref_x[i] >> lstm1_ref.scale;

		UARTprintf("%d\r\n",x1);

		if (abs(x1 - x2) > 2) {
			UARTprintf("[FAIL] test 2  at iter %d\r\n",i);
		}
	}

	if (tensor_out->delete_me) {
		tensor_out->delete_me(tensor_out);
	}

	if (tensor_in->delete_me) {
		tensor_in->delete_me(tensor_in);
	}
}

void test3(void) {
	Tensor_t * tensor_in;
	Tensor_t * tensor_out;
	const uint32_t * d;
	int n;
	int i;

    tensor_in = tinytensor_clone_new_tensor(&lstm1_input);

    UARTprintf("---test3---\r\n");

    ConstSequentialNetwork_t net = initialize_network03();
    tensor_out = tinytensor_eval_partial_net(&net,tensor_in,3);

    d = lstm3_ref.dims;
    n = d[0] * d[1] * d[2] * d[3];

    for (i = 0; i < n; i++) {
        int x1 = tensor_out->x[i] >> tensor_out->scale;
        int x2 = lstm3_ref_x[i] >> lstm3_ref.scale;

        UARTprintf("%d\n",x1);

        if (abs(x1 - x2) > 2) {
        	UARTprintf("[FAIL] test 3  at iter %d\r\n",i);
        }
    }

    if (tensor_out->delete_me) {
    	tensor_out->delete_me(tensor_out);
    }

    if (tensor_in->delete_me) {
    	tensor_in->delete_me(tensor_in);
    }

}

int cmd_test_neural_net(int argc, char * argv[]) {
	int16_t samples[256];
	int i;

	test1();
	test2();
	test3();

	/*
	memset(samples,0,sizeof(samples));
	keyword_net_initialize();

	keyword_net_register_callback(0,okay_sense,20,0,0);

	UARTprintf("start test\n\n");

	for (i = 0; i < 1024; i++) {
		keyword_net_add_audio_samples(samples,256);
	}

	UARTprintf("\n\nstop test\n\n");

	keyword_net_deinitialize();
	 */
	return 0;

}
