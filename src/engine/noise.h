#pragma once

#include <stdint.h>

typedef struct
{
	bool initialized;
	uint64_t seed;
	unsigned char influences[256 * 2];
} NoiseMaker;

unsigned char* Noise_Generate2D(NoiseMaker* nm, int ofsX, int ofsY, float* progress);
unsigned char* Noise_Generate3D(NoiseMaker* nm, int ofsX, int ofsY, float* progress);
