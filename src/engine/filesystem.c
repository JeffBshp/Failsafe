#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "filesystem.h"

// This concatenates the strings in the PathBuilder and creates missing directories in the path.
void Path_BuildStrAndMakeDirs(char *buffer, PathBuilder path)
{
	int len;
	char *p = buffer;

	for (int i = 0; i < path.numDirs; i++)
	{
		len = sprintf(p, path.dirs[i]);
		(void)mkdir(buffer, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

		p[len] = '/';
		p += len + 1;
	}

	if (path.file == NULL)
	{
		p[-1] = '\0';
	}
	else
	{
		len = sprintf(p, path.file);

		if (path.ext != NULL)
		{
			p += len;
			len = sprintf(p, path.ext);
		}

		p[len] = '\0';
	}
}

int Path_Combine(char *buffer, char *a, char *b)
{
	const int max = PATH_FULLMAXLEN;
	int i = 0;
	int j = 0;
	char c;

	do {
		c = a[i];
		buffer[i++] = c;
	}
	while (i < max && c != '\0');

	if (i > 0 && i < max) buffer[i - 1] = '/';

	do {
		c = b[j++];
		buffer[i++] = c;
	}
	while (i < max && c != '\0');

	buffer[--i] = '\0';
	return i;
}

void Path_ListFiles(char *dirPath, DirInfo *dirInfo)
{
	DIR *dir = opendir(dirPath);
	if (dir == NULL) return;

	char entryFullPath[PATH_FULLMAXLEN];
	struct dirent *entry;
	struct stat entryStats;

	dirInfo->numEntries = 0;
	dirInfo->totalSize = 0;

	while ((entry = readdir(dir)) != NULL && dirInfo->numEntries < PATH_MAXENT)
	{
		DirEntryInfo *entryInfo = dirInfo->entries + dirInfo->numEntries;
		RegFileInfo *reg = &entryInfo->reg;

		strncpy(entryInfo->name, entry->d_name, PATH_SHORTMAXLEN - 1);
		entryInfo->name[PATH_SHORTMAXLEN - 1] = '\0';
		Path_Combine(entryFullPath, dirPath, entryInfo->name);

		if (entry->d_type == DT_REG)
		{
			entryInfo->type = DIRENT_FILE;

			if (stat(entryFullPath, &entryStats) == 0)
			{
				reg->serial = entryStats.st_ino;
				reg->modified = entryStats.st_mtime;
				reg->size = entryStats.st_size;
				dirInfo->totalSize += entryStats.st_size;
			}
		}
		else
		{
			reg->serial = 0;
			reg->modified = 0;
			reg->size = 0;

			entryInfo->type = entry->d_type == DT_DIR
				? DIRENT_DIRECTORY
				: DIRENT_OTHER;
		}

		dirInfo->numEntries++;
	}

	closedir(dir);
}

RegFileInfo Path_GetFileInfo(char *fullPath)
{
	RegFileInfo info;
	struct stat stats;

	if (stat(fullPath, &stats) == 0)
	{
		info.serial = stats.st_ino;
		info.modified = stats.st_mtime;
		info.size = stats.st_size;
	}
	else
	{
		info.serial = 0;
		info.modified = 0;
		info.size = 0;
	}

	return info;
}
