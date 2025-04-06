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
#include "../hardware/device.h"
#include "../hardware/memory.h"
#include "../hardware/processor.h"

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

static void LoadTextures(RenderState* rs)
{
	enum { buffLen = 30 };
	char filePath[buffLen];
	int n = rs->numTextures;

	// generate GL texture IDs
	glGenTextures(n, rs->textures);
	// all textures will use the default "texture unit", which is zero
	glActiveTexture(GL_TEXTURE0);
	// map the currently bound texture to the shader (0 means GL_TEXTURE0)
	glUniform1i(glGetUniformLocation(rs->basicShader, "textureSampler"), 0);
	glUniform1i(glGetUniformLocation(rs->chunkShader, "textureSampler"), 0);

	// TODO: font.png also contains other textures
	LoadTextureArray(rs->textures[8], "res/font/font.png", 32, 20);
	printf("Loaded the texture array.\n");

	// unbind
	glBindTexture(GL_TEXTURE_2D, 0);
}

static bool InitSDL(RenderState* rs)
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
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	const int width = 1920;
	const int height = 1080;
	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
	rs->window = SDL_CreateWindow("Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, windowFlags);
	rs->glContext = SDL_GL_CreateContext(rs->window);

	if (!rs->window || !rs->glContext)
	{
		printf("ERROR: Could not create an OpenGL window.\n");
		printf(SDL_GetError());
		return false;
	}

	printf("Created OpenGL window.\n");
	return true;
}

static bool InitGLEW(RenderState* rs)
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
		rs->basicShader = shaderProgramId;
		glUseProgram(rs->basicShader);
	}

	shaderProgramId = Shader_LoadVoxelShaders("./res/glsl/cVert.glsl", "./res/glsl/cGeom.glsl", "./res/glsl/cFrag.glsl");
	if (shaderProgramId < 0)
	{
		printf("ERROR: Could not load chunk shaders.\n");
		return false;
	}
	else
	{
		rs->chunkShader = shaderProgramId;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	printf("Loaded shaders.\n");
	return true;
}

static bool InitStateObject(RenderState *rs)
{
	// TODO: use a dynamic list or something for textures and shapes
	rs->numTextures = 9;
	// spheres, planes, text box, loading text
	// TODO: keep track of objects properly, not with magical indexing
	rs->numShapes = 4;

	// allocate memory for lists
	size_t glObjListSize = sizeof(GLuint) * rs->numShapes;
	size_t glTexListSize = sizeof(GLuint) * rs->numTextures;
	size_t shapeListSize = sizeof(Shape) * rs->numShapes;
	size_t totalSize = (4 * glObjListSize) + glTexListSize + shapeListSize;
	void* rawMem = calloc(1, totalSize);

	if (rawMem == NULL) return false;

	// set pointers within the allocated space
	rs->VAO = rawMem;
	rs->VBO = rs->VAO + rs->numShapes;
	rs->IBO = rs->VBO + rs->numShapes;
	rs->EBO = rs->IBO + rs->numShapes;
	rs->textures = rs->EBO + rs->numShapes;
	rs->shapes = (void*)(rs->textures + rs->numTextures);

	// generate IDs for the Vertex Array Objects
	glGenVertexArrays(rs->numShapes, rs->VAO);
	glGenBuffers(rs->numShapes, rs->VBO);
	glGenBuffers(rs->numShapes, rs->IBO);
	glGenBuffers(rs->numShapes, rs->EBO);
	printf("Initialized GL arrays.\n");

	LoadTextures(rs);
	Camera_Init(&rs->camera);

	return true;
}

bool Render_Init(RenderState* rs)
{
	if (!InitSDL(rs)) return false;
	if (!InitGLEW(rs)) return false;
	if (!InitStateObject(rs)) return false;

	return true;
}

static void InitShapeBuffer(RenderState* rs, int shapeIndex)
{
	Shape* shape = rs->shapes + shapeIndex;

	// create and bind the VAO
	glBindVertexArray(rs->VAO[shapeIndex]);

	// vertex buffer: contains vertex and texture coords
	glBindBuffer(GL_ARRAY_BUFFER, rs->VBO[shapeIndex]);
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
	glBindBuffer(GL_ARRAY_BUFFER, rs->IBO[shapeIndex]);
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
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rs->EBO[shapeIndex]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape->numIndices * sizeof(GLushort), shape->indices, GL_STATIC_DRAW);

	glBindVertexArray(0); // unbind
}

// initializes OpenGL buffers
void Render_InitBuffers(RenderState* rs)
{
	const int n = 4;
	for (int i = 0; i < n; i++) InitShapeBuffer(rs, i);
	printf("Initialized shape buffers.\n");

	glGenVertexArrays(1, &rs->chunkBAO);
	glGenBuffers(1, &rs->chunkBBO);
	glGenBuffers(1, &rs->chunkQBO);
	glBindVertexArray(rs->chunkBAO);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, rs->chunkBBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, rs->chunkBBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, rs->chunkQBO);
	glEnableVertexAttribArray(0);
	glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 2 * sizeof(GLuint), (GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 2 * sizeof(GLuint), (GLvoid*)sizeof(GLuint));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
	printf("Initialized voxel buffers.\n");
}

// TODO: make sure this destroys everything
void Render_Destroy(RenderState* rs)
{
	if (rs == NULL) return;

	glDeleteVertexArrays(rs->numShapes, rs->VAO);
	glDeleteBuffers(rs->numShapes, rs->VBO);
	glDeleteBuffers(rs->numShapes, rs->IBO);
	glDeleteBuffers(rs->numShapes, rs->EBO);
	glDeleteVertexArrays(1, &rs->chunkBAO);
	glDeleteBuffers(1, &rs->chunkBBO);
	glDeleteBuffers(1, &rs->chunkQBO);
	glDeleteTextures(rs->numTextures, rs->textures);
	glDeleteProgram(rs->basicShader);
	glDeleteProgram(rs->chunkShader);
	free(rs->VAO); // also frees VBO, IBO, EBO, textures, and shapes

	SDL_GL_DeleteContext(rs->glContext);
	SDL_DestroyWindow(rs->window);

	printf("Renderer destroyed.\n");
}

// Sets the GL viewport while maintaining aspect ratio.
// Calculates the projection and view matrices.
static void SetViewport(RenderState *rs)
{
	const float ratio = 0.5625f; // 1080/1920
	int wWidth, wHeight, offsetX, offsetY;
	SDL_GL_GetDrawableSize(rs->window, &wWidth, &wHeight);

	if ((float)wHeight / (float)wWidth > ratio)
	{
		int temp = (int)floorf(ratio * (float)wWidth);
		offsetX = 0;
		offsetY = (wHeight - temp) / 2;
		wHeight = temp;
	}
	else
	{
		int temp = (int)floorf((float)wHeight / ratio);
		offsetX = (wWidth - temp) / 2;
		offsetY = 0;
		wWidth = temp;
	}

	glViewport(offsetX, offsetY, wWidth, wHeight);

	glm_mat4_identity(rs->matProj);
	glm_perspective(45.0f, (GLfloat)wWidth / (GLfloat)wHeight, 0.1f, 2000.0f, rs->matProj);

	glm_mat4_identity(rs->matView);
	Camera_GetViewMatrix(&rs->camera, rs->matView);
}

// Draws everything for one frame.
void Render_Draw(GameState *gs)
{
	RenderState *rs = gs->render;
	glClearColor(0.4f, 0.6f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	SetViewport(rs);

	glUseProgram(rs->basicShader);
	GLint projLoc = glGetUniformLocation(rs->basicShader, "ourProj");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, (void*)(rs->matProj));
	GLint viewLoc = glGetUniformLocation(rs->basicShader, "ourView");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (void*)(rs->matView));
	glBindTexture(GL_TEXTURE_2D, rs->textures[8]);

	vec3 scale = { 0, 0, 0 };

	for (int i = 0; i < rs->numShapes; i++)
	{
		Shape* shape = rs->shapes + i;

		if (i == 3)
		{
			void* groupMatrix = shape->groupMat;
			glm_mat4_identity(groupMatrix);
			glm_translate(groupMatrix, rs->camera.pos);
			glm_rotate_y(groupMatrix, glm_rad(-rs->camera.rot[0]), groupMatrix);
			glm_rotate_z(groupMatrix, glm_rad(rs->camera.rot[1]), groupMatrix);
			glm_translate(groupMatrix, (vec3) { 1.0, 0.5, -0.95 });
			float s = 0.01;
			glm_scale(groupMatrix, (vec3) { s, s, s });
		}

		// bind the VAO of the current shape
		glBindVertexArray(rs->VAO[i]);

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
		glBindBuffer(GL_ARRAY_BUFFER, rs->IBO[i]);
		glBufferData(GL_ARRAY_BUFFER, shape->numModels * MODEL_INSTANCE_SIZE, shape->instanceData, GL_DYNAMIC_DRAW);

		// draw all instances of the current shape
		glDrawElementsInstanced(GL_TRIANGLES, shape->numIndices, GL_UNSIGNED_SHORT, 0, shape->numModels);
	}

	glUseProgram(rs->chunkShader);
	projLoc = glGetUniformLocation(rs->chunkShader, "ourProj");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, (void*)(rs->matProj));
	viewLoc = glGetUniformLocation(rs->chunkShader, "ourView");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (void*)(rs->matView));
	glBindTexture(GL_TEXTURE_2D, rs->textures[8]);
	glBindVertexArray(rs->chunkBAO);
	const size_t numBlocks = 64 * 64 * 64;
	ListUInt64 chunkList = gs->world->allChunks;

	SDL_LockMutex(gs->world->mutex);

	for (int i = 0; i < chunkList.size; i++)
	{
		Chunk* chunk = (void *)chunkList.values[i];
		if (chunk == NULL) continue;
		if (!EnumHasFlag(chunk->flags, CHUNK_LOADED)) continue;
		if (EnumHasFlag(chunk->flags, CHUNK_DIRTY)) continue;
		if (EnumHasFlag(chunk->flags, CHUNK_DEAD)) continue;

		SDL_LockMutex(chunk->mutex);

		ListUInt64 quads = chunk->quads;
		vec3 chunkPos;
		chunkPos[0] = chunk->coords[0] * 64;
		chunkPos[1] = chunk->coords[1] * 64;
		chunkPos[2] = chunk->coords[2] * 64;

		mat4 chunkModel;
		glm_mat4_identity(chunkModel);
		glm_translate(chunkModel, chunkPos);
		int chunkScale = 1 << chunk->lodLevel;
		scale[0] = scale[1] = scale[2] = chunkScale;
		glm_scale(chunkModel, scale);

		glUniformMatrix4fv(glGetUniformLocation(rs->chunkShader, "ourModel"), 1, GL_FALSE, (void*)chunkModel);

		// TODO: Keep all chunks in one buffer and call glBufferSubData.
		// TODO: Only buffer if chunk has changed.
		if (true)
		{
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, rs->chunkBBO);
			glBufferData(GL_SHADER_STORAGE_BUFFER, numBlocks * sizeof(GLubyte), chunk->blocks, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, rs->chunkQBO);
			glBufferData(GL_ARRAY_BUFFER, quads.size * sizeof(GLuint64), quads.values, GL_DYNAMIC_DRAW);
		}
		SDL_UnlockMutex(chunk->mutex);

		glDrawArrays(GL_POINTS, 0, quads.size);
	}

	SDL_UnlockMutex(gs->world->mutex);

	SDL_GL_SwapWindow(rs->window);
}
