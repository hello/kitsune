#include "gtest/gtest.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>
#include "../kitsune/tensor/audio_buffering.h"
#include "../kitsune/tensor/features_types.h"

class TestAudioBuffering : public ::testing::Test {
protected:
    virtual void SetUp() {
        
    }
    
    virtual void TearDown() {
        
    }
    
    
};



TEST_F(TestAudioBuffering,TestExact) {
    int16_t outbuf[FEATURES_FFT_SIZE];
    audio_buffering_init();
    int count = 0;
    int16_t buf[240];
    
    for (int i = 0; i < 240; i++) {
        buf[i] = count++;
    }

    memset(outbuf,0,sizeof(outbuf));
    ASSERT_TRUE(audio_buffering_add(outbuf, buf, 240) == 1);

    for (int i = 0; i < 240; i++) {
        ASSERT_EQ(outbuf[i],i);
    }
    
    ASSERT_EQ(outbuf[240],0);
    
    

}
