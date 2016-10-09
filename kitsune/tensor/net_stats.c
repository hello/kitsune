#include "net_stats.h"
#include <string.h>
#include <stdbool.h>

#include "../protobuf/keyword_stats.pb.h"
#include "../nanopb/pb.h"

void net_stats_init(NetStats_t * stats, uint32_t num_keywords) {
    memset(stats,0,sizeof(NetStats_t));
    stats->num_keywords = num_keywords;
}

void net_stats_reset(NetStats_t * stats) {
	uint32_t num_keywords = stats->num_keywords;
    memset(stats,0,sizeof(NetStats_t));
    stats->num_keywords = num_keywords;
}

void net_stats_record_activation(NetStats_t * stats, Keyword_t keyword, uint32_t counter) {
	NetStatsActivation_t * pActivation = stats->activations[stats->iactivation];

	//behavior: increment no matter what, but if we've reached the max
	//then don't save the activation (we're out of memory)
	if (stats->iactivation++ >= NET_STATS_MAX_ACTIVATIONS) {
		return;
	}

	pActivation->keyword = keyword;
	pActivation->time_count = counter;
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

	for (i = 1; i < stats.num_keywords; i++) {
		IndividualKeywordHistogram hist;
		memset(&hist,0,sizeof(hist));

		switch (i) {

		case okay_sense:
			hist.key_word = keyword_OK_SENSE;
			break;

		case stop:
			hist.key_word = keyword_STOP;
			break;

		case snooze:
			hist.key_word = keyword_SNOOZE;
			break;

		default:
			continue;

		}

		hist.has_key_word = true;

		hist.histogram_counts.arg = &stats->counts[i];
		hist.histogram_counts.funcs.encode = encode_histogram_counts;

		pb_encode_submessage(stream,IndividualKeywordHistogram_fields,&hist);
	}

	return true;
}

static bool encode_activations(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	uint32_t i;
	const NetStats_t * stats = *arg;
	const uint32_t num_activations = stats->iactivation > NET_STATS_MAX_ACTIVATIONS ? NET_STATS_MAX_ACTIVATIONS : stats->iactivation;

	if (!stats) {
		return false;
	}


	//do this for a repeated field
	if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
		return false;
	}

	for (i = 0; i < num_activations; i++) {
		const NetStatsActivation_t pActivation = stats->activations[i];
		const KeywordActivation activation = {true,pActivation->time_count,true,pActivation->keyword};

		pb_encode_submessage(stream,KeywordActivation_fields,&activation);
	}

	return true;

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



void set_encoders(KeywordStats * keyword_stats_item, NetStats_t * stats, const char * neural_net_id) {
	keyword_stats_item->histograms.funcs.encode = encode_histogram;
	keyword_stats_item->histograms.arg = stats;

	keyword_stats_item->net_model.funcs.encode = encode_neural_net_id;
	keyword_stats_item->net_model.arg = neural_net_id;

	keyword_stats_item->keyword_activations = encode_activations;
	keyword_stats_item->keyword_activations.arg = stats;

}


