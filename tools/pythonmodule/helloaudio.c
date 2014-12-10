#include "../../kitsune/audiofeatures.h"
#include "../../kitsune/audioclassifier.h"

#include "../../kitsune/debugutils/debuglog.h"
#include <stdio.h>
#include <string.h>

static int64_t _counter;
static int _gotcallback;
static Segment_t _segment;
static int16_t _feats[NUM_AUDIO_FEATURES];
static uint8_t _bigbuf[100000];

static void AudioFeaturesCallback(int64_t samplecount, const AudioFeatures_t * pSegment) {
    const char * tag = NULL;
    
   // _gotcallback = 1;

   // memcpy(&_segment,pSegment,sizeof(Segment_t));
   // memcpy(_feats,feats,sizeof(int16_t)*NUM_AUDIO_FEATURES);
   
       
    //DEBUG_LOG_S16("featAudio",tag,feats,NUM_AUDIO_FEATURES,pSegment->t1,pSegment->t2);

}

void Init(void) {
    _counter = 0;
    DebugLog_Initialize(NULL);
    AudioClassifier_Init(NULL);
    AudioClassifier_SetStorageBuffers((void * ) _bigbuf,sizeof(_bigbuf));
    AudioFeatures_Init(AudioClassifier_DataCallback);

}

void Deinit(void) {
    DebugLog_Deinitialize();
}

void GetAudioFeatures(int feats[NUM_AUDIO_FEATURES]) {
    int16_t j;
    
    for (j = 0; j < NUM_AUDIO_FEATURES; j++) {
        feats[j] = _feats[j];
    }
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

int SetAudioData(int buf[AUDIO_FFT_SIZE]) {
    int16_t shortbuf[AUDIO_FFT_SIZE];
    int16_t i;

    for (i = 0; i < AUDIO_FFT_SIZE; i++) {
        shortbuf[i] = (int16_t)buf[i];
    }

    //printf("%d,%d,%d\n",shortbuf[0],shortbuf[1],shortbuf[2]);

    _gotcallback = 0;
    AudioFeatures_SetAudioData(shortbuf,_counter);

    _counter++;
    return _gotcallback;
}
