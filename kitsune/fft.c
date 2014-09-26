#define MIN_ENERGY (4)

#include "fft.h"
#include "audio_types.h"

#include <stdlib.h> //for abs
#include <string.h>
#include <stdint.h>

#define FIX_MPY(DEST,A,B) (DEST) = ((int32_t)(A) * (int32_t)(B))>>15

//#define fix_mpy(A, B) (FIX_MPY(a,a,b))

#define LOG2_N_WAVE     (10)
#define N_WAVE          (1 << LOG2_N_WAVE)        /* dimension of Sinewave[] */

static const uint16_t sin_lut[N_WAVE/4+1] = {
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

static inline uint16_t umultq16(uint16_t a, uint16_t b) {
    uint32_t c;
    uint16_t * p = (uint16_t *)(&c);
    
    c = (uint32_t)a * (uint32_t)b;
    return *(p + 1);
}

uint8_t bitlog(uint32_t n) {
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

uint32_t bitexp(uint16_t n) {
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


#define iter1(N) \
    t = root + (1 << (N)); \
    if (n >= t << (N))   \
    {   n -= t << (N);   \
        root |= 2 << (N); \
    }

unsigned int fxd_sqrt (unsigned int n)
{
    unsigned int root = 0,t;

    iter1 (15);    iter1 (14);    iter1 (13);    iter1 (12);
    iter1 (11);    iter1 (10);    iter1 ( 9);    iter1 ( 8);
    iter1 ( 7);    iter1 ( 6);    iter1 ( 5);    iter1 ( 4);
    iter1 ( 3);    iter1 ( 2);    iter1 ( 1);    iter1 ( 0);
    return root >> 1;
}



/*
  fix_mpy() - short-point multiplication
*/
inline static short fix_mpy(short a, short b)
{
  FIX_MPY(a, a, b);
  return a;
}

int fft(int16_t fr[], int16_t fi[], int32_t m)
{
    int32_t mr, nn, i, j, l, k, istep, n;
    int16_t  wr, wi;

    
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
            wr = fxd_sin(j + N_WAVE / 4);
            wi = -fxd_sin(j);
            
            for (i = m; i < n; i += istep) {
                int32_t tr,ti,qr, qi;

                j = i + l;
                
                //tr = fix_mpy(wr, fr[j]) - fix_mpy(wi, fi[j]);
                tr = (int32_t)wr * (int32_t)fr[j] - (int32_t)wi * (int32_t)fi[j];
                tr >>= 1;
                
                //ti = fix_mpy(wr, fi[j]) + fix_mpy(wi, fr[j]);
                ti = (int32_t)wr * (int32_t)fi[j] + (int32_t)wi*(int32_t)fr[j];
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

int fftr(int16_t f[], int32_t m)
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
void dct(int16_t fr[],int16_t fi[],const int16_t ndct) {
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
        //go from 0 to -pi
        //so sin will go from 0 --> 1 ---> 0
        //cos will go from 1 --> 0 --> -1
        
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

void fix_window(int16_t fr[], int32_t n)
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


void abs_fft(uint16_t psd[], const int16_t fr[],const int16_t fi[],const int16_t len)
{
    int i;
    for (i=0; i < len ; ++i) {
		psd[i] = abs(fr[i]) + abs(fi[i]);
    }
}

/*
 0000 - 0
 0001 - 1
 0010 - 2
 0011 - 2
 0100 - 3
 0101 - 3
 0110 - 3
 0111 - 3
 1000 - 4
 1001 - 4
 1010 - 4
 1011 - 4
 1100 - 4
 1101 - 4
 1110 - 4
 1111 - 4
 */
#define BIT_COUNT_LOOKUP_SIZE (16)
static const uint8_t k_bit_count_lookup[BIT_COUNT_LOOKUP_SIZE] =
{0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4};


uint8_t CountHighestMsb(uint64_t x) {
    short j;
    uint8_t count = 0;
    for (j = 0; j < 16; j++) {
        if (x < 16) {
            count += k_bit_count_lookup[x];
            break;
        }
        count += 4;
        x >>= 4;
    }
    return count;
}

/*  Not super high precision, but oh wells! */
#define LOG2_LOOKUP_SIZE_2N (8)
#define LOG2_LOOKUP_SIZE ((1 << LOG2_LOOKUP_SIZE_2N))

#if LOG2_LOOKUP_SIZE_2N == 8
static const int16_t k_log2_lookup_q10[LOG2_LOOKUP_SIZE] =
{-32767,-8192,-7168,-6569,-6144,-5814,-5545,-5317,-5120,-4946,-4790,-4650,-4521,-4403,-4293,-4191,-4096,-4006,-3922,-3842,-3766,-3694,-3626,-3560,-3497,-3437,-3379,-3323,-3269,-3217,-3167,-3119,-3072,-3027,-2982,-2940,-2898,-2858,-2818,-2780,-2742,-2706,-2670,-2636,-2602,-2568,-2536,-2504,-2473,-2443,-2413,-2383,-2355,-2327,-2299,-2272,-2245,-2219,-2193,-2168,-2143,-2119,-2095,-2071,-2048,-2025,-2003,-1980,-1958,-1937,-1916,-1895,-1874,-1854,-1834,-1814,-1794,-1775,-1756,-1737,-1718,-1700,-1682,-1664,-1646,-1629,-1612,-1594,-1578,-1561,-1544,-1528,-1512,-1496,-1480,-1464,-1449,-1434,-1419,-1404,-1389,-1374,-1359,-1345,-1331,-1317,-1303,-1289,-1275,-1261,-1248,-1235,-1221,-1208,-1195,-1182,-1169,-1157,-1144,-1132,-1119,-1107,-1095,-1083,-1071,-1059,-1047,-1036,-1024,-1013,-1001,-990,-979,-967,-956,-945,-934,-924,-913,-902,-892,-881,-871,-860,-850,-840,-830,-820,-810,-800,-790,-780,-770,-760,-751,-741,-732,-722,-713,-704,-694,-685,-676,-667,-658,-649,-640,-631,-622,-613,-605,-596,-588,-579,-570,-562,-554,-545,-537,-529,-520,-512,-504,-496,-488,-480,-472,-464,-456,-448,-440,-433,-425,-417,-410,-402,-395,-387,-380,-372,-365,-357,-350,-343,-335,-328,-321,-314,-307,-300,-293,-286,-279,-272,-265,-258,-251,-244,-237,-231,-224,-217,-211,-204,-197,-191,-184,-178,-171,-165,-158,-152,-145,-139,-133,-126,-120,-114,-108,-102,-95,-89,-83,-77,-71,-65,-59,-53,-47,-41,-35,-29,-23,-17,-12,-6};
#endif

#if LOG2_LOOKUP_SIZE_2N == 5
static const int16_t k_log2_lookup_q10[LOG2_LOOKUP_SIZE] =
{-32767,-5120,-4096,-3497,-3072,-2742,-2473,-2245,-2048,-1874,-1718,-1578,-1449,-1331,-1221,-1119,-1024,-934,-850,-770,-694,-622,-554,-488,-425,-365,-307,-251,-197,-145,-95,-47};
#endif


int16_t FixedPointLog2Q10(uint64_t x) {
    int16_t ret;
    int16_t msb;
    int16_t shift = 0;
    if (x <= 0) {
        return k_log2_lookup_q10[0];
    }
    
    msb = CountHighestMsb(x);
    
    if (msb > LOG2_LOOKUP_SIZE_2N) {
        x >>= (msb - LOG2_LOOKUP_SIZE_2N);
        shift += msb - LOG2_LOOKUP_SIZE_2N;
    }
    
    shift -= (10 - LOG2_LOOKUP_SIZE_2N);
    
    ret = k_log2_lookup_q10[(uint16_t)x];
    ret += shift * 1024;
    
    return ret;
}

/* 2^(a + b) ---> 2^a * 2^b
    and exp(log(2)* x) == 2^x
 
    if 0 <= b < 1.... can approximate exp11
 */
uint32_t FixedPointExp2Q10(const int16_t x) {
#define LOG2_Q16 (45426)
#define B_Q16 (33915)
#define C_Q16  (21698)
#define ONE_Q10 (1 << 10)
#define ONE_Q16 (1 << 16)

    uint32_t accumulator;
    uint16_t ux;
    uint16_t a,b;
    uint16_t utemp16;
    
    //2^22 is the max we can do (32 - 10 == 22)
    if (x >= 22527) {
        return 0xFFFFFFFF;
    }
    
    if (x == -32768) {
        return 0;
    }
    
    ux = abs(x);
    
    //split to the left and right of the binary point (Q10)
    a = ux & 0xFC00;
    b = ux & 0x03FF;
    a >>= 10; //get Q10 as ints

    //multipy by log2 so  we compute exp(log(2) b) ===> 2^b
    b = umultq16((uint16_t)b,LOG2_Q16);

   
    b <<= 6; //convert from Q10 to Q16
    
    if (x > 0) {
        accumulator = ONE_Q16 + b;
    }
    else {
        accumulator = ONE_Q16 - b;
    }
    
    
    utemp16 = umultq16(b, b);
    utemp16 = umultq16(B_Q16, utemp16);

    accumulator += utemp16;
    
    utemp16 = umultq16(utemp16,b);
    utemp16 = umultq16(C_Q16, utemp16);
    
    
    if (x > 0) {
        accumulator += utemp16;
    }
    else {
        accumulator -= utemp16;
    }
    
    accumulator >>= 6; //convert back to Q10


    if (x < 0) {
        accumulator >>= a;
    }
    else {
        accumulator <<= a;
    }
    
    return accumulator;
}

// b - size of the bin in the fft in hz
// ergo, b = Fs / 2 / (n/2) or if you prefer, Nyquist freq divided by half of the FFT
// n is the PSD size (nfft / 2)
// f is both input and output
// f as input is the PSD bin, and is always positive
// f as output is the mel bins
void logpsd(int16_t * logTotalEnergy,int16_t psd[],const int16_t fr[],const int16_t fi[],uint8_t log2scaleOfRawSignal,const uint16_t numelements ) {
    uint16_t i;
    uint16_t ufr;
    uint16_t ufi;
    uint64_t utemp64;
    uint64_t accumulator64 = 0;

#define WINDOW_SIZE_2N (2)
#define WINDOW_SIZE (1 << WINDOW_SIZE_2N)
#define WINDOW_SIZE_MASK (WINDOW_SIZE - 1)
#define MIN_ENERGY_LOGPSD (16)

    
    uint16_t bufr[WINDOW_SIZE] = {0};
    uint16_t bufi[WINDOW_SIZE] = {0};
    uint16_t idx;
    uint32_t accumr = 0;
    uint32_t accumi = 0;
    const uint16_t istart = 4;
    const int16_t ioutstart = istart - WINDOW_SIZE/2;
    

    idx = 0;
    
    //square window in place... this is very crappy... our window is even sized, not odd... so it will slightly move the frequencies
    for (i = istart; i < numelements; i++) {
        idx = i & WINDOW_SIZE_MASK;
        ufr = abs(fr[i]);
        ufi = abs(fi[i]);
        
        
        
        accumr += ufr;
        accumr -= bufr[idx];
        
        accumi += ufi;
        accumi -= bufi[idx];
        
        
        bufi[idx] = abs(fi[i]);
        bufr[idx] = abs(fr[i]);
        
        if (i - WINDOW_SIZE/2 >= 0) {
            ufr = accumr >> WINDOW_SIZE_2N;
            ufi = accumi >> WINDOW_SIZE_2N;
            
            utemp64 = MIN_ENERGY_LOGPSD;
            utemp64 += (uint32_t)ufr*(uint32_t)ufr;
            utemp64 += (uint32_t)ufi*(uint32_t)ufi;
            
            if (accumulator64 + utemp64 < accumulator64) {
                accumulator64 = 0xFFFFFFFFFFFFFFFF;
            }
            else {
                accumulator64 += utemp64;
            }
            
            psd[i - WINDOW_SIZE/2] = FixedPointLog2Q10(utemp64) - 2*log2scaleOfRawSignal*(1<<10);
        }
        
    }
    
    //copy last computed elements to end
    for (i = 1; i <= WINDOW_SIZE/2; i++) {
        psd[numelements - i] = psd[numelements - WINDOW_SIZE/2 - 1];
    }
    
    //copy first computed element to begnning
    for (i = 0; i  < ioutstart; i++) {
        psd[i] = psd[ioutstart];
    }
    
    //log2 (256 * 2^10) = log2 (256) + log2(2^10) = 8 + 10
    
    *logTotalEnergy = FixedPointLog2Q10(accumulator64) - 2*log2scaleOfRawSignal*(1<<10) - (FixedPointLog2Q10(numelements) + 10);

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
