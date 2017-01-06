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
    int16_t buf[NUM_SAMPLES_TO_RUN_FFT];
    int i;
   

    for (int ibuf = 0; ibuf < FFT_UNPADDED_SIZE/NUM_SAMPLES_TO_RUN_FFT; ibuf++) {
        for (i = 0; i < NUM_SAMPLES_TO_RUN_FFT; i++) {
            buf[i] = count++;
        }
        
        memset(outbuf,0,sizeof(outbuf));
        ASSERT_TRUE(audio_buffering_add(outbuf, buf, NUM_SAMPLES_TO_RUN_FFT) == 0);
    }
    
    for (i = 0; i < NUM_SAMPLES_TO_RUN_FFT; i++) {
        buf[i] = count++;
    }

    memset(outbuf,0,sizeof(outbuf));
    ASSERT_TRUE(audio_buffering_add(outbuf, buf, NUM_SAMPLES_TO_RUN_FFT) == 1);
    
    int istart = ((FFT_UNPADDED_SIZE/NUM_SAMPLES_TO_RUN_FFT + 1) * NUM_SAMPLES_TO_RUN_FFT) % FFT_UNPADDED_SIZE;

    for (i = 0; i < FFT_UNPADDED_SIZE; i++) {
        ASSERT_EQ(i + istart, outbuf[i]);
    }
    
    for (i = 0; i < NUM_SAMPLES_TO_RUN_FFT; i++) {
        buf[i] = count++;
    }

    memset(outbuf,0,sizeof(outbuf));
    ASSERT_TRUE(audio_buffering_add(outbuf, buf, NUM_SAMPLES_TO_RUN_FFT) == 1);

    for (i = 0; i < FFT_UNPADDED_SIZE; i++) {
        ASSERT_EQ(i + istart + NUM_SAMPLES_TO_RUN_FFT, outbuf[i]);
    }
    
    for (i = 0; i < NUM_SAMPLES_TO_RUN_FFT; i++) {
        buf[i] = count++;
    }
    
    memset(outbuf,0,sizeof(outbuf));
    ASSERT_TRUE(audio_buffering_add(outbuf, buf, NUM_SAMPLES_TO_RUN_FFT) == 1);
    
    for (i = 0; i < FFT_UNPADDED_SIZE; i++) {
        ASSERT_EQ(i + istart + 2*NUM_SAMPLES_TO_RUN_FFT, outbuf[i]);
    }
}
