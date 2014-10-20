
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

#define CIRCULAR_FEATBUF_SIZE_2N (5)
#define CIRCULAR_BUF_SIZE (1 << CIRCULAR_FEATBUF_SIZE_2N)
#define CIRCULAR_BUF_MASK (CIRCULAR_BUF_SIZE - 1)

#define MAX_NUMBER_CLASSES (5)
#define EXPECTED_NUMBER_OF_CLASSIFIER_INPUTS (NUM_AUDIO_FEATURES)

#define CLASS_OF_INTEREST_TO_ENABLE_CALLBACK (0)

#define RECORD_DURATION_IN_MS (5000)
#define RECORD_DURATION_IN_FRAMES (RECORD_DURATION_IN_MS / SAMPLE_PERIOD_IN_MILLISECONDS)


typedef struct {
    
} AudioClassifier_t;


#define BUF_SIZE_IN_CHUNK (32)

#define CHUNK_BUF_SIZE_2N (6)
#define CHUNK_BUF_SIZE (1 << CHUNK_BUF_SIZE_2N)
#define CHUNK_BUF_MASK (CHUNK_BUF_SIZE - 1)

typedef struct {
    uint8_t packedbuf[BUF_SIZE_IN_CHUNK][NUM_AUDIO_FEATURES/2];// 32 x 16 = 2^5 * 2^4 = 2^9 = 256 bytes
    int16_t relativeEnergy[BUF_SIZE_IN_CHUNK]; //32 * 2 = 64bytes
    int64_t samplecount; // 8 bytes
    int16_t maxenergy; // 2 bytes
} AudioFeatureChunk_t; //330 bytes

typedef struct {
    
    //cicular buffer of incoming data
    uint8_t packedbuf[CIRCULAR_BUF_SIZE][NUM_AUDIO_FEATURES/2];
    int16_t relativeenergy[CIRCULAR_BUF_SIZE];//
    int16_t totalenergy[CIRCULAR_BUF_SIZE];//

    //"long term storage"
    AudioFeatureChunk_t chunkbuf[CHUNK_BUF_SIZE];
    
    uint16_t chunkbufidx;
    uint16_t incomingidx;
    uint16_t numincoming;
    uint8_t isThereAnythingInteresting;

    
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

static void UnpackFeats8(int8_t * unpacked8, const uint8_t * datahead) {
    uint8_t i;

    for (i = 0; i < NUM_AUDIO_FEATURES/2; i++) {
        //lsb first
        *unpacked8 = (int16_t) (datahead[i] & 0x0F);
        unpacked8++;
        *unpacked8 = (int16_t) ( (datahead[i] & 0xF0) >> 4);
        unpacked8++;
    }
    
}

static void CopyCircularBufferToPermanentStorage(int64_t samplecount) {
    
    AudioFeatureChunk_t chunk;
    uint16_t idx1,idx2;
    int32_t size1,size2;
    uint16_t i;
    int16_t max;
    
 
    
    idx2 = (_buffer.incomingidx - 1) & CIRCULAR_BUF_MASK; //latest
    idx1 = _buffer.incomingidx; //oldest
    /* 
        t2 t1
      ---| |-------
     
     t1            t2
     |-------------|
     */
    
    //copy circular buf out in chronological order
    size1 = CIRCULAR_BUF_SIZE - idx1;
    memcpy(&chunk.packedbuf[0][0],&_buffer.packedbuf[idx1][0],size1*NUM_AUDIO_FEATURES*sizeof(uint8_t));
    memcpy(&chunk.relativeEnergy[0],&_buffer.relativeenergy[idx1],size1*sizeof(int16_t));
    
    if (size1 < CIRCULAR_BUF_SIZE) {
        size2 = idx2;
        memcpy(&chunk.packedbuf[size1][0],&_buffer.packedbuf[0][0],size2*NUM_AUDIO_FEATURES*sizeof(uint8_t));
        memcpy(&chunk.relativeEnergy[size1],&_buffer.relativeenergy[0],size2*sizeof(int16_t));
    }
    
    /* find max in energy buffer */
    max = MIN_INT_16;
    for (i = 0 ; i < CIRCULAR_BUF_SIZE; i++) {
        if (_buffer.totalenergy[i] > max) {
            max = _buffer.totalenergy[i];
        }
    }
    chunk.maxenergy = max;
    chunk.samplecount = samplecount;
    
    /* START CRITICAL SECTION AROUND STORAGE BUFFERS */
    if (_lockFunc) {
        _lockFunc();
    }
    
    memcpy(&_buffer.chunkbuf[_buffer.chunkbufidx],&chunk,sizeof(chunk));
    _buffer.chunkbufidx++;
    _buffer.chunkbufidx &= CHUNK_BUF_MASK;
    
    /* END CRITICAL SECTION  */
    if (_unlockFunc) {
        _unlockFunc();
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
    uint16_t idx;

    
    
    /* 
     
       Data comes in, and if it doesn't pass the energy threshold for being interesting, we don't save it off.
       
       Meanwhile, we keep a running circular buffer.
     
     */
    
    idx = _buffer.incomingidx;
    PackFeats(_buffer.packedbuf[idx],pfeats->feats4bit);
    _buffer.relativeenergy[idx] = pfeats->logenergyOverBackroundNoise;
    _buffer.totalenergy[idx] = pfeats->logenergy;
    
    //increment circular buffer index
    _buffer.incomingidx++;
    _buffer.incomingidx &= CIRCULAR_BUF_MASK;
    
    if (_buffer.numincoming < CIRCULAR_BUF_SIZE) {
        _buffer.numincoming++;
    }
    
    if (pfeats->logenergyOverBackroundNoise > MIN_CLASSIFICATION_ENERGY) {
        _buffer.isThereAnythingInteresting = TRUE;
    }
    
    //ready for storage!
    if (_buffer.isThereAnythingInteresting == TRUE && _buffer.numincoming == CIRCULAR_BUF_SIZE) {
        
        //this may block... hopefully not for too long?
        CopyCircularBufferToPermanentStorage(samplecount);
    }
    
   
    
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

/* sadly this is not stateless, but was the only way to serialize chunks one at a time */
static const_MatDesc_t * GetNextMatrixCallback(uint8_t isFirst) {
    /* STATIC!  */
    static uint16_t idx;
    static uint8_t state;
    

    /* On the stack */
    const AudioFeatureChunk_t * pchunk;
    int8_t unpackedbuffer[BUF_SIZE_IN_CHUNK][NUM_AUDIO_FEATURES]; //32 * 16 * 1 = 512 bytes
    int16_t * bufptr = (int16_t *) &unpackedbuffer[0][0];
    const_MatDesc_t desc;
    desc.tags = "";
    desc.source = "";
    
    uint16_t i;
    
    if (isFirst) {
        idx = _buffer.chunkbufidx; //oldest
        state = 0;
    }
    
    pchunk = &_buffer.chunkbuf[idx];

    desc.t1 = pchunk->samplecount;
    desc.t2 = desc.t1 + CHUNK_BUF_SIZE*2; //*2 because we are decimating audio samples by 2
   
    
    if (state == 0) {
        desc.id = "feature_chunk";

        for (i = 0; i < BUF_SIZE_IN_CHUNK; i++) {
            UnpackFeats8(unpackedbuffer[i], pchunk->packedbuf[i]);
        }
        
        desc.data.len = BUF_SIZE_IN_CHUNK * NUM_AUDIO_FEATURES;
        desc.data.type = esint8;
        desc.data.data.sint8 = &unpackedbuffer[0][0];
        desc.rows = BUF_SIZE_IN_CHUNK;
        desc.cols = NUM_AUDIO_FEATURES;

        state = 1;
        
    }
    else {
        desc.id = "energy_chunk";

        for (i = 0; i < BUF_SIZE_IN_CHUNK; i++) {
            *bufptr = pchunk->relativeEnergy[i];
            bufptr++;
        }
        
        *bufptr = pchunk->maxenergy;
        
        
        state = 0;
        idx++;
        idx &= CHUNK_BUF_MASK;

    }
    UnpackFeats16
    
    
    
    
    return 0;
}


uint32_t AudioClassifier_GetSerializedBuffer(pb_ostream_t * stream,const char * macbytes, uint32_t unix_time,const char * tags, const char * source) {
    
    uint32_t size = 0;
    
    /* START CRITICAL SECTION AROUND STORAGE BUFFERS */
    if (_lockFunc) {
        _lockFunc();
    }
    
    size = SetMatrixMessage(stream, macbytes, unix_time,GetNextMatrixCallback);

    /* END CRITICAL SECTION  */
    if (_unlockFunc) {
        _unlockFunc();
    }

    

    
#if 0
    bufinfo[0] = _buffer.idx;
    bufinfo[1] = _buffer.numsamples;
    
    const_MatDesc_t descs[CHUNK_BUF_SIZE];
        
    
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
    

    /* now reset the buffer */
    _buffer.idx = 0;
    _buffer.numsamples = 0;
#endif
    return size;

}

