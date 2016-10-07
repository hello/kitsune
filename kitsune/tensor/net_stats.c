#include "net_stats.h"
#include <string.h>
#include <stdbool.h>

#include "../protobuf/keyword_stats.pb.h"
#include "../nanopb/pb.h"

void net_stats_init(NetStats_t * stats) {
    memset(stats,0,sizeof(NetStats_t));
}

void net_stats_reset(NetStats_t * stats) {
    memset(stats,0,sizeof(NetStats_t));
}

static bool encode_histogram_counts(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	const uint32_t * p = (const uint32_t *)*arg;
	uint32_t i;

    pb_ostream_t sizestream = {0};

    //find size
    for (i = 0; i < NET_STATS_HISTOGRAM_BINS; i++) {
    	pb_encode_svarint(sizestream,p[i]);
    }

	//encode size
	if (!pb_encode_varint(stream,sizestream.bytes_written)) {
	    return false;
	}

	//write
	for (i = 0; i < NET_STATS_HISTOGRAM_BINS; i++) {
		pb_encode_svarint(stream,p[i]);
	}

	return true;

}



static bool encode_histogram(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {

	uint32_t i;

	const NetStats_t * stats = *arg;

	if (!stats) {
		return false;
	}

	//do this for a repeated field
	if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
		return false;
	}

	for (i = 0; i < stats.num_keywords; i++) {
		IndividualKeywordHistogram hist;
		hist.key_word = i;
		hist.has_key_word = true;

		hist.histogram_counts.arg = &stats->counts[i];
		hist.histogram_counts.funcs.encode = encode_histogram_counts;

		pb_encode_submessage(stream,IndividualKeywordHistogram_fields,&hist);
	}

}

static bool encode_neural_net_id(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	const char * str = (const char *)(*arg);
	static const char nullchar = '\0';

	if (!str) {
		str = &nullchar;
	}

	//write tag
	if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
		return 0;
	}

	pb_encode_string(stream, (uint8_t*)str, strlen(str));

	return true;
}

static bool encode_activations(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	return true;
}

void set_encoders(KeywordStats * keyword_stats_item, NetStats_t * stats, const char * neural_net_id) {
	keyword_stats_item->histograms.funcs.encode = encode_histogram;
	keyword_stats_item->histograms.arg = stats;

	keyword_stats_item->net_model.funcs.encode = encode_neural_net_id;
	keyword_stats_item->net_model.arg = neural_net_id;

	//keyword_stats_item->keyword_activations = encode_activations;
	//keyword_stats_item->keyword_activations.arg = stats;

}


