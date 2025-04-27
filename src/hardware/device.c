#include <stdbool.h>
#include <stdio.h>

#include "cglm/cglm.h"

#include "device.h"
#include "cpu.h"
#include "../engine/shape.h"
#include "../engine/world.h"

Device Device_New(World *world, Model *model)
{
	Device device;
	device.world = world;
	device.model = model;
	device.timerTicks = -1;
	return device;
}

static void BreakBlock(Device *device)
{
	ivec3 wPos, cPos;
	GetIntCoords(device->model->pos, wPos);
	wPos[1] -= 1; // go one block down
	Chunk *chunk = World_GetChunkAndCoords(device->world, wPos, cPos);
	uint8_t blockType = World_GetBlock(chunk, cPos);

	if (blockType != BLOCK_AIR)
	{
		printf("Breaking block type %d\n", blockType);
		World_SetBlock(device->world, wPos, BLOCK_AIR);
	}
}

int UnpackString(char *buffer, uint16_t *str)
{
	uint16_t pair;
	int i = 0;
	int n = 0;

	do
	{
		pair = str[i++];
		buffer[n++] = pair & 0x00ff;
		buffer[n++] = pair >> 8;

	} while ((pair & 0xff00) != 0);

	n--;
	if (buffer[n - 1] == '\0') n--;

	return n;
}

// This performs several services for the CPU.
// Later on they can be split into multiple IO devices.
bool Device_Update(Device *device, int ticks)
{
	uint16_t *mem = device->memory.data;

	// write the current time where the software can find it
	mem[IO_TIME_HIGH] = ticks >> 16;
	mem[IO_TIME_LOW] = ticks & 0x0000ffff;

	if (device->timerTicks > 0)
	{
		if (ticks > device->timerTicks)
		{
			// clear the timer and trigger an interrupt
			device->timerTicks = -1;
			mem[IO_TIMER_SET] = 0;
			*device->irq |= IRQ_TASK_TIMER;
		}
	}
	else if (mem[IO_TIMER_SET] != 0)
	{
		// set a timer
		device->timerTicks = ticks + mem[IO_TIMER_SET];
	}

	if (mem[IO_PRINT_STR_ADDR] != 0)
	{
		char buffer[100];
		uint16_t *str = mem + mem[IO_PRINT_STR_ADDR];
		int len = UnpackString(buffer, str);
		printf("String: %s\n", buffer);
		mem[IO_PRINT_STR_ADDR] = 0;
	}

	if (mem[IO_PRINT_INT_CMD] != 0)
	{
		printf("Integer: %d\n", (mem[IO_PRINT_INT_HIGH] << 16) | mem[IO_PRINT_INT_LOW]);
		mem[IO_PRINT_INT_CMD] = 0;
		mem[IO_PRINT_INT_HIGH] = 0;
		mem[IO_PRINT_INT_LOW] = 0;
	}

	if (mem[IO_BREAK_CMD] != 0)
	{
		BreakBlock(device);
		mem[IO_BREAK_CMD] = 0;
	}

	if (mem[IO_MOVE_CMD] != 0)
	{
		// scale the numbers down because they are given as integers
		device->model->vel[0] = 0.01f * (int16_t)(mem[IO_MOVE_X]);
		device->model->vel[1] = 0.01f * (int16_t)(mem[IO_MOVE_Y]);
		device->model->vel[2] = 0.01f * (int16_t)(mem[IO_MOVE_Z]);
		mem[IO_MOVE_CMD] = 0;
		mem[IO_MOVE_X] = 0;
		mem[IO_MOVE_Y] = 0;
		mem[IO_MOVE_Z] = 0;
	}
}

// interrupts the CPU and writes the input at a fixed location
void Device_GiveInput(Device *device, char input)
{
	device->memory.data[IO_INPUT_CHAR] = input;
	*device->irq |= IRQ_CHAR_INPUT;
}
