#include "gtest/gtest.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>

#include "../kitsune/audio_features_upload_task_helpers.h"
#include "../kitsune/proto_utils.h"
#include "../kitsune/hlo_circbuf_stream.h"

namespace embedded {
    #include "../kitsune/protobuf/simple_matrix.pb.h"
    #include "../kitsune/nanopb/pb_encode.h"
}

#include "cpp_proto/simple_matrix.pb.h"


class TestUploadFeats : public ::testing::Test {
protected:
    virtual void SetUp() {
        
    }
    
    virtual void TearDown() {
        
    }
    
};


TEST_F(TestUploadFeats,TestRateLimiting) {
 
    RateLimiter_t data = {3,10000,0,0,0};
    
    ASSERT_FALSE(is_rate_limited(&data, 0));
    ASSERT_FALSE(is_rate_limited(&data, 1));
    ASSERT_FALSE(is_rate_limited(&data, 2));
    ASSERT_TRUE(is_rate_limited(&data, 3));
    ASSERT_TRUE(is_rate_limited(&data, 4));
    ASSERT_TRUE(is_rate_limited(&data, 5));
    ASSERT_TRUE(is_rate_limited(&data, 6));
    ASSERT_TRUE(is_rate_limited(&data, 7));

    ASSERT_FALSE(is_rate_limited(&data, 10000));
    ASSERT_FALSE(is_rate_limited(&data, 10001));
    ASSERT_FALSE(is_rate_limited(&data, 10002));
    ASSERT_TRUE(is_rate_limited(&data, 10003));

}

TEST_F(TestUploadFeats, TestBytestreamEncoding) {
    
    char buf[1234];
    char outbuf[2048] = {0};
    for (int i = 0; i < sizeof(buf); i++) {
        buf[i] = 65 + (i % 26);
    }
    
    hlo_stream_t * pstream = hlo_circbuf_stream_open(1025);
    
    hlo_stream_write(pstream,buf,sizeof(buf));
    
    
    embedded::SimpleMatrix mat;
    memset(&mat,0,sizeof(mat));
    mat.payload.funcs.encode = encode_repeated_streaming_bytes;
    mat.payload.arg = pstream;
    

    
    pb_ostream_t pbout = pb_ostream_from_buffer((uint8_t *)outbuf, sizeof(outbuf));
    
    //encode_repeated_streaming_bytes(&pbout, &embedded::SimpleMatrix_fields[4],(void *const*)&pstream);
    
    pb_encode(&pbout, embedded::SimpleMatrix_fields, &mat);
    
    std::string s;
    s.assign(outbuf,pbout.bytes_written);

    SimpleMatrix decodedMat;
    ASSERT_TRUE(decodedMat.ParseFromString(s));
    ASSERT_EQ(decodedMat.payload_size(),5);
    int k = 0;
    for (int i = 0; i < decodedMat.payload_size(); i++) {
        const char * cbuf = decodedMat.payload(i).c_str();
        
        for (int j =0; j < decodedMat.payload(i).size(); j++) {
            ASSERT_EQ(cbuf[j],65 + (k++ % 26));
        }
    }
    
    
}

