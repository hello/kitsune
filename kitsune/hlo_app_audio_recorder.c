#include "hlo_app_audio_recorder.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"

#define CHUNK 512
void hlo_app_audio_recorder_task(void * data){
	hlo_stream_t * mic = hlo_open_mic_stream(2*CHUNK,CHUNK);
	uint8_t chunk[512];
	assert(mic);
	while(1){
		if( hlo_stream_read(mic,chunk, CHUNK) > 0){
			DISP("Read for write\r\n");
			continue;
		}
		vTaskDelay(3);
	}
}
