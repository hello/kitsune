
#include "fft.h"
#include "audio_types.h"
#include "hellomath.h"

#include <stdlib.h> //for abs
#include <string.h>
#include <stdint.h>

#define FIX_MPY(DEST,A,B) (DEST) = ((int32_t)(A) * (int32_t)(B))>>15

//#define fix_mpy(A, B) (FIX_MPY(a,a,b))

#define LOG2_N_WAVE     (10)
#define N_WAVE          (1 << LOG2_N_WAVE)        /* dimension of Sinewave[] */

__attribute__((section(".data"))) static const uint16_t sin_lut[N_WAVE/4+1] = {
  0, 201, 402, 603, 804, 1005, 1206, 1406,
  1607, 1808, 2009, 2209, 2410, 2610, 2811, 3011,
  3211, 3411, 3611, 3811, 4011, 4210, 4409, 4608,
  4807, 5006, 5205, 5403, 5601, 5799, 5997, 6195,
  6392, 6589, 6786, 6982, 7179, 7375, 7571, 7766,
  7961, 8156, 8351, 8545, 8739, 8932, 9126, 9319,
  9511, 9703, 9895, 10087, 10278, 10469, 10659, 10849,
  11038, 11227, 11416, 11604, 11792, 11980, 12166, 12353,
  12539, 12724, 12909, 13094, 13278, 13462, 13645, 13827,
  14009, 14191, 14372, 14552, 14732, 14911, 15090, 15268,
  15446, 15623, 15799, 15975, 16150, 16325, 16499, 16672,
  16845, 17017, 17189, 17360, 17530, 17699, 17868, 18036,
  18204, 18371, 18537, 18702, 18867, 19031, 19194, 19357,
  19519, 19680, 19840, 20000, 20159, 20317, 20474, 20631,
  20787, 20942, 21096, 21249, 21402, 21554, 21705, 21855,
  22004, 22153, 22301, 22448, 22594, 22739, 22883, 23027,
  23169, 23311, 23452, 23592, 23731, 23869, 24006, 24143,
  24278, 24413, 24546, 24679, 24811, 24942, 25072, 25201,
  25329, 25456, 25582, 25707, 25831, 25954, 26077, 26198,
  26318, 26437, 26556, 26673, 26789, 26905, 27019, 27132,
  27244, 27355, 27466, 27575, 27683, 27790, 27896, 28001,
  28105, 28208, 28309, 28410, 28510, 28608, 28706, 28802,
  28897, 28992, 29085, 29177, 29268, 29358, 29446, 29534,
  29621, 29706, 29790, 29873, 29955, 30036, 30116, 30195,
  30272, 30349, 30424, 30498, 30571, 30643, 30713, 30783,
  30851, 30918, 30984, 31049,
  31113, 31175, 31236, 31297,
  31356, 31413, 31470, 31525, 31580, 31633, 31684, 31735,
  31785, 31833, 31880, 31926, 31970, 32014, 32056, 32097,
  32137, 32176, 32213, 32249, 32284, 32318, 32350, 32382,
  32412, 32441, 32468, 32495, 32520, 32544, 32567, 32588,
  32609, 32628, 32646, 32662, 32678, 32692, 32705, 32717,
  32727, 32736, 32744, 32751, 32757, 32761, 32764, 32766, 32767
};



__attribute__((section(".ramcode"))) uint8_t bitlog(uint32_t n) {
    int16_t b;
    
    // shorten computation for small numbers
    if(n <= 8)
        return (int16_t)(2 * n);
    
    // find the highest non-zero bit
    b=31;
    while((b > 2) && ((int32_t)n > 0))
    {
        --b;
        n <<= 1;
    }
    n &= 0x70000000;
    n >>= 28; // keep top 4 bits, of course we're only using 3 of those
    
    b = (int16_t)n + 8 * (b - 1);
    return (uint8_t) b;
}

__attribute__((section(".ramcode"))) uint32_t bitexp(uint16_t n) {
    uint16_t b = n/8;
    uint32_t retval;
    
    // make sure no overflow
    if(n > 247)
        return 0xf0000000;
    
    // shorten computation for small numbers
    if(n <= 16)
        return (uint32_t)(n / 2);
    
    retval = n & 7;
    retval |= 8;
    
    return (retval << (b - 2));
}

 __attribute__((section(".ramcode")))
 short fxd_sin( uint16_t x ) {
	x &= 0x3FF;
	if( x > 3*N_WAVE/4 ) {
		return -sin_lut[N_WAVE - x];
	}
	if( x > N_WAVE/2 ) {
		return -sin_lut[x - N_WAVE/2];
	}
	if( x > N_WAVE/4 ) {
		return sin_lut[N_WAVE/2 - x];
	}
	if( x <= N_WAVE/4 ) {
		return sin_lut[x];
	}
	return 0;
}


#if 0
/*
  fix_mpy() - short-point multiplication
*/
inline static short fix_mpy(short a, short b)
{
  FIX_MPY(a, a, b);
  return a;
}
#endif
#include "uart_logger.h"
#include "cmsis_ccs.h"
#include "arm_math.h"
__attribute__((section(".ramcode"))) int fft(int16_t fr[], int16_t fi[], int32_t m)
{
    int32_t mr, nn, i, j, l, k, istep, n;
    int16_t  wr, wi;
#if 0
    typedef union
    {
        struct {
        	int16_t LSW; /* Least significant Word */
            int16_t MSW; /* Most significant Word */
        }regs;
        uint32_t pair;
    }pair_t;
    pair_t p1, p2;
#endif

    n = 1 << m;
    
    if (n > N_WAVE)
        return -1;
    
    mr = 0;
    nn = n - 1;
    
    /* decimation in time - re-order data */
    for (m = 1; m <= nn; ++m) {
        short tmp;
        l = n;
        do {
            l >>= 1;
        } while (mr + l > nn);
        mr = (mr & (l - 1)) + l;
        
        if (mr <= m)
            continue;
        
        //swap
        tmp = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tmp;
        
        //swap
        tmp = fi[m];
        fi[m] = fi[mr];
        fi[mr] = tmp;
    }

    l = 1;
    k = LOG2_N_WAVE - 1;
    while (l < n) {
        /* short scaling, for proper normalization -
         there will be log2(n) passes, so this
         results in an overall factor of 1/n,
         distributed to maximize arithmetic accuracy. */
        istep = l << 1;
        for (m = 0; m < l; ++m) {
            j = m << k;
            /* 0 <= j < N_WAVE/2 */
        //    p1.regs.MSW =
            		wr = fxd_sin(j + N_WAVE / 4);
        //    p1.regs.LSW =
            		wi = -fxd_sin(j);
            
            for (i = m; i < n; i += istep) {
                int32_t tr,ti,qr, qi;

                j = i + l;

#if 0
                p2.regs.MSW = fr[j];
                p2.regs.LSW = fi[j];
                tr = __SMUSD( p1.pair, p2.pair  );
                ti = __SMUADX( p1.pair, p2.pair  );
##elif 0
                tr = (int32_t)p1.regs.MSW * (int32_t)p2.regs.MSW
				   - (int32_t)p1.regs.LSW * (int32_t)p2.regs.LSW;
                ti = (int32_t)p1.regs.MSW * (int32_t)p2.regs.LSW
			       + (int32_t)p1.regs.LSW * (int32_t)p2.regs.MSW;
#else
                tr = (int32_t)wr * (int32_t)fr[j] - (int32_t)wi * (int32_t)fi[j];
                ti = (int32_t)wr * (int32_t)fi[j] + (int32_t)wi*(int32_t)fr[j];
#endif

                tr >>= 1;
                ti >>= 1;
                
                qr = fr[i] << 14;
                qi = fi[i] << 14;
                
                fr[j] = (qr - tr) >> 15;
                fi[j] = (qi - ti) >> 15;
                fr[i] = (qr + tr) >> 15;
                fi[i] = (qi + ti) >> 15;
            }
        }
        --k;
        l = istep;
    }
    return 0;
}

__attribute__((section(".ramcode"))) int fftr(int16_t f[], int32_t m)
{
    int32_t i, N = 1<<(m-1);
    int16_t tt, *fr=f, *fi=&f[N];

    for (i=1; i<N; i+=2) {
        tt = f[N+i-1];
        f[N+i-1] = f[i];
        f[i] = tt;
    }
    return fft(fi, fr, m-1);
}

//requires 2N memory... for now
//ndct can be no greater than 8 (ie. length 256)
//so fr should be length (2^(ndct + 1))
__attribute__((section(".ramcode"))) void dct(int16_t fr[],int16_t fi[],const int16_t ndct) {
    uint32_t i,k;
    int16_t sine,cosine;
    uint16_t stheta;
    uint16_t ctheta;
    static const uint16_t wavemask = (N_WAVE/4) - 1;
    const uint32_t n = (1 << ndct);
    const int8_t step2n = LOG2_N_WAVE - ndct - 2;
    const uint16_t step = 1 << step2n;
    int32_t temp32;
    memset(fi,0,2*n*sizeof(int16_t));
    //mirror mirror
    for (i = n; i < 2*n; i++) {
        k = 2*n - i - 1;
        fr[i] = fr[k];
    }
    
    fft(fr,fi,ndct + 1);
    
    k = 0;
    for (i = 0; i  < n; i++) {
        //go from 0 to pi/2
        //so sin will go from 0 --> 1
        //cos will go from 1 --> 0
        
        if (k > N_WAVE / 4) {
            stheta = (N_WAVE/2 - k) & wavemask;
            ctheta = k & wavemask;
            
            sine = sin_lut[stheta];
            cosine = -sin_lut[ctheta];
        }
        else {
            stheta = k;
            ctheta = k == 0 ? N_WAVE/4 : (N_WAVE/2 - k) & wavemask;
            
            sine = sin_lut[stheta];
            cosine = sin_lut[ctheta];
        }
        
        temp32 = (int32_t)fr[i] * (int32_t)cosine;
        temp32 -= (int32_t)fi[i] * (int32_t)sine;
        temp32 >>= 15;
        fr[i] = (int16_t)temp32;
        k += step;
    }
    
}

__attribute__((section(".ramcode"))) void fix_window(int16_t fr[], int32_t n)
{
  int i, j, k;

  j = N_WAVE / n;
  n >>= 1;
  for (i = 0, k = N_WAVE / 4; i < n; ++i, k += j)
    FIX_MPY(fr[i], fr[i], 16384 - (fxd_sin(k) >> 1));
  n <<= 1;
  for (k -= j; i < n; ++i, k -= j)
    FIX_MPY(fr[i], fr[i], 16384 - (fxd_sin(k) >> 1));
}


__attribute__((section(".ramcode"))) void abs_fft(uint16_t psd[], const int16_t fr[],const int16_t fi[],const int16_t len)
{
    int i;
    for (i=0; i < len ; ++i) {
		psd[i] = abs(fr[i]) + abs(fi[i]);
    }
}

/* call this post-fft  */
__attribute__((section(".ramcode"))) void updateoctogram(const int16_t fr[],const int16_t fi[]) {

}

// b - size of the bin in the fft in hz
// ergo, b = Fs / 2 / (n/2) or if you prefer, Nyquist freq divided by half of the FFT
// n is the PSD size (nfft / 2)
// f is both input and output
// f as input is the PSD bin, and is always positive
// f as output is the mel bins
__attribute__((section(".ramcode"))) void logpsdmel(int16_t * logTotalEnergy,int16_t psd[],const int16_t fr[],const int16_t fi[],uint8_t log2scaleOfRawSignal,uint16_t min_energy) {
    uint16_t i;
    uint16_t ufr;
    uint16_t ufi;
    uint64_t utemp64;
    uint64_t non_weighted_energy;
	uint64_t accumulator64 = 0;
	int32_t temp32;
	static const uint8_t spacings[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 10, 11, 13, 14, 15 };

	static const int16_t binaveragingcoeff[18] = { 0, 0, -1024, -1623, -2048,
			-2378, -2647, -2875, -3072, -3246, -3402, -3542, -3671, -3789,
			-3899, -4001, -4096, -4186 };

	const static uint16_t a_weight_q10[128] = { 0, 0, 100, 150, 263, 379, 489,
			510, 725, 763, 823, 859, 896, 934, 963, 994, 1024, 1054, 1085, 1093,
			1101, 1110, 1123, 1136, 1149, 1152, 1156, 1159, 1162, 1166, 1169,
			1172, 1176, 1166, 1170, 1174, 1178, 1182, 1184, 1185, 1187, 1188,
			1189, 1185, 1180, 1176, 1171, 1167, 1162, 1162, 1162, 1162, 1162,
			1162, 1162, 1162, 1161, 1159, 1157, 1156, 1154, 1152, 1151, 1149,
			1146, 1142, 1139, 1136, 1133, 1129, 1126, 1123, 1120, 1116, 1112,
			1107, 1103, 1098, 1094, 1089, 1085, 1081, 1076, 1072, 1067, 1063,
			1059, 1054, 1050, 1046, 1042, 1037, 1033, 1029, 1025, 1021, 1016,
			1012, 1012, 1009, 1005, 1002, 998, 995, 991, 987, 984, 981, 977,
			974, 970, 967, 963, 959, 956, 952, 948, 945, 941, 937, 934, 930,
			927, 923, 920, 916, 913, 913 };

	uint16_t idx, ifft, iend;

	accumulator64 = 0;
    

    ifft = 1;
    for (idx = 0; idx < 32; idx++) {
       // assert(idx-1 <= 31);
        iend = spacings[idx];
        utemp64 = 0;
        non_weighted_energy = 0;

        for (i = 0; i < iend; i++) {
        	uint64_t a = 0;
          //  assert(ifft < 128);
            ufr = abs(fr[ifft]);
            ufi = abs(fi[ifft]);
            
            a += (uint32_t)ufr*(uint32_t)ufr;
            a += (uint32_t)ufi*(uint32_t)ufi;

            utemp64 += a * a_weight_q10[ifft] >> 10;
            non_weighted_energy += a;
            ifft++;
        }

        temp32 = FixedPointLog2Q10(non_weighted_energy + min_energy) + binaveragingcoeff[iend] - log2scaleOfRawSignal*1024;
        
        if (temp32 > INT16_MAX) {
            temp32 = INT16_MAX;
        }
        
        if (temp32 < INT16_MIN) {
            temp32 = INT16_MIN;
        }
        
        psd[idx] = temp32;
        
        utemp64 = accumulator64 + utemp64;
        if (utemp64 < accumulator64) {
            accumulator64 = 0xFFFFFFFFFFFFFFFF;
        }
        else {
            accumulator64 = utemp64;
        }
    }
    
    temp32 =  FixedPointLog2Q10(accumulator64) - log2scaleOfRawSignal*1024;

    if (temp32 > INT16_MAX) {
    	temp32 = INT16_MAX;
    }
    
    *logTotalEnergy = (int16_t)temp32;


}



#if 0

void freq_energy_bins(int16_t logbinpower[],const int16_t fr[],const int16_t fi[],uint8_t log2scaleOfRawSignal) {
	
    // in Hz
    static const uint16_t k_bin_start = 4;
    static const uint16_t k_bin_width_hz = EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE;
    static const uint16_t k_fundamental_width = 800 / k_bin_width_hz; //800 hz
    
    static const uint16_t k_width_indices[NUM_FREQ_ENERGY_BINS] = {1,2,3,4,5,6}; // k_fundamental_width * the index gets you the top of the bin.
	static const int16_t k_log2_normalization[NUM_FREQ_ENERGY_BINS] = {2,2,2,2,2,2};
    uint16_t iBin;
    uint16_t iBinEdge;
    uint16_t nextBinEdge;

    uint64_t accumulator64;
    int32_t temp32;
    uint64_t utemp64;
    
    uint16_t ufr;
    uint16_t ufi;
    int16_t scale;


    
	accumulator64 = 0;
    iBinEdge = 0;
    nextBinEdge = k_width_indices[0]*k_fundamental_width - 1;
    iBin = k_bin_start;

    /* skip dc, start at 1 */
    while(1) {
        //take PSD here
        ufr = abs(fr[iBin]);
        ufi = abs(fi[iBin]);
        
        utemp64 = 0;
        utemp64 += (uint32_t)ufr*(uint32_t)ufr;
        utemp64 += (uint32_t)ufi*(uint32_t)ufi;

        if (accumulator64 + utemp64 < accumulator64) {
            accumulator64 = 0xFFFFFFFFFFFFFFFF;
        }
        else {
            accumulator64 += utemp64;
        }
        
        
		if(iBin == nextBinEdge) {
            //deal with different bin widths and original signal scaling at the same time
            scale = -2*log2scaleOfRawSignal + k_log2_normalization[iBinEdge];
            
            temp32 = FixedPointLog2Q10(accumulator64);
            temp32 += scale*(1<<10);
            
            if (temp32 > INT16_MAX) {
                temp32 = INT16_MAX;
            }
            
            if (temp32 < INT16_MIN) {
                temp32 = INT16_MIN;
            }
            
            logbinpower[iBinEdge]  =  (int16_t)temp32;


            /// prepare for next iterator ///
    
            accumulator64 = 0;

            iBinEdge++;
            
            if (iBinEdge >= sizeof(k_width_indices) / sizeof(k_width_indices[0]) ) {
                break;
            }
            
            nextBinEdge = k_width_indices[iBinEdge]*k_fundamental_width -  1;

		}
        
        iBin++;
	}
}
#endif

#if 0
void norm(short f[], int n) {
    int i;
	unsigned long long m=0;

    for (i=1; i<n; ++i) {
		m += (long long)f[i]*f[i];
    }
	if( m > (1ll<<31) )
		return;
	m = fxd_sqrt( m );
	for (i=0; i<n; ++i) {
		f[i] = 32768 / m * f[i];
    }
}


const short loud_amp[] = {794,704,625,554,491,436,387,343,304,270,239,212,188,167,148,131,116,103,92,81,72,64,57,50,45,40,35,31,28,24,22,19,17,15,13,10,0};

short power_to_dB(short pwr)
{
  short i=sizeof(loud_amp)/sizeof(loud_amp[0]);

  while( pwr > loud_amp[i] && i > -1 ) { --i; }

  return sizeof(loud_amp)/sizeof(loud_amp[0]) - i;
}
#endif
