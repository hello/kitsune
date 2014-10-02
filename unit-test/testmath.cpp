#include "gtest/gtest.h"
#include "../kitsune/hellomath.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>

class TestMath : public ::testing::Test {
protected:
    virtual void SetUp() {
        
    }
    
    virtual void TearDown() {
        
    }
    
    
};


TEST_F(TestMath,TestLog2Q10) {
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

TEST_F (TestMath,TestExp2Q10) {
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

TEST_F (TestMath,TestVecNormalize8) {
    int8_t vec[16];
    
    
    memset(vec,0,sizeof(vec));
    
    ASSERT_FALSE(VecNormalize8(vec, 16));
    
    memset(vec,0x7F,16);
    
    VecNormalize8(vec, 16);
    
    for (int i = 0; i < 16; i++) {
        ASSERT_TRUE(vec[i] == 32); //0.25 in q7
    }
    
    memset(vec,0x80,16);
    
    VecNormalize8(vec, 16);
    
    for (int i = 0; i < 16; i++) {
        ASSERT_TRUE(vec[i] == -32); //0.25 in q7
    }
    
    memset(vec,0,16);
    vec[0] = 1;
    
    VecNormalize8(vec, 16);
    
    ASSERT_TRUE(vec[0] == 127);
    
    for (int i = 1; i < 16; i++) {
        ASSERT_TRUE(vec[i] == 0); //0.25 in q7
    }
    
    
    
}


TEST_F(TestMath,TestSqrt) {
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


TEST_F(TestMath,TestCountMsb) {
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

TEST_F(TestMath,TestMatMul) {
    int16_t a[2][3] = { {-3201  ,-16695  , 27070},
        {761 , -24506  , 14100}};
    
    int16_t b[3][4] = { {-19918  , -1423  ,  5729   , 9816},
        {-8167  , 14371   ,19104  , -5184},
        {-28528 ,  17237  ,   965 , -20749}};
    
    int16_t c[2][4];
    const int16_t cref[2][4] = { {-17461  ,  7057  , -9496 , -15459},
                                 {-6631  , -3363  ,-13739  , -4824}};
    
    MatMul(&c[0][0], &a[0][0], &b[0][0], 2, 3, 4, 15);
    
    int maxerr = 0;
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 4; i++) {
            int diff = cref[j][i] - c[j][i];
            diff = abs(diff);
            
            if (diff > maxerr) {
                maxerr = diff;
            }
        }
    }
    
    ASSERT_TRUE(maxerr < 5);
}

