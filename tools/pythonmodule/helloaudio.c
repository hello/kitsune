#include "../../kitsune/audiofeatures.h"
#include "../../kitsune/debugutils/debuglog.h"
#include <stdio.h>
#include <string.h>

static int64_t _counter;
static int _gotcallback;
static Segment_t _segment;
static int32_t _mfccfeats[NUM_MFCC_FEATURES];

static void AudioFeaturesCallback(const int32_t * mfccfeats, const Segment_t * pSegment) {
    const char * tag = NULL;
    
    _gotcallback = 1;

    memcpy(&_segment,pSegment,sizeof(Segment_t));
    memcpy(_mfccfeats,mfccfeats,sizeof(int32_t)*NUM_MFCC_FEATURES);
   
    if (pSegment->type == segmentPacket) {
        tag = "packet";
    }
    else if (pSegment->type == segmentSteadyState){
        tag = "steady";
    }
    
    DEBUG_LOG_S32("featAudio",tag,mfccfeats,NUM_MFCC_FEATURES,pSegment->t1,pSegment->t2);

}

void Init(void) {
    _counter = 0;
    DebugLog_Initialize(NULL);
    AudioFeatures_Init(AudioFeaturesCallback);

}

void Deinit(void) {
    DebugLog_Deinitialize();
}

void GetAudioFeatures(int feats[NUM_MFCC_FEATURES]) {
    memcpy(feats,_mfccfeats,NUM_MFCC_FEATURES*sizeof(int));
}

const char * DumpDebugBuffer() {
    return DebugLog_DumpStringBuf();
}

int GetT1(void) {
    return (int)_segment.t1;
}

int GetT2(void) {
    return (int)_segment.t2;
}

int GetSegmentType(void) {
    return (int)_segment.type;
}

int SetAudioData(int buf[1024]) {
    int16_t shortbuf[1024];
    int16_t i;

    for (i = 0; i < 1024; i++) {
        shortbuf[i] = (int16_t)buf[i];
    }

    //printf("%d,%d,%d\n",shortbuf[0],shortbuf[1],shortbuf[2]);

    _gotcallback = 0;
    AudioFeatures_SetAudioData(shortbuf,10,_counter);

    _counter++;
    return _gotcallback;
}
