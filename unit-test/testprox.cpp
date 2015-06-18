#include "gtest/gtest.h"
#include "../kitsune/prox_signal.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>
#include <random>

class TestProx : public ::testing::Test {
protected:
    virtual void SetUp() {
        std::srand(std::time(0));
        ProxSignal_Init();
    }
    
    virtual void TearDown() {
        
    }
    
};


static int getRandom(int mean, int radius) {
    int r = std::rand() % (2*radius);
    return mean + r - radius;
}



TEST_F(TestProx,TestSimpleWave) {
    ProxGesture_t gesture = proxGestureNone;
    
    for (int t = 0; t < 100; t++) {
        gesture = ProxSignal_UpdateChangeSignals(20000);
        
        ASSERT_TRUE(gesture == proxGestureNone);
    }
    
    gesture = ProxSignal_UpdateChangeSignals(18000);
    
    ASSERT_TRUE(gesture == proxGestureWave);
    
    
}


TEST_F(TestProx,TestSimpleHold) {
    ProxGesture_t gesture = proxGestureNone;
    
    for (int t = 0; t < 100; t++) {
        ProxSignal_UpdateChangeSignals(20000);
    }
    
    bool foundHold = false;
    
    for (int t = 0; t < 100; t++) {
        gesture = ProxSignal_UpdateChangeSignals(9000);
        if (gesture == proxGestureHold) {
            foundHold = true;
        }
    }
    
    ASSERT_TRUE(foundHold);
    
    bool foundRelease = false;

    for (int t = 0; t < 100; t++) {
        gesture = ProxSignal_UpdateChangeSignals(20000);
        if (gesture == proxGestureRelease) {
            foundRelease = true;
        }
    }
    
    ASSERT_TRUE(foundRelease);

    
}

TEST_F(TestProx, TestIsStableWhenNoisy) {
    ProxGesture_t gesture = proxGestureNone;

    for (int t = 0; t < 100; t++) {
        gesture = ProxSignal_UpdateChangeSignals(20000);
    }
    
    int radius = 50;
    
    bool wasEverUnstable = false;
    for (int t = 0; t < 1000; t++) {
        gesture = ProxSignal_UpdateChangeSignals(getRandom(20000,radius));
        
        if (gesture != proxGestureNone) {
            wasEverUnstable = true;
        }
    }
    
    ASSERT_FALSE(wasEverUnstable);

}

