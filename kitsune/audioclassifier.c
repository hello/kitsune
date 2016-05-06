
#include "audioclassifier.h"
#include "fft.h"
#include "debugutils/debuglog.h"
#include "debugutils/matmessageutils.h"
#include "hellomath.h"
#include "machinelearning/audiohmm.h"
#include <math.h>
#include <string.h>
//#include "uartstdio.h"

#define CIRCULAR_FEATBUF_SIZE_2N (5)
#define CIRCULAR_BUF_SIZE (1 << CIRCULAR_FEATBUF_SIZE_2N)
#define CIRCULAR_BUF_MASK (CIRCULAR_BUF_SIZE - 1)
#define BUF_SIZE_IN_CHUNK (32)

#define CLASSIFIER_BUF_LEN (16)
#define MAX_NUMBER_CLASSES (5)
#define EXPECTED_NUMBER_OF_CLASSIFIER_INPUTS (NUM_AUDIO_FEATURES)

#define CLASS_OF_INTEREST_TO_ENABLE_CALLBACK (0)

#define RECORD_DURATION_IN_MS (10000)
#define RECORD_DURATION_IN_FRAMES (RECORD_DURATION_IN_MS / SAMPLE_PERIOD_IN_MILLISECONDS)

#define SNORING_LOG_LIK_THRESHOLD_Q10 (600)


#ifndef true
#define true (1)
#endif

#ifndef false
#define false (0)
#endif




typedef struct {
    uint8_t packedbuf[BUF_SIZE_IN_CHUNK][NUM_AUDIO_FEATURES/2];// 32 x 16 = 2^5 * 2^4 = 2^9 = 256 bytes
    int16_t energy[BUF_SIZE_IN_CHUNK]; //32 * 2 = 64bytes
    int64_t samplecount; // 8 bytes
    int16_t maxenergy; // 2 bytes
} AudioFeatureChunk_t; //330 bytes

typedef struct {
    int8_t classifier_feat_buf[CLASSIFIER_BUF_LEN][NUM_AUDIO_FEATURES];
    uint16_t classifier_feat_idx;
    
    //cicular buffer of incoming data
    uint8_t packedbuf[CIRCULAR_BUF_SIZE][NUM_AUDIO_FEATURES/2]; //32 * 8 = 256 bytes
    int16_t totalenergy[CIRCULAR_BUF_SIZE];//

    uint16_t chunkbufidx; //current index of chunk to write to, wraps to zero when it >= chunk_buf_size
    uint16_t numchunkbuf; //number of chunks written, saturated at chunk_buf_size
    
    AudioFeatureChunk_t * pchunkbuf;
    uint32_t chunk_buf_size;

    
    uint16_t incomingidx;
    uint16_t numincoming;
    
    uint8_t isThereAnythingInteresting;
    uint8_t isWorthClassifying;

    
} DataBuffer_t;

typedef struct {
    const DataBuffer_t * buf;
    uint32_t currentidx;
    uint32_t endidx;
    uint8_t state;
    int8_t unpackedbuffer[BUF_SIZE_IN_CHUNK][NUM_AUDIO_FEATURES]; //32 * 16 * 1  = 512 bytes

} Encoder_t;

static const char * k_id_feature_chunk = "feature_chunk";
static const char * k_id_energy_chunk = "energy_chunk";

//"long term storage"
static DataBuffer_t _buffer;
static Classifier_t _classifier;
static Classifier_t _hmm;


static inline uint8_t pack_int8_to_int4(const int8_t x) {
    const uint8_t sign = x < 0;
    
    return (x & 0x07) | (sign * 8);
}

// assumes two's complement architecture (who the heck doesn't do this these days?)
static inline void unpack_int4_pair_to_int8(const uint8_t packed, int8_t * upper, int8_t * lower) {
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
    memcpy(&chunk.energy[0],&_buffer.totalenergy[idx],size1*sizeof(int16_t));
    
    if (size1 < CIRCULAR_BUF_SIZE) {
        size2 = idx;
        memcpy(&chunk.packedbuf[size1][0],&_buffer.packedbuf[0][0],size2*NUM_AUDIO_FEATURES/2*sizeof(uint8_t));
        memcpy(&chunk.energy[size1],&_buffer.totalenergy[0],size2*sizeof(int16_t));
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
    
    //track how full the buffer is even if you wrapped
    //if you wrapped, you're discarding data, but you're still full
    if (_buffer.numchunkbuf < _buffer.chunk_buf_size) {
        _buffer.numchunkbuf++;
    }
    
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
    
}


void AudioClassifier_DataCallback( AudioFeatures_t * pfeats) {

    uint16_t idx;

    
    
    /* 
     
       Data comes in, and we save it to a circular buffer.
       Classification is disabled for now
     
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

    _buffer.totalenergy[idx] = pfeats->logenergy;
    
    //increment circular buffer index
    _buffer.incomingidx++;
    _buffer.incomingidx &= CIRCULAR_BUF_MASK;
    
    //increment until full
    if (_buffer.numincoming < CIRCULAR_BUF_SIZE) {
        _buffer.numincoming++;
    }
    
    //everything is interesting
    _buffer.isThereAnythingInteresting = true;
    _buffer.isWorthClassifying = false; //don't classify anything for now
    
    
    //if something interesting happend and the circular buffer is full
    //dump it to storage
   // if (_buffer.isThereAnythingInteresting == true &&
    if( _buffer.numincoming == CIRCULAR_BUF_SIZE )
    {
        //this may block... hopefully not for too long?
        CopyCircularBufferToPermanentStorage(pfeats->samplecount);
        _buffer.numincoming = 0; //"empty" the buffer
        _buffer.incomingidx = 0;
        _buffer.isThereAnythingInteresting = false;
    }
    
}

/* sadly this is not stateless, but was the only way to serialize chunks one at a time */
static uint8_t GetNextMatrixCallback(uint8_t isFirst,const_MatDesc_t * pdesc,void * data) {
    
    
    Encoder_t * encodedata = (Encoder_t *)data;
    
    const AudioFeatureChunk_t * pchunk;
    int16_t * bufptr16 = (int16_t *) &encodedata->unpackedbuffer[0][0];
    const int16_t * beginning16 = (int16_t *) &encodedata->unpackedbuffer[0][0];

    uint32_t i;
    const uint32_t chunk_buf_size = encodedata->buf->chunk_buf_size;
    const uint32_t numchunkbuf = encodedata->buf->numchunkbuf;
    //assert(encodedata->buf == &_buffer);
    
    memset(pdesc,0,sizeof(const_MatDesc_t));
    
    if (encodedata->buf->numchunkbuf == 0 || encodedata->buf->pchunkbuf == NULL) {
        return MAT_MESSAGE_FAIL; //stop
    }
    
    if (isFirst) {
        encodedata->currentidx = 0;
        encodedata->endidx = encodedata->buf->chunkbufidx;
        
        if (chunk_buf_size == numchunkbuf) {
            encodedata->currentidx = encodedata->buf->chunkbufidx + 1; //oldest untouched
            
            if (encodedata->currentidx >= chunk_buf_size) {
                encodedata->currentidx -= chunk_buf_size;
            }
        }
        
        encodedata->state = 1;
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
        for (int j = 0; j < BUF_SIZE_IN_CHUNK; j++) {
            printf("up=");
            for (i = 0; i < NUM_AUDIO_FEATURES; i++) {
                printf("%d,",encodedata->unpackedbuffer[j][i]);
            }
            printf("\n");

        }
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
            *bufptr16 = pchunk->energy[i];
            bufptr16++;
        }
        
        /*
        for (i = 0; i < BUF_SIZE_IN_CHUNK + 1; i++) {
            printf("%d,",beginning16[i]);
        }
        printf("\n");
        */
        
        
        pdesc->data.len = BUF_SIZE_IN_CHUNK;
        pdesc->data.type = esint16;
        pdesc->data.data.sint16 = beginning16;
        pdesc->rows = 1;
        pdesc->cols = BUF_SIZE_IN_CHUNK;
        
        encodedata->state = 1;
        encodedata->currentidx++;
        
        //wrap
        if (encodedata->currentidx >= chunk_buf_size) {
            encodedata->currentidx -= chunk_buf_size;
        }
        
        //have we reached the end?
        if (encodedata->currentidx == encodedata->endidx) {
            encodedata->state = 0;
            return MAT_MESSAGE_DONE;
        }

    }
    
    return MAT_MESSAGE_CONTINUE;
}

void AudioClassifier_ResetStorageBuffer(void) {
	/* Buffer read out?  Great, let's reset it */

	_buffer.chunkbufidx = 0;
	_buffer.numchunkbuf = 0;
}

#include "proto_utils.h"
#include "proto_utils.h"
#include "sys_time.h"
#include "matrix.pb.h"
#include "debugutils/matmessageutils.h"

#ifdef USED_ON_DESKTOP
uint32_t get_time(void) {
    return 0;
}

bool encode_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const char * k_device_id = "DESKTOP";
    return pb_encode_tag_for_field(stream, field) && pb_encode_string(stream, (uint8_t*)k_device_id, strlen(k_device_id));
}

#endif

void * getMatrixClientMessage() {
	//this code leaks references to these, can't have them on the stack
	//also makes this function non reentrant
	static Encoder_t encoderstruct;
	static MatrixClientMessage mess;
	static MatrixListEncodeContext_t matrix_list_context;

	memset(&mess, 0, sizeof(mess));
	mess.unix_time = get_time();
	mess.has_unix_time = 1;
	mess.has_matrix_payload = 0;

	memset(&matrix_list_context, 0, sizeof(matrix_list_context));
	memset(&encoderstruct, 0, sizeof(encoderstruct));
	encoderstruct.buf = &_buffer;

	matrix_list_context.data = &encoderstruct;
	matrix_list_context.func = GetNextMatrixCallback;

	mess.matrix_list.funcs.encode = write_mat_array;
	mess.matrix_list.arg = (void *) &matrix_list_context;
	mess.device_id.funcs.encode = encode_device_id_string;

	return &mess;
}
