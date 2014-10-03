#ifndef __WIFI_CMD_H__
#define __WIFI_CMD_H__

#include "periodic.pb.h"
typedef struct {
	uint32_t magic;
	char id[256];
	periodic_data_pill_data pill_data;
} periodic_data_pill_data_container;
#define PILL_MAGIC 0xAAAAAAAA
#define MAX_PILLS 8
#include "FreeRTOS.h"
#include "semphr.h"
extern xSemaphoreHandle pill_smphr;

typedef struct {
	unsigned int time;
	int light, light_variability, light_tonality, temp, humid, dust;
	periodic_data_pill_data_container * pill_list;
} data_t;

extern
int sl_mode;

extern periodic_data_pill_data_container pill_list[MAX_PILLS];

#define CONNECT   0x00000001
#define HAS_IP    0x00000002
#define IP_LEASED 0x00000004

#include "stdint.h"
#include "SyncResponse.pb.h"

#define INV_TIME 0xffffffff
extern
SyncResponse_Alarm alarm;

extern
unsigned int sl_status;

#define IFA_ANT 1
#define CHIP_ANT 2
void antsel(unsigned char a);
int Cmd_antsel(int argc, char *argv[]);

int Cmd_connect(int argc, char *argv[]);
int Cmd_disconnect(int argc, char *argv[]);
int Cmd_ping(int argc, char *argv[]);
int Cmd_status(int argc, char *argv[]);
int Cmd_audio_test(int argc, char *argv[]);
int Cmd_time(int argc, char*argv[]);
int Cmd_sl(int argc, char*argv[]);
int Cmd_mode(int argc, char*argv[]);


int Cmd_RadioStartRX(int argc, char*argv[]);
int Cmd_RadioStopRX(int argc, char*argv[]);
int Cmd_RadioStopTX(int argc, char*argv[]);
int Cmd_RadioStartTX(int argc, char*argv[]);
int Cmd_RadioGetStats(int argc, char*argv[]);


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
