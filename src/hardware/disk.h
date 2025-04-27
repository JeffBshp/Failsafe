#pragma once

#include <stdint.h>

#include "memory.h"

typedef enum
{
	DISK_STARTING,
	DISK_READY,
	DISK_BUSY,
	DISK_READING,
	DISK_WRITING,
} DiskStatus;

typedef enum
{
	DISK_NOCMD,
	DISK_LIST,
	DISK_READ,
	DISK_WRITE,
} DiskCommand;

typedef enum
{
	DISK_SUCCESS,
	DISK_UNK_ERR,			// unknown error
	DISK_404,				// file not found
	DISK_BUFFER_FULL,		// filled memory buffer before finished reading
	DISK_SPACE_FULL,		// not enough disk space to write
} DiskResult;

// file info broken into 16-bit chunks for the virtual machine
typedef struct
{
	uint16_t serial3;
	uint16_t serial2;
	uint16_t serial1;
	uint16_t serial0;
	uint16_t modified3;
	uint16_t modified2;
	uint16_t modified1;
	uint16_t modified0;
	uint16_t size1;
	uint16_t size0;
} FileInfo16;

typedef struct
{
	uint32_t size;
	uint32_t usage;
	uint32_t etaTicks;
	DiskStatus status;
	Memory memory;
	char *realPath; // path to irl folder that contains this virtual disk's filesystem
} Disk;

Disk Disk_New(int size, char *realPath);
void Disk_Reset(Disk *disk);
void Disk_Update(Disk *disk, int ticks);
