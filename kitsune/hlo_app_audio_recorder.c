#include "hlo_app_audio_recorder.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"

typedef enum{
	RECORDER_STOPPED = 0,
	RECORDER_RECORDING = 1,
	RECORDER_PLAYBACK = 2,
}recorder_status;
static recorder_status next_status;


#define CHUNK 512
void hlo_app_audio_recorder_task(void * data){
	hlo_stream_t * mic = hlo_open_mic_stream(2*CHUNK,CHUNK);
	uint8_t chunk[512];
	hlo_stream_t * fs = NULL;
	recorder_status last_status = RECORDER_STOPPED;
	assert(mic);
	while(1){
		recorder_status my_status = next_status;
		int res;
		switch(my_status){
		case RECORDER_STOPPED:
			if(last_status != RECORDER_STOPPED){
				if(fs){
					hlo_stream_close(fs);
				}
			}
			while( (res = hlo_stream_read(mic,NULL, CHUNK)) > 0){
				//just burning off the most recent buffer
			}
			break;
		case RECORDER_RECORDING:
			DISP("rec.\r");
			if(last_status != RECORDER_RECORDING){
				if(fs){
					hlo_stream_close(fs);
				}
				hello_fs_unlink("rec.raw");
				fs = fs_stream_open("rec.raw",HLO_STREAM_WRITE);
			}
			res = hlo_stream_transfer_all(FROM_STREAM,mic,chunk,CHUNK,4);
			if(res < 0){
				//handle error;
			}else if(fs){
				res = hlo_stream_transfer_all(INTO_STREAM, fs, chunk, CHUNK, 4);
				if(res < 0){
					hello_fs_close(fs);
					fs = NULL;
					//try to stop
					next_status = RECORDER_STOPPED;
				}
			}
			break;
		case RECORDER_PLAYBACK:
			if(fs && last_status != RECORDER_PLAYBACK){
				//on transition
				hlo_stream_close(fs);
				fs = fs_stream_open("rec.raw",HLO_STREAM_READ);
				if(fs){
					hlo_set_playback_stream(1,fs);
				}
			}
			break;
		}
		last_status = my_status;
		vTaskDelay(4);
	}
}


void hlo_app_audio_recorder_start(const char * location){
	next_status = RECORDER_RECORDING;
}
void hlo_app_audio_recorder_stop(void){
	next_status = RECORDER_STOPPED;
}
void hlo_app_audio_recorder_replay(void){
	next_status = RECORDER_PLAYBACK;
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
