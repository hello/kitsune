#include "voice_net.h"
#include "model_aug15_lstm_vad_dist_vad_tiny_914_ep199.c"


ConstSequentialNetwork_t get_voice_network() {
	return initialize_network();
}
