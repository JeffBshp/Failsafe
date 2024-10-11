#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "noise.h"

// Credit:
// https://rtouti.github.io/graphics/perlin-noise-algorithm

static void Init(NoiseMaker* nm)
{
	if (nm->initialized) return;

	nm->initialized = true;
	// TODO: random seed
	//nm->seed = time(NULL);
	nm->seed = 10180386957696756865;
	srand(nm->seed);

	// fill array
	for (int i = 0; i < 256; i++)
		nm->influences[i] = i;

	// shuffle
	for (int i = 255; i > 0; i--)
	{
		int r = rand() % i;
		unsigned char temp = nm->influences[i];
		nm->influences[i] = nm->influences[r];
		nm->influences[r] = temp;
	}

	// repeat the shuffled values
	for (int i = 0; i < 256; i++)
		nm->influences[i + 256] = nm->influences[i];
}

static inline double Fade(double t)
{
	return ((6.0 * t - 15.0) * t + 10) * t * t * t;
}

static inline double Lerp(double t, double a, double b)
{
	return a + t * (b - a);
}

static inline double DotProductInfluence(double x, double y, unsigned char influence)
{
	const double infConvFactor = 6.283185307179586 / 255.0;
	double infRadians = (double)influence * infConvFactor;
	return (x * cos(infRadians)) + (y * sin(infRadians));
}

static double PerlinNoise2D(NoiseMaker* nm, double x, double y)
{
	unsigned char* inf = nm->influences;
	unsigned char byteX = floor(x);
	unsigned char byteY = floor(y);
	double fracX = x - floor(x);
	double fracY = y - floor(y);

	unsigned char infLL = inf[inf[byteX] + byteY];
	unsigned char infLR = inf[inf[byteX + 1] + byteY];
	unsigned char infUL = inf[inf[byteX] + byteY + 1];
	unsigned char infUR = inf[inf[byteX + 1] + byteY + 1];

	double dotLL = DotProductInfluence(fracX, fracY, infLL);
	double dotLR = DotProductInfluence(fracX - 1.0, fracY, infLR);
	double dotUL = DotProductInfluence(fracX, fracY - 1.0, infUL);
	double dotUR = DotProductInfluence(fracX - 1.0, fracY - 1.0, infUR);

	double u = Fade(fracX);
	double v = Fade(fracY);
	double lerpL = Lerp(v, dotLL, dotUL);
	double lerpR = Lerp(v, dotLR, dotUR);
	double noise = Lerp(u, lerpL, lerpR);

	return noise;
}

static inline double DotProductInfluence3D(double x, double y, double z, unsigned char influence)
{
	const double pi = 3.141592653589793;
	double yawRadians = (influence & 0x0F) * pi * 0.125;
	double pitchRadians = ((influence >> 4) + 0.5) * pi * 0.0625;
	double ix = sin(pitchRadians) * cos(yawRadians);
	double iy = sin(pitchRadians) * sin(yawRadians);
	double iz = cos(pitchRadians);
	return (x * ix) + (y * iy) + (z * iz);
}

static double PerlinNoise3D(NoiseMaker* nm, double x, double y, double z)
{
	unsigned char* inf = nm->influences;
	unsigned char byteX = floor(x);
	unsigned char byteY = floor(y);
	unsigned char byteZ = floor(z);
	double fracX = x - floor(x);
	double fracY = y - floor(y);
	double fracZ = z - floor(z);

	unsigned char infLLN = inf[inf[inf[byteX]]		+ inf[byteY]		+ byteZ];
	unsigned char infLRN = inf[inf[inf[byteX + 1]]	+ inf[byteY]		+ byteZ];
	unsigned char infULN = inf[inf[inf[byteX]]		+ inf[byteY + 1]	+ byteZ];
	unsigned char infURN = inf[inf[inf[byteX + 1]]	+ inf[byteY + 1]	+ byteZ];
	unsigned char infLLP = inf[inf[inf[byteX]]		+ inf[byteY]		+ byteZ + 1];
	unsigned char infLRP = inf[inf[inf[byteX + 1]]	+ inf[byteY]		+ byteZ + 1];
	unsigned char infULP = inf[inf[inf[byteX]]		+ inf[byteY + 1]	+ byteZ + 1];
	unsigned char infURP = inf[inf[inf[byteX + 1]]	+ inf[byteY + 1]	+ byteZ + 1];

	double dotLLN = DotProductInfluence3D(fracX,		fracY,			fracZ,			infLLN);
	double dotLRN = DotProductInfluence3D(fracX - 1.0,	fracY,			fracZ,			infLRN);
	double dotULN = DotProductInfluence3D(fracX,		fracY - 1.0,	fracZ,			infULN);
	double dotURN = DotProductInfluence3D(fracX - 1.0,	fracY - 1.0,	fracZ,			infURN);
	double dotLLP = DotProductInfluence3D(fracX,		fracY,			fracZ - 1.0,	infLLN);
	double dotLRP = DotProductInfluence3D(fracX - 1.0,	fracY,			fracZ - 1.0,	infLRN);
	double dotULP = DotProductInfluence3D(fracX,		fracY - 1.0,	fracZ - 1.0,	infULN);
	double dotURP = DotProductInfluence3D(fracX - 1.0,	fracY - 1.0,	fracZ - 1.0,	infURN);

	double u = Fade(fracX);
	double v = Fade(fracY);
	double w = Fade(fracZ);
	double lerpL = Lerp(v, dotLLN, dotULN);
	double lerpR = Lerp(v, dotLRN, dotURN);
	double lerpN = Lerp(u, lerpL, lerpR);
	lerpL = Lerp(v, dotLLP, dotULP);
	lerpR = Lerp(v, dotLRP, dotURP);
	double lerpP = Lerp(u, lerpL, lerpR);
	double noise = Lerp(w, lerpN, lerpP);

	return noise;
}

// Fractal Brownian Motion
static double FBM2D(NoiseMaker* nm, double x, double y, int octaves)
{
	const double lacunarity = 2.0; // each octave twice as dense as previous
	const double persistence = 0.5; // each octave half as influencial as previous

	double frequency = 0.005;
	double amplitude = 1.0;
	double result = 0.0;

	for (int o = 0; o < octaves; o++)
	{
		double noise = PerlinNoise2D(nm, x * frequency, y * frequency);
		result += amplitude * noise;
		amplitude *= persistence;
		frequency *= lacunarity;
	}

	return result;
}

static double FBM3D(NoiseMaker* nm, double x, double y, double z, int octaves)
{
	const double lacunarity = 2.0;
	const double persistence = 0.7;

	double frequency = 0.005;
	double amplitude = 1.5;
	double result = 0.0;

	for (int o = 0; o < octaves; o++)
	{
		double noise = PerlinNoise3D(nm, x * frequency, y * frequency, z * frequency);
		result += amplitude * noise;
		amplitude *= persistence;
		frequency *= lacunarity;
	}

	return result;
}

unsigned char* Noise_Generate2D(NoiseMaker* nm, int ofsX, int ofsY, float* progress)
{
	const int width = 64, octaves = 8;
	const int width2 = width * width;
	double min = 1000, max = -1000;
	float deltaProgress = 100.0f / (width2 + 1);
	double* noiseData = malloc(width2 * sizeof(double));
	unsigned char* imageData = malloc(width2 * sizeof(unsigned char));

	if (noiseData == NULL || imageData == NULL) return NULL;

	Init(nm);
	*progress = 0.0f;

	for (int y = 0; y < width; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int i = (y * width) + x;
			double noise = FBM2D(nm, x + (ofsX * width), y + (ofsY * width), octaves);
			noiseData[i] = noise;

			if (noise < min) min = noise;
			else if (noise > max) max = noise;

			*progress += deltaProgress;
		}
	}

	min = -0.8;
	max = 0.8;
	double conversion = 256.0 / (max - min);

	for (int y = 0; y < width; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int i = (y * width) + x;
			double noise = noiseData[i] - min;
			imageData[i] = noise * conversion;
		}
	}

	*progress += deltaProgress;
	free(noiseData);

	return imageData;
}

unsigned char* Noise_Generate3D(NoiseMaker* nm, int ofsX, int ofsY, float* progress)
{
	const int width = 64, octaves = 4;
	const int width2 = width * width;
	const int width3 = width2 * width;
	int ofsZ = 0;
	double min = 1000, max = -1000;
	float deltaProgress = 100.0f / (width3 + width);
	double* noiseData = malloc(width3 * sizeof(double));
	//unsigned char* imageData = malloc(width2 * sizeof(unsigned char));
	unsigned char* retData = malloc(width3 * sizeof(unsigned char));

	if (noiseData == NULL /*|| imageData == NULL*/ || retData == NULL) return NULL;

	Init(nm);
	*progress = 0.0f;

	for (int z = 0; z < width; z++)
	{
		for (int y = 0; y < width; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int i = (z * width2) + (y * width) + x;
				double noise = FBM3D(nm, x + (ofsX * width), y + (ofsY * width), z + (ofsZ * width), octaves);
				noiseData[i] = noise;

				if (noise < min) min = noise;
				else if (noise > max) max = noise;

				*progress += deltaProgress;
			}
		}

		min = -0.8;
		max = 0.8;
		double conversion = 256.0 / (max - min);

		for (int y = 0; y < width; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int ii = (y * width) + x;
				int in = (z * width2) + ii;
				double noise = noiseData[in] - min;
				unsigned char retNoise = noise * conversion;
				//imageData[ii] = retNoise;
				retData[in] = retNoise;
			}
		}

		// TODO: (Optional) SOIL_save_image(imageFilePath, SOIL_SAVE_TYPE_PNG, width, width, 1, imageData);

		*progress += deltaProgress;
	}

	free(noiseData);
	//free(imageData);

	return retData;
}
