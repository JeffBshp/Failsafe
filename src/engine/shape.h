#pragma once

#include "GL/glew.h"
#include "cglm/cglm.h"

enum
{
	MODEL_INSTANCE_SIZE = sizeof(float) + sizeof(mat4)
};

typedef struct
{
	vec3 pos;
	vec3 rot;
	vec3 vel;
	float scale;
	float radius;
	float mass;
	bool isFixed;
} Model;

typedef struct
{
	GLfloat* vertices;
	size_t numVertices;
	GLushort* indices;
	size_t numIndices;
	Model* models;
	float* instanceData;
	size_t numModels;
	mat4* groupMat;
} Shape;

typedef struct
{
	Shape* shape;
	char* text;
	int nCols;
	int nRows;
	int texOffset;
	int i;
	int selectStart;
	bool focused;
	bool showWhiteSpace;
} TextBox;

void Shape_FreeShape(Shape* shape);
Model* Shape_AddModel(Shape* shape);
void Shape_MakeCube(Shape* shape, int numModels);
void Shape_MakeGroup(Shape* shape);
TextBox* Shape_MakeTextBox(Shape* shape, int nCols, int nRows, bool showWhiteSpace, char* initialText);
void Shape_FreeTextBox(TextBox* textBox);
void Shape_MakePyramid(Shape* shape, int numModels);
void Shape_MakePlane(Shape* shape);
void Shape_MakeSphere(Shape* shape, int numModels);
void Shape_MakeFixedSpheres(Shape* shape, int numModels);
