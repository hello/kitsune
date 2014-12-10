#ifndef _OCTOGRAM_H_
#define _OCTOGRAM_H_

#include "audio_types.h"


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
