#include "gtest/gtest.h"
#include "../kitsune/fft.h"
#include "testvec1.c"
#include "refvec1.c"
#include <string.h>
#include <iostream>

class TestFrequencyFeatures : public ::testing::Test {
protected:
    virtual void SetUp() {
        
    }
    
    virtual void TearDown() {
        
    }
    
    
};

/*  Ben: So the FFT as compared to octave is scaled by 32
         if I compare integers of the fft to the floating point output of octave
         
         The imaginary component is sign flipped from octave.
 
         but other than that looks good!
 
 
 */

static short CompareShortVec(const short * v1, const short * v2, size_t n) {
    short maxerr = 0;
    for (size_t j = 0; j < n; j++) {
        short err = abs(v1[j] - v2[j]);
        if (err > maxerr) {
            maxerr = err;
        }
    }
    
    return maxerr;
}

TEST_F(TestFrequencyFeatures,TestFFT1) {
    short vecr[1024];
    short veci[1024];
    
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vecr,testvec1,sizeof(vecr));
    memset(veci,0,sizeof(veci));
    
    //2^10 = 1024
    fft(vecr,veci,10);
    
    short err1 = CompareShortVec(refvec1[0],vecr,1024);
    short err2 = CompareShortVec(refvec1[1],veci,1024);
    
    ASSERT_NEAR (err1,0,10);
    ASSERT_NEAR (err2,0,10);
    
    /*
    for (int j = 0; j < 1024; j++) {
        std::cout << vecr[j] << std::endl;
    }
    
    for (int j = 0; j < 1024; j++) {
        std::cout << veci[j] << std::endl;
    }
     */
    
    
}

TEST_F(TestFrequencyFeatures,TestFFTR1) {
    short vec[1024];
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vec,testvec1,sizeof(vec));
    
    
    fftr(vec, 10);
    /*
    for (int j = 0; j < 1024; j++) {
        std::cout << vec[j] << std::endl;
    }
     */
    
}
