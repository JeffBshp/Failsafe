#pragma once

#include "cglm/cglm.h"

#include "../engine/shape.h"
#include "../engine/world.h"

typedef struct
{
	World *world;
	Model *model;
} Device;

void Device_BreakBlock(Device *device);
