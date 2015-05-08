#include "hlo_app_audio_tools.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"
#include "hellofilesystem.h"
#include "hlo_pipe.h"



#define CHUNK_SIZE 512
int audio_sig_stop = 0;
void hlo_app_audio_recorder_task(void * data){
	int ret;
	uint8_t chunk[CHUNK_SIZE];
	hlo_stream_t *fs, * mic;
	mic = hlo_open_mic_stream(2*CHUNK_SIZE,0);
	fs = fs_stream_open_wlimit((char*)data, 48000 * 6); //max six seconds of audio

	while( (ret = hlo_stream_transfer_between(mic,fs,chunk, sizeof(chunk),4)) > 0){
		if(audio_sig_stop){
			break;
		}
	}

	hlo_stream_close(fs);
	hlo_stream_close(mic);
	DISP("Recorder Task Finished %d\r\n", ret);
}



void hlo_app_audio_playback_task(void * data){
	int ret;
	uint8_t chunk[CHUNK_SIZE];
	hlo_stream_t * spkr = hlo_open_spkr_stream(2*CHUNK_SIZE);
	hlo_stream_t * fs = fs_stream_open_media((char*)data, 0);

	while( (ret = hlo_stream_transfer_between(fs,spkr,chunk, sizeof(chunk),4)) > 0){
		if(audio_sig_stop){
			audio_sig_stop = 0;
			break;
		}
	}

	hlo_stream_close(fs);
	hlo_stream_close(spkr);
	DISP("Playback Task Finished %d\r\n", ret);

}
////-----------------------------------------
//commands

int Cmd_app_record_start(int argc, char *argv[]){
	audio_sig_stop = 0;
	hlo_app_audio_recorder_task("rec.raw");
	return 0;
}
int Cmd_app_record_stop(int argc, char *argv[]){
	audio_sig_stop = 1;
	return 0;

}
int Cmd_app_record_replay(int argc, char *argv[]){
	DISP("Playing back %s ...\r\n", argv[1]);
	audio_sig_stop = 0;
	hlo_app_audio_playback_task(argv[1]);
	return 0;
}

