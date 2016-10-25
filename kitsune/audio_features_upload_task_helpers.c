#include "audio_features_upload_task_helpers.h"
#include "hlo_stream.h"
#include "hlo_pipe.h"
#include "pb_encode.h"

bool is_rate_limited(RateLimiter_t * data,const uint32_t current_time) {
	const uint32_t dt = current_time - data->last_upload_time;

	//update elapsed time
	data->elapsed_time += dt;

	//if elapsed time is greater than one period
	if (data->elapsed_time >=  data->ticks_per_upload) {

		//decrement upload count
		data->upload_count -= (data->elapsed_time / data->ticks_per_upload) * data->max_uploads_per_period;

		if (data->upload_count < 0) {
			data->upload_count = 0;
		}

		//reset
		data->elapsed_time = 0;

	}

	data->last_upload_time = current_time;

	if (data->upload_count < data->max_uploads_per_period) {
        data->upload_count++;
		return false;
	}

	return true;

}

static bool write_streams(pb_ostream_t *stream, const pb_field_t *field,hlo_stream_t * hlo_stream) {
	int bytes_read;
	unsigned char buffer[256];

	while (1) {
		bytes_read = hlo_stream_transfer_all(FROM_STREAM,hlo_stream,buffer,sizeof(buffer),0);

		if (bytes_read < 0) {
			break;
		}

		//write string tag for delimited field
		if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
			return false;
		}

		//write size
		if (!pb_encode_varint(stream, (uint64_t)bytes_read)) {
			return false;
		}

		//write buffer
		if (!pb_write(stream, buffer, bytes_read)) {
			return false;
		}
	}

	return true;
}

bool encode_repeated_streaming_bytes_and_mark_done(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	hlo_stream_t * hlo_stream = *arg;

    if(!hlo_stream) {
        return false;
    }

    if (!write_streams(stream,field,hlo_stream)) {
    	return false;
    }

    //mark stream as ended -- someone else will do the free
    hlo_stream_end(hlo_stream);

    return true;
}

bool encode_repeated_streaming_bytes_and_close(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	hlo_stream_t * hlo_stream = *arg;

	if(!hlo_stream) {
		return false;
	}

	if (!write_streams(stream,field,hlo_stream)) {
		return false;
	}

	//close stream, always
	hlo_stream_close(hlo_stream);

	return true;
}


