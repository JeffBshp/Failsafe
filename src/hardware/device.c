#include <stdbool.h>
#include <stdio.h>

#include "cglm/cglm.h"

#include "device.h"
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

static int UnpackString(uint16_t *str, char *buffer)
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

bool Device_Update(Device *device, int ticks)
{
	uint16_t *mem = device->memory.data;

	// write the current time where the software can find it
	mem[320] = ticks >> 16;
	mem[321] = ticks & 0x0000ffff;

	// software will put data in these locations when it wants the device to do something
	uint16_t timerRequest = mem[322];
	uint16_t printStrRequest = mem[328];
	uint16_t printIntRequest = mem[329];
	uint16_t breakBlockRequest = mem[336];
	uint16_t moveRequest = mem[344];

	if (device->timerTicks > 0)
	{
		if (ticks > device->timerTicks)
		{
			// clear the timer
			device->timerTicks = -1;
			mem[322] = 0;
			// trigger an interrupt on irq line 0
			*device->irq |= 1 << 0;
		}
	}
	else if (timerRequest != 0)
	{
		// set a timer
		device->timerTicks = ticks + timerRequest;
	}

	if (printStrRequest != 0)
	{
		char buffer[100];
		uint16_t *str = mem + printStrRequest;
		int len = UnpackString(str, buffer);
		printf("String: %s\n", buffer);
		mem[328] = 0;
	}

	if (printIntRequest != 0)
	{
		printf("Integer: %d\n", (mem[330] << 16) | mem[331]);
		mem[329] = 0;
		mem[330] = 0;
		mem[331] = 0;
	}

	if (breakBlockRequest != 0)
	{
		BreakBlock(device);
		mem[336] = 0;
	}

	if (moveRequest != 0)
	{
		// scale the numbers down because they are given as integers
		device->model->vel[0] = 0.01f * (int16_t)(mem[345]);
		device->model->vel[1] = 0.01f * (int16_t)(mem[346]);
		device->model->vel[2] = 0.01f * (int16_t)(mem[347]);
		mem[344] = 0;
		mem[345] = 0;
		mem[346] = 0;
		mem[347] = 0;
	}
}

// interrupts the CPU and writes the input at a fixed location
void Device_GiveInput(Device *device, char input)
{
	device->memory.data[360] = input;
	*device->irq |= 1 << 1;
}
