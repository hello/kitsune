#ifndef _NETSTATS_H_
#define _NETSTATS_H_

#ifdef __cplusplus
extern "C" {
#endif

    
#include <stdint.h>
#include "tinytensor_types.h"
#include "tinytensor_math_defs.h"
#include <stdbool.h>

#define NET_STATS_MAX_OUTPUTS (5)
#define NET_STATS_HISTOGRAM_BINS_2N (3)
#define NET_STATS_HISTOGRAM_BINS (1 << NET_STATS_HISTOGRAM_BINS_2N)
#define NET_STATS_SHIFT (QFIXEDPOINT - NET_STATS_HISTOGRAM_BINS_2N)
#define NET_STATS_MAX_ACTIVATIONS (32)
    
typedef struct {
	uint32_t time_count;
	uint32_t keyword;
} NetStatsActivation_t;

typedef uint16_t NetStatsHistogramCountType_t;
    
typedef struct {
    NetStatsHistogramCountType_t counts[NET_STATS_MAX_OUTPUTS][NET_STATS_HISTOGRAM_BINS];
    uint32_t num_keywords;
    NetStatsActivation_t activations[NET_STATS_MAX_ACTIVATIONS];
    uint32_t iactivation;
    const char * neural_net_id;
} NetStats_t;

void net_stats_init(NetStats_t * stats, uint32_t num_keywords,const char * neural_net_id);

void net_stats_reset(NetStats_t * stats);

void net_stats_record_activation(NetStats_t * stats, uint32_t keyword, uint32_t counter);

static inline void net_stats_update_counts(NetStats_t * stats,const Weight_t * output) {
    uint32_t i;
    uint32_t idx;
    for (i = 0; i < stats->num_keywords; i++) {
        idx = output[i] >> NET_STATS_SHIFT;

        //bound range, for safety.  In theory these checks shouldn't be necessary.
        idx = idx >= NET_STATS_HISTOGRAM_BINS ? NET_STATS_HISTOGRAM_BINS - 1 : idx;

        stats->counts[i][idx]++;
    }
}  

#ifdef __cplusplus
}
#endif

#endif //_NETSTATS_H_
