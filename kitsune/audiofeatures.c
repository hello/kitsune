#include "audiofeatures.h"
#include "fft.h"

#define MEL_BUF_SIZE_2N (8)
#define MEL_BUF_SIZE (1 << MEL_BUF_SIZE_2N)
#define MEL_BUF_MASK (MEL_BUF_SIZE - 1)


/*--------------------------------
 *   Types
 *--------------------------------*/

typedef struct {
    uint8_t melbuf[MEL_SCALE_SIZE][MEL_BUF_SIZE]; //8 x 256 --> 2K
    uint16_t melaccumulator[MEL_SCALE_SIZE];
    
} MelFeatures_t;

/*--------------------------------
 *   Static Memory Declarations
 *--------------------------------*/
static MelFeatures_t _data;


/*--------------------------------
 *   Functions
 *--------------------------------*/

uint8_t AudioFeatures_MelAveraging(uint32_t idx, uint8_t x,uint8_t * buf, uint16_t * accumulator) {
    /* accumulate */
    uint16_t a = *accumulator;
    /* add new measurement to accumulator */
    a += x;
    
    /*  remove oldest measurement from accumulator */
    a -= buf[idx];
    
    /*  store new measurement */
    buf[idx] = x;
    
    *accumulator = a;
    
    return (uint8_t) (a >> MEL_BUF_SIZE_2N);
}


void AudioFeatures_Extract(uint8_t * melavg, const int16_t buf[],int16_t nfftsize) {
    /* Get FFT */
    
    /* Get "PSD" */
    
    /* Get Mel */
    
    /* Moving Average */
    //AudioFeatures_MelAveraging
}
