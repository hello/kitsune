#include "net_stats.h"
#include <string.h>
#include <stdbool.h>

#include "keywords.h"
#include "../nanopb/pb.h"
#include "../nanopb/pb_encode.h"
#include "../protobuf/keyword_stats.pb.h"

void net_stats_init(NetStats_t * stats, uint32_t num_keywords, const char * neural_net_id) {
    memset(stats,0,sizeof(NetStats_t));
    stats->num_keywords = num_keywords;
    stats->neural_net_id = neural_net_id;
}

void net_stats_reset(NetStats_t * stats) {
	uint32_t num_keywords = stats->num_keywords;
	const char * neural_net_id = stats->neural_net_id;

    memset(stats,0,sizeof(NetStats_t));

    stats->num_keywords = num_keywords;
    stats->neural_net_id = neural_net_id;
}

void net_stats_record_activation(NetStats_t * stats, uint32_t keyword, uint32_t counter) {
	NetStatsActivation_t * pActivation = &stats->activations[stats->iactivation];

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
    	pb_encode_svarint(&sizestream,p[i]);
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


static bool map_protobuf_keywords(int * mapout, int mapin) {
	//TODO support other maps
	switch (mapin) {

	case okay_sense:
		*mapout = Keyword_OK_SENSE;
		break;

	case stop:
		*mapout = Keyword_STOP;
		break;

	case snooze:
		*mapout = Keyword_SNOOZE;
		break;

	case okay:
		*mapout = Keyword_OKAY;
		break;

	default:
		return false;

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

	for (i = 1; i < stats->num_keywords; i++) {
		IndividualKeywordHistogram hist;
		memset(&hist,0,sizeof(hist));


		hist.has_key_word = map_protobuf_keywords(&hist.key_word,i);

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
		const NetStatsActivation_t * pActivation = &stats->activations[i];
		const KeywordActivation activation = {true,pActivation->time_count,true,pActivation->keyword};

		pb_encode_submessage(stream,KeywordActivation_fields,&activation);
	}

	return true;

}

static bool encode_neural_net_id(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	const NetStats_t * stats = *arg;

	if (!arg) {
		return false;
	}

	//write tag
	if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
		return 0;
	}

	pb_encode_string(stream, (uint8_t*)stats->neural_net_id, strlen(stats->neural_net_id));

	return true;
}



void set_encoders_with_data(KeywordStats * keyword_stats_item, NetStats_t * stats) {
    memset(keyword_stats_item,0,sizeof(KeywordStats));
    
    keyword_stats_item->net_model.funcs.encode = encode_neural_net_id;
    keyword_stats_item->net_model.arg = stats;
    
	//keyword_stats_item->histograms.funcs.encode = encode_histogram;
	//keyword_stats_item->histograms.arg = stats;

	//keyword_stats_item->keyword_activations.funcs.encode = encode_activations;
	//keyword_stats_item->keyword_activations.arg = stats;

}


