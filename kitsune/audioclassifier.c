
#include "audioclassifier.h"
#include "fft.h"
#include "debugutils/debuglog.h"
#include "debugutils/matmessageutils.h"
#include "machinelearning/classifier_factory.h"
#include "machinelearning/linear_svm.h"
#include "hellomath.h"
#include "machinelearning/hmm.h"
#include <math.h>

#include <string.h>

#define CIRCULAR_FEATBUF_SIZE_2N (11)
#define CIRCULAR_BUF_SIZE (1 << CIRCULAR_FEATBUF_SIZE_2N)
#define CIRCULAR_BUF_MASK (CIRCULAR_BUF_SIZE - 1)

#define MAX_NUMBER_CLASSES (5)
#define EXPECTED_NUMBER_OF_CLASSIFIER_INPUTS (NUM_AUDIO_FEATURES)

#define CLASS_OF_INTEREST_TO_ENABLE_CALLBACK (0)

#define RECORD_DURATION_IN_MS (5000)
#define RECORD_DURATION_IN_FRAMES (RECORD_DURATION_IN_MS / SAMPLE_PERIOD_IN_MILLISECONDS)


typedef struct {
    
} AudioClassifier_t;

typedef struct {
    
    uint8_t data[CIRCULAR_BUF_SIZE][NUM_AUDIO_FEATURES/2]; //2048 * 8 = 16K bytes
    int8_t energy[CIRCULAR_BUF_SIZE];// 2K;
    int8_t energyAboveBackground[CIRCULAR_BUF_SIZE];// 2K;

    uint16_t idx;
    uint16_t numsamples;
    int64_t epoch;
    
} DataBuffer_t;


static const char * k_feats_buf_id = "feats4bit";
static const char * k_energies_buf_id = "logenergy";
static const char * k_bufinfo_buf_id = "bufinfo";


static DataBuffer_t _buffer;
static RecordAudioCallback_t _playbackFunc;
static MutexCallback_t _lockFunc;
static MutexCallback_t _unlockFunc;
static Classifier_t _classifier;
static Classifier_t _hmm;

//we have 17 * N * 2 bytes for N classes if it's a linear SVM
//N = 5, let's say, so 17*5 * 2 = 170.  1K is really safe.
static uint32_t _classifierdata[128]; //0.5K
static uint32_t _hmmdata[128]; //0.5K

//Snoring, talking, null -- BEJ 10/15/2014
static const int16_t _defaultsvmdata[3][NUM_AUDIO_FEATURES + 1] =
{{-5996,  1434,  3625,  1872, -6107, -5934,  4998, -4661, -4099,-1030, -9283,  2146, -1400, -7023,  -307,     0, -5635},
    {5848, -1311, -2462,    72,  1673,  3468, -3947,  4357, -1205,3495,  4265,  3143,  -148,  4575,  1832,     0,  2086},
    {-1807,   294,   777,   -53,  1590,   558,  -176,   -17,  4309, -2146,  1908, -3682,  1351,  2740,   239,     0, -2961},
    
    };


#define LOG2_06 (-0.73697f)
#define LOG2_04 (-1.3219f)
static const int16_t _defaulthmmdata[2][5] = {
    {TOFIX(LOG2_06,HMM_LOGPROB_QFIXEDPOINT),TOFIX(LOG2_04,HMM_LOGPROB_QFIXEDPOINT),TOFIX(LOG2_04,HMM_LOGPROB_QFIXEDPOINT),   TOFIX(0.98f,15),TOFIX(0.02f,15)},
    {TOFIX(LOG2_04,HMM_LOGPROB_QFIXEDPOINT),TOFIX(LOG2_06,HMM_LOGPROB_QFIXEDPOINT),TOFIX(LOG2_06,HMM_LOGPROB_QFIXEDPOINT),   TOFIX(0.006f,15),TOFIX(0.994f,15)},
};


static void PackFeats(uint8_t * datahead, const int8_t * feats4bit) {
    uint8_t i;
    const uint8_t * pbytes = (const uint8_t * )feats4bit;
    
    for (i = 0; i < NUM_AUDIO_FEATURES/2; i++) {
        uint8_t idx = 2*i;
        
        datahead[i] = (pbytes[idx]) | (pbytes[idx + 1] << 4);
    }
}

static void InitDefaultClassifier(void) {
    
    _classifier.data = _classifierdata;
    _classifier.classifiertype = linearsvm;
    _classifier.numClasses = 3;
    _classifier.numInputs = NUM_AUDIO_FEATURES;
    _classifier.fpClassifier = LinearSvm_Evaluate;
    
    LinearSvm_Init(_classifier.data, &_defaultsvmdata[0][0], 3, NUM_AUDIO_FEATURES + 1, sizeof(_classifierdata));
    
    
    _hmm.data = _hmmdata;
    _hmm.classifiertype = hmm;
    _hmm.numClasses = 2;
    _hmm.numInputs = 3;
    _hmm.fpClassifier = Hmm_Evaluate;
    
    Hmm_Init(_hmm.data , &_defaulthmmdata[0][0], 2, 5, sizeof(_hmmdata));
    
}


void AudioClassifier_Init(RecordAudioCallback_t recordfunc,MutexCallback_t lockfunc, MutexCallback_t unlockfunc) {
    memset(&_buffer,0,sizeof(_buffer));
    memset(&_classifier,0,sizeof(Classifier_t));
    
    InitDefaultClassifier();
    
    _playbackFunc = recordfunc;
    _lockFunc = lockfunc;
    _unlockFunc = unlockfunc;
}


void AudioClassifier_DeserializeClassifier(pb_istream_t * stream) {
   
    if (!ClassifierFactory_CreateFromSerializedData(_classifierdata,&_classifier,stream,sizeof(_classifierdata))) {
        memset(&_classifier,0,sizeof(Classifier_t));
    }
    
    //input sanity check
    if (_classifier.numInputs != EXPECTED_NUMBER_OF_CLASSIFIER_INPUTS) {
        memset(&_classifier,0,sizeof(Classifier_t));
    }
    
    /* NEED GODDAMNED LOGGING.  Tyrion: Where do errors go?  Tywin: Wherever errors go. */
   
}

void AudioClassifier_DataCallback(int64_t samplecount, const AudioFeatures_t * pfeats) {
    int8_t classes[MAX_NUMBER_CLASSES];
    int8_t probs[2];

    
    /* START CRITICAL SECTION AROUND STORAGE BUFFERS */
    if (_lockFunc) {
        _lockFunc();
    }
    /* Save off data in the big massive buffer */
    PackFeats(_buffer.data[_buffer.idx],pfeats->feats4bit);
    _buffer.energy[_buffer.idx] = pfeats->logenergy >> 8;
    _buffer.idx++;
    _buffer.idx &= CIRCULAR_BUF_MASK;
    
    //increment and limit
    if (++_buffer.numsamples > CIRCULAR_BUF_SIZE) {
        _buffer.numsamples = CIRCULAR_BUF_SIZE;
    }
    
    if (_unlockFunc) {
        _unlockFunc();
    }
    
    /* END CRITICAL SECTION  */
    
    /* Run classifier if energy is signficant */
    if (pfeats->logenergyOverBackroundNoise > MIN_CLASSIFICATION_ENERGY) {
        if (_classifier.data && _classifier.fpClassifier) {

            _classifier.fpClassifier(_classifier.data,classes,pfeats->feats4bit,3); //4 bits feats, SQ3 feats.

            
            _hmm.fpClassifier(_hmm.data,probs,classes,0);
            
            if (probs[CLASS_OF_INTEREST_TO_ENABLE_CALLBACK] > TOFIX(0.95f,HMM_LOGPROB_QFIXEDPOINT) && _playbackFunc) {
                RecordAudioRequest_t req;
                memset(&req,0,sizeof(req));
                
                req.durationInFrames = RECORD_DURATION_IN_FRAMES;
                
                _playbackFunc(&req);
            }
        }
    }
    else {
        //update the state transition matrix
        _hmm.fpClassifier(_hmm.data,probs,NULL,0);

    }
    
    DEBUG_LOG_S8("probs", NULL, probs, 2, samplecount, samplecount);

}



uint32_t AudioClassifier_GetSerializedBuffer(pb_ostream_t * stream,const char * macbytes, uint32_t unix_time,const char * tags, const char * source) {
    
    uint32_t size;
    uint16_t bufinfo[2];
    
    bufinfo[0] = _buffer.idx;
    bufinfo[1] = _buffer.numsamples;
    
    const_MatDesc_t descs[3] = {
        {k_feats_buf_id,tags,source,{},_buffer.numsamples,NUM_AUDIO_FEATURES/2,_buffer.epoch,_buffer.epoch},
        {k_energies_buf_id,tags,source,{},1,_buffer.numsamples,_buffer.epoch,_buffer.epoch},
        {k_bufinfo_buf_id,tags,source,{},1,2,_buffer.epoch,_buffer.epoch}
    };
    
    /*****************/
    descs[0].data.len = _buffer.numsamples*NUM_AUDIO_FEATURES/2;
    descs[0].data.type = euint8;
    descs[0].data.data.uint8 = &_buffer.data[0][0];
    
    descs[1].data.len = _buffer.numsamples;
    descs[1].data.type = esint8;
    descs[1].data.data.sint8 = _buffer.energy;
    
    descs[2].data.len = sizeof(bufinfo) / sizeof(uint16_t);
    descs[2].data.type = euint16;
    descs[2].data.data.uint16 = bufinfo;
    
    size = SetMatrixMessage(stream, macbytes, unix_time, descs, sizeof(descs) / sizeof(MatDesc_t));

    /* now reset the buffer */
    _buffer.idx = 0;
    _buffer.numsamples = 0;
    
    return size;

}

