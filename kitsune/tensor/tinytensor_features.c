#include "tinytensor_features.h"
#include "tinytensor_memory.h"

#include "uart_logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fft.h"
#include "hellomath.h"
#include "tinytensor_math.h"


void set_background_energy(const int16_t fr[], const int16_t fi[]);

#define USE_BACKGROUND_NORMALIZATION (1)
#define BACKGROUND_NOISE_MAX_ATTENUATION (-2048)

//this controls how much less to "descale" the FFT output (NOT USED CURRENTLY)

#define FFT_SIZE_2N (9)
#define FFT_SIZE (1 << FFT_SIZE_2N)

//0.95 in Q15
#define PREEMPHASIS (31129)


#define MUL16(a,b)\
((int16_t)(((int32_t)(a * b)) >> QFIXEDPOINT_INT16))


#define NOISE_FLOOR (2)

#define MOVING_AVG_COEFF (0.99f)
#define MOVING_AVG_COEFF2 (0.99f)
#define MOVING_AVG_COEFF3 (0.9995f)

#define SCALE_TO_8_BITS (7)

#define NOMINAL_TARGET (50)


#define SPEECH_LPF_CEILING (-1000)
#define START_SPEECH_THRESHOLD (4000)
#define STOP_SPEECH_THRESHOLD (1000)

#define NUM_NONSPEECH_FRAMES_TO_TURN_OFF   (33)
#define NUM_SPEECH_FRAMES_TO_TURN_ON       (2)
//hanning window
__attribute__((section(".data")))
const static int16_t k_hanning[FFT_UNPADDED_SIZE] = {0,2,8,18,32,51,73,99,130,164,203,245,292,342,397,455,517,584,654,728,806,888,973,1063,1156,1253,1354,1459,1567,1679,1794,1914,2036,2163,2293,2426,2563,2703,2847,2994,3144,3298,3455,3615,3778,3944,4114,4286,4462,4640,4821,5006,5193,5383,5575,5770,5968,6169,6372,6577,6785,6995,7208,7423,7640,7859,8080,8304,8529,8757,8986,9217,9450,9684,9921,10159,10398,10639,10881,11125,11370,11616,11863,12112,12362,12612,12864,13116,13369,13623,13878,14133,14389,14645,14902,15159,15417,15674,15932,16190,16448,16706,16964,17222,17479,17736,17993,18250,18506,18762,19016,19271,19524,19777,20029,20280,20530,20779,21027,21274,21520,21764,22007,22249,22489,22728,22965,23200,23434,23666,23896,24124,24351,24575,24798,25018,25236,25452,25666,25877,26086,26293,26497,26699,26898,27095,27289,27480,27668,27854,28037,28216,28393,28567,28738,28906,29071,29233,29391,29546,29698,29847,29992,30134,30273,30408,30540,30668,30792,30913,31031,31145,31255,31361,31464,31563,31658,31749,31837,31921,32001,32077,32149,32217,32281,32342,32398,32451,32499,32544,32584,32620,32653,32681,32706,32726,32742,32754,32762,32766,32766,32762,32754,32742,32726,32706,32681,32653,32620,32584,32544,32499,32451,32398,32342,32281,32217,32149,32077,32001,31921,31837,31749,31658,31563,31464,31361,31255,31145,31031,30913,30792,30668,30540,30408,30273,30134,29992,29847,29698,29546,29391,29233,29071,28906,28738,28567,28393,28216,28037,27854,27668,27480,27289,27095,26898,26699,26497,26293,26086,25877,25666,25452,25236,25018,24798,24575,24351,24124,23896,23666,23434,23200,22965,22728,22489,22249,22007,21764,21520,21274,21027,20779,20530,20280,20029,19777,19524,19271,19016,18762,18506,18250,17993,17736,17479,17222,16964,16706,16448,16190,15932,15674,15417,15159,14902,14645,14389,14133,13878,13623,13369,13116,12864,12612,12362,12112,11863,11616,11370,11125,10881,10639,10398,10159,9921,9684,9450,9217,8986,8757,8529,8304,8080,7859,7640,7423,7208,6995,6785,6577,6372,6169,5968,5770,5575,5383,5193,5006,4821,4640,4462,4286,4114,3944,3778,3615,3455,3298,3144,2994,2847,2703,2563,2426,2293,2163,2036,1914,1794,1679,1567,1459,1354,1253,1156,1063,973,888,806,728,654,584,517,455,397,342,292,245,203,164,130,99,73,51,32,18,8,2,0};
__attribute__((section(".data")))
const static uint8_t k_coeffs[454] = {255,255,127,127,255,127,127,255,127,127,255,127,127,255,127,127,255,127,127,255,127,127,255,170,85,85,170,255,127,127,255,170,85,85,170,255,170,85,85,170,255,170,85,85,170,255,170,85,85,170,255,191,127,63,63,127,191,255,191,127,63,63,127,191,255,191,127,63,63,127,191,255,191,127,63,63,127,191,255,204,153,102,51,51,102,153,204,255,204,153,102,51,51,102,153,204,255,204,153,102,51,51,102,153,204,255,204,153,102,51,51,102,153,204,255,212,170,127,85,42,42,85,127,170,212,255,212,170,127,85,42,42,85,127,170,212,255,218,182,145,109,72,36,36,72,109,145,182,218,255,218,182,145,109,72,36,36,72,109,145,182,218,255,223,191,159,127,95,63,31,31,63,95,127,159,191,223,255,218,182,145,109,72,36,36,72,109,145,182,218,255,226,198,170,141,113,85,56,28,28,56,85,113,141,170,198,226,255,226,198,170,141,113,85,56,28,28,56,85,113,141,170,198,226,255,226,198,170,141,113,85,56,28,28,56,85,113,141,170,198,226,255,231,208,185,162,139,115,92,69,46,23,23,46,69,92,115,139,162,185,208,231,255,229,204,178,153,127,102,76,51,25,25,51,76,102,127,153,178,204,229,255,233,212,191,170,148,127,106,85,63,42,21,21,42,63,85,106,127,148,170,191,212,233,255,233,212,191,170,148,127,106,85,63,42,21,21,42,63,85,106,127,148,170,191,212,233,255,235,215,196,176,156,137,117,98,78,58,39,19,19,39,58,78,98,117,137,156,176,196,215,235,255,236,218,200,182,163,145,127,109,91,72,54,36,18,18,36,54,72,91,109,127,145,163,182,200,218,236,255,238,221,204,187,170,153,136,119,102,85,68,51,34,17,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255,238,221,204,187,170,153,136,119,102,85,68,51,34,17,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255,240,225,210,195,180,165,150,135,120,105,90,75,60,45,30,15};
__attribute__((section(".data")))
const static uint8_t k_fft_index_pairs[40][2] = {{1,1},{2,3},{3,5},{5,7},{7,9},{9,11},{11,13},{13,15},{15,18},{17,20},{20,23},{22,26},{25,29},{28,32},{31,36},{34,40},{38,44},{42,48},{46,53},{50,58},{55,63},{60,68},{65,74},{70,80},{76,87},{82,94},{89,102},{96,109},{104,118},{111,127},{120,136},{129,147},{138,157},{149,169},{159,181},{171,194},{183,208},{196,223},{210,238},{225,255}};

typedef struct {
#if 0
    int16_t * buf;
    int16_t * pbuf_write;
    int16_t * pbuf_read;
    int16_t * end;
    uint32_t num_samples_in_buffer;
#endif
    int16_t melbank_avg[NUM_MEL_BINS];
    void * results_context;
    tinytensor_audio_feat_callback_t results_callback;
    tinytensor_speech_detector_callback_t speech_detector_callback;
    uint8_t passed_first;
    int16_t max_mel_lpf;
    
    int16_t bins[2][NUM_SAMPLES_TO_RUN_FFT];
    uint32_t binidx;
    uint32_t bintot;
    
    int16_t log_speech_lpf;
    
    uint32_t speech_detector_counter;
    uint32_t num_speech_frames;
    int32_t num_nonspeech_frames;
    int16_t last_speech_energy_average;
    
    int32_t log_liklihood_of_speech;
    uint8_t is_speech;


} TinyTensorFeatures_t;

static TinyTensorFeatures_t _this;

void tinytensor_features_initialize(void * results_context, tinytensor_audio_feat_callback_t results_callback,tinytensor_speech_detector_callback_t speech_detector_callback) {
    memset(&_this,0,sizeof(TinyTensorFeatures_t));
   // _this.buf = MALLOC(BUF_SIZE_IN_SAMPLES*sizeof(int16_t));
  //  memset(_this.buf,0,BUF_SIZE_IN_SAMPLES*sizeof(int16_t));
  //  _this.pbuf_write = _this.buf;
  //  _this.pbuf_read = _this.buf;
 //   _this.end = _this.buf + BUF_SIZE_IN_SAMPLES;
    _this.results_callback = results_callback;
    _this.speech_detector_callback = speech_detector_callback;
    _this.results_context = results_context;
    _this.log_speech_lpf = -6000;
}

void tinytensor_features_deinitialize(void) {
//    FREE(_this.buf);
}

__attribute__((section(".ramcode")))
void tinytensor_features_get_mel_bank(int16_t * melbank,const int16_t * fr, const int16_t * fi,const int16_t input_scaling) {
    uint32_t ifft;
    uint32_t imel;
    uint32_t idx;
    uint32_t utemp32;
    int32_t temp32;
    uint64_t accumulator;

    //get mel bank feats
    idx = 0;
    for (imel = 0; imel < NUM_MEL_BINS; imel++) {
        accumulator = 0;
        for (ifft = k_fft_index_pairs[imel][0]; ifft <= k_fft_index_pairs[imel][1]; ++ifft) {
            utemp32 = ((uint32_t)fr[ifft]*fr[ifft]) + ((uint32_t)fi[ifft]*fi[ifft]); //q15 + q15 = q30, q30 * 2 --> q31, unsigned 32 is safe
            accumulator += (uint32_t)((((uint64_t) utemp32) * k_coeffs[idx]) >> 8);
            ++idx;
        }
        
        if (accumulator < NOISE_FLOOR) {
            accumulator = NOISE_FLOOR;
        }
        
        //factor of two because of the squaring
        //1024 for the Q10
        temp32 = FixedPointLog2Q10(accumulator) - 2*input_scaling * 1024;
        
        
        if (temp32 > MAX_INT_16) {
            temp32 = MAX_INT_16;
        }
        
        if (temp32 < MIN_INT_16) {
            temp32 = MIN_INT_16;
        }
        
        
        melbank[imel] = (int16_t)temp32;
    }

}

void tinytensor_features_force_voice_activity_detection(void) {
	_this.is_speech = 1;
	_this.num_nonspeech_frames = -100;
}

#define SPEECH_BIN_START (4)
#define SPEECH_BIN_END (16)
#define ENERGY_END (FFT_SIZE/2)
static void do_voice_activity_detection(int16_t * fr,int16_t * fi,int16_t input_scaling) {
    uint32_t i;
    int16_t log_energy_frac;
    uint64_t speech_energy = 0;
    uint64_t total_energy = 0;
    int32_t temp32;
    
    for (i = SPEECH_BIN_START; i < SPEECH_BIN_END; i++) {
        speech_energy += fr[i] * fr[i] + fi[i] * fi[i];
    }
    
    total_energy = speech_energy;
    
    for (i = SPEECH_BIN_END; i < ENERGY_END; i++) {
        total_energy += fr[i] * fr[i] + fi[i] * fi[i];
    }
    
    
    //log (a/b) = log(a) - log(b)
    temp32 = FixedPointLog2Q10(speech_energy) - 2*input_scaling * 1024;

    if (temp32 > INT16_MAX) {
        log_energy_frac = INT16_MAX;
    }
    else if (temp32 < -INT16_MAX) {
        log_energy_frac = -INT16_MAX;
    }
    else {
        log_energy_frac = (int16_t)temp32;
    }
    //moving average of log energy fraction
    
    
    _this.log_speech_lpf = MUL16(_this.log_speech_lpf,TOFIXQ(MOVING_AVG_COEFF3,QFIXEDPOINT_INT16));
    _this.log_speech_lpf += MUL16(log_energy_frac,TOFIXQ(1.0 - MOVING_AVG_COEFF3,QFIXEDPOINT_INT16));
    //moving average of diff of average (so we can compute 2nd derivitaive)
    if (_this.log_speech_lpf > SPEECH_LPF_CEILING) {
        _this.log_speech_lpf = SPEECH_LPF_CEILING;
    }

    log_energy_frac -= _this.log_speech_lpf;
    
    
    if (log_energy_frac > START_SPEECH_THRESHOLD) {
        _this.num_speech_frames++;
        _this.num_nonspeech_frames = 0;
    }
    
    if (log_energy_frac < STOP_SPEECH_THRESHOLD) {
        _this.num_nonspeech_frames++;
        _this.num_speech_frames = 0;
    }
    
        
    if (_this.num_nonspeech_frames == NUM_NONSPEECH_FRAMES_TO_TURN_OFF && _this.is_speech) {
        if (_this.speech_detector_callback) {
            _this.speech_detector_callback(_this.results_context,stop_speech);
        }

        _this.is_speech = 0;
    }

    if (_this.num_speech_frames == NUM_SPEECH_FRAMES_TO_TURN_ON && !_this.is_speech) {
        if (_this.speech_detector_callback) {
            _this.speech_detector_callback(_this.results_context,start_speech);
        }
        _this.is_speech = 1;
    }
    
    

    
    //printf("%d,%d\n",temp32,_this.log_liklihood_of_speech);
    
    
    
}

#include "arm_math.h"

__attribute__((section(".ramcode")))
static uint8_t add_samples_and_get_mel(int16_t * maxmel,int16_t * avgmel, int16_t * melbank, const int16_t * samples, const uint32_t num_samples) {
    int16_t fr[FFT_SIZE] = {0};
    int16_t fi[FFT_SIZE] = {0};
    const int16_t preemphasis_coeff = PREEMPHASIS;
    uint32_t i;

    DECLCHKCYC

    int32_t temp32;
    int16_t temp16;
    int16_t last;
    uint16_t max;
    /* add samples to circular buffer
       
        while current pointer is NUM_SAMPLES_TO_RUN_FFT behind the buffer pointer
        then we copy the last FFT_UNPADDED_SIZE samples to the FFT buf, zero pad it up to FFT_SIZE
    
     */

    /*
    tiny_tensor_features_add_to_buffer(samples,num_samples);

    if (_this.num_samples_in_buffer < FFT_UNPADDED_SIZE) {
        return 0;
    }
     */

    //num_samples must be NUM_SAMPLES_TO_RUN_FFT...
    memcpy( (void*)_this.bins[_this.binidx], (void*)samples, num_samples*sizeof(int16_t) );
    _this.binidx = (_this.binidx+1)% 2;
    if( ++_this.bintot < 2 ) return 0;
    _this.bintot = 2;
    if( _this.binidx == 0 ) {
    	memcpy( fr, _this.bins, 400*2);
        CHKCYC("copy 0");
    } else if( _this.binidx == 1 )  {
		memcpy( fr+320, _this.bins[0], 160*2);
		memcpy( fr, _this.bins[1], 240*2);
        CHKCYC("copy 1");
    }//tiny_tensor_features_get_latest_samples(fr,FFT_UNPADDED_SIZE);

    last = 0;
    max = 0;
    //"preemphasis", and apply window as you go
    for (i = 1; i < FFT_UNPADDED_SIZE; i++) {

        temp32 = fr[i] - MUL16(preemphasis_coeff,last);
        temp16 = temp32 >> 1; //never overflow

        last = fr[i];
        //APPLY WINDOW
        fr[i]  = MUL16(temp16,k_hanning[i]);

        temp16 = fr[i];
        if (temp16 < 0) {
            temp16 = -temp16;
        }

        max = temp16 > max ? temp16 : max; //max
    }

    fr[0] = 0;

    temp16 = 0;
    //find max scaling factor, brute force
    for (i = 1; i < 16; i++) {
        if ( (max << i) > 0x7FFF ) {
            break;
        }
    }
    
    temp16 = i - 1;
    
    if (temp16 > 0) {
        for (i = 0; i < FFT_UNPADDED_SIZE; i++) {
            fr[i] <<= temp16;
        }
    }

    CHKCYC(" fft prep");

    //PERFORM FFT
    fft(fr,fi,FFT_SIZE_2N);

    CHKCYC("FFT");

    //get "speech" energy ratio
    do_voice_activity_detection(fr,fi,temp16);

    //update counter
    _this.speech_detector_counter++;
    //GET MEL FEATURES (one time slice in the mel spectrogram)
    tinytensor_features_get_mel_bank(melbank,fr,fi,temp16);

    if( (_this.speech_detector_counter & 0x3)==0 ) {
    	set_background_energy(fr, fi);
    }

    //GET MAX
    temp16 = MIN_INT_16;
    for (i = 0; i < NUM_MEL_BINS; i++) {
        temp16 = melbank[i] > temp16 ? melbank[i] : temp16;
    }

    //slow to fall, quick to rise

    //average it -- this is the slow to fall part
    _this.max_mel_lpf = MUL16(_this.max_mel_lpf,TOFIXQ(MOVING_AVG_COEFF2,QFIXEDPOINT_INT16));
    _this.max_mel_lpf += MUL16(temp16,TOFIXQ(1.0 - MOVING_AVG_COEFF2,QFIXEDPOINT_INT16));
    
    //this is the quick to rise part
    if (_this.max_mel_lpf < temp16) {
        _this.max_mel_lpf = temp16;
    }
    
    *maxmel = _this.max_mel_lpf;
    
    if (!_this.passed_first) {
        _this.passed_first = 1;
        
        memcpy(_this.melbank_avg,melbank,NUM_MEL_BINS * sizeof(int16_t));
    }
    
    //get AVG
    temp32 = 0;
    //compute moving avergage
    for (i = 0; i < NUM_MEL_BINS; i++) {
        _this.melbank_avg[i] = MUL16(_this.melbank_avg[i],TOFIXQ(MOVING_AVG_COEFF,QFIXEDPOINT_INT16));
        _this.melbank_avg[i] += MUL16(melbank[i],TOFIXQ(1.0 - MOVING_AVG_COEFF,QFIXEDPOINT_INT16));

        temp32 += _this.melbank_avg[i];
    }

    temp32 /= NUM_MEL_BINS;

    temp16 = temp32;
    *avgmel = temp16;
    
#if USE_BACKGROUND_NORMALIZATION
    for (i = 0; i < NUM_MEL_BINS; i++) {
        temp32 = temp16 - _this.melbank_avg[i];
        
        //only attenuate a bit
        if (temp32 < BACKGROUND_NOISE_MAX_ATTENUATION) {
            temp32 = BACKGROUND_NOISE_MAX_ATTENUATION;
        }
        
        //no gaining up
        if (temp32 > 0) {
            temp32 = 0;
        }
        
        melbank[i] += temp32;
    }
#endif

    CHKCYC("mel");

    return 1;
}
__attribute__((section(".ramcode")))
void tinytensor_features_add_samples(const int16_t * samples, const uint32_t num_samples) {
    int16_t melbank[NUM_MEL_BINS];
    int32_t temp32;
    int16_t maxmel;
    int16_t avgmel;
    int32_t nominal_offset;
    int32_t offset;
    int32_t offset_adjustment;
    const int8_t shift = 7 - QFIXEDPOINT;
    
    uint32_t i;

    DECLCHKCYC

    if (add_samples_and_get_mel(&maxmel,&avgmel,melbank,samples,num_samples)) {

        avgmel >>= SCALE_TO_8_BITS;
        maxmel >>= SCALE_TO_8_BITS;
        //general idea is that as maxmel increases (say due to some LOUD THINGS happening)
        //that the offset backs off quickly

        nominal_offset = NOMINAL_TARGET - avgmel;

        offset_adjustment = maxmel + nominal_offset - INT8_MAX;


        if (offset_adjustment < 0) {
            offset_adjustment = 0;
        }
        
        offset = nominal_offset - offset_adjustment;

        //printf("%d\n",offset);
        for (i = 0; i <NUM_MEL_BINS; i++) {
            melbank[i] = (melbank[i]>>SCALE_TO_8_BITS)+offset;

            if (shift < 0) {
                melbank[i] <<= -shift;
            }
            else {
                melbank[i] >>= shift;
            }
        }

        if (_this.results_callback) {
            _this.results_callback(_this.results_context,melbank);
        }
#ifdef PRINT_MEL_BINS
        for (i = 0; i < 40; i++) {
            if (i!= 0) {
                printf (",");
            }
            printf("%d",melbank8[i]);
        }
        
        printf("\n");
#endif //PRINT_MEL_BINS
        
    }
	CHKCYC("cmplt");

}
