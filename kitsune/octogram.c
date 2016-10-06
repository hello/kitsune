#include "octogram.h"
#include <string.h>
#include <stdlib.h>
#include "fft.h"

#include "hellomath.h"


inline static void uadd64(uint128_t * pn,uint64_t x) {

	uint64_t temp = pn->low;

	temp += x;

	if (temp < pn->low) {
		pn->high++;
	}

	pn->low = temp;

}

inline static void rightshift(uint128_t * pn,uint8_t n) {
	uint64_t mask = (1 << n) - 1;
	pn->low >>= n;
	pn->low |= (pn->high & mask) << (64 - n);
	pn->high >>= n;
}

#if 0
inline static void uadd128(uint128_t * pn,const uint128_t * px) {

	uint64_t temp = pn->high;

	uadd64(pn,px->low);
	temp += px->high;

	//saturate instead of overflowing
	if (temp < pn->high) {
		pn->high = 0xFFFFFFFFFFFFFFFF;
		pn->low = 0xFFFFFFFFFFFFFFFF;
	}


}
#endif




static void update_octogram(uint128_t * energies,const int16_t fr[], const int16_t fi[]) {
	uint16_t ioctogram;
	uint16_t ibin;
	uint16_t idx = 1;
	uint16_t len = 1;
	uint16_t ufr,ufi;

	for (ioctogram = 0; ioctogram < OCTOGRAM_SIZE; ioctogram++) {

		for (ibin = 0; ibin < len; ibin++) {

			ufr = abs(fr[idx]);
			ufi = abs(fi[idx]);

			uadd64(&energies[ioctogram],(uint32_t)ufr*(uint32_t)ufr);
			uadd64(&energies[ioctogram],(uint32_t)ufi*(uint32_t)ufi);

			idx++;
		}
		len <<= 1;
	}
}

/*  */
void Octogram_Init(Octogram_t * data) {
	memset(data,0,sizeof(Octogram_t));
}


void Octogram_Update(Octogram_t * data,const int16_t samples[]) {
	int16_t fr[OCTOGRAM_FFT_SIZE]; //512K
	int16_t fi[OCTOGRAM_FFT_SIZE] = {0}; //512K

	//copy over samples
	memcpy(fr,samples,OCTOGRAM_FFT_SIZE*sizeof(int16_t));

	/* window  */
	fix_window(fr,OCTOGRAM_FFT_SIZE);

	/* Get FFT */
	fft(fr,fi, OCTOGRAM_FFT_SIZE_2N);
	update_octogram(data->energies,fr,fi);

}

void Octogram_GetResult(Octogram_t * data,OctogramResult_t * result) {
	uint16_t i;
	int32_t logenergy;
	uint128_t * p;
	uint8_t highmsb;


	//right shift out high bits
	uint64_t maxhigh = 0;
	for (i = 0; i < OCTOGRAM_SIZE; i++) {
		p = &data->energies[i];
		if (p->high > maxhigh) {
			maxhigh = p->high;
		}
	}

	highmsb = CountHighestMsb(maxhigh);
//	UARTprintf("maxhigh=%d\r\n",highmsb);
	for (i = 0; i < OCTOGRAM_SIZE; i++) {
		p = &data->energies[i];
		rightshift(p,highmsb);
	}



	for (i = 0; i < OCTOGRAM_SIZE; i++) {
		p = &data->energies[i];
		logenergy  = FixedPointLog2Q10(p->low) + highmsb*1024;

	//	UARTprintf("high = %d, loglow=%d\r\n",(int)p->high,FixedPointLog2Q10(p->low));

		result->logenergy[i] = logenergy;

	}
}
