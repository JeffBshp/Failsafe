#include <stdlib.h>

#include "cglm/cglm.h"

#include "game.h"
#include "camera.h"
#include "editor.h"
#include "input.h"
#include "mesher.h"
#include "physics.h"
#include "render.h"
#include "shape.h"
#include "utility.h"
#include "world.h"

// Struct for a big block of memory to hold the state objects.
// This ensures that the members are aligned properly for cglm.
struct StateBlock
{
	GameState g;
	InputState i;
	RenderState r;
	World w;
};

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
	Camera* cam = &gs->render->camera;
	int x = (int)floorf(cam->pos[0]);
	int y = (int)floorf(cam->pos[1]);
	int z = (int)floorf(cam->pos[2]);
	int wX = gs->world->visibleCenter[0] * 64;
	int wY = gs->world->visibleCenter[1] * 64;
	int wZ = gs->world->visibleCenter[2] * 64;
	int dir = -1;

	if (x < wX) dir = 0;
	else if (x >= wX + 64) dir = 1;
	else if (y < wY) dir = 2;
	else if (y >= wY + 64) dir = 3;
	else if (z < wZ) dir = 4;
	else if (z >= wZ + 64) dir = 5;

	if (dir >= 0)
	{
		ivec3 p;
		p[0] = x;
		p[1] = y;
		p[2] = z;
		World_UpdatePosition(gs->world, p);
	}

	Mesher_MeshWorld(gs->world);
}

GameState *Game_New(void)
{
	struct StateBlock *state = calloc(1, sizeof(struct StateBlock));
	if (state == NULL) return NULL;

	GameState *gs = &state->g;
	gs->input = &state->i;
	gs->render = &state->r;
	gs->world = &state->w;

	if (!Render_Init(gs->render))
	{
		free(gs);
		return NULL;
	}

	// create world and game objects
	World_Init(gs->world);
	Shape *shapes = gs->render->shapes;
	Shape_MakeSphere(shapes + 0, 3);
	Shape_MakePlane(shapes + 1);

	// initially select one of the spheres
	const int i = 0;
	gs->selectedObject = shapes[0].models + i;
	shapes[0].instanceData[i * 17] = TEX_BLUE;

	// code text box
	char initialText[5000];
	size_t n = ReadWholeFile("res/code/kernel.txt", initialText, 5000);
	int nCols = 75, nRows = 50;
	gs->codeTextBox = Shape_MakeTextBox(shapes + 2, nCols, nRows, true, initialText);
	gs->codeTextBox->i = n - 1;

	// HUD text box
	nCols = 60; nRows = 10;
	gs->hudTextBox = Shape_MakeTextBox(shapes + 3, nCols, nRows, false, NULL);
	gs->hudTextBox->texOffset = TEX_SET2;
	glm_translate((void*)(gs->hudTextBox->shape->groupMat), (vec3) { 30.0f, 0.0f, -30.0f });
	printf("Created shapes.\n");

	// virtual computer
	gs->programFilePath = "res/code/kernel.tmp";
	Memory mem = Memory_New(16384);
	Device device = Device_New(gs->world, gs->selectedObject);
	gs->codeDemoProcessor = Processor_New(device, mem);

	// finish setting up GL buffers
	Render_InitBuffers(gs->render);

	gs->running = true;
	return gs;
}

void Game_Destroy(GameState *gs)
{
	gs->world->alive = false;

	Shape_FreeTextBox(gs->codeTextBox);
	Shape_FreeTextBox(gs->hudTextBox);

	for (int i = 0; i < gs->render->numShapes; i++)
	{
		Shape_FreeShape(gs->render->shapes + i);
	}

	Memory_Destroy(gs->codeDemoProcessor->memory);
	free(gs->codeDemoProcessor);

	Render_Destroy(gs->render);
}

void Game_Update(GameState* gs)
{
	InputState *key = gs->input;
	RenderState *rs = gs->render;
	Camera *cam = &rs->camera;
	Uint32 ticks = SDL_GetTicks();
	const int frameTargetTime = 1000 / 30; // 30 fps
	int waitTicks = gs->lastTicks + frameTargetTime - ticks;

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
		GetMoveVector(cam, move, key->space, key->lshift, key->a, key->d, key->w, key->s);

		float accel = 6.0f;
		if (gs->gravity)
		{
			move[1] -= 1.6f;
			accel = 3.0f;
		}

		glm_vec3_scale(move, accel * deltaTime, move); // scale acceleration by dt
		glm_vec3_add(cam->vel, move, cam->vel); // apply acceleration to velocity
		glm_vec3_scale(cam->vel, 0.9f, cam->vel); // scale down for pseudo-drag

		// round small velocities to zero
		if (fabsf(cam->vel[0]) < 0.001f) cam->vel[0] = 0.0f;
		if (fabsf(cam->vel[1]) < 0.001f) cam->vel[1] = 0.0f;
		if (fabsf(cam->vel[2]) < 0.001f) cam->vel[2] = 0.0f;

		if (gs->gravity)
			Physics_MoveAabbThroughVoxels(gs->world, cam->pos, cam->width, cam->vel);
		else
			glm_vec3_add(cam->pos, cam->vel, cam->pos);

		// prevent falling into the abyss
		if (cam->pos[1] < -1000.0f)
		{
			cam->pos[1] = 100.0f;
			glm_vec3_zero(cam->vel);
		}

		// move the selected object with ikjluo
		GetMoveVector(cam, move, key->i, key->k, key->j, key->l, key->u, key->o);
		glm_vec3_scale(move, 2.0f * deltaTime, move); // scale acceleration by dt
		glm_vec3_add(gs->selectedObject->vel, move, gs->selectedObject->vel); // apply acceleration to velocity
	}

	// object movement
	for (int i = 0; i < rs->numShapes; i++)
	{
		Shape* shape = rs->shapes + i;

		for (int j = 0; j < shape->numModels; j++)
		{
			Model* model = shape->models + j;

			if (model->isFixed) continue;

			// update velocity and position
			float dy = -1.6f * 1.0f * deltaTime;
			if (gs->gravity) model->vel[1]+= dy;
			float f = 1.0f - (1.0f / (model->mass * 2.0f));
			glm_vec3_scale(model->vel, f, model->vel);			// apply drag
			if (gs->gravity)
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
	camPos[0] = cam->pos[0] - (cam->width[0] / 2.0f);
	camPos[1] = cam->pos[1] - (cam->width[1] / 2.0f);
	camPos[2] = cam->pos[2] - (cam->width[2] / 2.0f);
	GetIntCoords(camPos, camLocal);
	Chunk* chunk = World_GetChunkAndCoords(gs->world, camLocal, camLocal);

	snprintf(gs->hudTextBox->text, gs->hudTextBox->nCols * gs->hudTextBox->nRows,
		"Chunk  (%5d, %5d, %5d  )\nLocal  (%5d, %5d, %5d  )\nGlobal (  %5.1f, %5.1f, %5.1f)\nVel    (  %5.1f, %5.1f, %5.1f)\n%d regions / %d chunks",
		chunk->coords[0], chunk->coords[1], chunk->coords[2],
		camLocal[0], camLocal[1], camLocal[2],
		camPos[0], camPos[1], camPos[2],
		cam->vel[0], cam->vel[1], cam->vel[2],
		gs->world->regions.size, gs->world->allChunks.size);

	Processor_Run(gs->codeDemoProcessor, ticks);
	Physics_Collide(rs->shapes, rs->numShapes);
	Editor_Update(gs->codeTextBox, ticks);
	Editor_Update(gs->hudTextBox, ticks);
	Camera_UpdateVectors(cam);
	CheckChunks(gs);
}
