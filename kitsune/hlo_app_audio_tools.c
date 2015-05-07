#include "hlo_app_audio_tools.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"
#include "hellofilesystem.h"
#include "hlo_pipe.h"



#define CHUNK_SIZE 512
static int sig = 0;
void hlo_app_audio_recorder_task(void * data){
	int ret;
	uint8_t chunk[CHUNK_SIZE];
	hlo_stream_t * mic = hlo_open_mic_stream(2*CHUNK_SIZE,CHUNK_SIZE,0);

	if(!mic){
		DISP("Unable to open mic\r\n");
		goto exit_fail;
	}
	hlo_stream_t * fs = fs_stream_open_wlimit((char*)data, 48000 * 6); //max six seconds of audio
	if(!fs){
		DISP("Unable to open wbuffer\r\n");
		goto exit_mic;
	}
	while(1){
		if( (ret = hlo_stream_transfer_all(FROM_STREAM,mic,chunk, sizeof(chunk), 4)) > 0){
			if( (ret = hlo_stream_transfer_all(INTO_STREAM,fs,chunk, sizeof(chunk),4)) > 0){
				if(sig){
					break;
				}
			}else{
				break;
			}
		}else{
			break;
		}
	}
	hlo_stream_close(fs);
exit_mic:
	hlo_stream_close(mic);
exit_fail:
	DISP("Recorder Task Finished %d\r\n", ret);
}



void hlo_app_audio_playback_task(void * data){

}
////-----------------------------------------
//commands

int Cmd_app_record_start(int argc, char *argv[]){
	sig = 0;
	hlo_app_audio_recorder_task("rec.raw");
	return 0;
}
int Cmd_app_record_stop(int argc, char *argv[]){
	sig = 1;
	return 0;

}
int Cmd_app_record_replay(int argc, char *argv[]){
	return 0;
}

