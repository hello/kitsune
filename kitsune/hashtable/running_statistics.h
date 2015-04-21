#ifndef RUNNING_STAT_H
#define RUNNING_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "boolean.h"

typedef struct {
  float n,m,m2;
} running_variance; /**< Holds intermediate values for online variance */

typedef struct {
  float sum;
  float kahan_add;
  float kahan_sub;
  running_variance var;
  float* arr;
  unsigned int idx;
  unsigned int max_sz;
  boolean wrapped;
  boolean cmpt_var;
} running_statistics;

void running_stat_init( running_statistics* data, float* ptr, boolean compute_variance, unsigned int max_sz );
float running_stat_cmpt_mean( running_statistics* data );
float running_stat_cmpt_variance ( running_statistics* data );
void running_stat_add_sample( running_statistics* data, float new_value );

#ifdef __cplusplus
}
#endif

#endif
