/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.1 at Thu Oct  6 09:50:13 2016. */

#ifndef PB_KEYWORD_STATS_PB_H_INCLUDED
#define PB_KEYWORD_STATS_PB_H_INCLUDED
#include <pb.h>

#include "speech.pb.h"

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
/* Struct definitions */
typedef struct _KeywordStats {
    pb_callback_t net_model;
    pb_callback_t histograms;
    pb_callback_t keyword_activations;
} KeywordStats;

typedef struct _IndividualKeywordHistogram {
    pb_callback_t histogram_counts;
    bool has_key_word;
    keyword key_word;
} IndividualKeywordHistogram;

typedef struct _KeywordActivation {
    bool has_time_counter;
    int64_t time_counter;
    bool has_key_word;
    keyword key_word;
} KeywordActivation;

/* Default values for struct fields */

/* Initializer values for message structs */
#define IndividualKeywordHistogram_init_default  {{{NULL}, NULL}, false, (keyword)0}
#define KeywordActivation_init_default           {false, 0, false, (keyword)0}
#define KeywordStats_init_default                {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define IndividualKeywordHistogram_init_zero     {{{NULL}, NULL}, false, (keyword)0}
#define KeywordActivation_init_zero              {false, 0, false, (keyword)0}
#define KeywordStats_init_zero                   {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}

/* Field tags (for use in manual encoding/decoding) */
#define KeywordStats_net_model_tag               1
#define KeywordStats_histograms_tag              2
#define KeywordStats_keyword_activations_tag     3
#define IndividualKeywordHistogram_histogram_counts_tag 1
#define IndividualKeywordHistogram_key_word_tag  2
#define KeywordActivation_time_counter_tag       1
#define KeywordActivation_key_word_tag           2

/* Struct field encoding specification for nanopb */
extern const pb_field_t IndividualKeywordHistogram_fields[3];
extern const pb_field_t KeywordActivation_fields[3];
extern const pb_field_t KeywordStats_fields[4];

/* Maximum encoded size of messages (where known) */
#define KeywordActivation_size                   17

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
