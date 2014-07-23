#ifndef __WIFI_CMD_H__
#define __WIFI_CMD_H__

typedef struct {
	int time, light, temp, humid, dust;
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
int Cmd_time(int argc, char*argv[]);
int Cmd_sl(int argc, char*argv[]);
int Cmd_mode(int argc, char*argv[]);

#endif
