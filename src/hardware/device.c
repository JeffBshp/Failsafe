#include "cglm/cglm.h"

#include "device.h"
#include "../engine/shape.h"
#include "../engine/world.h"

void Device_BreakBlock(Device *device)
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
