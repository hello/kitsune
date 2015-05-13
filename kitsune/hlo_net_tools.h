#ifndef HLO_NET_TOOLS_H
#define HLO_NET_TOOLS_H
#include "hlo_async.h"
#include "wlan.h"

/**
 * host name must remain in scope
 */
unsigned long resolve_ip_by_host_name(const char * host_name);

//scans and copies a list of unique result
int get_unique_wifi_list(Sl_WlanNetworkEntry_t * result, size_t num_entries);


int Cmd_dig(int argc, char *argv[]);
int Cmd_scan_wifi(int argc, char *argv[]);
#endif
