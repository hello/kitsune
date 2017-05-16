#include "snoring_net.h"

#include "snorec2weights.c"

ConstSequentialNetwork_t get_snoring_network() {
	return initialize_network();
}

