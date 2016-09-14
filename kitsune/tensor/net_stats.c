#include "net_stats.h"
#include <string.h>

void net_stats_init(NetStats_t * stats) {
    memset(stats,0,sizeof(NetStats_t));
}

void net_stats_reset(NetStats_t * stats) {
    memset(stats,0,sizeof(NetStats_t));
}
