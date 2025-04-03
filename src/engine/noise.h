#pragma once

#include <stdint.h>

typedef struct
{
	bool initialized;
	uint64_t seed;
	uint8_t influences[256 * 2];
} NoiseMaker;

uint8_t* Noise_Generate2D(NoiseMaker* nm, int ofsX, int ofsY, float* progress);
uint8_t* Noise_Generate3D(NoiseMaker* nm, int ofsX, int ofsY, int ofsZ, float* progress);
