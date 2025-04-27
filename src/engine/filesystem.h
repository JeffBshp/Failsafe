#pragma once

enum
{
	PATH_FULLMAXLEN = 200,
	PATH_SHORTMAXLEN = 50,
	PATH_MAXENT = 50,
};

typedef struct
{
	char **dirs;
	char *file;
	char *ext;
	int numDirs;
} PathBuilder;

typedef enum
{
	DIRENT_FILE,
	DIRENT_DIRECTORY,
	DIRENT_OTHER,
} DirEntryType;

typedef struct
{
	unsigned long long serial;
	unsigned long long modified;
	unsigned int size;
} RegFileInfo;

typedef struct
{
	DirEntryType type;
	RegFileInfo reg;
	char name[PATH_SHORTMAXLEN];
} DirEntryInfo;

typedef struct
{
	DirEntryInfo entries[PATH_MAXENT];
	int numEntries;
	int totalSize;
} DirInfo;

void Path_BuildStrAndMakeDirs(char *buffer, PathBuilder path);
int Path_Combine(char *buffer, char *a, char *b);
void Path_ListFiles(char *dirPath, DirInfo *dirInfo);
RegFileInfo Path_GetFileInfo(char *fullPath);
