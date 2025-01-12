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

static Chunk* WorldToChunkCoords(World* world, vec3 wPos, vec3 cPos)
{
	int x = (int)floorf(wPos[0]);
	int y = (int)floorf(wPos[1]);
	int z = (int)floorf(wPos[2]);

	int cx = (x - (x < 0 ? 63 : 0)) / 64;
	int cy = (y - (y < 0 ? 63 : 0)) / 64;
	int cz = (z - (z < 0 ? 63 : 0)) / 64;

	cPos[0] = x - (cx * 64);
	cPos[1] = y - (cy * 64);
	cPos[2] = z - (cz * 64);

	return Treadmill3DGet(world->chunks, cx, cy, cz);
}

static unsigned char GetBlock(Chunk* chunk, vec3 pos)
{
	int x = floorf(pos[0]);
	int y = floorf(pos[1]);
	int z = floorf(pos[2]);

	if (x < 0 || x > 63 || y < 0 || y > 63 || z < 0 || z > 63)
		return 0;

	int i = (z * 64 * 64) + (y * 64) + x;
	return chunk->blocks[i];
}

static bool IsSolidBlock(World* world, vec3 pos)
{
	vec3 cPos;
	Chunk* chunk = WorldToChunkCoords(world, pos, cPos);
	if (chunk == NULL) return false;
	return GetBlock(chunk, cPos) != 0;
}

static bool DDA(Chunk* chunk, vec3 a, vec3 move, vec3 block, int maxSteps)
{
	double x = a[0];
	double y = a[1];
	double z = a[2];
	
	double dx = fabs(move[0]);
	double dy = fabs(move[1]);
	double dz = fabs(move[2]);

	if (dx == 0 && dy == 0 && dz == 0) return false;

	int step = 0;
	int stepX = move[0] < 0 ? -1 : 1;
	int stepY = move[1] < 0 ? -1 : 1;
	int stepZ = move[2] < 0 ? -1 : 1;

	double remX = fabs(floor(x + stepX) - x);
	double remY = fabs(floor(y + stepY) - y);
	double remZ = fabs(floor(z + stepZ) - z);

	if (remX >= 1.0) remX -= 1.0;
	if (remY >= 1.0) remY -= 1.0;
	if (remZ >= 1.0) remZ -= 1.0;

	double hypotenuse = sqrt(pow(dx, 2) + pow(dy, 2) + pow(dz, 2));

	// distance along ray per x/y/z unit
	double tDeltaX = dx == 0 ? DBL_MAX : hypotenuse / dx;
	double tDeltaY = dy == 0 ? DBL_MAX : hypotenuse / dy;
	double tDeltaZ = dz == 0 ? DBL_MAX : hypotenuse / dz;

	double tMaxX = dx == 0 ? DBL_MAX : remX * tDeltaX;
	double tMaxY = dy == 0 ? DBL_MAX : remY * tDeltaY;
	double tMaxZ = dz == 0 ? DBL_MAX : remZ * tDeltaZ;

	while (step < maxSteps)
	{
		block[0] = x;
		block[1] = y;
		block[2] = z;
		unsigned char blockType = GetBlock(chunk, block);

		if (blockType != 0 && blockType != 7)
		{
			return true;
		}

		if (tMaxX < tMaxY && tMaxX < tMaxZ) {
			x += stepX;
			tMaxX += tDeltaX;
		} else if (tMaxY < tMaxZ) {
			y += stepY;
			tMaxY += tDeltaY;
		} else {
			z += stepZ;
			tMaxZ += tDeltaZ;
		}

		step++;
	}

	return false;
}

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

// TODO: misses collisions when crossing chunk boundaries
static void CameraCollideVoxels(World* world, Camera* cam)
{
	float cx = cam->pos[0];
	float cy = cam->pos[1];
	float cz = cam->pos[2];
	float vx = cam->vel[0];
	float vy = cam->vel[1];
	float vz = cam->vel[2];
	float ox = vx > 0 ? 0.3 : -0.3;
	float oz = vz > 0 ? 0.3 : -0.3;

	vec3* posToTest = (void*)((vec3){ cx, cy - 1.4f, cz });
	if (IsSolidBlock(world, *posToTest))
	{
		cy = floorf(cy - 1.4f) + 2.3f;
		cam->pos[1] = cy;
		cam->vel[1] = 0;
	}

	posToTest = (void*)((vec3){ cx + ox + ox, cy - 1.1f, cz });
	if (IsSolidBlock(world, *posToTest))
	{
		cx = floorf(cx + ox + ox) - ox + (vx > 0 ? 0 : 1.0f);
		cam->pos[0] = cx;
		cam->vel[0] = 0;
	}

	posToTest = (void*)((vec3){ cx, cy - 1.1f, cz + oz + oz });
	if (IsSolidBlock(world, *posToTest))
	{
		cz = floorf(cz + oz + oz) - oz + (vz > 0 ? 0 : 1.0f);
		cam->pos[2] = cz;
		cam->vel[2] = 0;
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
	if (!gs->textBox->focused)
	{
		// move the camera with wsad/space/lshift
		vec3 move, blockPos;
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
		if (key->gravity) CameraCollideVoxels(gs->world, gs->cam); // avoid flying through blocks
		glm_vec3_add(gs->cam->pos, gs->cam->vel, gs->cam->pos); // apply velocity to position
		
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
			float f = 1.0f - (1.0f / (model->mass * 2.0f));
			glm_vec3_scale(model->vel, f, model->vel);			// apply drag
			glm_vec3_add(model->pos, model->vel, model->pos);	// move the object

			// rotate over time
			model->rot[0] += 0.1 * deltaTime;
			model->rot[1] += 0.2 * deltaTime;
			model->rot[2] += 0.3 * deltaTime;
		}
	}

	snprintf(gs->loadingTextBox->text, gs->loadingTextBox->nCols * gs->loadingTextBox->nRows,
		"(%.1f, %.1f, %.1f)", gs->cam->pos[0], gs->cam->pos[1], gs->cam->pos[2]);

	Physics_Collide(gs->shapes, gs->numShapes);
	Editor_Update(gs->textBox, ticks);
	Editor_Update(gs->loadingTextBox, ticks);
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

static void HandleKeyDown(InputState* key, GameState* gs, SDL_KeyCode sym)
{
	switch (sym)
	{
	case SDLK_ESCAPE:
		if (gs->textBox->focused)
			gs->textBox->focused = false;
		else
			key->running = false;
		break;

	// easier way to release the mouse than Alt+Tab
	case SDLK_BACKQUOTE:
		SDL_SetWindowGrab(gs->window, SDL_FALSE);
		SDL_MinimizeWindow(gs->window);
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

	case SDLK_RETURN:
		*(gs->processorHalt) = true;
		gs->runProgram = true;
		break;
	case SDLK_SPACE:
		key->space = true;
		if (key->gravity)
		{
			gs->cam->pos[1] += 0.2;
			gs->cam->vel[1] += 1.2;
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
		gs->textBox->focused = !gs->textBox->focused;
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
	vec3 blockPos;

	switch (e.button)
	{
	case 1:
		DDA(Treadmill3DGet(gs->world->chunks, 0, 0, 0), gs->cam->pos, gs->cam->front, blockPos, 50);
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
				if (gs->textBox->focused)
					if (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_LALT || ev.key.keysym.sym == SDLK_RALT)
						gs->textBox->focused = false;
					else
						Editor_Edit(gs->textBox, ev.key.keysym);
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
