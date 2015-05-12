#ifndef HLO_NET_TOOLS_H
#define HLO_NET_TOOLS_H
#include "hlo_async.h"


/**
 * host name must remain in scope
 */
unsigned long resolve_ip_by_host_name(const char * host_name);


int Cmd_dig(int argc, char *argv[]);
#endif
