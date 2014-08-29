#ifndef __WIFI_CMD_H__
#define __WIFI_CMD_H__

typedef struct {
	unsigned int time;
	int light, light_variability, light_tonality, temp, humid, dust;
} data_t;

extern
int sl_mode;


#define CONNECT   0x00000001
#define HAS_IP    0x00000002
#define IP_LEASED 0x00000004

extern
unsigned int sl_status;

int Cmd_connect(int argc, char *argv[]);
int Cmd_ping(int argc, char *argv[]);
int Cmd_status(int argc, char *argv[]);
int Cmd_audio_test(int argc, char *argv[]);
int Cmd_time(int argc, char*argv[]);
int Cmd_sl(int argc, char*argv[]);
int Cmd_mode(int argc, char*argv[]);

unsigned long unix_time();
void load_aes();

int send_periodic_data( data_t * data );
int send_audio_data( data_t * data );

void thread_ota( void * unused );

//#define MORPH_NAME "KingShy's morpheus"

//#define MORPH_NAME "Chris's morpheus"
#define MORPH_NAME "test morpheus 10"
//#define MORPH_NAME "test morpheus 80"
#define KIT_VER 2

#endif
