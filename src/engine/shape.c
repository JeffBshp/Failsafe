#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gl/glew.h"
#include "cglm/cglm.h"
#include "utility.h"
#include "shape.h"

static void MakeShape(Shape* shape, GLfloat* vertices, size_t numBytesVertices, GLushort* indices, size_t numBytesIndices, int numModels, float y, float mass, float radius)
{
	if (shape == NULL || vertices == NULL || indices == NULL || numBytesVertices <= 0 || numBytesIndices <= 0)
		return;

	shape->vertices = malloc(numBytesVertices + numBytesIndices);
	shape->numVertices = numBytesVertices / sizeof(vertices[0]);
	memcpy(shape->vertices, vertices, numBytesVertices);
	
	shape->indices = shape->vertices + shape->numVertices;
	shape->numIndices = numBytesIndices / sizeof(indices[0]);
	memcpy(shape->indices, indices, numBytesIndices);

	shape->models = malloc(numModels * (sizeof(Model) + MODEL_INSTANCE_SIZE));
	shape->instanceData = shape->models + numModels;
	shape->numModels = numModels;
	shape->groupMat = NULL;

	for (int i = 0; i < shape->numModels; i++)
	{
		Model* model = shape->models + i;
		model->pos[0] = -10.0f;
		model->pos[1] = y;
		model->pos[2] = (5.0f * i) - 8.0f;
		model->scale = 1.0f;
		model->radius = radius;
		model->mass = mass;
		model->isFixed = false;
		glm_vec3_zero(model->vel);
		glm_vec3_zero(model->rot);
		float* texI = shape->instanceData + (i * 17);
		*texI = TEX_WHITE;
		mat4* matrix = texI + 1;
		glm_mat4_zero(matrix);
	}
}

void Shape_FreeShape(Shape* shape)
{
	if (shape == NULL)
		return;

	free(shape->vertices); // also frees indices
	free(shape->models); // also frees instanceData
	free(shape->groupMat);
}

Model* Shape_AddModel(Shape* shape)
{
	shape->numModels++;
	shape->models = realloc(shape->models, shape->numModels * (sizeof(Model) + MODEL_INSTANCE_SIZE));
	shape->instanceData = shape->models + shape->numModels;

	Model* newModel = shape->models + shape->numModels - 1;
	Model lastModel = shape->models[shape->numModels - 2];
	newModel->pos[0] = lastModel.pos[0];
	newModel->pos[1] = lastModel.pos[1];
	newModel->pos[2] = lastModel.pos[2] + 5.0;
	newModel->scale = lastModel.scale;
	newModel->radius = lastModel.radius;
	newModel->mass = lastModel.mass;
	newModel->isFixed = false;
	glm_vec3_zero(newModel->vel);
	glm_vec3_zero(newModel->rot);

	for (int i = 0; i < shape->numModels; i++)
	{
		float* texI = shape->instanceData + (i * 17);
		*texI = TEX_WHITE;
		mat4* matrix = texI + 1;
		glm_mat4_zero(matrix);
	}

	float* texI = shape->instanceData + ((shape->numModels - 1) * 17);
	*texI = TEX_BLUE;
	mat4* matrix = texI + 1;
	glm_mat4_zero(matrix);

	return newModel;
}

static void InitPlane(Shape* shape, int i, int tex, int yaw, int pitch, int roll, int x, int y, int z)
{
	shape->instanceData[i * 17] = tex;
	shape->models[i].isFixed = true;
	shape->models[i].rot[0] = glm_rad(yaw);
	shape->models[i].rot[1] = glm_rad(pitch);
	shape->models[i].rot[2] = glm_rad(roll);
	shape->models[i].pos[0] = x;
	shape->models[i].pos[1] = y;
	shape->models[i].pos[2] = z;
}

void Shape_MakePlane(Shape* shape)
{
	GLfloat vertices[] =
	{
		-30.0f,  0.0f, -30.0f,		0.0f, 0.0f,
		-30.0f,  0.0f,  30.0f,		0.0f, 1.0f,
		 30.0f,  0.0f,  30.0f,		1.0f, 1.0f,
		 30.0f,  0.0f, -30.0f,		1.0f, 0.0f,
	};

	GLushort indices[] =
	{
		1, 2, 0,
		2, 3, 0
	};

	MakeShape(shape, vertices, sizeof(vertices), indices, sizeof(indices), 5, 90.0f, 0.0f, 0.0f);
	
	InitPlane(shape, 0, TEX_GRAY, 0, 90, 0, 0, 90, 0);
	InitPlane(shape, 1, TEX_GRAY, -90, 0, 0, 0, 120, 30);
	InitPlane(shape, 2, TEX_GRAY, 180, -90, 0, 0, 150, 0);
	InitPlane(shape, 3, TEX_GRAY, -90, 0, -90, -30, 120, 0);
	InitPlane(shape, 4, TEX_GRAY, 90, 180, 0, 0, 120, -30);
}

void Shape_MakeCube(Shape* shape, int numModels)
{
	GLfloat vertices[] =
	{
		 0.5f,  0.5f, -0.5f,		1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,		1.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,		0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,		0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,		0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,		1.0f, 1.0f,

		-0.5f, -0.5f,  0.5f,		0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,		1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,		1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,		1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,		0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,		0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,		1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,		1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,		0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,		0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,		0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,		1.0f, 0.0f,

		 0.5f, -0.5f, -0.5f,		0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,		1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,		1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,		1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,		0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,		0.0f, 1.0f,

		-0.5f, -0.5f, -0.5f,		0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,		1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,		1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,		1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,		0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,		0.0f, 1.0f,

		 0.5f,  0.5f,  0.5f,		1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,		1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,		0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,		0.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,		0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,		1.0f, 0.0f,
	};

	GLushort indices[] =
	{
		0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 10, 11,
		12, 13, 14, 15, 16, 17,
		18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29,
		30, 31, 32, 33, 34, 35
	};

	MakeShape(shape, vertices, sizeof(vertices), indices, sizeof(indices), numModels, 120.0f, 1.0f, 1.0f);
}

static void InitGroupMember(Shape* shape, int i, vec3 p)
{
	shape->instanceData[i * 17] = (i % 6) + TEX_RED;
	Model* model = shape->models + i;
	model->isFixed = true;
	glm_vec3_copy(p, model->pos);
	glm_vec3_zero(model->rot);
}

void Shape_MakeGroup(Shape* shape)
{
	Shape_MakeCube(shape, 6);

	shape->groupMat = malloc(sizeof(mat4));
	glm_mat4_identity(shape->groupMat);
	glm_translate(shape->groupMat, (vec3) { 50.0f, 100.0f, 0.0f });

	InitGroupMember(shape, 0, (vec3) { 1.5f, 0.0f, 0.0f });
	InitGroupMember(shape, 1, (vec3) { -1.5f, 0.0f, 0.0f });
	InitGroupMember(shape, 2, (vec3) { 0.0f, 1.5f, 0.0f });
	InitGroupMember(shape, 3, (vec3) { 0.0f, -1.5f, 0.0f });
	InitGroupMember(shape, 4, (vec3) { 0.0f, 0.0f, 1.5f });
	InitGroupMember(shape, 5, (vec3) { 0.0f, 0.0f, -1.5f });
}

TextBox* Shape_MakeTextBox(Shape* shape, int nCols, int nRows, bool showWhiteSpace, char* initialText)
{
	GLfloat vertices[] =
	{
		0.0f, -1.2f, -0.8f,		1.0f, 0.0f,
		0.0f, -1.2f,  0.8f,		0.0f, 0.0f,
		0.0f,  1.2f,  0.8f,		0.0f, 1.0f,
		0.0f,  1.2f, -0.8f,		1.0f, 1.0f,
	};

	GLushort indices[] = { 1, 2, 0, 2, 3, 0 };

	size_t nChars = nCols * nRows; // the number of physical character slots, therefore the max string length
	size_t stringSize = (nChars + 1) * sizeof(char); // +1 for null terminator
	// Allocates space for the chars of the string in the same block as the struct.
	// The struct also has a pointer (textBox->text) to the beginning of the string.
	TextBox* textBox = malloc(sizeof(TextBox) + stringSize);
	textBox->shape = shape;
	textBox->text = textBox + 1;
	memset(textBox->text, '\0', nChars + 1);
	//textBox->text[0] = '\0';
	textBox->nCols = nCols;
	textBox->nRows = nRows;
	textBox->texOffset = 0;
	textBox->i = 0;
	textBox->selectStart = -1;
	textBox->focused = false;
	textBox->showWhiteSpace = showWhiteSpace;

	if (initialText != NULL) strncpy(textBox->text, initialText, nCols * nRows);

	MakeShape(shape, vertices, sizeof(vertices), indices, sizeof(indices), nChars, 0.0f, 0.0f, 0.0f);

	shape->groupMat = malloc(sizeof(mat4));
	glm_mat4_identity(shape->groupMat);
	glm_translate(shape->groupMat, (vec3) { 30.0f, 149.4f, -29.6f });
	glm_scale(shape->groupMat, (vec3) { 0.5f, 0.5f, 0.5f });


	for (int y = 0; y < nRows; y++)
	{
		for (int x = 0; x < nCols; x++)
		{
			int i = x + (y * nCols);
			InitGroupMember(shape, i, (vec3) { 0.0f, y * -2.4f, x * 1.6f });
		}
	}

	return textBox;
}

void Shape_FreeTextBox(TextBox* textBox)
{
	if (textBox == NULL)
		return;

	// the shape is included in the main array of shapes and will be freed elsewhere
	//Shape_FreeShape(textBox->shape);

	free(textBox); // the string is part of the same block and is also freed
}

void Shape_MakePyramid(Shape* shape, int numModels)
{
	GLfloat vertices[] =
	{
		-1.0f, -1.0f, -1.0f,		0.0f, 0.0f,
		 1.0f, -1.0f, -1.0f,		1.0f, 0.0f,
		 1.0f, -1.0f,  1.0f,		1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,		0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f,		1.0f, 1.0f,
		-1.0f, -1.0f,  1.0f,		0.0f, 1.0f,

		 1.0f, -1.0f, -1.0f,		1.0f, 0.0f,
		-1.0f, -1.0f, -1.0f,		0.0f, 0.0f,
		 0.0f,  1.0f,  0.0f,		0.5f, 1.0f,

		 1.0f, -1.0f,  1.0f,		1.0f, 0.0f,
		 1.0f, -1.0f, -1.0f,		0.0f, 0.0f,
		 0.0f,  1.0f,  0.0f,		0.5f, 1.0f,

		-1.0f, -1.0f,  1.0f,		1.0f, 0.0f,
		 1.0f, -1.0f,  1.0f,		0.0f, 0.0f,
		 0.0f,  1.0f,  0.0f,		0.5f, 1.0f,

		-1.0f, -1.0f, -1.0f,		1.0f, 0.0f,
		-1.0f, -1.0f,  1.0f,		0.0f, 0.0f,
		 0.0f,  1.0f,  0.0f,		0.5f, 1.0f,
	};

	GLushort indices[] =
	{
		0, 1, 2,
		3, 4, 5,
		6, 7, 8,
		9, 10, 11,
		12, 13, 14,
		15, 16, 17
	};

	MakeShape(shape, vertices, sizeof(vertices), indices, sizeof(indices), numModels, 125.0f, 10.0f, 1.0f);
}

static inline float _x(float theta, float phi)
{
	return 2.0f * sin(glm_rad(theta)) * cos(glm_rad(phi));
}

static inline float _y(float theta, float phi)
{
	return 2.0f * cos(glm_rad(theta));
}

static inline float _z(float theta, float phi)
{
	return 2.0f * sin(glm_rad(theta)) * sin(glm_rad(phi));
}

void Shape_MakeSphere(Shape* shape, int numModels)
{
	// Coordinates are converted from spherical to cartesian with the _x, _y, and _z functions.
	// Each grouping creates a circle of latitude.
	GLfloat vertices[] =
	{
		_x(  0.0f,   0.0f), _y(  0.0f,   0.0f), _z(  0.0f,   0.0f),		0.50f, 1.00f,	// 0 (north pole)

		_x( 22.5f,   0.0f), _y( 22.5f,   0.0f), _z( 22.5f,   0.0f),		0.00f, 0.90f,	// 1
		_x( 22.5f,  45.0f), _y( 22.5f,  45.0f), _z( 22.5f,  45.0f),		0.30f, 0.90f,
		_x( 22.5f,  90.0f), _y( 22.5f,  90.0f), _z( 22.5f,  90.0f),		0.60f, 0.90f,
		_x( 22.5f, 135.0f), _y( 22.5f, 135.0f), _z( 22.5f, 135.0f),		0.90f, 0.90f,
		_x( 22.5f, 180.0f), _y( 22.5f, 180.0f), _z( 22.5f, 180.0f),		0.00f, 0.90f,
		_x( 22.5f, 225.0f), _y( 22.5f, 225.0f), _z( 22.5f, 225.0f),		0.30f, 0.90f,
		_x( 22.5f, 270.0f), _y( 22.5f, 270.0f), _z( 22.5f, 270.0f),		0.60f, 0.90f,
		_x( 22.5f, 315.0f), _y( 22.5f, 315.0f), _z( 22.5f, 315.0f),		0.90f, 0.90f,	// 8

		_x( 45.0f,   0.0f), _y( 45.0f,   0.0f), _z( 45.0f,   0.0f),		0.00f, 0.80f,	// 9
		_x( 45.0f,  22.5f), _y( 45.0f,  22.5f), _z( 45.0f,  22.5f),		0.10f, 0.80f,
		_x( 45.0f,  45.0f), _y( 45.0f,  45.0f), _z( 45.0f,  45.0f),		0.20f, 0.80f,
		_x( 45.0f,  67.5f), _y( 45.0f,  67.5f), _z( 45.0f,  67.5f),		0.35f, 0.80f,
		_x( 45.0f,  90.0f), _y( 45.0f,  90.0f), _z( 45.0f,  90.0f),		0.50f, 0.80f,
		_x( 45.0f, 112.5f), _y( 45.0f, 112.5f), _z( 45.0f, 112.5f),		0.65f, 0.80f,
		_x( 45.0f, 135.0f), _y( 45.0f, 135.0f), _z( 45.0f, 135.0f),		0.80f, 0.80f,
		_x( 45.0f, 157.5f), _y( 45.0f, 157.5f), _z( 45.0f, 157.5f),		0.90f, 0.80f,
		_x( 45.0f, 180.0f), _y( 45.0f, 180.0f), _z( 45.0f, 180.0f),		0.00f, 0.80f,
		_x( 45.0f, 202.5f), _y( 45.0f, 202.5f), _z( 45.0f, 202.5f),		0.10f, 0.80f,
		_x( 45.0f, 225.0f), _y( 45.0f, 225.0f), _z( 45.0f, 225.0f),		0.20f, 0.80f,
		_x( 45.0f, 247.5f), _y( 45.0f, 247.5f), _z( 45.0f, 247.5f),		0.35f, 0.80f,
		_x( 45.0f, 270.0f), _y( 45.0f, 270.0f), _z( 45.0f, 270.0f),		0.50f, 0.80f,
		_x( 45.0f, 292.5f), _y( 45.0f, 292.5f), _z( 45.0f, 292.5f),		0.65f, 0.80f,
		_x( 45.0f, 315.0f), _y( 45.0f, 315.0f), _z( 45.0f, 315.0f),		0.80f, 0.80f,
		_x( 45.0f, 337.5f), _y( 45.0f, 337.5f), _z( 45.0f, 337.5f),		0.90f, 0.80f,	// 24

		_x( 67.5f,   0.0f), _y( 67.5f,   0.0f), _z( 67.5f,   0.0f),		0.00f, 0.65f,	// 25
		_x( 67.5f,  22.5f), _y( 67.5f,  22.5f), _z( 67.5f,  22.5f),		0.10f, 0.65f,
		_x( 67.5f,  45.0f), _y( 67.5f,  45.0f), _z( 67.5f,  45.0f),		0.20f, 0.65f,
		_x( 67.5f,  67.5f), _y( 67.5f,  67.5f), _z( 67.5f,  67.5f),		0.35f, 0.65f,
		_x( 67.5f,  90.0f), _y( 67.5f,  90.0f), _z( 67.5f,  90.0f),		0.50f, 0.65f,
		_x( 67.5f, 112.5f), _y( 67.5f, 112.5f), _z( 67.5f, 112.5f),		0.65f, 0.65f,
		_x( 67.5f, 135.0f), _y( 67.5f, 135.0f), _z( 67.5f, 135.0f),		0.80f, 0.65f,
		_x( 67.5f, 157.5f), _y( 67.5f, 157.5f), _z( 67.5f, 157.5f),		0.90f, 0.65f,
		_x( 67.5f, 180.0f), _y( 67.5f, 180.0f), _z( 67.5f, 180.0f),		0.00f, 0.65f,
		_x( 67.5f, 202.5f), _y( 67.5f, 202.5f), _z( 67.5f, 202.5f),		0.10f, 0.65f,
		_x( 67.5f, 225.0f), _y( 67.5f, 225.0f), _z( 67.5f, 225.0f),		0.20f, 0.65f,
		_x( 67.5f, 247.5f), _y( 67.5f, 247.5f), _z( 67.5f, 247.5f),		0.35f, 0.65f,
		_x( 67.5f, 270.0f), _y( 67.5f, 270.0f), _z( 67.5f, 270.0f),		0.50f, 0.65f,
		_x( 67.5f, 292.5f), _y( 67.5f, 292.5f), _z( 67.5f, 292.5f),		0.65f, 0.65f,
		_x( 67.5f, 315.0f), _y( 67.5f, 315.0f), _z( 67.5f, 315.0f),		0.80f, 0.65f,
		_x( 67.5f, 337.5f), _y( 67.5f, 337.5f), _z( 67.5f, 337.5f),		0.90f, 0.65f,	// 40

		_x( 90.0f,   0.0f), _y( 90.0f,   0.0f), _z( 90.0f,   0.0f),		0.00f, 0.50f,	// 41 (equator)
		_x( 90.0f,  22.5f), _y( 90.0f,  22.5f), _z( 90.0f,  22.5f),		0.10f, 0.50f,
		_x( 90.0f,  45.0f), _y( 90.0f,  45.0f), _z( 90.0f,  45.0f),		0.20f, 0.50f,
		_x( 90.0f,  67.5f), _y( 90.0f,  67.5f), _z( 90.0f,  67.5f),		0.35f, 0.50f,
		_x( 90.0f,  90.0f), _y( 90.0f,  90.0f), _z( 90.0f,  90.0f),		0.50f, 0.50f,
		_x( 90.0f, 112.5f), _y( 90.0f, 112.5f), _z( 90.0f, 112.5f),		0.65f, 0.50f,
		_x( 90.0f, 135.0f), _y( 90.0f, 135.0f), _z( 90.0f, 135.0f),		0.80f, 0.50f,
		_x( 90.0f, 157.5f), _y( 90.0f, 157.5f), _z( 90.0f, 157.5f),		0.90f, 0.50f,
		_x( 90.0f, 180.0f), _y( 90.0f, 180.0f), _z( 90.0f, 180.0f),		0.00f, 0.50f,
		_x( 90.0f, 202.5f), _y( 90.0f, 202.5f), _z( 90.0f, 202.5f),		0.10f, 0.50f,
		_x( 90.0f, 225.0f), _y( 90.0f, 225.0f), _z( 90.0f, 225.0f),		0.20f, 0.50f,
		_x( 90.0f, 247.5f), _y( 90.0f, 247.5f), _z( 90.0f, 247.5f),		0.35f, 0.50f,
		_x( 90.0f, 270.0f), _y( 90.0f, 270.0f), _z( 90.0f, 270.0f),		0.50f, 0.50f,
		_x( 90.0f, 292.5f), _y( 90.0f, 292.5f), _z( 90.0f, 292.5f),		0.65f, 0.50f,
		_x( 90.0f, 315.0f), _y( 90.0f, 315.0f), _z( 90.0f, 315.0f),		0.80f, 0.50f,
		_x( 90.0f, 337.5f), _y( 90.0f, 337.5f), _z( 90.0f, 337.5f),		0.90f, 0.50f,	// 56

		_x(112.5f,   0.0f), _y(112.5f,   0.0f), _z(112.5f,   0.0f),		0.00f, 0.35f,	// 57
		_x(112.5f,  22.5f), _y(112.5f,  22.5f), _z(112.5f,  22.5f),		0.10f, 0.35f,
		_x(112.5f,  45.0f), _y(112.5f,  45.0f), _z(112.5f,  45.0f),		0.20f, 0.35f,
		_x(112.5f,  67.5f), _y(112.5f,  67.5f), _z(112.5f,  67.5f),		0.35f, 0.35f,
		_x(112.5f,  90.0f), _y(112.5f,  90.0f), _z(112.5f,  90.0f),		0.50f, 0.35f,
		_x(112.5f, 112.5f), _y(112.5f, 112.5f), _z(112.5f, 112.5f),		0.65f, 0.35f,
		_x(112.5f, 135.0f), _y(112.5f, 135.0f), _z(112.5f, 135.0f),		0.80f, 0.35f,
		_x(112.5f, 157.5f), _y(112.5f, 157.5f), _z(112.5f, 157.5f),		0.90f, 0.35f,
		_x(112.5f, 180.0f), _y(112.5f, 180.0f), _z(112.5f, 180.0f),		0.00f, 0.35f,
		_x(112.5f, 202.5f), _y(112.5f, 202.5f), _z(112.5f, 202.5f),		0.10f, 0.35f,
		_x(112.5f, 225.0f), _y(112.5f, 225.0f), _z(112.5f, 225.0f),		0.20f, 0.35f,
		_x(112.5f, 247.5f), _y(112.5f, 247.5f), _z(112.5f, 247.5f),		0.35f, 0.35f,
		_x(112.5f, 270.0f), _y(112.5f, 270.0f), _z(112.5f, 270.0f),		0.50f, 0.35f,
		_x(112.5f, 292.5f), _y(112.5f, 292.5f), _z(112.5f, 292.5f),		0.65f, 0.35f,
		_x(112.5f, 315.0f), _y(112.5f, 315.0f), _z(112.5f, 315.0f),		0.80f, 0.35f,
		_x(112.5f, 337.5f), _y(112.5f, 337.5f), _z(112.5f, 337.5f),		0.90f, 0.35f,	// 72

		_x(135.0f,   0.0f), _y(135.0f,   0.0f), _z(135.0f,   0.0f),		0.00f, 0.20f,	// 73
		_x(135.0f,  22.5f), _y(135.0f,  22.5f), _z(135.0f,  22.5f),		0.10f, 0.20f,
		_x(135.0f,  45.0f), _y(135.0f,  45.0f), _z(135.0f,  45.0f),		0.20f, 0.20f,
		_x(135.0f,  67.5f), _y(135.0f,  67.5f), _z(135.0f,  67.5f),		0.35f, 0.20f,
		_x(135.0f,  90.0f), _y(135.0f,  90.0f), _z(135.0f,  90.0f),		0.50f, 0.20f,
		_x(135.0f, 112.5f), _y(135.0f, 112.5f), _z(135.0f, 112.5f),		0.65f, 0.20f,
		_x(135.0f, 135.0f), _y(135.0f, 135.0f), _z(135.0f, 135.0f),		0.80f, 0.20f,
		_x(135.0f, 157.5f), _y(135.0f, 157.5f), _z(135.0f, 157.5f),		0.90f, 0.20f,
		_x(135.0f, 180.0f), _y(135.0f, 180.0f), _z(135.0f, 180.0f),		0.00f, 0.20f,
		_x(135.0f, 202.5f), _y(135.0f, 202.5f), _z(135.0f, 202.5f),		0.10f, 0.20f,
		_x(135.0f, 225.0f), _y(135.0f, 225.0f), _z(135.0f, 225.0f),		0.20f, 0.20f,
		_x(135.0f, 247.5f), _y(135.0f, 247.5f), _z(135.0f, 247.5f),		0.35f, 0.20f,
		_x(135.0f, 270.0f), _y(135.0f, 270.0f), _z(135.0f, 270.0f),		0.50f, 0.20f,
		_x(135.0f, 292.5f), _y(135.0f, 292.5f), _z(135.0f, 292.5f),		0.65f, 0.20f,
		_x(135.0f, 315.0f), _y(135.0f, 315.0f), _z(135.0f, 315.0f),		0.80f, 0.20f,
		_x(135.0f, 337.5f), _y(135.0f, 337.5f), _z(135.0f, 337.5f),		0.90f, 0.20f,	// 88

		_x(157.5f,   0.0f), _y(157.5f,   0.0f), _z(157.5f,   0.0f),		0.00f, 0.10f,	// 89
		_x(157.5f,  45.0f), _y(157.5f,  45.0f), _z(157.5f,  45.0f),		0.30f, 0.10f,
		_x(157.5f,  90.0f), _y(157.5f,  90.0f), _z(157.5f,  90.0f),		0.60f, 0.10f,
		_x(157.5f, 135.0f), _y(157.5f, 135.0f), _z(157.5f, 135.0f),		0.90f, 0.10f,
		_x(157.5f, 180.0f), _y(157.5f, 180.0f), _z(157.5f, 180.0f),		0.00f, 0.10f,
		_x(157.5f, 225.0f), _y(157.5f, 225.0f), _z(157.5f, 225.0f),		0.30f, 0.10f,
		_x(157.5f, 270.0f), _y(157.5f, 270.0f), _z(157.5f, 270.0f),		0.60f, 0.10f,
		_x(157.5f, 315.0f), _y(157.5f, 315.0f), _z(157.5f, 315.0f),		0.90f, 0.10f,	// 96

		_x(180.0f,   0.0f), _y(180.0f,   0.0f), _z(180.0f,   0.0f),		0.50f, 0.00f	// 97 (south pole)
	};

	GLushort indices[] =
	{
		 0,  2,  1,		 0,  3,  2,		 0,  4,  3,		 0,  5,  4,		 0,  6,  5,		 0,  7,  6,		 0,  8,  7,		 0,  1,  8,

		 1, 10,  9,		 1,  2, 10,		 2, 11, 10,		 2, 12, 11,		 2,  3, 12,		 3, 13, 12,		 3, 14, 13,		 3,  4, 14,
		 4, 15, 14,		 4, 16, 15,		 4,  5, 16,		 5, 17, 16,		 5, 18, 17,		 5,  6, 18,		 6, 19, 18,		 6, 20, 19,
		 6,  7, 20,		 7, 21, 20,		 7, 22, 21,		 7,  8, 22,		 8, 23, 22,		 8, 24, 23,		 8,  1, 24,		 1,  9, 24,

		 9, 10, 25,		10, 26, 25,		10, 11, 26,		11, 27, 26,		11, 28, 27,		11, 12, 28,		12, 29, 28,		12, 13, 29,
		13, 14, 29,		14, 30, 29,		14, 15, 30,		15, 31, 30,		15, 32, 31,		15, 16, 32,		16, 33, 32,		16, 17, 33,
		17, 18, 33,		18, 34, 33,		18, 19, 34,		19, 35, 34,		19, 36, 35,		19, 20, 36,		20, 37, 36,		20, 21, 37,
		21, 22, 37,		22, 38, 37,		22, 23, 38,		23, 39, 38,		23, 40, 39,		23, 24, 40,		24, 25, 40,		24,  9, 25,

		25, 26, 41,		26, 42, 41,		26, 27, 42,		27, 43, 42,		27, 44, 43,		27, 28, 44,		28, 45, 44,		28, 29, 45,
		29, 30, 45,		30, 46, 45,		30, 31, 46,		31, 47, 46,		31, 48, 47,		31, 32, 48,		32, 49, 48,		32, 33, 49,
		33, 34, 49,		34, 50, 49,		34, 35, 50,		35, 51, 50,		35, 52, 51,		35, 36, 52,		36, 53, 52,		36, 37, 53,
		37, 38, 53,		38, 54, 53,		38, 39, 54,		39, 55, 54,		39, 56, 55,		39, 40, 56,		40, 41, 56,		40, 25, 41,

		41, 42, 57,		42, 58, 57,		42, 43, 58,		43, 59, 58,		43, 60, 59,		43, 44, 60,		44, 61, 60,		44, 45, 61,
		45, 46, 61,		46, 62, 61,		46, 47, 62,		47, 63, 62,		47, 64, 63,		47, 48, 64,		48, 65, 64,		48, 49, 65,
		49, 50, 65,		50, 66, 65,		50, 51, 66,		51, 67, 66,		51, 68, 67,		51, 52, 68,		52, 69, 68,		52, 53, 69,
		53, 54, 69,		54, 70, 69,		54, 55, 70,		55, 71, 70,		55, 72, 71,		55, 56, 72,		56, 57, 72,		56, 41, 57,

		57, 58, 73,		58, 74, 73,		58, 59, 74,		59, 75, 74,		59, 76, 75,		59, 60, 76,		60, 77, 76,		60, 61, 77,
		61, 62, 77,		62, 78, 77,		62, 63, 78,		63, 79, 78,		63, 80, 79,		63, 64, 80,		64, 81, 80,		64, 65, 81,
		65, 66, 81,		66, 82, 81,		66, 67, 82,		67, 83, 82,		67, 84, 83,		67, 68, 84,		68, 85, 84,		68, 69, 85,
		69, 70, 85,		70, 86, 85,		70, 71, 86,		71, 87, 86,		71, 88, 87,		71, 72, 88,		72, 73, 88,		72, 57, 73,

		73, 74, 89,		74, 90, 89,		74, 75, 90,		75, 76, 90,		76, 91, 90,		76, 77, 91,		77, 78, 91,		78, 92, 91,
		78, 79, 92,		79, 80, 92,		80, 93, 92,		80, 81, 93,		81, 82, 93,		82, 94, 93,		82, 83, 94,		83, 84, 94,
		84, 95, 94,		84, 85, 95,		85, 86, 95,		86, 96, 95,		86, 87, 96,		87, 88, 96,		88, 89, 96,		88, 73, 89,

		89, 90, 97,		90, 91, 97,		91, 92, 97,		92, 93, 97,		93, 94, 97,		94, 95, 97,		95, 96, 97,		96, 89, 97
	};

	MakeShape(shape, vertices, sizeof(vertices), indices, sizeof(indices), numModels, 130.0f, 50.0f, 2.0f);
}

void Shape_MakeFixedSpheres(Shape* shape, int numModels)
{
	Shape_MakeSphere(shape, numModels);
	shape->groupMat = malloc(sizeof(mat4));
	glm_mat4_identity(shape->groupMat);

	for (int i = 0; i < numModels; i++)
	{
		Model* model = shape->models + i;
		model->scale *= 0.05;
		model->radius *= 0.01;
		InitGroupMember(shape, i, (vec3) { -40.0f, i, -40.0f });
	}
}
