#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "SDL2/SDL.h"
#include "cglm/cglm.h"
#include "input.h"
#include "render.h"
#include "shape.h"
#include "physics.h"
#include "editor.h"
#include "mesher.h"
#include "world.h"
#include "utility.h"
#include "compress.h"

// gets a unit-vector based on directional keyboard keys, relative to the camera's direction
static void GetMoveVector(Camera* cam, vec3 dest, bool up, bool down, bool left, bool right, bool fwd, bool back)
{
	glm_vec3_zero(dest);

	if (up)
		glm_vec3_add(dest, cam->up, dest);
	else if (down)
		glm_vec3_sub(dest, cam->up, dest);

	if (left)
		glm_vec3_sub(dest, cam->right, dest);
	else if (right)
		glm_vec3_add(dest, cam->right, dest);

	if (fwd)
		glm_vec3_add(dest, cam->front, dest);
	else if (back)
		glm_vec3_sub(dest, cam->front, dest);

	glm_vec3_normalize(dest);
}

static void CheckChunks(GameState* gs)
{
	Camera* cam = gs->cam;
	Treadmill3D* chunks = gs->world->chunks;
	float x = cam->pos[0];
	float y = cam->pos[1];
	float z = cam->pos[2];
	int wX = chunks->x * 64;
	int wY = chunks->y * 64;
	int wZ = chunks->z * 64;
	int dir = -1;

	if (x < wX) dir = 0;
	else if (x > wX + 64) dir = 1;
	else if (y < wY) dir = 2;
	else if (y > wY + 64) dir = 3;
	else if (z < wZ) dir = 4;
	else if (z > wZ + 64) dir = 5;

	if (dir >= 0)
	{
		Treadmill3DMove(chunks, dir);
	}

	if (gs->world->dirty)
	{
		Mesher_MeshWorld(gs->world);
	}
}

void Input_Update(InputState* key, GameState* gs)
{
	Uint32 ticks = SDL_GetTicks();
	int waitTicks = gs->lastTicks + FRAME_TARGET_TIME - ticks;

	if (waitTicks > 0)
	{
		SDL_Delay(waitTicks);
		ticks = SDL_GetTicks();
	}

	float deltaTime = (ticks - gs->lastTicks) / 1000.0f;
	gs->lastTicks = ticks;

	if (ticks - gs->lastSecondTicks > 1000.0f)
	{
		gs->lastSecondTicks = ticks;
	}

	// controls and camera movement
	if (!gs->codeTextBox->focused)
	{
		// move the camera with wsad/space/lshift
		vec3 move;
		GetMoveVector(gs->cam, move, key->space, key->lshift, key->a, key->d, key->w, key->s);

		float accel = 6.0f;
		if (key->gravity)
		{
			move[1] -= 1.6f;
			accel = 3.0f;
		}

		glm_vec3_scale(move, accel * deltaTime, move); // scale acceleration by dt
		glm_vec3_add(gs->cam->vel, move, gs->cam->vel); // apply acceleration to velocity
		glm_vec3_scale(gs->cam->vel, 0.9f, gs->cam->vel); // scale down for pseudo-drag

		// round small velocities to zero
		if (fabsf(gs->cam->vel[0]) < 0.001f) gs->cam->vel[0] = 0.0f;
		if (fabsf(gs->cam->vel[1]) < 0.001f) gs->cam->vel[1] = 0.0f;
		if (fabsf(gs->cam->vel[2]) < 0.001f) gs->cam->vel[2] = 0.0f;

		if (key->gravity)
			Physics_MoveAabbThroughVoxels(gs->world, gs->cam->pos, gs->cam->width, gs->cam->vel);
		else
			glm_vec3_add(gs->cam->pos, gs->cam->vel, gs->cam->pos);

		// prevent falling into the abyss
		if (gs->cam->pos[1] < -1000.0f)
		{
			gs->cam->pos[1] = 100.0f;
			glm_vec3_zero(gs->cam->vel);
		}

		// move the selected object with ikjluo
		GetMoveVector(gs->cam, move, key->i, key->k, key->j, key->l, key->u, key->o);
		glm_vec3_scale(move, 2.0f * deltaTime, move); // scale acceleration by dt
		glm_vec3_add(gs->currentModel->vel, move, gs->currentModel->vel); // apply acceleration to velocity
	}

	// object movement
	for (int i = 0; i < gs->numShapes; i++)
	{
		Shape* shape = gs->shapes + i;

		for (int j = 0; j < shape->numModels; j++)
		{
			Model* model = shape->models + j;

			if (model->isFixed) continue;

			// update velocity and position
			float dy = -1.6f * 1.0f * deltaTime;
			if (key->gravity) model->vel[1]+= dy;
			float f = 1.0f - (1.0f / (model->mass * 2.0f));
			glm_vec3_scale(model->vel, f, model->vel);			// apply drag
			if (key->gravity)
			{
				vec3 width;
				width[0] = width[1] = width[2] = model->radius * 2.0f;
				Physics_MoveAabbThroughVoxels(gs->world, model->pos, width, model->vel);
			}
			else
			{
				glm_vec3_add(model->pos, model->vel, model->pos);
			}

			// rotate over time
			model->rot[0] += 0.1 * deltaTime;
			model->rot[1] += 0.2 * deltaTime;
			model->rot[2] += 0.3 * deltaTime;
		}
	}

	vec3 camPos;
	ivec3 camLocal;
	camPos[0] = gs->cam->pos[0] - (gs->cam->width[0] / 2.0f);
	camPos[1] = gs->cam->pos[1] - (gs->cam->width[1] / 2.0f);
	camPos[2] = gs->cam->pos[2] - (gs->cam->width[2] / 2.0f);
	GetIntCoords(camPos, camLocal);
	Chunk* chunk = World_GetChunkAndCoords(gs->world, camLocal, camLocal);

	snprintf(gs->hudTextBox->text, gs->hudTextBox->nCols * gs->hudTextBox->nRows,
		"Chunk  (%5d, %5d, %5d  )\nLocal  (%5d, %5d, %5d  )\nGlobal (  %5.1f, %5.1f, %5.1f)\nVel    (  %5.1f, %5.1f, %5.1f)",
		chunk->coords[0], chunk->coords[1], chunk->coords[2],
		camLocal[0], camLocal[1], camLocal[2],
		camPos[0], camPos[1], camPos[2],
		gs->cam->vel[0], gs->cam->vel[1], gs->cam->vel[2]);

	Physics_Collide(gs->shapes, gs->numShapes);
	Editor_Update(gs->codeTextBox, ticks);
	Editor_Update(gs->hudTextBox, ticks);
	Camera_UpdateVectors(gs->cam);
	CheckChunks(gs);
}

static void SelectSphere(GameState* gs, int next)
{
	Shape* shape = gs->shapes + 0;
	int i = gs->currentModel - shape->models;
	int n = shape->numModels;
	shape->instanceData[i * 17] = TEX_WHITE;

	i += next > 0 ? 1 : next < 0 ? -1 : 0;
	if (i >= n) i = n - 1;
	else if (i <= 0) i = 0;

	gs->currentModel = shape->models + i;

	if (next != 0) shape->instanceData[i * 17] = TEX_BLUE;
}

static void TestChunkRle(GameState* gs)
{
	printf("Testing RLE chunk compression.\n");
	ivec3 wPos, cPos;
	GetIntCoords(gs->cam->pos, wPos);
	Chunk* chunk = World_GetChunkAndCoords(gs->world, wPos, cPos);
	RleChunk rle = Compress_Chunk(chunk);
	Chunk tempChunk;
	tempChunk.blocks = malloc(64 * 64 * 64);
	Decompress_Chunk(&tempChunk, rle);
	const float rawSize = (64 * 64 * 64) / 1024.0f;
	float kb = rle.n / 512.0f;
	float percent = (kb / rawSize) * 100.0f;
	printf("\nRLE Compressed Size: %.2f kiB (%.2f\%) in %d runs\n", kb, percent, rle.n);
	printf("Freeing the temporary chunk.\n");
	free(tempChunk.blocks);
	free(rle.runs);
}

static void HandleKeyDown(InputState* key, GameState* gs, SDL_KeyCode sym)
{
	switch (sym)
	{
	case SDLK_ESCAPE:
		if (gs->codeTextBox->focused)
			gs->codeTextBox->focused = false;
		else
			key->running = false;
		break;

	// easier way to release the mouse than Alt+Tab
	case SDLK_BACKQUOTE:
		SDL_bool currentMode = SDL_GetRelativeMouseMode();
		SDL_SetRelativeMouseMode(currentMode == SDL_FALSE ? SDL_TRUE : SDL_FALSE);
		//SDL_SetWindowGrab(gs->window, SDL_FALSE);
		//SDL_MinimizeWindow(gs->window);
		break;

	case SDLK_w:
		key->w = true;
		break;
	case SDLK_s:
		key->s = true;
		break;
	case SDLK_a:
		key->a = true;
		break;
	case SDLK_d:
		key->d = true;
		break;
	case SDLK_q:
		key->q = true;
		break;
	case SDLK_e:
		key->e = true;
		break;

	case SDLK_t:
		key->t = true;
		break;
	case SDLK_g:
		key->g = true;
		key->gravity = !key->gravity;
		break;
	case SDLK_f:
		key->f = true;
		break;
	case SDLK_h:
		key->h = true;
		break;
	case SDLK_r:
		key->r = true;
		break;
	case SDLK_y:
		key->y = true;
		break;

	case SDLK_i:
		key->i = true;
		break;
	case SDLK_k:
		key->k = true;
		break;
	case SDLK_j:
		key->j = true;
		break;
	case SDLK_l:
		key->l = true;
		break;
	case SDLK_u:
		key->u = true;
		break;
	case SDLK_o:
		key->o = true;
		break;

	case SDLK_UP:
		key->up = true;
		break;
	case SDLK_DOWN:
		key->down = true;
		break;
	case SDLK_LEFT:
		key->left = true;
		break;
	case SDLK_RIGHT:
		key->right = true;
		break;
	case SDLK_RSHIFT:
		key->rshift = true;
		break;
	case SDLK_RCTRL:
		key->rctrl = true;
		glm_vec3_zero(gs->currentModel->vel);
		break;

	case SDLK_z:
		key->z = true;
		SelectSphere(gs, -1);
		break;
	case SDLK_x:
		key->x = true;
		SelectSphere(gs, 1);
		break;
	case SDLK_c:
		key->c = true;
		gs->currentModel->mass *= 10.0f;
		break;
	case SDLK_v:
		SelectSphere(gs, 0);
		gs->currentModel = Shape_AddModel(gs->shapes + 0);
		break;
	case SDLK_b:
		TestChunkRle(gs);
		break;

	case SDLK_RETURN:
		if (*(gs->processorHalt)) gs->runProgram = true;
		else
		{
			printf("Halt!\n");
			gs->runProgram = false;
			*(gs->processorHalt) = true;
		}
		break;
	case SDLK_SPACE:
		key->space = true;
		if (key->gravity)
		{
			gs->cam->pos[1] += 0.2;
			gs->cam->vel[1] = 1.2;
		}
		break;
	case SDLK_LSHIFT:
		key->lshift = true;
		break;
	case SDLK_LCTRL:
		key->lctrl = true;
		glm_vec3_zero(gs->currentModel->vel);
		break;

	case SDLK_LALT:
	case SDLK_RALT:
		gs->codeTextBox->focused = !gs->codeTextBox->focused;
		break;
	}
}

static void HandleKeyUp(InputState* key, GameState* gs, SDL_KeyCode sym)
{
	switch (sym)
	{
	case SDLK_w:
		key->w = false;
		break;
	case SDLK_s:
		key->s = false;
		break;
	case SDLK_a:
		key->a = false;
		break;
	case SDLK_d:
		key->d = false;
		break;
	case SDLK_q:
		key->q = false;
		break;
	case SDLK_e:
		key->e = false;
		break;

	case SDLK_t:
		key->t = false;
		break;
	case SDLK_g:
		key->g = false;
		break;
	case SDLK_f:
		key->f = false;
		break;
	case SDLK_h:
		key->h = false;
		break;
	case SDLK_r:
		key->r = false;
		break;
	case SDLK_y:
		key->y = false;
		break;

	case SDLK_i:
		key->i = false;
		break;
	case SDLK_k:
		key->k = false;
		break;
	case SDLK_j:
		key->j = false;
		break;
	case SDLK_l:
		key->l = false;
		break;
	case SDLK_u:
		key->u = false;
		break;
	case SDLK_o:
		key->o = false;
		break;

	case SDLK_UP:
		key->up = false;
		break;
	case SDLK_DOWN:
		key->down = false;
		break;
	case SDLK_LEFT:
		key->left = false;
		break;
	case SDLK_RIGHT:
		key->right = false;
		break;
	case SDLK_RSHIFT:
		key->rshift = false;
		break;
	case SDLK_RCTRL:
		key->rctrl = false;
		break;

	case SDLK_z:
		key->z = false;
		break;
	case SDLK_x:
		key->x = false;
		break;
	case SDLK_c:
		key->c = false;
		break;
	case SDLK_SPACE:
		key->space = false;
		break;
	case SDLK_LSHIFT:
		key->lshift = false;
		break;
	case SDLK_LCTRL:
		key->lctrl = false;
		break;
	}
}

static void HandleMouseMotion(SDL_MouseMotionEvent e, GameState* gs)
{
	const float sens = 0.4f;
	gs->cam->rot[0] += e.xrel * sens;
	gs->cam->rot[1] -= e.yrel * sens;
}

static void HandleMouseDown(SDL_MouseButtonEvent e, GameState* gs)
{
	ivec3 wPos;

	switch (e.button)
	{
	case 1:
		GetIntCoords(gs->cam->pos, wPos);
		wPos[1] -= (gs->cam->width[1] / 2);
		World_SetBlock(gs->world, wPos, BLOCK_WOOD);
		Mesher_MeshWorld(gs->world);
		gs->buffer = true;
		break;
	}
}

void Input_HandleInput(InputState* key, GameState* gs)
{
	SDL_Event ev;

	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		case SDL_QUIT:
			printf("EVENT: SDL_QUIT\n");
			key->running = false;
			break;
		case SDL_KEYDOWN:
			if (ev.key.repeat == 0)
			{
				if (gs->codeTextBox->focused)
					if (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_LALT || ev.key.keysym.sym == SDLK_RALT)
						gs->codeTextBox->focused = false;
					else
						Editor_Edit(gs->codeTextBox, ev.key.keysym);
				else
					HandleKeyDown(key, gs, ev.key.keysym.sym);
			}
			break;
		case SDL_KEYUP:
			HandleKeyUp(key, gs, ev.key.keysym.sym);
			break;
		case SDL_MOUSEMOTION:
			HandleMouseMotion(ev.motion, gs);
			break;
		case SDL_MOUSEBUTTONDOWN:
			HandleMouseDown(ev.button, gs);
			break;
		}
	}
}
