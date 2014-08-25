
#include "fft.h"
#include "stdlib.h" //for abs


#define FIX_MPY(DEST,A,B) (DEST) = ((long int)(A) * (long int)(B))>>15
//#define fix_mpy(A, B) (FIX_MPY(a,a,b))

#define N_WAVE          1024        /* dimension of Sinewave[] */
#define LOG2_N_WAVE     10            /* log2(N_WAVE) */

static const unsigned short sin_lut[N_WAVE/4+1] = {
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
short fxd_sin( unsigned short x ) {
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
static short fix_mpy(short a, short b)
{
  FIX_MPY(a, a, b);
  return a;
}

int fft(short fr[], short fi[], int m)
{
  int mr, nn, i, j, l, k, istep, n;
  short qr, qi, wr, wi;

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
    tmp = fr[m];
    fr[m] = fr[mr];
    fr[mr] = tmp;
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
      wr >>= 1;
      wi >>= 1;
      for (i = m; i < n; i += istep) {
    short tr,ti;
    j = i + l;
    tr = fix_mpy(wr, fr[j]) - fix_mpy(wi, fi[j]);
    ti = fix_mpy(wr, fi[j]) + fix_mpy(wi, fr[j]);
    qr = fr[i];
    qi = fi[i];
    qr >>= 1;
    qi >>= 1;
    fr[j] = qr - tr;
    fi[j] = qi - ti;
    fr[i] = qr + tr;
    fi[i] = qi + ti;
      }
    }
    --k;
    l = istep;
  }
  return 0;
}

int fftr(short f[], int m)
{
    int i, N = 1<<(m-1);
    short tt, *fr=f, *fi=&f[N];

    for (i=1; i<N; i+=2) {
        tt = f[N+i-1];
        f[N+i-1] = f[i];
        f[i] = tt;
    }
    return fft(fi, fr, m-1);
}

void fix_window(short fr[], int n)
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


void abs_fft(short psd[], const short fr[],const short fi[], short nfft)
{
    int i;
	short n2 = 1 << (nfft - 1);
    
    for (i=0; i < n2 ; ++i) {
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
static const char k_bit_count_lookup[BIT_COUNT_LOOKUP_SIZE] =
{0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4};


short CountHighestMsb(unsigned int x) {
    short j;
    short count = 0;
    for (j = 0; j < 8; j++) {
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
#define LOG2_LOOKUP_SIZE_2N (5)
#define LOG2_LOOKUP_SIZE ((1 << LOG2_LOOKUP_SIZE_2N) + 1)

static const short k_log2_lookup_q8[LOG2_LOOKUP_SIZE] =
{-32768,-2048,-1792,-1642,-1536,-1454,-1386,-1329,-1280,-1236,-1198,-1162,-1130,-1101,-1073,-1048,-1024,-1002,-980,-961,-942,-924,-906,-890,-874,-859,-845,-831,-817,-804,-792,-780,-768};

short FixedPointLog2Q8(unsigned int x) {
    short msb;
    short shift = 0;
    if (x <= 0) {
        return k_log2_lookup_q8[0];
    }
    
    msb = CountHighestMsb(x);
    
    if (msb > LOG2_LOOKUP_SIZE_2N) {
        shift = msb - LOG2_LOOKUP_SIZE_2N;
        x >>= shift;
    }
    
    x = k_log2_lookup_q8[x];
    x += shift * 256;
    
    return x;
}

// b - size of the bin in the fft in hz
// ergo, b = Fs / 2 / (n/2) or if you prefer, Nyquist freq divided by half of the FFT
// n is the PSD size (nfft / 2)
// f is both input and output
// f as input is the PSD bin, and is always positive
// f as output is the mel bins

void mel_freq(short mel[],const short f[], int nfft, int b ) {
	
    // in Hz
    static const unsigned int mel_scale[MEL_SCALE_SIZE] = {20,160,394,670,1000,1420,1900,2450,3120,4000,5100,6600,9000,14000,22050};
	static const short mel_log2_normalization_q8[MEL_SCALE_SIZE] = {0,-719,-908,-969,-1035,-1124,-1174,-1224,-1297,-1398,-1480,-1595,-1768,-2039,-2215};
    int iBin;
	int iMel;
    int nbins = 1 << (nfft - 1);
    unsigned int accumulator;
    unsigned int test;
    int log2mel;
    unsigned int freq;


#if 0 //For generating mel scales
	for( int i=0; i<50; ++i ) {
		mel = 1127 * log( 1.0 + (double)freq / 700.0 );
		printf( "%d,", mel );
		freq+=250;
	}
#endif // 0

	accumulator = 0;
    iMel = 0;
    freq = 0;
	
    /* skip dc, start at 1 */
    for(iBin = 1; iBin < nbins; iBin++ ) {
        test = accumulator + f[iBin];
        
        //check for saturation
		if (test < accumulator) {
            accumulator = 0xFFFFFFFF;
        }
        else {
            accumulator = test;
        }
        
        
		if( freq > mel_scale[iMel] || iBin == nbins - 1 ) {
			         
            log2mel = FixedPointLog2Q8(accumulator);
            log2mel += mel_log2_normalization_q8[iMel];
            
            if (log2mel < -32768) {
                log2mel = -32768;
            }
            
            if (log2mel > 32767) {
                log2mel = 32767;
            }
            
            mel[iMel] = (short) log2mel;
			
            accumulator = 0;
            
            iMel++;
            
            if( iMel == MEL_SCALE_SIZE ) {
				break;
            }

		}
        
        freq += b;

	}
}

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
