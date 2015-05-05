#include "hlo_audio_manager.h"
#include "hlo_audio.h"
#include "kit_assert.h"
#include "circ_buff.h"
#include <string.h>

#define min(a,b) ((a)<(b)?(a):(b))  /**< Find the minimum of 2 numbers. */

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
	mic_client_t * client = (mic_client_t*)ctx;
	size_t remain = GetBufferEmptySize(client->buf);
	int bytes_to_fill = min(remain, size);
	FillBuffer(client->buf, (uint8_t*)buf, bytes_to_fill);
	return bytes_to_fill;
}
static int copy_to_client(void * ctx, void * buf, size_t size){
	mic_client_t * client = (mic_client_t*)ctx;
	size_t filled = GetBufferSize(client->buf);
	if(client->water_mark && filled < client->water_mark){
		return 0;
	}else{
		int bytes_to_fill = min(filled, size);
		if(!buf){
			//This is an optimization hack that skips reading to buffer to save cpu cycle
			//but still increments the read ptr to
			UpdateReadPtr(client->buf, bytes_to_fill);
		}else{
			ReadBuffer(client->buf,(uint8_t*)buf,bytes_to_fill);
		}
		return bytes_to_fill;
	}
}
static int close_client(void * ctx){
	//todo thread safety although processing app shouldn't really close this at all
	mic_client_t * client = (mic_client_t*)ctx;
	DestroyCircularBuffer(client->buf);
	vPortFree(client);
	return 0;
}
hlo_stream_t * hlo_open_mic_stream(size_t buffer_size, size_t opt_water_mark){
	hlo_stream_vftbl_t tbl = {
			.write = copy_from_master,
			.read = copy_to_client,
			.close = close_client,
	};
	xSemaphoreTake(self.mic_client_lock, portMAX_DELAY);
	int i;
	for(i = 0; i < NUM_MAX_MIC_CHANNELS; i++){
		if(!self.mic_clients[i]){
			mic_client_t * client = pvPortMalloc(sizeof(mic_client_t));
			assert(client);
			memset(client, 0, sizeof(mic_client_t));

			client->buf = CreateCircularBuffer(buffer_size);
			assert(client->buf);

			client->water_mark = opt_water_mark;

			self.mic_clients[i] = hlo_stream_new(&tbl,client,HLO_STREAM_READ);
			xSemaphoreGive(self.mic_client_lock);
			return self.mic_clients[i];
		}
	}
	xSemaphoreGive(self.mic_client_lock);
	return NULL;

}
void hlo_audio_manager_thread(void * data){
	uint8_t master_buffer[512];
	while(1){
		//playback task
		//todo impl
		//mic task
		int mic_in_bytes = hlo_stream_read(self.master,master_buffer,sizeof(master_buffer));
		if(mic_in_bytes > 0){
			//dispatch to clients, todo thread safety
			int i;
			for(i = 0; i < NUM_MAX_MIC_CHANNELS; i++){
				if(self.mic_clients[i]){
					int res = hlo_stream_write(self.mic_clients[i], master_buffer, sizeof(master_buffer));
					if(res ==  0){
						//handle buffer overflow
					}else if(res < 0){
						//this shouldn't happen
					}
				}
			}
		}else if(mic_in_bytes < 0){
			LOGE("An error has occured in reading mic data, %d\r\n", mic_in_bytes);
		}
		vTaskDelay(2);
	}
}

