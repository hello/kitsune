#include "audioclassifier.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>     /* abs */
#include <stdio.h>
#include "fft.h"
#include "debugutils/debuglog.h"


/*  Local definitions  */
#define SIMILARITY_MODES_SIZE_2N (4)
#define SIMILARITY_MODES_SIZE (1 << SIMILARITY_MODES_SIZE_2N)
#define SIMILARITY_MODES_MASK (SIMILARITY_MODES_SIZE - 1)

static const int16_t k_similarity_threshold = TOFIX(0.707f,10);

typedef struct {
    Segment_t seg;
    int8_t feat[NUM_AUDIO_FEATURES];
} FeatMode_t;

/* Local types */
typedef struct {
    FeatMode_t featmodes[SIMILARITY_MODES_SIZE];
    uint8_t currentIdx;
    uint8_t numvecs;
    SegmentAndFeatureCallback_t fpNovelDataCallback;
} AudoClassifier_t;


/* static memory  */
static AudoClassifier_t _data;


void AudioClassifier_Init(SegmentAndFeatureCallback_t novelDataCallback) {
    memset(&_data,0,sizeof(_data));
    _data.fpNovelDataCallback = novelDataCallback;
}


void Scale16VecTo8(int8_t * pscaled, const int16_t * vec,uint16_t n) {
    int16_t extrema = 0;
    int16_t i;
    int16_t absval;
    int16_t shift;
    
    for (i = 0; i < n; i++) {
        absval = abs(vec[i]);
        if (absval > extrema) {
            extrema = absval;
        }
    }
    
    shift = CountHighestMsb(extrema);
    shift -= 7;
    
    if (shift > 0 ) {
        for (i = 0; i < n; i++) {
            pscaled[i] = vec[i] >> shift;
        }
    }
    else {
        for (i = 0; i < n; i++) {
            pscaled[i] = (int8_t)vec[i];
        }
    }
    
}

uint8_t CheckSimilarity(const int8_t * featvec8) {
    /*  Identify if novel  */
    uint16_t idx,i;
    uint8_t isSimilar = FALSE;
    int16_t cosvec;
    
    for (i = _data.currentIdx; i < _data.currentIdx + _data.numvecs; i++) {
        idx = i & SIMILARITY_MODES_MASK;
        const FeatMode_t * pmode = &_data.featmodes[idx];
        
        cosvec = cosvec8(pmode->feat, featvec8, NUM_AUDIO_FEATURES);
        
        if (cosvec > k_similarity_threshold) {
            isSimilar = TRUE;
            break;
        }
    }

    return isSimilar;
}

void AddFeatVec(const int8_t * featvec8,const Segment_t * pSegment) {
    uint16_t newidx = (_data.currentIdx + _data.numvecs) & SIMILARITY_MODES_MASK;
    
    FeatMode_t * pmode  = &_data.featmodes[newidx];
    
    memcpy(pmode->feat,featvec8,NUM_AUDIO_FEATURES*sizeof(int8_t));
    memcpy(&pmode->seg,pSegment,sizeof(Segment_t));
    
    
    if (++_data.numvecs > SIMILARITY_MODES_SIZE) {
        _data.numvecs = SIMILARITY_MODES_SIZE;
        _data.currentIdx++;
        _data.currentIdx &= SIMILARITY_MODES_MASK;
    }
}

void AudioClassifier_SegmentCallback(const int16_t * feats, const Segment_t * pSegment) {
    int8_t featvec8[NUM_AUDIO_FEATURES];

    Scale16VecTo8(featvec8,feats,NUM_AUDIO_FEATURES);

    if (!CheckSimilarity(featvec8)) {
        //add similarity vector
        AddFeatVec(featvec8,pSegment);
        
        //this is novel, so upload it
        //! \todo UPLOAD THIS FEATURE VECTOR
        DEBUG_LOG_S16("novelfeat",NULL,feats,NUM_AUDIO_FEATURES,pSegment->t1,pSegment->t2);
        if (_data.fpNovelDataCallback) {
            _data.fpNovelDataCallback(feats,pSegment);
        }
        
    }
    else {
        printf("SIMILAR: %lld, %lld\n",pSegment->t1,pSegment->t2 - pSegment->t1);
    }
    
    DEBUG_LOG_S16("featAudio",NULL,feats,NUM_AUDIO_FEATURES,pSegment->t1,pSegment->t2);
    
    //go classify feature vector
    //! \todo CLASSIFY THIS FEATURE VECTOR
    
}
















