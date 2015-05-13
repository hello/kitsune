#ifndef HLO_NET_TOOLS_H
#define HLO_NET_TOOLS_H
#include "hlo_async.h"
#include "wlan.h"

/**
 * host name must remain in scope
 */
unsigned long resolve_ip_by_host_name(const char * host_name);
/**
 * scans wifi into the result
 * @return >= 0 for # of valid endpoints, < 0 for error
 *
 */
int scan_for_wifi(Sl_WlanNetworkEntry_t * result, size_t max_entries, int ant_select);


int Cmd_dig(int argc, char *argv[]);
int Cmd_scan_wifi(int argc, char *argv[]);
#endif
