#include "gtest/gtest.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>
#include "../kitsune/tensor/net_stats.h"

//this is here because we can't have both the nanopb and the normal C++ protobuf defs in the same namespace
namespace embedded {
#include "../kitsune/protobuf/keyword_stats.pb.h"
#include "../kitsune/nanopb/pb_encode.h"
}

#include "cpp_proto/keyword_stats.pb.h"


extern "C" {
    void set_encoders_with_data(embedded::KeywordStats * keyword_stats_item, NetStats_t * stats);
}

class TestNetStats : public ::testing::Test {
protected:
    virtual void SetUp() {
        
    }
    
    virtual void TearDown() {
        
    }
    
    
};


TEST_F(TestNetStats,TestAccumulationOfStats) {
 
    NetStats_t stats;
    
    net_stats_init(&stats, 3, "foobars");
    Weight_t output [3] = {0,0,1<<QFIXEDPOINT};
    
    net_stats_update_counts(&stats, output);
    
    ASSERT_TRUE(stats.counts[0][0] == 1);
    ASSERT_TRUE(stats.counts[0][1] == 0);
    ASSERT_TRUE(stats.counts[1][0] == 1);
    ASSERT_TRUE(stats.counts[2][0] == 0);
    ASSERT_TRUE(stats.counts[2][NET_STATS_HISTOGRAM_BINS - 1] == 1);

    
}

typedef struct {
    char buf[1024];
} Buf_t;

bool callback(embedded::pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    Buf_t * item = (Buf_t*) stream->state;
    memcpy(&item->buf[stream->bytes_written],buf,count);
    return true;
}

bool callback_file_write(embedded::pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    FILE *file = (FILE*) stream->state;
    return fwrite(buf, 1, count, file) == count;
}

TEST_F(TestNetStats,TestProtobuf) {

    NetStats_t stats;
    net_stats_init(&stats, 3, "foobars");
    Weight_t output [3] = {0,0,(1 << QFIXEDPOINT) - 1};
    
    
    net_stats_update_counts(&stats, output);
    
    net_stats_record_activation(&stats, 1, 42);
    net_stats_record_activation(&stats, 2, 85);
    net_stats_record_activation(&stats, 3, 93);

    embedded::KeywordStats pb_kwstats;
    memset(&pb_kwstats,0,sizeof(embedded::KeywordStats));
 
 
    set_encoders_with_data(&pb_kwstats,&stats);
    
    
    Buf_t state;
    memset(&state,0,sizeof(state));
    
    embedded::pb_ostream_t bufstream = {&callback, &state, 1024, 0};
    pb_encode(&bufstream, embedded::KeywordStats_fields, &pb_kwstats);
    
    KeywordStats a;
    
    std::string s;
    s.assign(state.buf,bufstream.bytes_written);
    
    ASSERT_TRUE(a.ParseFromString(s));
    ASSERT_TRUE(a.has_net_model());
    ASSERT_EQ(a.net_model(), "foobars");
    ASSERT_GE(a.histograms_size(), 2);
    ASSERT_TRUE(a.histograms(1).has_key_word_index());
    ASSERT_TRUE(a.histograms(1).histogram_counts(7) == 1);
    ASSERT_EQ(a.keyword_activations_size(),3);
    ASSERT_EQ(a.keyword_activations(0).time_counter(),42);
    ASSERT_EQ(a.keyword_activations(0).key_word_index(),1);
    ASSERT_EQ(a.keyword_activations(1).time_counter(),85);
    ASSERT_EQ(a.keyword_activations(1).key_word_index(),2);
    ASSERT_EQ(a.keyword_activations(2).time_counter(),93);
    ASSERT_EQ(a.keyword_activations(2).key_word_index(),3);

}

