#ifndef _NETSTATS_H_
#define _NETSTATS_H_

#include <stdint.h>
#include "tinytensor_types.h"

#define NET_STATS_MAX_OUTPUTS (10)
#define NET_STATS_HISTOGRAM_BINS (8)
#define NET_STATS_SHIFT (5)


typedef struct {
    uint16_t counts[NET_STATS_MAX_OUTPUTS][NET_STATS_HISTOGRAM_BINS];
    uint16_t activations[NET_STATS_MAX_OUTPUTS];
} NetStats_t;

void net_stats_init(NetStats_t * stats);

static inline net_stats_update_counts(NetStats_t * stats,const Weight_t * output, const uint32_t len) {
    uint32_t i;
    uint32_t idx;
    for (i = 0; i < len; i++) {
        idx = output[i] >> NET_STATS_SHIFT;

        //bound range, for safety.  In theory these checks shouldn't be necessary.
        idx = idx < 0 ? 0 : idx;
        idx >= NET_STATS_HISTOGRAM_BINS ? NET_STATS_HISTOGRAM_BINS - 1 : idx;

        stats->counts[i][idx]++;
    }
}  

#endif //_NETSTATS_H_
