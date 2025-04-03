#pragma once

typedef struct
{
	char **dirs;
	char *file;
	char *ext;
	int numDirs;
} PathBuilder;

void Path_BuildStrAndMakeDirs(char *buffer, PathBuilder path);
