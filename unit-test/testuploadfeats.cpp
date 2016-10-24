#include "gtest/gtest.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>
#include "../kitsune/audio_features_upload_task_helpers.h"



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

