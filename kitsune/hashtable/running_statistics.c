#include "running_statistics.h"
#include "string.h"

static void variance_init( running_variance * var );
static float variance_get( running_variance * var );
static void variance_add( running_variance * var, float x );
static boolean variance_has_values( running_variance * var );
static void variance_remove( running_variance * var, float x );
static float kahan_sum( float* kahan, float a, float b );

void running_stat_init( running_statistics* data, float* ptr, boolean compute_variance, unsigned int max_sz ) {
  memset( data, 0, sizeof(running_statistics) );
  data->arr = ptr;
  data->max_sz = max_sz;
  data->cmpt_var = compute_variance;
  if( data->cmpt_var ) {
    variance_init( &data->var );
  }
}
float running_stat_cmpt_mean( running_statistics* data ) {
  /* Avoid divide by zero when the array is empty */
  if( !data->wrapped && ( data->idx == 0 ) ) {
    return 0.0f;
  }

  return data->sum / (float)( data->wrapped ? data->max_sz : data->idx );
}
float running_stat_cmpt_variance( running_statistics* data ) {
  if( data->cmpt_var ) {
    return variance_get( &data->var );
  }
  return -1.0;
}

void running_stat_add_sample( running_statistics* data, float new_value ) {
  /* oldest value is the current index as it's one past the last value added */
  float old_value = data->arr[data->idx];

  /* Remove oldest value from sum if the index has wrapped around */
  if( data->wrapped ) {
    /* remove old value from sum using Kahan summation */
    data->sum = kahan_sum( &data->kahan_sub, -old_value, data->sum );
    if( data->cmpt_var ) {
      variance_remove( &data->var, old_value );
    }
  }

  /* Add new value in place of oldest value */
  data->arr[data->idx++] = new_value;

  /* Add new value to sum using Kahan summation */
  data->sum = kahan_sum( &data->kahan_add, new_value, data->sum );

  /* Reset idx to 0 when it reaches max_sz and remember that it wrapped */
  if( data->idx >= data->max_sz ) {
    data->idx = 0;
    data->wrapped = 1;
  }

  if( data->cmpt_var ) {
    variance_add( &data->var, new_value );
  }
}
static void variance_init( running_variance * var ) {
  memset( var, 0, sizeof(running_variance) );
}
static float variance_get( running_variance * var ) {
  return variance_has_values( var ) ? var->m2/(var->n-1) : 0;
}
static void variance_add( running_variance * var, float x ) {
  float delta;
  ++var->n;
  delta = x - var->m;
  var->m += delta / var->n;
  var->m2 += delta * ( x - var->m );
}
static boolean variance_has_values( running_variance * var ) {
  return var->n > 1;
}
static void variance_remove ( running_variance * var, float x ) {
  if( variance_has_values( var ) ) {
    float m = ( var->n * var->m - x ) / ( var->n - 1 );
    var->m2 -= ( x - var->m ) * ( x - m );
    var->m = m;
    var->n--;
  } else {
    variance_init( var );
  }
}
static float kahan_sum( float* kahan, float a, float b ) {
  float t, y;

  /* Add new value to sum */
  y = a - *kahan;
  t = b + y;
  *kahan = (t - b ) - y;

  return t;
}
