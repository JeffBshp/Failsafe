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
#include "../hardware/device.h"
#include "../hardware/memory.h"
#include "../hardware/processor.h"
#include "../language/parser.h"
#include "../language/compiler.h"

static void SelectSphere(GameState* gs, int next)
{
	Shape* shape = gs->render->shapes + 0;
	int i = gs->selectedObject - shape->models;
	int n = shape->numModels;
	shape->instanceData[i * 17] = TEX_WHITE;

	i += next > 0 ? 1 : next < 0 ? -1 : 0;
	if (i >= n) i = n - 1;
	else if (i <= 0) i = 0;

	gs->selectedObject = shape->models + i;

	if (next != 0) shape->instanceData[i * 17] = TEX_BLUE;
}

static void RunProgram(GameState *gs)
{
	Processor *processor = gs->codeDemoProcessor;

	if (processor->poweredOn)
	{
		printf("Shutting down processor...\n");
		processor->poweredOn = false;
	}
	else
	{
		// Save a temporary copy of the code. It will be compiled during boot instead of the original.
		Editor_SaveToFile(gs->codeTextBox, gs->programFilePath);

		printf("Booting virtual machine...\n");
		if (Processor_Boot(processor))
		{
			Memory_WriteFile(processor->memory, "res/code/out.mem");
		}
	}
}

static void HandleKeyDown(GameState *gs, SDL_KeyCode sym)
{
	InputState *key = gs->input;
	Uint32 windowFlags;

	switch (sym)
	{
	case SDLK_ESCAPE:
		if (gs->codeTextBox->focused)
			gs->codeTextBox->focused = false;
		else
			gs->running = false;
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
		gs->gravity = !gs->gravity;
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
		glm_vec3_zero(gs->selectedObject->vel);
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
		gs->selectedObject->mass *= 10.0f;
		break;
	case SDLK_v:
		SelectSphere(gs, 0);
		gs->selectedObject = Shape_AddModel(gs->render->shapes + 0);
		break;
	case SDLK_b:
		break;

	case SDLK_RETURN:
		RunProgram(gs);
		break;
	case SDLK_SPACE:
		key->space = true;
		if (gs->gravity)
		{
			gs->render->camera.pos[1] += 0.2;
			gs->render->camera.vel[1] = 1.2;
		}
		break;
	case SDLK_LSHIFT:
		key->lshift = true;
		break;
	case SDLK_LCTRL:
		key->lctrl = true;
		glm_vec3_zero(gs->selectedObject->vel);
		break;

	case SDLK_LALT:
	case SDLK_RALT:
		gs->codeTextBox->focused = !gs->codeTextBox->focused;
		break;

	case SDLK_F11:
		windowFlags = SDL_GetWindowFlags(gs->render->window);
		if ((windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) windowFlags = 0;
		else windowFlags = SDL_WINDOW_FULLSCREEN_DESKTOP;
		SDL_SetWindowFullscreen(gs->render->window, windowFlags);
		break;
	}
}

static void HandleKeyUp(GameState *gs, SDL_KeyCode sym)
{
	InputState *key = gs->input;

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
	Camera *cam = &gs->render->camera;
	cam->rot[0] += e.xrel * sens;
	cam->rot[1] -= e.yrel * sens;
}

static void HandleMouseDown(SDL_MouseButtonEvent e, GameState* gs)
{
	ivec3 wPos;
	Camera *cam = &gs->render->camera;

	switch (e.button)
	{
	case 1:
		GetIntCoords(cam->pos, wPos);
		wPos[1] -= (cam->width[1] / 2);
		World_SetBlock(gs->world, wPos, BLOCK_WOOD);
		Mesher_MeshWorld(gs->world);
		break;
	}
}

void Input_Poll(GameState* gs)
{
	SDL_Event ev;

	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		case SDL_QUIT:
			printf("EVENT: SDL_QUIT\n");
			gs->running = false;
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
					HandleKeyDown(gs, ev.key.keysym.sym);
			}
			break;
		case SDL_KEYUP:
			HandleKeyUp(gs, ev.key.keysym.sym);
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
