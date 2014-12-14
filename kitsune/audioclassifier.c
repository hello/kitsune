
#include "audioclassifier.h"
#include "fft.h"
#include "debugutils/debuglog.h"
#include "debugutils/matmessageutils.h"
#include "machinelearning/classifier_factory.h"
#include "machinelearning/linear_svm.h"
#include "hellomath.h"
#include "machinelearning/hmm.h"
#include <math.h>
#include <assert.h>
#include <string.h>
#include "rawaudiostatemachine.h"

#define CIRCULAR_FEATBUF_SIZE_2N (5)
#define CIRCULAR_BUF_SIZE (1 << CIRCULAR_FEATBUF_SIZE_2N)
#define CIRCULAR_BUF_MASK (CIRCULAR_BUF_SIZE - 1)
#define BUF_SIZE_IN_CHUNK (32)

#define MAX_NUMBER_CLASSES (5)
#define EXPECTED_NUMBER_OF_CLASSIFIER_INPUTS (NUM_AUDIO_FEATURES)

#define CLASS_OF_INTEREST_TO_ENABLE_CALLBACK (0)

#define RECORD_DURATION_IN_MS (10000)
#define RECORD_DURATION_IN_FRAMES (RECORD_DURATION_IN_MS / SAMPLE_PERIOD_IN_MILLISECONDS)


#ifndef true
#define true (1)
#endif

#ifndef false
#define false (0)
#endif




typedef struct {
    uint8_t packedbuf[BUF_SIZE_IN_CHUNK][NUM_AUDIO_FEATURES/2];// 32 x 16 = 2^5 * 2^4 = 2^9 = 256 bytes
    int16_t relativeEnergy[BUF_SIZE_IN_CHUNK]; //32 * 2 = 64bytes
    int64_t samplecount; // 8 bytes
    int16_t maxenergy; // 2 bytes
} AudioFeatureChunk_t; //330 bytes

typedef struct {
    
    //cicular buffer of incoming data
    uint8_t packedbuf[CIRCULAR_BUF_SIZE][NUM_AUDIO_FEATURES/2]; //32 * 8 = 256 bytes
    int16_t relativeenergy[CIRCULAR_BUF_SIZE];//
    int16_t totalenergy[CIRCULAR_BUF_SIZE];//

    uint16_t chunkbufidx;
    uint16_t numchunkbuf;
    
    AudioFeatureChunk_t * pchunkbuf;
    uint32_t chunk_buf_size;

    
    uint16_t incomingidx;
    uint16_t numincoming;
    
    uint8_t isThereAnythingInteresting;

    
} DataBuffer_t;

typedef struct {
    const DataBuffer_t * buf;
    uint16_t currentidx;
    uint8_t state;
    int8_t unpackedbuffer[BUF_SIZE_IN_CHUNK][NUM_AUDIO_FEATURES]; //32 * 16 * 1  = 512 bytes

} Encoder_t;

static const char * k_id_feature_chunk = "feature_chunk";
static const char * k_id_energy_chunk = "energy_chunk";

//"long term storage"
static DataBuffer_t _buffer;
static Classifier_t _classifier;
static Classifier_t _hmm;

//we have 17 * N * 2 bytes for N classes if it's a linear SVM
//N = 5, let's say, so 17*5 * 2 = 170.  1K is really safe.
static uint32_t _classifierdata[128]; //0.5K
static uint32_t _hmmdata[128]; //0.5K

//Snoring, talking, null -- BEJ 10/15/2014
static const int16_t _defaultsvmdata[3][NUM_AUDIO_FEATURES + 1] =
{
{-5584,299,4639,2525,-5064,-6144,5710,-6894,-8295,-697,-7778,-329,-1929,-3721,319,10,-6453},
{6444,181,-4261,-64,5062,7000,-937,4706,2571,5085,5564,1009,1499,3564,2169,-156,4466},
{-2728,-106,1799,-622,-240,-2879,-4268,-344,5197,-5411,697,-1083,2594,193,-2716,87,-4480}
    
    };


/* ATTENTION!   
  
  - The conditional probablities are in Q10   
  - The state transition matrix is in Q15 
 
   This info will come in handy when you send a classifier on down from the server.
 */

static const int16_t _defaulthmmdata[2][5] = {
    {TOFIX(0.7f,HMM_LOGPROB_QFIXEDPOINT),TOFIX(0.4f,HMM_LOGPROB_QFIXEDPOINT),TOFIX(0.4f,HMM_LOGPROB_QFIXEDPOINT),   TOFIX(0.98f,15),TOFIX(0.02f,15)},
    {TOFIX(0.3f,HMM_LOGPROB_QFIXEDPOINT),TOFIX(0.6f,HMM_LOGPROB_QFIXEDPOINT),TOFIX(0.6f,HMM_LOGPROB_QFIXEDPOINT),   TOFIX(0.006f,15),TOFIX(0.994f,15)},
};

static inline uint8_t pack_int8_to_int4(int8_t x) {
    const uint8_t sign = x < 0;
    
    return (x & 0x07) | (sign * 8);
}

// assumes two's complement architecture (who the heck doesn't do this these days?)
static inline void unpack_int4_pair_to_int8(uint8_t packed, int8_t * upper, int8_t * lower) {
    *lower = packed & 0x07;
    if (packed & 0x08) {
        *lower |= 0xF8; //sign extension
    }
    
    *upper = (packed & 0x70) >> 4;
    if (packed & 0x80) {
        *upper |= 0xF8; //sign extension
    }
    
    
}

static void PackFeats(uint8_t * datahead, const int8_t * feats4bit) {
    uint8_t i;
    uint8_t idx;
    
    
    for (i = 0; i < NUM_AUDIO_FEATURES/2; i++) {
        idx = 2*i;
        
        datahead[i] = pack_int8_to_int4(feats4bit[idx]);
        datahead[i] |= (pack_int8_to_int4(feats4bit[idx + 1]) << 4);
    }
}



static void UnpackFeats8(int8_t * unpacked8, const uint8_t * datahead) {
    uint8_t i;
    
    for (i = 0; i < NUM_AUDIO_FEATURES/2; i++) {
        //lsb first
        
        unpack_int4_pair_to_int8(datahead[i],unpacked8+1,unpacked8);
        unpacked8 += 2;
    }
    
}

#if 0
static void TestPackUnpack(void) {
    int8_t input[NUM_AUDIO_FEATURES] = {-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7};
    int8_t out[NUM_AUDIO_FEATURES] = {0};
    uint8_t packed[NUM_AUDIO_FEATURES/2];
    int foo = 3;
    PackFeats(packed, input);
    UnpackFeats8(out, packed);
    
    foo++;
    
}
#endif

static void CopyCircularBufferToPermanentStorage(int64_t samplecount) {
    
    AudioFeatureChunk_t chunk;
    uint16_t idx;
    int32_t size1,size2;
    uint16_t i;
    int16_t max;
    
    idx = _buffer.incomingidx; //oldest
    /* 
        t2 t1
      ---| |-------
     
     t1            t2
     |-------------|
     */
    
    //copy circular buf out in chronological order
    size1 = CIRCULAR_BUF_SIZE - idx;
    memcpy(&chunk.packedbuf[0][0],&_buffer.packedbuf[idx][0],size1*NUM_AUDIO_FEATURES/2*sizeof(uint8_t));
    memcpy(&chunk.relativeEnergy[0],&_buffer.relativeenergy[idx],size1*sizeof(int16_t));
    
    if (size1 < CIRCULAR_BUF_SIZE) {
        size2 = idx;
        memcpy(&chunk.packedbuf[size1][0],&_buffer.packedbuf[0][0],size2*NUM_AUDIO_FEATURES/2*sizeof(uint8_t));
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
    
    
    memcpy(&_buffer.pchunkbuf[_buffer.chunkbufidx],&chunk,sizeof(chunk));
    _buffer.chunkbufidx++;
    
    //wrap
    if (_buffer.chunkbufidx >= _buffer.chunk_buf_size) {
        _buffer.chunkbufidx -= _buffer.chunk_buf_size;
    }
    
    if (_buffer.numchunkbuf < _buffer.chunk_buf_size) {
        _buffer.numchunkbuf++;
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

void AudioClassifier_SetStorageBuffers(void * buffer, uint32_t buf_size_in_bytes) {
    memset(&_buffer,0,sizeof(_buffer));

	_buffer.pchunkbuf = (AudioFeatureChunk_t *)buffer;
    _buffer.chunk_buf_size = buf_size_in_bytes / sizeof(AudioFeatureChunk_t);

}

void AudioClassifier_Init(RecordAudioCallback_t recordfunc) {
    memset(&_buffer,0,sizeof(_buffer));
    memset(&_classifier,0,sizeof(Classifier_t));
    memset(&_hmm,0,sizeof(_hmm));
    
    InitDefaultClassifier();

    RawAudioStateMachine_Init(recordfunc);

}


void AudioClassifier_DeserializeClassifier(pb_istream_t * stream, void * data) {
   
    if (!ClassifierFactory_CreateFromSerializedData(_classifierdata,&_classifier,stream,sizeof(_classifierdata))) {
        memset(&_classifier,0,sizeof(Classifier_t));
    }
    
    //input sanity check
    if (_classifier.numInputs != EXPECTED_NUMBER_OF_CLASSIFIER_INPUTS) {
        memset(&_classifier,0,sizeof(Classifier_t));
    }
    
    /* NEED GODDAMNED LOGGING.  Tyrion: Where do errors go?  Tywin: Wherever errors go. */
   
}

void AudioClassifier_DataCallback(const AudioFeatures_t * pfeats) {
    int8_t classes[MAX_NUMBER_CLASSES];
    int8_t probs[2];
    uint16_t idx;

    
    
    /* 
     
       Data comes in, and if it doesn't pass the energy threshold for being interesting, we don't save it off.
       
       Meanwhile, we keep a running circular buffer.
     
     */
    
    /************************
     THE BUFFERING SECTION
     ***********************/
    
    //do nothing if we do not have a storage buffer allocated
    if (!_buffer.pchunkbuf || _buffer.chunk_buf_size == 0) {
    	return;
    }
    
    idx = _buffer.incomingidx;
    PackFeats(_buffer.packedbuf[idx],pfeats->feats4bit);
    _buffer.relativeenergy[idx] = pfeats->logenergyOverBackroundNoise;
    _buffer.totalenergy[idx] = pfeats->logenergy;
    
    //increment circular buffer index
    _buffer.incomingidx++;
    _buffer.incomingidx &= CIRCULAR_BUF_MASK;
    
    //increment until full
    if (_buffer.numincoming < CIRCULAR_BUF_SIZE) {
        _buffer.numincoming++;
    }
    
    //determine if anything interesting happend, energy-wise
    if (pfeats->logenergyOverBackroundNoise > MIN_CLASSIFICATION_ENERGY) {
        _buffer.isThereAnythingInteresting = true;
    }
    
    //if something interesting happend and the circular buffer is full
    //dump it to storage
    if (_buffer.isThereAnythingInteresting == true && _buffer.numincoming == CIRCULAR_BUF_SIZE) {
        
        //this may block... hopefully not for too long?
        CopyCircularBufferToPermanentStorage(pfeats->samplecount);
        _buffer.numincoming = 0; //"empty" the buffer
        _buffer.incomingidx = 0;
    }
    
   
    /************************
     THE CLASSIFIER SECTION
     ***********************/
    
    /* Run classifier if energy is signficant */
    if (pfeats->logenergyOverBackroundNoise > MIN_CLASSIFICATION_ENERGY) {
        if (_classifier.data && _classifier.fpClassifier) {

            _classifier.fpClassifier(_classifier.data,classes,pfeats->feats4bit,3); //4 bits feats, SQ3 feats.

            
            /*  This is just naive Bayes */
            _hmm.fpClassifier(_hmm.data,probs,classes,0);
            
#if 0
            /* This could trigger an upload */
            RawAudioStateMachine_SetProbabilityOfDesiredClass(probs[CLASS_OF_INTEREST_TO_ENABLE_CALLBACK]);

#endif
        }
    }
    else {
        //update the state transition matrix
        _hmm.fpClassifier(_hmm.data,probs,NULL,0);

    }
    
    DEBUG_LOG_S8("probs", NULL, probs, 2, pfeats->samplecount, pfeats->samplecount);

}

/* sadly this is not stateless, but was the only way to serialize chunks one at a time */
static uint8_t GetNextMatrixCallback(uint8_t isFirst,const_MatDesc_t * pdesc,void * data) {
    
    
    Encoder_t * encodedata = (Encoder_t *)data;
    
    const AudioFeatureChunk_t * pchunk;
    int16_t * bufptr16 = (int16_t *) &encodedata->unpackedbuffer[0][0];
    const int16_t * beginning16 = (int16_t *) &encodedata->unpackedbuffer[0][0];

    uint16_t endidx;
    uint16_t i;
    
    assert(encodedata->buf == &_buffer);
    
    memset(pdesc,0,sizeof(const_MatDesc_t));
    
    if (encodedata->buf->numchunkbuf == 0 || encodedata->buf->pchunkbuf == NULL) {
        return false; //stop
    }
    
    if (isFirst) {
        encodedata->currentidx = encodedata->buf->chunkbufidx; //oldest
        encodedata->state = 1;
    }
    else if (encodedata->state == 0) {
        return false; //stop, termination condition
    }
    
    endidx = encodedata->buf->chunkbufidx + encodedata->buf->numchunkbuf;
    
    //wrap
    if (endidx >= encodedata->buf->chunk_buf_size) {
        endidx -= encodedata->buf->chunk_buf_size;
    }
    
    pchunk = &encodedata->buf->pchunkbuf[encodedata->currentidx];

    pdesc->t1 = pchunk->samplecount;
    pdesc->t2 = pdesc->t1 + BUF_SIZE_IN_CHUNK*2; //*2 because we are decimating audio samples by 2
   
    
    if (encodedata->state == 1) {
        pdesc->id = k_id_feature_chunk;

        for (i = 0; i < BUF_SIZE_IN_CHUNK; i++) {
            UnpackFeats8(encodedata->unpackedbuffer[i], pchunk->packedbuf[i]);
        }
      /*
        for (i = 0; i < NUM_AUDIO_FEATURES; i++) {
            printf("%d,",unpackedbuffer[BUF_SIZE_IN_CHUNK-1][i]);
        }
        
        printf("\n");
       */
       
        pdesc->data.len = BUF_SIZE_IN_CHUNK * NUM_AUDIO_FEATURES;
        pdesc->data.type = esint8;
        pdesc->data.data.sint8 = &encodedata->unpackedbuffer[0][0];
        pdesc->rows = BUF_SIZE_IN_CHUNK;
        pdesc->cols = NUM_AUDIO_FEATURES;

        encodedata->state = 2;
        
    }
    else if (encodedata->state == 2) {
        pdesc->id = k_id_energy_chunk;

        //re-use the unpacked buffer, but let's pretend it's 16 bit...
        for (i = 0; i < BUF_SIZE_IN_CHUNK; i++) {
            *bufptr16 = pchunk->relativeEnergy[i];
            bufptr16++;
        }
        
        *bufptr16 = pchunk->maxenergy;
        
        /*
        for (i = 0; i < BUF_SIZE_IN_CHUNK + 1; i++) {
            printf("%d,",beginning16[i]);
        }
        printf("\n");
        */
        
        
        pdesc->data.len = BUF_SIZE_IN_CHUNK + 1;
        pdesc->data.type = esint16;
        pdesc->data.data.sint16 = beginning16;
        pdesc->rows = 1;
        pdesc->cols = BUF_SIZE_IN_CHUNK + 1;
        
        encodedata->state = 1;
        encodedata->currentidx++;
        
        //wrap
        if (encodedata->currentidx >= encodedata->buf->chunk_buf_size) {
            encodedata->currentidx -= encodedata->buf->chunk_buf_size;
        }
        
        //have we reached the end?
        if (encodedata->currentidx == endidx) {
            encodedata->state = 0;
        }

    }
    
    return true;
}


uint32_t AudioClassifier_EncodeAudioFeatures(pb_ostream_t * stream,void * encode_data) {
    
    uint32_t size = 0;
    uint8_t macbytes[6] = {0};
    char device_id[32] = {0};
    uint32_t unix_time = 0;

    if (encode_data) {
        DeviceCurrentInfo_t * pinfo = (DeviceCurrentInfo_t *) encode_data; //passing in mac address
        memcpy(macbytes,pinfo->mac,6);
        strncpy(device_id,pinfo->device_id,sizeof(device_id));
        unix_time = pinfo->unix_time;
    }

    MatrixListEncodeContext_t context;
    Encoder_t encoderstruct;
    
    memset(&context,0,sizeof(context));
    context.data = &encoderstruct;
    context.func = GetNextMatrixCallback;
    
    memset(&encoderstruct,0,sizeof(encoderstruct));
    encoderstruct.buf = &_buffer;
    
    size = SetMatrixMessage(stream, macbytes,device_id, unix_time,&context);
    

    return size;

}

void AudioClassifier_ResetStorageBuffer(void) {
	/* Buffer read out?  Great, let's reset it */

	_buffer.chunkbufidx = 0;
	_buffer.numchunkbuf = 0;
}
