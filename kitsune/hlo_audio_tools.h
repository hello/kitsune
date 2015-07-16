#ifndef HLO_AUDIO_TOOLS_H
#define HLO_AUDIO_TOOLS_H
//an audio recorder app
//that demonstrates how to record and playback audio
void hlo_audio_recorder_task(void * data);
void hlo_audio_playback_task(void * data);
void hlo_audio_octogram_task(void * data);

//uart commands
int Cmd_audio_record_start(int argc, char *argv[]);
int Cmd_audio_record_stop(int argc, char *argv[]);
int Cmd_audio_record_replay(int argc, char *argv[]);
int Cmd_audio_octogram(int argc, char *argv[]);
int Cmd_audio_features(int argc, char *argv[]);
#endif
