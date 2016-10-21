#ifndef _AUDIO_FEATURES_UPLOAD_TASK_H_
#define _AUDIO_FEATURES_UPLOAD_TASK_H_

#include "hlo_stream.h"

typedef enum {
	feats_sint8,
} FeaturesPayloadType_t;

typedef struct {
	hlo_stream_t * stream;
	const char * id;
	int num_cols;
	FeaturesPayloadType_t feats_type;
} AudioFeaturesUploadTaskMessage_t;





void audio_features_upload_task_add_message(const AudioFeaturesUploadTaskMessage_t * message);

void audio_features_upload_task(void * ctx);

#endif //_AUDIO_FEATURES_UPLOAD_TASK_H_
