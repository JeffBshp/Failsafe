#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_thread.h"
#include "GL/glew.h"
#include "SDL2/SDL_opengl.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "cglm/cglm.h"
#include "camera.h"
#include "shader.h"
#include "render.h"
#include "shape.h"
#include "editor.h"
#include "utility.h"
#include "mesher.h"

static void LoadTextureArray(GLuint texture, const char* filePath, int nCols, int nRows)
{
	// https://stackoverflow.com/questions/59260533/using-texture-atlas-as-texture-array-in-opengl

	const int nMipmaps = 1;
	int nTiles = nCols * nRows;
	int tWidth, tHeight, tChannels;

	unsigned char* textureBytes = stbi_load(filePath, &tWidth, &tHeight, &tChannels, 0);

	tWidth /= nCols;
	tHeight /= nRows;

	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, tWidth, tHeight, nTiles, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	int tBytes = 4 * tWidth * tHeight;
	unsigned char* buffer = malloc(tBytes * sizeof(unsigned char));

	for (int z = 0; z < nTiles; z++)
	{
		for (int b = 0; b < tBytes; b++)
		{
			// TODO: I think there's an OpenGL function that would do these calculations for me
			int bWidth = 4 * tWidth; // width of each tile in bytes
			int i = ((z / nCols) * nCols * bWidth * tHeight)	// size (in bytes) of all tiles above
				+ ((b / bWidth) * bWidth * nCols)				// size of pixel rows above, within the current tile row
				+ ((z % nCols) * bWidth)						// width of tiles to the left
				+ (b % bWidth);									// width from the left side of the current tile

			buffer[b] = textureBytes[i];
		}

		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, z, tWidth, tHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	}

	free(buffer);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

	stbi_image_free(textureBytes);
}

static void LoadTextures(GameState* gs)
{
	enum { buffLen = 30 };
	char filePath[buffLen];
	int n = gs->numTextures;

	// generate GL texture IDs
	glGenTextures(n, gs->textures);
	// all textures will use the default "texture unit", which is zero
	glActiveTexture(GL_TEXTURE0);
	// map the currently bound texture to the shader (0 means GL_TEXTURE0)
	glUniform1i(glGetUniformLocation(gs->basicShader, "textureSampler"), 0);
	glUniform1i(glGetUniformLocation(gs->chunkShader, "textureSampler"), 0);

	// TODO: font.png also contains other textures
	LoadTextureArray(gs->textures[8], "res/font/font.png", 32, 20);
	printf("Loaded the texture array.\n");

	// unbind
	glBindTexture(GL_TEXTURE_2D, 0);
}

static void InitShapeBuffer(GameState* gs, int shapeIndex)
{
	Shape* shape = gs->shapes + shapeIndex;

	// create and bind the VAO
	glBindVertexArray(gs->VAO[shapeIndex]);

	// vertex buffer: contains vertex and texture coords
	glBindBuffer(GL_ARRAY_BUFFER, gs->VBO[shapeIndex]);
	glBufferData(GL_ARRAY_BUFFER, shape->numVertices * sizeof(GLfloat), shape->vertices, GL_STATIC_DRAW);
	GLsizei stride = 5 * sizeof(GLfloat);

	// vertex coordinates
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0);

	// texture coordinates
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(3 * sizeof(GLfloat)));

	// instance buffer: contains per-instance texture index and model transformation matrix
	// therefore each instance of the same shape, in the same draw call, can have a different texture and transformation
	glBindBuffer(GL_ARRAY_BUFFER, gs->IBO[shapeIndex]);
	stride = MODEL_INSTANCE_SIZE;

	// texture index: basically another tex coord in the third dimension because a texture array is used
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0);
	glVertexAttribDivisor(2, 1);

	// model matrix: contains 16 floats, each vert attrib has space for 4 floats, therefore it spans 4 attrib locations
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);
	glEnableVertexAttribArray(5);
	glEnableVertexAttribArray(6);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(1 * sizeof(GLfloat)));
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(5 * sizeof(GLfloat)));
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(9 * sizeof(GLfloat)));
	glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(13 * sizeof(GLfloat)));
	glVertexAttribDivisor(3, 1);
	glVertexAttribDivisor(4, 1);
	glVertexAttribDivisor(5, 1);
	glVertexAttribDivisor(6, 1);

	// index buffer: contains vertex indices, reducing the data to be buffered on the GPU
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gs->EBO[shapeIndex]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape->numIndices * sizeof(GLushort), shape->indices, GL_STATIC_DRAW);

	glBindVertexArray(0); // unbind
}

// initializes OpenGL buffers
static void InitBuffers(GameState* gs)
{
	const int n = 4;
	for (int i = 0; i < n; i++) InitShapeBuffer(gs, i);
	printf("Initialized main buffers.\n");

	glGenVertexArrays(1, &gs->chunkBAO);
	glGenBuffers(1, &gs->chunkBBO);
	glGenBuffers(1, &gs->chunkQBO);
	glBindVertexArray(gs->chunkBAO);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gs->chunkBBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gs->chunkBBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, gs->chunkQBO);
	glEnableVertexAttribArray(0);
	glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 2 * sizeof(GLuint), (GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 2 * sizeof(GLuint), (GLvoid*)sizeof(GLuint));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	printf("Initialized voxel buffers.\n");
}

static bool InitSDL(GameState* gs)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("ERROR: SDL_Init\n");
		printf(SDL_GetError());
		return false;
	}

	printf("Initialized SDL.\n");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	gs->window = SDL_CreateWindow("Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
	gs->glContext = SDL_GL_CreateContext(gs->window);
	//gs->renderer = SDL_CreateRenderer(gs->window, -1, SDL_RENDERER_TARGETTEXTURE);

	if (!gs->window || !gs->glContext)// || !gs->renderer)
	{
		printf("ERROR: Could not create an OpenGL window.\n");
		printf(SDL_GetError());
		return false;
	}

	//SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
	//SDL_SetWindowGrab(gs->window, SDL_TRUE);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	//SDL_ShowCursor(SDL_DISABLE);

	printf("Created OpenGL window.\n");
	return true;
}

static bool InitGLEW(GameState* gs)
{
	GLenum glewError = glewInit();
	if (glewError != GLEW_OK)
	{
		printf("ERROR: glewInit\n");
		printf(glewGetErrorString(glewError));
		return false;
	}

	printf("Initialized GLEW.\n");

	int shaderProgramId = Shader_LoadBasicShaders("./res/glsl/vertex.glsl", "./res/glsl/fragment.glsl");
	if (shaderProgramId < 0)
	{
		printf("ERROR: Could not load basic shaders.\n");
		return false;
	}
	else
	{
		gs->basicShader = shaderProgramId;
		glUseProgram(gs->basicShader);
	}

	shaderProgramId = Shader_LoadVoxelShaders("./res/glsl/cVert.glsl", "./res/glsl/cGeom.glsl", "./res/glsl/cFrag.glsl");
	if (shaderProgramId < 0)
	{
		printf("ERROR: Could not load chunk shaders.\n");
		return false;
	}
	else
	{
		gs->chunkShader = shaderProgramId;
	}

	glViewport(0, 0, WIDTH, HEIGHT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	printf("Loaded shaders.\n");
	return true;
}

// gets the game to a point where it can start rendering things
static bool InitStateObject(GameState* gs)
{
	// TODO: use a dynamic list or something for textures and shapes
	gs->numTextures = 9;
	// spheres, planes, text box, loading text
	// TODO: keep track of objects properly, not with magical indexing
	gs->numShapes = 4;

	// allocate memory for lists
	size_t glObjListSize = sizeof(GLuint) * gs->numShapes;
	size_t glTexListSize = sizeof(GLuint) * gs->numTextures;
	size_t shapeListSize = sizeof(Shape) * gs->numShapes;
	size_t totalSize = (4 * glObjListSize) + glTexListSize + shapeListSize;
	void* rawMem = malloc(totalSize);

	if (rawMem == NULL) return false;

	memset(rawMem, 0, totalSize);

	// set pointers within the allocated space
	gs->VAO = rawMem;
	gs->VBO = gs->VAO + gs->numShapes;
	gs->IBO = gs->VBO + gs->numShapes;
	gs->EBO = gs->IBO + gs->numShapes;
	gs->textures = gs->EBO + gs->numShapes;
	gs->shapes = (void*)(gs->textures + gs->numTextures);
	printf("Initialized arrays.\n");

	Camera_Init(gs->cam);
	gs->lastTicks = 0;
	gs->lastSecondTicks = 0;

	return true;
}

static size_t ReadProgramFile(char* buffer, int max)
{
	FILE* file = fopen("res/code/example.txt", "r");
	size_t n = fread(buffer, sizeof(char), max, file);
	fclose(file);
	return n;
}

bool Render_Init(GameState* gs)
{
	if (!InitSDL(gs)) return false;
	if (!InitGLEW(gs)) return false;
	if (!InitStateObject(gs)) return false;

	// create game objects
	World_Init(gs->world);
	Shape_MakeSphere(gs->shapes + 0, 3);
	Shape_MakePlane(gs->shapes + 1);
	char initialText[5000];
	size_t n = ReadProgramFile(initialText, 5000);
	initialText[n] = '\0';
	int nCols = 75, nRows = 50;
	gs->textBox = Shape_MakeTextBox(gs->shapes + 2, nCols, nRows, true, initialText);
	gs->textBox->i = n - 1;
	gs->currentModel = gs->shapes[0].models + 2;
	gs->shapes[0].instanceData[17 * 2] = TEX_BLUE;
	nCols = 60; nRows = 10;
	gs->loadingTextBox = Shape_MakeTextBox(gs->shapes + 3, nCols, nRows, false, NULL);
	gs->loadingTextBox->texOffset = TEX_SET2;
	glm_translate((void*)(gs->loadingTextBox->shape->groupMat), (vec3) { 30.0f, 0.0f, -30.0f });
	printf("Created shapes.\n");

	// generate IDs for the Vertex Array Objects
	glGenVertexArrays(gs->numShapes, gs->VAO);
	glGenBuffers(gs->numShapes, gs->VBO);
	glGenBuffers(gs->numShapes, gs->IBO);
	glGenBuffers(gs->numShapes, gs->EBO);
	LoadTextures(gs);
	InitBuffers(gs);
	printf("Initialized renderer.\n");

	return true;
}

// TODO: make sure this destroys everything
void Render_Destroy(GameState* gs)
{
	if (gs == NULL)
		return;

	gs->world->alive = false;

	Shape_FreeTextBox(gs->textBox);

	for (int i = 0; i < gs->numShapes; i++)
	{
		Shape_FreeShape(gs->shapes + i);
	}

	glDeleteVertexArrays(gs->numShapes, gs->VAO);
	glDeleteBuffers(gs->numShapes, gs->VBO);
	glDeleteBuffers(gs->numShapes, gs->IBO);
	glDeleteBuffers(gs->numShapes, gs->EBO);
	glDeleteVertexArrays(1, &gs->chunkBAO);
	glDeleteBuffers(1, &gs->chunkBBO);
	glDeleteBuffers(1, &gs->chunkQBO);
	glDeleteTextures(gs->numTextures, gs->textures);
	glDeleteProgram(gs->basicShader);
	free(gs->VAO); // also frees VBO, IBO, EBO, textures, and shapes

	SDL_GL_DeleteContext(gs->glContext);
	SDL_DestroyWindow(gs->window);

	printf("Renderer destroyed.\n");
}

void Render_Draw(GameState* gs)
{
	glClearColor(0.4f, 0.6f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glUseProgram(gs->basicShader);

	glm_mat4_identity(gs->matProj);
	glm_perspective(45.0f, (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 500.0f, gs->matProj);
	GLint projLoc = glGetUniformLocation(gs->basicShader, "ourProj");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, (void*)(gs->matProj));

	glm_mat4_identity(gs->matView);
	Camera_GetViewMatrix(gs->cam, gs->matView);
	GLint viewLoc = glGetUniformLocation(gs->basicShader, "ourView");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (void*)(gs->matView));

	glBindTexture(GL_TEXTURE_2D, gs->textures[8]);

	vec3 scale = { 0, 0, 0 };

	for (int i = 0; i < gs->numShapes; i++)
	{
		Shape* shape = gs->shapes + i;

		if (i == 3)
		{
			void* groupMatrix = shape->groupMat;
			glm_mat4_identity(groupMatrix);
			glm_translate(groupMatrix, gs->cam->pos);
			glm_rotate_y(groupMatrix, glm_rad(-gs->cam->rot[0]), groupMatrix);
			glm_rotate_z(groupMatrix, glm_rad(gs->cam->rot[1]), groupMatrix);
			glm_translate(groupMatrix, (vec3) { 1.0, 0.5, -0.95 });
			float s = 0.01;
			glm_scale(groupMatrix, (vec3) { s, s, s });
		}

		// bind the VAO of the current shape
		glBindVertexArray(gs->VAO[i]);

		// apply transformations to each model instance
		for (int j = 0; j < shape->numModels; j++)
		{
			Model* model = shape->models + j;
			mat4* matrix = (void*)(shape->instanceData + (j * 17) + 1);
			mat4 tempMat;
			glm_mat4_identity(tempMat);

			if (shape->groupMat != NULL)
				glm_mat4_mul((void*)(shape->groupMat), tempMat, tempMat);

			glm_translate(tempMat, model->pos);
			glm_rotate_x(tempMat, model->rot[0], tempMat);
			glm_rotate_y(tempMat, model->rot[1], tempMat);
			glm_rotate_z(tempMat, model->rot[2], tempMat);

			scale[0] = model->scale;
			scale[1] = model->scale;
			scale[2] = model->scale;
			glm_scale(tempMat, scale);

			memcpy(matrix, tempMat, sizeof(mat4));
		}

		// re-buffer the instance data because transformations may have changed
		glBindBuffer(GL_ARRAY_BUFFER, gs->IBO[i]);
		glBufferData(GL_ARRAY_BUFFER, shape->numModels * MODEL_INSTANCE_SIZE, shape->instanceData, GL_DYNAMIC_DRAW);

		// draw all instances of the current shape
		glDrawElementsInstanced(GL_TRIANGLES, shape->numIndices, GL_UNSIGNED_SHORT, 0, shape->numModels);
	}

	glUseProgram(gs->chunkShader);
	glUniformMatrix4fv(glGetUniformLocation(gs->chunkShader, "ourProj"), 1, GL_FALSE, (void*)(gs->matProj));
	glUniformMatrix4fv(glGetUniformLocation(gs->chunkShader, "ourView"), 1, GL_FALSE, (void*)(gs->matView));
	glBindTexture(GL_TEXTURE_2D, gs->textures[8]);
	glBindVertexArray(gs->chunkBAO);
	const size_t numBlocks = 64 * 64 * 64;
	Treadmill3D* chunks = gs->world->chunks;
	int wX = chunks->x;
	int wY = chunks->y;
	int wZ = chunks->z;
	int r = chunks->radius;

	SDL_LockMutex(gs->world->mutex);

	for (int z = wZ - r; z <= wZ + r; z++)
	{
		for (int y = wY - r; y <= wY + r; y++)
		{
			for (int x = wX - r; x <= wX + r; x++)
			{
				Chunk* chunk = Treadmill3DGet(chunks, x, y, z);
				if (chunk == NULL) continue;
				if (!EnumHasFlag(chunk->flags, CHUNK_LOADED)) continue;
				if (EnumHasFlag(chunk->flags, CHUNK_DIRTY)) continue;

				SDL_LockMutex(chunk->mutex);

				ListUInt64 quads = chunk->quads;
				vec3 chunkPos;
				mat4 chunkModel;
				chunkPos[0] = chunk->coords[0] * 64;
				chunkPos[1] = chunk->coords[1] * 64;
				chunkPos[2] = chunk->coords[2] * 64;

				glm_mat4_identity(chunkModel);
				glm_translate(chunkModel, chunkPos);
				glUniformMatrix4fv(glGetUniformLocation(gs->chunkShader, "ourModel"), 1, GL_FALSE, (void*)chunkModel);

				// TODO: Keep all chunks in one buffer and call glBufferSubData.
				// TODO: Only buffer if chunk has changed.
				gs->buffer = true;
				if (gs->buffer)
				{
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, gs->chunkBBO);
					glBufferData(GL_SHADER_STORAGE_BUFFER, numBlocks * sizeof(GLubyte), chunk->blocks, GL_DYNAMIC_DRAW);
					glBindBuffer(GL_ARRAY_BUFFER, gs->chunkQBO);
					glBufferData(GL_ARRAY_BUFFER, quads.size * sizeof(GLuint64), quads.values, GL_DYNAMIC_DRAW);
					gs->buffer = false;
				}
				SDL_UnlockMutex(chunk->mutex);

				glDrawArrays(GL_POINTS, 0, quads.size);
			}
		}
	}

	SDL_UnlockMutex(gs->world->mutex);

	SDL_GL_SwapWindow(gs->window);
}
