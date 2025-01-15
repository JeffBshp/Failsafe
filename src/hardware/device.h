#pragma once

#include "cglm/cglm.h"

typedef struct
{
	void* world;
	// function pointer that breaks a block, pass in world and position
	void (*funcBreakBlock)(void*, float*);
	// references to the position and velocity of the corresponding object in the game world
	vec3* pos;
	vec3* vel;
} Device;
