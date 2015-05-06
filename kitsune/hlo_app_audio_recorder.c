#include "hlo_app_audio_recorder.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"

typedef enum{
	RECORDER_STOPPED = 0,
	RECORDER_RECORDING = 1,
	RECORDER_PLAYBACK = 2,
}recorder_status;
static recorder_status status;


#define CHUNK 512
void hlo_app_audio_recorder_task(void * data){
	hlo_stream_t * mic = hlo_open_mic_stream(2*CHUNK,CHUNK);
	uint8_t chunk[512];
	hlo_stream_t * fs = NULL;
	recorder_status last_status = RECORDER_STOPPED;
	assert(mic);
	while(1){
		recorder_status my_status = status;
		int res;
		switch(my_status){
		case RECORDER_STOPPED:
			 hlo_stream_read(mic,NULL,CHUNK);
			break;
		case RECORDER_RECORDING:
			while( (res = hlo_stream_read(mic,chunk, CHUNK)) > 0){
				DISP("rec.\r");
				if(fs && last_status != RECORDER_RECORDING){
					//on transition
					hello_fs_unlink("rec.raw");
					fs = fs_stream_open("rec.raw",HLO_STREAM_WRITE);
				}else if(fs){
					hlo_stream_write(fs,chunk,res);
				}
			}
			break;
		case RECORDER_PLAYBACK:
			if(fs && last_status != RECORDER_PLAYBACK){
				//on transition
				fs = fs_stream_open("rec.raw",HLO_STREAM_READ);
				hlo_set_playback_stream(1,fs);
			}
			break;
		}
		last_status = my_status;
		vTaskDelay(4);
	}
}


void hlo_app_audio_recorder_start(const char * location){
	status = RECORDER_RECORDING;
}
void hlo_app_audio_recorder_stop(void){
	status = RECORDER_STOPPED;
}
void hlo_app_audio_recorder_replay(void){
	status = RECORDER_PLAYBACK;
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
