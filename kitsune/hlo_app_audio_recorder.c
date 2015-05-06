#include "hlo_app_audio_recorder.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"
#include "hellofilesystem.h"
#include "hlo_pipe.h"

typedef enum{
	RECORDER_STOPPED = 0,
	RECORDER_RECORDING = 1,
	RECORDER_PLAYBACK = 2,
}recorder_status;
static recorder_status next_status;


#define CHUNK 512
void hlo_app_audio_recorder_task(void * data){
	hlo_stream_t * mic = hlo_open_mic_stream(2*CHUNK,CHUNK,0);
	uint8_t chunk[512];
	hlo_stream_t * fs = NULL;
	recorder_status last_status = RECORDER_STOPPED;
	assert(mic);
	while(1){
		recorder_status my_status = next_status;
		switch(my_status){
		case RECORDER_STOPPED:
			if(last_status != RECORDER_STOPPED){
				if(fs){
					hlo_stream_close(fs);
				}
			}
			if(hlo_stream_transfer_all(FROM_STREAM,mic,NULL,CHUNK,4) < 0){
				//handle error
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
			if( hlo_stream_transfer_all(FROM_STREAM,mic,chunk,CHUNK,4) > 0 ){
				if(hlo_stream_transfer_all(INTO_STREAM, fs, chunk, CHUNK, 4) > 0){

				}else{
					hlo_stream_close(fs);
					fs = NULL;
					//try to stop
					next_status = RECORDER_STOPPED;
				}
			}else{
				//handle error
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
	hlo_stream_close(mic);
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
int Cmd_app_record_replay(int argc, char *argv[]){
	hlo_app_audio_recorder_replay();
	return 0;

}
