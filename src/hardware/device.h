#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cglm/cglm.h"

#include "memory.h"
#include "../engine/shape.h"
#include "../engine/world.h"

typedef enum
{
	IO_TIME_HIGH = 320,
	IO_TIME_LOW = 321,
	IO_TIMER_SET = 322,

	IO_PRINT_STR_ADDR = 328,
	IO_PRINT_INT_CMD = 329,
	IO_PRINT_INT_HIGH = 330,
	IO_PRINT_INT_LOW = 331,

	IO_BREAK_CMD = 336,

	IO_MOVE_CMD = 344,
	IO_MOVE_X = 345,
	IO_MOVE_Y = 346,
	IO_MOVE_Z = 347,

	IO_INPUT_CHAR = 360,

	IO_DISK_STATUS = 384,
	IO_DISK_RESULT = 385,
	IO_DISK_CMD = 386,
	IO_DISK_PATH = 387,
	IO_DISK_ADDR = 388,
	IO_DISK_LEN = 389,
	IO_DISK_FINFO = 396,
} IoMap;

typedef struct
{
	Memory memory;
	World *world;
	Model *model;
	uint16_t *irq;
	int timerTicks;
} Device;

int UnpackString(char *buffer, uint16_t *str);
Device Device_New(World *world, Model *model);
bool Device_Update(Device *device, int ticks);
void Device_GiveInput(Device *device, char input);
