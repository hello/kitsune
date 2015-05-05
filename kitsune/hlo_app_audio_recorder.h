#ifndef HLO_APP_AUDIO_RECORDER_H
#define HLO_APP_AUDIO_RECORDER_H
//an audio recording app
//todo make it queue based
void hlo_app_audio_recorder_task(void * data);

void hlo_app_audio_recorder_start(const char * location);
void hlo_app_audio_recorder_stop(void);

//uart commands
int Cmd_app_record_start(int argc, char *argv[]);
int Cmd_app_record_stop(int argc, char *argv[]);
#endif
