#ifndef HLO_NET_TOOLS_H
#define HLO_NET_TOOLS_H
#include "hlo_async.h"
#include "wlan.h"

/**
 * host name must remain in scope
 */
unsigned long resolve_ip_by_host_name(const char * host_name);

//scans and copies a list of unique result
int get_unique_wifi_list(SlWlanNetworkEntry_t * result, size_t num_entries);

//gives you a future result.
//IMPORTANT, a read response will return the number of actual entries
//and not the number of bytes written
hlo_future_t * prescan_wifi(size_t num_entries);


hlo_future_t * http_get_stream(const char  * url);

int Cmd_dig(int argc, char *argv[]);
int Cmd_scan_wifi(int argc, char *argv[]);
int Cmd_test_get(int argc, char * argv[]);
#endif
