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
	device.ticks = 0;
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
	// Calibrate ticks on the first frame. It will begin executing next frame.
	if (device->ticks == 0) device->ticks = ticks;

	while (device->ticks < ticks)
	{
		device->ticks += 10;

		// software will put data in these locations when it wants the device to do something
		uint16_t timerRequest = device->memory.data[256];
		uint16_t printStrRequest = device->memory.data[257];
		uint16_t printIntRequest = device->memory.data[258];
		uint16_t breakBlockRequest = device->memory.data[259];
		uint16_t moveRequest = device->memory.data[260];

		if (device->timerTicks > 0)
		{
			if (device->ticks > device->timerTicks)
			{
				// clear the timer
				device->timerTicks = -1;
				device->memory.data[256] = 0;
				// trigger an interrupt on line 7 (arbitrarily chosen)
				*device->irq |= 1 << 7;
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
			uint16_t *str = device->memory.data + printStrRequest;
			int len = UnpackString(str, buffer);
			printf("%s\n", buffer);
			device->memory.data[257] = 0;
		}

		if (printIntRequest != 0)
		{
			printf("%d\n", printIntRequest);
			device->memory.data[258] = 0;
		}

		if (breakBlockRequest != 0)
		{
			BreakBlock(device);
			device->memory.data[259] = 0;
		}

		if (moveRequest != 0)
		{
			// scale the numbers down because they are given as integers
			device->model->vel[0] = 0.01f * (int16_t)(device->memory.data[261]);
			device->model->vel[1] = 0.01f * (int16_t)(device->memory.data[262]);
			device->model->vel[2] = 0.01f * (int16_t)(device->memory.data[263]);
			device->memory.data[260] = 0;
			device->memory.data[261] = 0;
			device->memory.data[262] = 0;
			device->memory.data[263] = 0;
		}
	}
}

// interrupts the CPU and writes the input at a fixed location
void Device_GiveInput(Device *device, char input)
{
	device->memory.data[128] = input;
	*device->irq |= 1 << 3;
}
