#pragma once

#include <stdbool.h>

#include "SDL2/SDL.h"
#include "GL/glew.h"
#include "SDL2/SDL_opengl.h"
#include "cglm/cglm.h"

#include "camera.h"
#include "shape.h"
#include "world.h"
#include "../hardware/processor.h"

typedef struct
{
	bool w;
	bool s;
	bool a;
	bool d;
	bool q;
	bool e;

	bool t;
	bool g;
	bool f;
	bool h;
	bool r;
	bool y;

	bool i;
	bool k;
	bool j;
	bool l;
	bool u;
	bool o;

	bool up;
	bool down;
	bool left;
	bool right;
	bool rshift;
	bool rctrl;

	bool z;
	bool x;
	bool c;
	bool space;
	bool lshift;
	bool lctrl;
} InputState;

typedef struct
{
	mat4 matProj;
	mat4 matView;

	Camera camera;

	SDL_Window *window;
	SDL_GLContext *glContext;

	GLuint *VAO; // vertex array objects (one of these per shape)
	GLuint *VBO; // vertex buffer objects (vertex and texture coords)
	GLuint *IBO; // instance buffer objects (per-instance data)
	GLuint *EBO; // element buffer objects (vertex indices)

	Shape *shapes;
	GLuint *textures;

	GLuint basicShader;
	GLuint chunkShader;
	GLuint chunkBAO; // block array object
	GLuint chunkBBO; // block buffer object
	GLuint chunkQBO; // quad buffer object

	int numShapes;
	int numTextures;
} RenderState;

typedef struct
{
	InputState *input;
	RenderState *render;

	World *world;
	Model *selectedObject;
	char *programFilePath;
	Processor *codeDemoProcessor;
	TextBox *codeTextBox;
	TextBox *hudTextBox;

	int lastTicks;
	int lastSecondTicks;

	bool running;
	bool gravity;
} GameState;

GameState *Game_New(void);
void Game_Destroy(GameState *gs);
void Game_Update(GameState* gs);
