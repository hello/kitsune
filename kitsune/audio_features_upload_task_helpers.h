#ifndef _AUDIOFEATURESUPLOADTASKHELPERS_H_
#define _AUDIOFEATURESUPLOADTASKHELPERS_H_

#include <stdbool.h>
#include <stdint.h>
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct  {
	const int max_uploads_per_period;
	const int ticks_per_upload;
	uint32_t last_upload_time;
	uint32_t elapsed_time;
	int upload_count;
} RateLimiter_t;

bool is_rate_limited(RateLimiter_t * data,const uint32_t current_time);

bool encode_repeated_streaming_bytes(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

#ifdef __cplusplus
}
#endif
    
#endif //_AUDIOFEATURESUPLOADTASKHELPERS_H_
