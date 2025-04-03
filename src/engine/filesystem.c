#include <stdio.h>
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
		p += len;
		len = sprintf(p, path.ext);
		p[len] = '\0';
	}
}
