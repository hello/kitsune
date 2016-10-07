#ifndef _NETSTATS_H_
#define _NETSTATS_H_

#include <stdint.h>
#include "tinytensor_types.h"
#include "tinytensor_math.h"

#define NET_STATS_MAX_OUTPUTS (10)
#define NET_STATS_HISTOGRAM_BINS_2N (3)
#define NET_STATS_HISTOGRAM_BINS (1 << NET_STATS_HISTOGRAM_BINS_2N)
#define NET_STATS_SHIFT (QFIXEDPOINT - NET_STATS_HISTOGRAM_BINS_2N)


typedef struct {
    uint32_t counts[NET_STATS_MAX_OUTPUTS][NET_STATS_HISTOGRAM_BINS];
    uint32_t num_keywords;
} NetStats_t;

void net_stats_init(NetStats_t * stats);

void net_stats_reset(NetStats_t * stats);

static inline net_stats_update_counts(NetStats_t * stats,const Weight_t * output, const uint32_t len) {
    uint32_t i;
    uint32_t idx;
    for (i = 0; i < len; i++) {
        idx = output[i] >> NET_STATS_SHIFT;

        //bound range, for safety.  In theory these checks shouldn't be necessary.
        idx = idx >= NET_STATS_HISTOGRAM_BINS ? NET_STATS_HISTOGRAM_BINS - 1 : idx;

        stats->counts[i][idx]++;
    }
}  



#endif //_NETSTATS_H_
