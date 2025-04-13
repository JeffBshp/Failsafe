#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cglm/cglm.h"

#include "memory.h"
#include "../engine/shape.h"
#include "../engine/world.h"

typedef struct
{
	Memory memory;
	World *world;
	Model *model;
	uint16_t *irq;
	int ticks;
	int timerTicks;
} Device;

Device Device_New(World *world, Model *model);
bool Device_Update(Device *device, int ticks);
void Device_GiveInput(Device *device, char input);
