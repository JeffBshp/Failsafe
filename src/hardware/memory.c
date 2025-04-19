#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "memory.h"

Memory Memory_New(uword size)
{
	Memory mem;
	mem.n = size;
	mem.data = calloc(size, sizeof(uword));
	return mem;
}

void Memory_Destroy(Memory mem)
{
	free(mem.data);
}

// reverses two bytes
static inline uword SwapEndian(uword x)
{
	return (x >> 8) | (x << 8);
}

void Memory_ReadFile(Memory mem, char* filePath)
{
	FILE* file = fopen(filePath, "rb");

	// Could just do this if the byte order didn't have to be reversed:
	//fread(mem.data, sizeof(uword), mem.n, file);

	uword buffer = 0;
	uword i = 0;

	while (0 != fread(&buffer, sizeof(uword), 1, file))
		mem.data[i++] = SwapEndian(buffer);

	fclose(file);
}

void Memory_WriteFile(Memory mem, char* filePath)
{
	FILE* file = fopen(filePath, "wb");

	// Could just do this if the byte order didn't have to be reversed:
	//fwrite(mem.data, sizeof(uword), mem.n, file);

	uword buffer = 0;
	uword i = 0;

	do
	{
		buffer = SwapEndian(mem.data[i++]);
		fwrite(&buffer, sizeof(uword), 1, file);
	}
	while (i < mem.n);

	fclose(file);
}

uword Memory_Allocate(Memory mem, uword size)
{
	if (size <= 0 || size > mem.n - 4) return 0;

	uword prev = 0, prevSize = 1;
	uword next = mem.data[0];
	uword start, end;

	while (0 == 0)
	{
		start = prev + prevSize + 3;		// The next potential free address
		end = next == 0 ? mem.n : next - 3;	// The next unavailable address

		if (end - start >= size)
		{
			// Link the previous allocation to the new one
			uword prevLink = prev < 2 ? 0 : prev - 2;
			mem.data[prevLink] = start;

			// Set up the new allocation
			mem.data[start - 3] = prev;
			mem.data[start - 2] = next;
			mem.data[start - 1] = size;

			// Link the next one to the new one
			if (next > 3) mem.data[next - 3] = start;

			return start;
		}

		if (next == 0 || next <= prev) break; // out of memory, or memory corrupted

		prev = next;
		prevSize = mem.data[next - 1];
		next = mem.data[next - 2];
	}

	return 0;
}

void Memory_Free(Memory mem, uword address)
{
	if (address <= 4 || address >= mem.n) return;

	uword prev = mem.data[address - 3];
	uword next = mem.data[address - 2];

	uword prevLink = prev < 2 ? 0 : prev - 2;
	mem.data[prevLink] = next;

	if (next > 3) mem.data[next - 3] = prev;
}
