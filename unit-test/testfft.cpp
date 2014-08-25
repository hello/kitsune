#include "gtest/gtest.h"
#include "../kitsune/fft.h"
#include "testvec1.c"
#include "refvec1.c"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>

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
static void PrintShortVecToFile(const std::string & filename,const short * x,size_t n) {
    std::ofstream fileout(filename.c_str());
    for (int j = 0; j < n; j++) {
        fileout << x[j] << std::endl;
    }
    fileout.close();
}

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
    /*
    std::ofstream fileout("out1.txt");
    for (int j = 0; j < 1024; j++) {
        fileout << vecr[j] << "," << veci[j] << std::endl;
    }
    fileout.close();
    */
    
    short err1 = CompareShortVec(refvec1[0],vecr,1024);
    short err2 = CompareShortVec(refvec1[1],veci,1024);
    
    ASSERT_NEAR (err1,0,10);
    ASSERT_NEAR (err2,0,10);
    
    
    
    
}

TEST_F(TestFrequencyFeatures,TestFFTR1) {
    short vec[1024];
    short * vecr;
    short * veci;
    
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vec,testvec1,sizeof(vec));
    
    
    fftr(vec, 10);
    /*
    std::ofstream fileout("out2.txt");
    for (int j = 0; j < 1024; j++) {
        fileout << vec[j] << std::endl;
    }
    fileout.close();
     */
    
    vecr = &vec[0];
    veci = &vec[512];
    
    short err1 = CompareShortVec(refvec1[0],vecr,512);
    short err2 = CompareShortVec(refvec1[1],veci,512);
    
    ASSERT_NEAR (err1,0,10);
    ASSERT_NEAR (err2,0,10);
    
}

TEST_F(TestFrequencyFeatures,TestMel) {
    short vecr[1024];
    short veci[1024];
    short mypsd[512];
    short b = 44100 / 1024;
    short mel[MEL_SCALE_SIZE];
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vecr,testvec1,sizeof(vecr));
    memset(veci,0,sizeof(veci));
    
    //2^10 = 1024
    fft(vecr,veci,10);

    abs_fft(mypsd, vecr, veci, 10);
    
    PrintShortVecToFile("mypsd.txt",mypsd,1<<9);
    
    mel_freq(mel,mypsd, 10, b);
    
    PrintShortVecToFile("mel.txt",mel,MEL_SCALE_SIZE);

    
}

TEST_F(TestFrequencyFeatures,TestCountMsb) {
    short y;
    unsigned int x;
    unsigned int j;
    short ref;
    
    ASSERT_TRUE(CountHighestMsb(0) == 0);
    
    for (j = 0 ; j < 32; j++) {
        x = 1 << j;
        y = CountHighestMsb(x);
        ref = j + 1;
        
        ASSERT_TRUE(y == ref);
    }
    
}


TEST_F(TestFrequencyFeatures,TestLog2Q8) {
    short x;
    short y;
    float fx;
    float fy,fy2;
    
    y = FixedPointLog2Q8(33);
    
    for (x = 1; x < 0x7FFF; x++) {
        fx = (float)x / (float)(1<<8);
        fy = log2(fx);
        y = FixedPointLog2Q8(x);

        fy2 = (float)y / (float)(1 <<8);
        ASSERT_NEAR(fy2,fy,0.5f);
    }
    
}

