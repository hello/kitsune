#ifndef _OCTOGRAM_H_
#define _OCTOGRAM_H_

#include "audio_types.h"

#define OCTOGRAM_SIZE (7)
#define OCTOGRAM_FFT_SIZE_2N (OCTOGRAM_SIZE + 1)
#define OCTOGRAM_FFT_SIZE (1 << OCTOGRAM_FFT_SIZE_2N)

typedef struct {
	int32_t logenergy[OCTOGRAM_SIZE];
} OctogramResult_t;

typedef struct {
	uint64_t high;
	uint64_t low;
} uint128_t;

typedef struct {
	uint128_t energies[OCTOGRAM_SIZE];
} Octogram_t;

void Octogram_Init(Octogram_t * data);
void Octogram_Update(Octogram_t * data,const int16_t samples[]);
void Octogram_GetResult(Octogram_t * data,OctogramResult_t * result);

#endif
