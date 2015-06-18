#include "gtest/gtest.h"
#include "../kitsune/prox_signal.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>

class TestProx : public ::testing::Test {
protected:
    virtual void SetUp() {
        
    }
    
    virtual void TearDown() {
        
    }
    
};


TEST_F(TestProx,TestFlatSignal) {
    ProxSignal_Init();
    ProxGesture_t gesture = proxGestureNone;
    
    for (int t = 0; t < 100; t++) {
        gesture = ProxSignal_UpdateChangeSignals(20000);
        
        ASSERT_TRUE(gesture == proxGestureNone);
    }
    
    gesture = ProxSignal_UpdateChangeSignals(18000);
    
    ASSERT_TRUE(gesture == proxGestureWave);
    
    
}