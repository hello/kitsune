#include "gtest/gtest.h"
#include "../kitsune/fft.h"
#include "../kitsune/audiofeatures.h"
#include "testvec1.c"
#include "refvec1.c"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>

class TestFrequencyFeatures : public ::testing::Test {
protected:
    virtual void SetUp() {
        
    }
    
    virtual void TearDown() {
        
    }
    
    
};

class DISABLED_TestFrequencyFeatures : public TestFrequencyFeatures {};

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

static void PrintUint8VecToFile(const std::string & filename,const uint8_t * x,size_t n) {
    std::ofstream fileout(filename.c_str());
    for (int j = 0; j < n; j++) {
        fileout << (int)x[j] << std::endl;
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
    
    PrintShortVecToFile("fftcomplexr.txt",vecr,1024);
    PrintShortVecToFile("fftcomplexi.txt",veci,1024);

    
    short err1 = CompareShortVec(refvec1[0],vecr,1024);
    short err2 = CompareShortVec(refvec1[1],veci,1024);
    
    ASSERT_NEAR (err1,0,10);
    ASSERT_NEAR (err2,0,10);
    
    

}

TEST_F(TestFrequencyFeatures,TestFFT256) {
    short vecr[256];
    short veci[256];
    
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vecr,testvec1,sizeof(vecr));
    
    PrintShortVecToFile("ref256.txt",vecr,256);

    memset(veci,0,sizeof(veci));
    
    //2^10 = 1024
    fft(vecr,veci,8);
    

    PrintShortVecToFile("fftcomplexr256.txt",vecr,256);
    PrintShortVecToFile("fftcomplexi256.txt",veci,256);
    
    
    
    
}

TEST_F(TestFrequencyFeatures,TestPsd256) {
    short vecr[256];
    short veci[256];
    int16_t logTotalEnergy;
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vecr,testvec1,sizeof(vecr));
    memset(veci,0,sizeof(veci));
    
    fft(vecr,veci,8);
    
    logpsdmel(&logTotalEnergy,&vecr[128], vecr, veci, 0,32);
    
    PrintShortVecToFile("logpsd256.txt",&vecr[128],32);
    
}


TEST_F(TestFrequencyFeatures,TestDct) {
    short vecr[512];
    short veci[512];
    
    for (int j = 0; j < 16; j++) {
        vecr[j] = (j - 8) * (1024);
    }
    
    dct(vecr,veci,4);
    
    for (int j = 0; j < 32; j++) {
        vecr[j] = (j - 16) * (1024);
    }
    
    dct(vecr,veci,5);
    
    
    
    int foo = 3;
    foo++;
    
}


TEST_F(DISABLED_TestFrequencyFeatures,TestFFTR1) {
    short vec[1024];
    short * vecr;
    short * veci;
    
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vec,testvec1,sizeof(vec));
    
    
    fftr(vec, 10);
 
    PrintShortVecToFile("fftr.txt",vec,1024);
    
    vecr = &vec[0];
    veci = &vec[512];
    
    short err1 = CompareShortVec(refvec1[0],vecr,512);
    short err2 = CompareShortVec(refvec1[1],veci,512);
    
    ASSERT_NEAR (err1,0,10);
    ASSERT_NEAR (err2,0,10);
    
}

static AudioFeatures_t _feats;

static void AudioFeatCallback(const AudioFeatures_t * pfeats) {
   // std::cout << "GOT AUDIO FEATURE CALLBACK" << std::endl;
    memcpy(&_feats,pfeats,sizeof(_feats));
}

static void AudioFeatCallbackOncePerMinute (const AudioOncePerMinuteData_t * pdata) {
    
}


TEST_F(TestFrequencyFeatures,TestMel) {
    int i,ichunk;
	int16_t x[1024];
    uint32_t icount = 0;
    
    memset(&_feats,0,sizeof(_feats));
    memset(&x,0,sizeof(x));

	srand(0);
    

	AudioFeatures_Init(AudioFeatCallback,AudioFeatCallbackOncePerMinute);
    
    //warmup period
    for (ichunk = 0; ichunk < 1000; ichunk++) {
        for (i = 0; i < 1024; i++) {
            x[i] = (rand() % 32767) - (1<<14);
        }
        
        AudioFeatures_SetAudioData(x,icount++);
    }

    int64_t featsum[NUM_AUDIO_FEATURES];
    memset(featsum,0,sizeof(featsum));
    //noise
    int64_t lastSampleCount = _feats.samplecount;
	for (ichunk = 0; ichunk < 10000; ichunk++) {
        for (i = 0; i < 1024; i++) {
            x[i] = (rand() % 32767) - (1<<14);
        }
		
		AudioFeatures_SetAudioData(x,icount++);
        
        
        //check range
        if (lastSampleCount != _feats.samplecount) {
            for (int i = 0; i < NUM_AUDIO_FEATURES; i++) {
                ASSERT_TRUE(_feats.feats4bit[i] >= -7);
                ASSERT_TRUE(_feats.feats4bit[i] <= 7);
                featsum[i] += _feats.feats4bit[i];
            }
            ASSERT_TRUE(_feats.logenergy > 0);
            
        }
        
        lastSampleCount = _feats.samplecount;
        
	}
    
    //test flatness
    for (i = 0; i < NUM_AUDIO_FEATURES; i++) {
        ASSERT_TRUE(std::abs(featsum[i])/100 < 10);
    }
  
}




TEST_F(TestFrequencyFeatures,TestBitLog) {
    
//    short bitlog(unsigned long n)
    uint32_t x;
    uint32_t prevx;

    uint8_t y;
    float ypred;
    x = 1;
    
//    ASSERT_TRUE(bitlog(0) == 0);
//    y = bitlog(0x7FFFFFFF);
//    ASSERT_TRUE(bitlog(0xFFFFFFFF) == 255);
    x = 16;
    prevx = 16;
    while (x + prevx > x) {
        //std:: cout << x << std::endl;
        y = bitlog(x);
        ypred = 8*log2( (float)x + 1);
        ASSERT_NEAR(ypred,y,10.0f);
        prevx = x;
        x = x + prevx; //fibonacci!
    }
    
    
}
