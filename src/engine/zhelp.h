#pragma once

#include <stdio.h>
#include <stdint.h>

int Z_Deflate(uint8_t *source, int size, FILE *dest, int *compressedSize);
int Z_Inflate(FILE *source, int compressedSize, uint8_t *dest, int *size);
