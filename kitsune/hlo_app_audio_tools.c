#include "hlo_app_audio_tools.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"
#include "hellofilesystem.h"
#include "hlo_pipe.h"
#include "octogram.h"
#include "audio_types.h"


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
#define OCTOGRAM_BUFFER_SIZE ((AUDIO_FFT_SIZE)*3*2)
void hlo_app_audio_octogram_task(void * data){
	Octogram_t octogramdata = {0};
	int ret,i;
	int32_t duration = 500;
	Octogram_Init(&octogramdata);
	OctogramResult_t result;
	int16_t * samples = pvPortMalloc(OCTOGRAM_BUFFER_SIZE);
	if(!samples){
		goto exit;
	}
	hlo_stream_t * input = (hlo_stream_t*)data;//hlo_open_mic_stream(CHUNK_SIZE*2, 1);
	while( (ret = hlo_stream_transfer_all(FROM_STREAM,input,(uint8_t*)samples,OCTOGRAM_BUFFER_SIZE,4)) > 0){
		//convert from 48K to 16K
		for(i = 0; i < 256; i++){
			int32_t sum = samples[i] + samples[256+i] + samples[512+i];
			samples[i] = sum / 3;
		}
		Octogram_Update(&octogramdata,samples);
		if(duration-- < 0){
			//keep going
			Octogram_GetResult(&octogramdata, &result);
			LOGF("octogram log energies: ");
			for (i = 0; i < OCTOGRAM_SIZE; i++) {
				if (i != 0) {
					LOGF(",");
				}
				LOGF("%d",result.logenergy[i]);
			}
			LOGF("\r\n");
			break;
		}
	}
	vPortFree(samples);
exit:
	DISP("Octogram Task Finished %d\r\n", ret);
	hlo_stream_close(input);
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
int Cmd_app_octogram(int argc, char *argv[]){
	audio_sig_stop = 0;
	hlo_app_audio_octogram_task(random_stream_open());
	return 0;
}

