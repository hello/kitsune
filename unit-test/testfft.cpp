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

TEST_F(TestFrequencyFeatures,TestPsd) {
    short vecr[1024];
    short veci[1024];
    int16_t logTotalEnergy;
    int n = sizeof(testvec1) / sizeof(short);
    
    
    ASSERT_TRUE(n == 1024);
    
    memcpy(vecr,testvec1,sizeof(vecr));
    memset(veci,0,sizeof(veci));
    
    //2^10 = 1024
    fft(vecr,veci,10);
    
    logpsd(&logTotalEnergy,&vecr[512], vecr, veci, 0, 10);
    
    PrintShortVecToFile("logpsd.txt",&vecr[512],512);
    
}

TEST_F(TestFrequencyFeatures,TestSqrt) {
    uint32_t i;
    uint32_t res;
    uint32_t x;
    for (i = 0; i < 0x7FFF; i++) {
        float fx = i * 0.5f;
        float fy = sqrt(fx);
        float fres;
        
        x = i << 9;
        res = fxd_sqrt_q10(x);
        fres  = res / 1024.0;
        
        ASSERT_NEAR(fres, fy, 1e-2);
        
    }
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


TEST_F(TestFrequencyFeatures,TestFFTR1) {
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

static Segment_t _myseg;

static int16_t _mfcc[NUM_AUDIO_FEATURES];

static void AudioFeatCallback(const int16_t * feats, const Segment_t * pSegment) {
    memcpy(&_myseg,pSegment,sizeof(Segment_t));
    memcpy(_mfcc,feats,sizeof(_mfcc));
    
}

TEST_F(TestFrequencyFeatures,TestMel) {
    int i,ichunk;
	int16_t x[1024];
    
    memset(_mfcc,0,sizeof(_mfcc));
    memset(&_myseg,0,sizeof(_myseg));
    
	srand(0);
    
	printf("EXPECT: t1=%d,t2=%d,energy=something not zero\n",43,86);
    
    
	AudioFeatures_Init(AudioFeatCallback);
    
	//still ---> white random noise ---> still
	for (ichunk = 0; ichunk < 43*8; ichunk++) {
		if (ichunk > 43 && ichunk <= 86) {
			for (i = 0; i < 1024; i++) {
				x[i] = (rand() % 32767) - (1<<14);
			}
		}
		else {
			memset(x,0,sizeof(x));
		}
        
		AudioFeatures_SetAudioData(x,10,ichunk);
        
	}
    
    ASSERT_TRUE(_myseg.t1 < 86);
    ASSERT_TRUE(_myseg.t2 >= 86);
    ASSERT_TRUE(_mfcc[0] > 0);
}


TEST_F(TestFrequencyFeatures,TestMel2) {
    int i,ichunk;
    int16_t x[1024];
    
    memset(_mfcc,0,sizeof(_mfcc));
    memset(&_myseg,0,sizeof(_myseg));
    
    srand(0);
    
    
    AudioFeatures_Init(AudioFeatCallback);
#define amplitude (2)
    //still ---> white random noise ---> still
    for (ichunk = 0; ichunk < 43*100; ichunk++) {
        for (i = 0; i < 1024; i++) {
            x[i] = (rand() % amplitude) - (amplitude >> 1);
        }
        
        AudioFeatures_SetAudioData(x,10,ichunk);
        
    }
    
    ASSERT_TRUE(_myseg.t1 < 86);
    ASSERT_TRUE(_myseg.t2 >= 86);
    ASSERT_TRUE(_mfcc[0] > 0);
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
TEST_F(TestFrequencyFeatures,TestLog2Q10) {
    int64_t x;
    short y;
    float fx;
    float fy,fy2;
    
    for (x = 2; x < 0x7FFF; x++) {
        fx = (float)x / (float)(1<<10);
        fy = log2(fx);
        y = FixedPointLog2Q10(x);

        fy2 = (float)y / (float)(1 <<10);
        ASSERT_NEAR(fy2,fy,0.5f);
    }
    
    x = 0xFFFFFFFF;
    fx = (float)x / (float)(1<<10);
    fy = log2(fx);
    y = FixedPointLog2Q10(x);
    
    fy2 = (float)y / (float)(1 <<10);
    ASSERT_NEAR(fy2,fy,0.5f);

}

TEST_F (TestFrequencyFeatures,TestExp2Q10) {
    int16_t x;
    uint32_t y;
    float fx,fy,fy2;
    const float reltol = 0.02f;
    const float tol = 5.0f / (1<<10);
    float relerr;
    float err;
    
    for (x = 0; x < 22527; x++) {
        fx = (float)x / (float)(1<<10);
        fy = exp(log(2)*fx);
        y = FixedPointExp2Q10(x);
        
        fy2 = (float)y / (float)(1 <<10);
        relerr = fabs(2.0f * (fy2 - fy) / (fy2 + fy));
        ASSERT_TRUE(relerr < reltol);
    }
    
    
    for (x = -1; x > -32768; x--) {
        fx = (float)x / (float)(1<<10);
        fy = exp(log(2)*fx);
        y = FixedPointExp2Q10(x);
        
        fy2 = (float)y / (float)(1 <<10);
        err = fabs(fy2 - fy);
        
        if (err >= tol) {
            std::cerr << err << ":"<< x << std::endl;
            ASSERT_TRUE(err < tol);
        }
    }

    
    
    int foo =3 ;
    foo++;
}

