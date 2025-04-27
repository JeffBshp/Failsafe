#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "device.h"
#include "memory.h"
#include "../engine/filesystem.h"

Disk Disk_New(int size, char *realPath)
{
	Disk disk;
	disk.size = size;
	disk.usage = 0;
	disk.etaTicks = 0;
	disk.status = DISK_STARTING;
	disk.realPath = realPath;
	return disk;
}

void Disk_Reset(Disk *disk)
{
	disk->usage = 0;
	disk->etaTicks = 0;
	disk->status = DISK_STARTING;
}

static void Initialize(Disk *disk)
{
	DirInfo dirInfo;
	Path_ListFiles(disk->realPath, &dirInfo);
	disk->usage = dirInfo.totalSize;
	int count = 0;

	for (int i = 0; i < dirInfo.numEntries; i++)
	{
		if (dirInfo.entries[i].type == DIRENT_FILE) count++;
	}

	printf("Initialized Disk: %d files, %dk/%dk used\n", count, disk->usage / 1024, disk->size / 1024);
	disk->memory.data[IO_DISK_STATUS] = DISK_READY;
	disk->status = DISK_READY;
	disk->etaTicks = 0;
}

static void PutFileInfo(Disk *disk, char *path)
{
	RegFileInfo info = Path_GetFileInfo(path);
	FileInfo16 info16;

	info16.serial3 = info.serial >> 48;
	info16.serial2 = info.serial >> 32;
	info16.serial1 = info.serial >> 16;
	info16.serial0 = info.serial & 0xffff;
	info16.modified3 = info.modified >> 48;
	info16.modified2 = info.modified >> 32;
	info16.modified1 = info.modified >> 16;
	info16.modified0 = info.modified & 0xffff;
	info16.size1 = info.size >> 16;
	info16.size0 = info.size & 0xffff;

	uint16_t *dest = disk->memory.data + IO_DISK_FINFO;
	memcpy(dest, &info16, sizeof(FileInfo16));
}

static void DoCommand(Disk *disk)
{
	uint16_t *mem = disk->memory.data;
	FILE *file = NULL;
	char fullPath[PATH_FULLMAXLEN];
	char filename[PATH_SHORTMAXLEN];
	uint16_t *str = mem + mem[IO_DISK_PATH];
	uint16_t *addr = mem + mem[IO_DISK_ADDR];
	uint16_t len = mem[IO_DISK_LEN];
	size_t n;
	DiskResult result;
	UnpackString(filename, str);
	Path_Combine(fullPath, disk->realPath, filename);
	printf("Disk accessing file: %s\n", fullPath);

	switch (mem[IO_DISK_CMD])
	{
	case DISK_LIST:
		// not implemented
		result = DISK_SUCCESS;
		break;
	case DISK_READ:
		file = fopen(fullPath, "rb");

		if (file == NULL)
		{
			result = DISK_404;
			break;
		}

		n = fread(addr, sizeof(uint16_t), len, file);
		PutFileInfo(disk, fullPath);

		if (ferror(file) != 0)
		{
			result = DISK_UNK_ERR;
		}
		else if (feof(file) != 0)
		{
			result = n <= len
				? DISK_SUCCESS
				: DISK_UNK_ERR;
		}
		else
		{
			result = n == len
				? DISK_BUFFER_FULL
				: DISK_UNK_ERR;
		}
		break;
	case DISK_WRITE:
		if (len > (disk->size - disk->usage))
		{
			result = DISK_SPACE_FULL;
			break;
		}

		file = fopen(fullPath, "wb");

		if (file == NULL)
		{
			result = DISK_UNK_ERR;
			break;
		}

		n = fwrite(addr, sizeof(uint16_t), len, file);
		PutFileInfo(disk, fullPath);

		result = n != len || ferror(file) != 0
			? DISK_UNK_ERR
			: DISK_SUCCESS;
		break;
	default: break;
	}

	if (file != NULL) fclose(file);

	mem[IO_DISK_STATUS] = DISK_READY;
	mem[IO_DISK_RESULT] = result;
	mem[IO_DISK_CMD] = DISK_NOCMD;
	mem[IO_DISK_PATH] = 0;
	mem[IO_DISK_ADDR] = 0;
	mem[IO_DISK_LEN] = 0;

	disk->status = DISK_READY;
	disk->etaTicks = 0;
}

void Disk_Update(Disk *disk, int ticks)
{
	uint16_t *mem = disk->memory.data;

	if (disk->status == DISK_STARTING)
	{
		if (disk->etaTicks == 0)
		{
			mem[IO_DISK_STATUS] = DISK_STARTING;
			disk->etaTicks = ticks + 1000;
		}
		else if (ticks > disk->etaTicks)
		{
			Initialize(disk);
		}
	}
	else if (disk->status == DISK_READY)
	{
		switch (mem[IO_DISK_CMD])
		{
		case DISK_LIST:
			disk->status = DISK_BUSY;
			disk->etaTicks = ticks + 500;
			break;
		case DISK_READ:
			disk->status = DISK_READING;
			disk->etaTicks = ticks + 600 + mem[IO_DISK_LEN];
			break;
		case DISK_WRITE:
			disk->status = DISK_WRITING;
			disk->etaTicks = ticks + 800 + (2 * mem[IO_DISK_LEN]);
			break;
		}

		if (disk->status != DISK_READY)
			mem[IO_DISK_STATUS] = disk->status;
	}
	else if (ticks > disk->etaTicks)
	{
		DoCommand(disk);
	}
}
