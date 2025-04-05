#pragma once

#include <stdbool.h>
#include "SDL2/SDL.h"
#include "GL/glew.h"
#include "SDL2/SDL_opengl.h"
#include "cglm/cglm.h"
#include "shape.h"
#include "camera.h"
#include "mesher.h"
#include "utility.h"

// a big ugly singleton where I shove misc variables that get passed around a lot
typedef struct
{
	SDL_Window* window;
	SDL_GLContext* glContext;
	GLuint basicShader;
	GLuint chunkShader;
	GLuint* VAO;
	GLuint* VBO;
	GLuint* IBO;
	GLuint* EBO;
	GLuint* textures;
	int numTextures;
	World* world;
	GLuint chunkBAO; // block array object
	GLuint chunkBBO; // block buffer object
	GLuint chunkQBO; // quad buffer object
	Shape* shapes;
	int numShapes;
	Camera* cam;
	mat4 matProj;
	mat4 matView;
	Model* currentModel;
	int lastTicks;
	int lastSecondTicks;
	bool initialized;
	bool buffer;
	bool runProgram;
	bool* processorHalt;
	char* programFilePath;
	TextBox* codeTextBox;
	TextBox* hudTextBox;
	Progress* progress;
} GameState;

bool Render_Init(GameState* gs);
void Render_Destroy(GameState* gs);
void Render_Draw(GameState* gs);
