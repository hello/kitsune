#include "hlo_app_audio_recorder.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"

static uint8_t recording;

#define CHUNK 512
void hlo_app_audio_recorder_task(void * data){
	hlo_stream_t * mic = hlo_open_mic_stream(2*CHUNK,CHUNK);
	uint8_t chunk[512];
	assert(mic);
	while(1){
		if(recording){
			if( hlo_stream_read(mic,chunk, CHUNK) > 0){
				DISP("rec.\r");
				continue;
			}
		}else{
			if( hlo_stream_read(mic,NULL, CHUNK) > 0){
				continue;
			}
		}
		vTaskDelay(3);
	}
}


void hlo_app_audio_recorder_start(const char * location){
	recording = 1;
}
void hlo_app_audio_recorder_stop(void){
	recording = 0;
}

////-----------------------------------------
//commands

int Cmd_app_record_start(int argc, char *argv[]){
	hlo_app_audio_recorder_start("");
	return 0;
}
int Cmd_app_record_stop(int argc, char *argv[]){
	hlo_app_audio_recorder_stop();
	return 0;

}
