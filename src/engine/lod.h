#pragma once

#include <stdint.h>

int GetMortonCode(int x, int y, int z);
void SplitMortonCode(int morton, int *x, int *y, int *z);
void Lod_Generate(uint8_t **blocks, uint8_t *lod);
