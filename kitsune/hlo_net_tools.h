#ifndef HLO_NET_TOOLS_H
#define HLO_NET_TOOLS_H
#include "hlo_async.h"
/*
 * provides net utility in a centralized place
 */
void hlo_net_tools_task(void * ctx);

/**
 * returns a long ip
 */
hlo_future_t * resolve_host_by_name(const char * host_name);


int Cmd_dig(int argc, char *argv[]);
#endif
