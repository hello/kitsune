#include "hlo_audio_manager.h"
#include "hlo_audio.h"
#include "kit_assert.h"
#include "circ_buff.h"

typedef struct{
	tCircularBuffer * buf;
	size_t water_mark;
}mic_client_t;

static struct{
	hlo_stream_t * master;
	xSemaphoreHandle mic_client_lock;	//for open close and write operations
	hlo_stream_t * mic_clients[NUM_MAX_MIC_CHANNELS];
}self;

void hlo_audio_manager_init(void){
	self.master = hlo_audio_open_mono(48000,44,HLO_AUDIO_RECORD);
	vSemaphoreCreateBinary(self.mic_client_lock);
	assert(self.master);
	assert(self.mic_client_lock);

}
static int copy_from_master(void * ctx, const void * buf, size_t size){
	return 0;
}
static int copy_to_client(void * ctx, void * buf, size_t size){
	return 0;
}
static int close_client(void * ctx){
	return 0;
}
hlo_stream_t * hlo_open_mic_stream(size_t buffer_size, size_t opt_water_mark){
	hlo_stream_vftbl_t tbl = {
			.write = copy_from_master,
			.read = copy_to_client,
			.close = close_client,
	};
	return NULL;
}
void hlo_audio_manager_thread(void * data){
	while(1){

	}
}

