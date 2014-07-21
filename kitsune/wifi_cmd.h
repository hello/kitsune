#ifndef __WIFI_CMD_H__
#define __WIFI_CMD_H__

typedef struct {
		int time,light,temp,humid,dust;
	} data_t;

int Cmd_connect(int argc, char *argv[]);
int Cmd_ping(int argc, char *argv[]);
int Cmd_status(int argc, char *argv[]);
int Cmd_time(int argc, char*argv[]);
int Cmd_skeletor(int argc, char*argv[]);


#endif
