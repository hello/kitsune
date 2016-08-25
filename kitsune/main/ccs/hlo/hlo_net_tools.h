#ifndef HLO_NET_TOOLS_H
#define HLO_NET_TOOLS_H
#include "hlo_async.h"
#include "wlan.h"


//scans and copies a list of unique result
int get_unique_wifi_list(SlWlanNetworkEntry_t * result, size_t num_entries);

//gives you a future result.
//IMPORTANT, a read response will return the number of actual entries
//and not the number of bytes written
hlo_future_t * prescan_wifi(size_t num_entries);


int Cmd_scan_wifi(int argc, char *argv[]);

typedef struct{
	enum{
		HTTP,
		HTTPS,
	} protocol;
	char host[32];
	char path[64];
}url_desc_t;
int parse_url(url_desc_t * desc, const char * url);

void itohexstring(size_t i, char output[9]);
#endif
