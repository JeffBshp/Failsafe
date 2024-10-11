#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glew.h>
#include "shader.h"

#define MAX_LINES 128
#define MAX_LINE_LENGTH 256

typedef struct
{
	GLchar* sourcePath;
	GLenum type;
} ShaderFile;

static char** ReadFileLines(const char* path, int* n)
{
	FILE* file = fopen(path, "r");
	char line[MAX_LINE_LENGTH];
	char** lines = calloc(MAX_LINES, sizeof(char*));
	*n = 0;

	if (lines == NULL) return -1;

	while (fgets(&line, MAX_LINE_LENGTH, file) != NULL && *n < MAX_LINES)
	{
		GLchar* l = strdup(&line);
		if (l == NULL) return -1;
		lines[*n] = l;
		(*n)++;
	}

	fclose(file);
	int s = sizeof(char*) * (*n);
	lines = realloc(lines, s);

	return lines;
}

static void FreeLines(char** lines, int n)
{
	for (int i = 0; i < n; i++)
	{
		free(lines[i]);
	}

	free(lines);
}

static int CompileShader(ShaderFile shader, char*** outCode, int* outNumLines)
{
	GLint success;
	*outCode = ReadFileLines(shader.sourcePath, outNumLines);
	int shaderId = glCreateShader(shader.type);
	glShaderSource(shaderId, *outNumLines, *outCode, NULL);
	glCompileShader(shaderId);
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		GLchar log[512];
		glGetShaderInfoLog(shaderId, 512, NULL, log);
		printf("%s\n", log);
		return -1;
	}

	return shaderId;
}

static int LoadShaders(ShaderFile* shaders, int numShaders)
{
	GLint success;
	GLuint shaderProgram = glCreateProgram();

	for (int i = 0; i < numShaders; i++)
	{
		char** code = NULL;
		int numLines = 0;
		int shaderId = CompileShader(shaders[i], &code, &numLines);
		if (shaderId < 0) return shaderId;
		glAttachShader(shaderProgram, shaderId);
		glDeleteShader(shaderId);
		FreeLines(code, numLines);
	}

	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

	if (!success)
	{
		GLchar log[512];
		glGetProgramInfoLog(shaderProgram, 512, NULL, log);
		printf("%s\n", log);
		return -1;
	}

	return shaderProgram;
}

int Shader_LoadBasicShaders(const GLchar* vertPath, const GLchar* fragPath)
{
	ShaderFile vert = { .sourcePath = vertPath, .type = GL_VERTEX_SHADER };
	char** vertCode = NULL;
	int vertNumLines = 0;
	int vertShaderId = CompileShader(vert, &vertCode, &vertNumLines);
	if (vertShaderId < 0) return vertShaderId;

	ShaderFile frag = { .sourcePath = fragPath, .type = GL_FRAGMENT_SHADER };
	char** fragCode = NULL;
	int fragNumLines = 0;
	int fragShaderId = CompileShader(frag, &fragCode, &fragNumLines);
	if (fragShaderId < 0) return fragShaderId;

	GLint success;
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertShaderId);
	glAttachShader(shaderProgram, fragShaderId);
	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

	if (!success)
	{
		GLchar log[512];
		glGetProgramInfoLog(shaderProgram, 512, NULL, log);
		printf("%s", log);
		return -1;
	}

	// program created, shaders no longer needed
	glDeleteShader(vertShaderId);
	glDeleteShader(fragShaderId);
	FreeLines(vertCode, vertNumLines);
	FreeLines(fragCode, fragNumLines);

	return shaderProgram;
}

int Shader_LoadVoxelShaders(const GLchar* vertPath, const GLchar* geomPath, const GLchar* fragPath)
{
	enum { n = 3 };

	ShaderFile shaders[n] =
	{
		{ .sourcePath = vertPath, .type = GL_VERTEX_SHADER },
		{ .sourcePath = geomPath, .type = GL_GEOMETRY_SHADER },
		{ .sourcePath = fragPath, .type = GL_FRAGMENT_SHADER }
	};

	return LoadShaders(shaders, n);
}
