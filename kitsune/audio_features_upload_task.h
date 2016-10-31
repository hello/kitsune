#ifndef _AUDIO_FEATURES_UPLOAD_TASK_H_
#define _AUDIO_FEATURES_UPLOAD_TASK_H_

#include "hlo_stream.h"
#include <stdbool.h>

typedef enum {
	feats_sint8,
} FeaturesPayloadType_t;


void audio_features_upload_task_buffer_bytes(void * data, uint32_t len);

void audio_features_upload_trigger_async_upload(const char * net_id,const char * keyword,const uint32_t num_cols,FeaturesPayloadType_t feats_type);

bool audio_features_upload_set_finished_buffering(void);

#endif //_AUDIO_FEATURES_UPLOAD_TASK_H_
