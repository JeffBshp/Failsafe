// Adapted from Mark Adler's example of zlib usage.
// Original: https://www.zlib.net/zlib_how.html

#include <stdio.h>
#include <stdint.h>
#include "zhelp.h"
#include "zlib.h"

#define CHUNK 65536 // zlib buffer size, not related to voxels

static inline z_stream StreamInit()
{
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = Z_NULL;
	strm.next_out = Z_NULL;
	strm.avail_in = 0;
	strm.avail_out = 0;
	return strm;
}

// Compresses bytes with zlib and writes to a file.
// Sets the integer pointed to by compressedSize equal to the number of compressed bytes.
// Returns Z_OK or a zlib error code.
int Z_Deflate(uint8_t *source, int size, FILE *dest, int *compressedSize)
{
	const int level = Z_DEFAULT_COMPRESSION;
	int ret, flush;
	uint8_t out[CHUNK];
	z_stream strm = StreamInit();
	*compressedSize = 0;

	ret = deflateInit(&strm, level);
	if (ret != Z_OK) return ret;

	do {
		if (size - strm.total_in < CHUNK)
		{
			// tell zlib that this is the last/only batch of data
			strm.avail_in = size - strm.total_in;
			flush = Z_FINISH;
		}
		else
		{
			// give zlib a full input buffer and tell it that there may be more to come
			strm.avail_in = CHUNK;
			flush = Z_NO_FLUSH;
		}

		// input buffer starts at this pointer
		strm.next_in = source + strm.total_in;

		// Deflate until an error occurs, input is used up, or stream is finished, hence the second loop.
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = deflate(&strm, flush);

			if (ret == Z_OK || ret == Z_STREAM_END)
			{
				// write output to disk
				int numCompressed = CHUNK - strm.avail_out;
				int numWritten = fwrite(out, 1, numCompressed, dest);
				if (numWritten != numCompressed || ferror(dest)) ret = Z_ERRNO;
			}

			// continue as long as progress is made, as indicated by Z_OK
		} while (ret == Z_OK);

		// all input should be used up
		if (strm.avail_in != 0) ret = Z_DATA_ERROR;

		// Z_BUF_ERROR means zlib needs more input. Continue until some other error or Z_STREAM_END.
	} while (ret == Z_BUF_ERROR);

	if (ret == Z_STREAM_END)
	{
		ret = Z_OK;
		*compressedSize = strm.total_out;
	}

	(void)deflateEnd(&strm);

	return ret;
}

// Decompresses a file with zlib, filling in the destination buffer.
// Sets the integer pointed to by size equal to the number of decompressed bytes.
// Returns Z_OK or a zlib error code.
int Z_Inflate(FILE *source, int compressedSize, uint8_t *dest, int *size)
{
	int ret;
	uint8_t in[CHUNK];
	z_stream strm = StreamInit();
	*size = 0;

	ret = inflateInit(&strm);
	if (ret != Z_OK) return ret;

	do {
		int sizeToRead = compressedSize - strm.total_in;
		if (sizeToRead > CHUNK) sizeToRead = CHUNK;

		strm.avail_in = fread(in, 1, sizeToRead, source);

		if (ferror(source)) { ret = Z_ERRNO; break; }
		if (strm.avail_in == 0 || strm.avail_in != sizeToRead) { ret = Z_DATA_ERROR; break; }

		strm.next_in = in;

		// Inflate until an error occurs, input is used up, or stream is finished, hence the second loop.
		do {
			strm.avail_out = CHUNK;
			strm.next_out = dest + strm.total_out;
			ret = inflate(&strm, Z_NO_FLUSH);
		} while (ret == Z_OK);

		// all input should be used up
		//if (strm.avail_in != 0) ret = Z_DATA_ERROR;

		// Z_BUF_ERROR means zlib needs more input. Continue until some other error or Z_STREAM_END.
	} while (ret == Z_BUF_ERROR);

	if (ret == Z_STREAM_END)
	{
		ret = Z_OK;
		*size = strm.total_out;
	}

	(void)inflateEnd(&strm);

	return ret;
}
