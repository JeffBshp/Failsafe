#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include "SDL2/SDL.h"
#include "cglm/cglm.h"
#include "shape.h"
#include "physics.h"
#include "utility.h"
#include "world.h"

#define GAP 0.001f
#define COLLISION_VARS ivec3 coords;\
	int x1 = (int)floorf(pos[0]);\
	int y1 = (int)floorf(pos[1]);\
	int z1 = (int)floorf(pos[2]);\
	int x2 = (int)floorf(pos[0] + width[0]);\
	int y2 = (int)floorf(pos[1] + width[1]);\
	int z2 = (int)floorf(pos[2] + width[2]);\

static int CollideVoxelAabbX(World* world, vec3 pos, vec3 width, bool positive)
{
	COLLISION_VARS
	coords[0] = positive ? x2 + 1 : x1 - 1;

	for (coords[2] = z1; coords[2] <= z2; coords[2]++)
		for (coords[1] = y1; coords[1] <= y2; coords[1]++)
			if (World_IsSolidBlock(world, coords))
				return 0;

	return -1;
}

static int CollideVoxelAabbY(World* world, vec3 pos, vec3 width, bool positive)
{
	COLLISION_VARS
	coords[1] = positive ? y2 + 1 : y1 - 1;

	for (coords[2] = z1; coords[2] <= z2; coords[2]++)
		for (coords[0] = x1; coords[0] <= x2; coords[0]++)
			if (World_IsSolidBlock(world, coords))
				return 1;

	return -1;
}

static int CollideVoxelAabbZ(World* world, vec3 pos, vec3 width, bool positive)
{
	COLLISION_VARS
	coords[2] = positive ? z2 + 1 : z1 - 1;

	for (coords[1] = y1; coords[1] <= y2; coords[1]++)
		for (coords[0] = x1; coords[0] <= x2; coords[0]++)
			if (World_IsSolidBlock(world, coords))
				return 2;

	return -1;
}

static float DdaSetup(vec3 pos, vec3 width, vec3 vel, vec3 direction, vec3 tMax, vec3 tDelta)
{
	glm_vec3_normalize_to(vel, direction);
	float rayLength = glm_vec3_norm(vel);

	vec3 lead;
	lead[0] = vel[0] < 0 ? pos[0] : pos[0] + width[0];
	lead[1] = vel[1] < 0 ? pos[1] : pos[1] + width[1];
	lead[2] = vel[2] < 0 ? pos[2] : pos[2] + width[2];

	// A small GAP prevents actually crossing the integer boundary when advancing, which would break collisions.
	tMax[0] = (floorf(lead[0]) + (vel[0] < 0 ? GAP : 1.0f - GAP) - lead[0]) / direction[0];
	tMax[1] = (floorf(lead[1]) + (vel[1] < 0 ? GAP : 1.0f - GAP) - lead[1]) / direction[1];
	tMax[2] = (floorf(lead[2]) + (vel[2] < 0 ? GAP : 1.0f - GAP) - lead[2]) / direction[2];

	tDelta[0] = (vel[0] < 0 ? -1.0f : 1.0f) / direction[0];
	tDelta[1] = (vel[1] < 0 ? -1.0f : 1.0f) / direction[1];
	tDelta[2] = (vel[2] < 0 ? -1.0f : 1.0f) / direction[2];

	if (direction[0] == 0.0f) tMax[0] = tDelta[0] = INFINITY;
	if (direction[1] == 0.0f) tMax[1] = tDelta[1] = INFINITY;
	if (direction[2] == 0.0f) tMax[2] = tDelta[2] = INFINITY;

	return rayLength;
}

// This uses a DDA algorithm to check collisions between an AABB and a voxel grid.
static void Physics_CollideVoxelAabb(World* world, vec3 pos, vec3 width, vec3 vel, int maxIterations)
{
	// tMax is the cumulative distance along the ray before crossing the next voxel boundary per each dimension.
	// Initial value of tMax is based on the nearest voxel to the start. Then it gets incremented by tDelta.
	// tDelta is the delta along the ray corresponding to a unit increment in each dimension.
	vec3 direction, tMax, tDelta;
	vec3 rayToNextPoint, nextPoint, tempVel;
	glm_vec3_copy(vel, tempVel);

	int iterations = 0;
	int collision = -1; // 0, 1, or 2 indicates a collision on the x, y, or z axis
	float rayLength = DdaSetup(pos, width, tempVel, direction, tMax, tDelta);

	while (iterations < maxIterations && rayLength > 0.001f)
	{
		iterations++;

		// Find which of the three faces is closest and step in that direction.
		// If that doesn't bring us to the end of the ray, check for a collision on the integer boundary.
		// Stepping increases tMax in that dimension, possibly causing the next iteration to deal with
		// one of the other two dimensions.
		if (tMax[0] < tMax[1] && tMax[0] < tMax[2])
		{
			if (tMax[0] >= rayLength) break;
			glm_vec3_scale(direction, tMax[0], rayToNextPoint);
			glm_vec3_add(pos, rayToNextPoint, nextPoint);
			tMax[0] += tDelta[0];
			collision = CollideVoxelAabbX(world, nextPoint, width, vel[0] >= 0);
		}
		else if (tMax[1] < tMax[2])
		{
			if (tMax[1] >= rayLength) break;
			glm_vec3_scale(direction, tMax[1], rayToNextPoint);
			glm_vec3_add(pos, rayToNextPoint, nextPoint);
			tMax[1] += tDelta[1];
			collision = CollideVoxelAabbY(world, nextPoint, width, vel[1] >= 0);
		}
		else if (tMax[2] < INFINITY)
		{
			if (tMax[2] >= rayLength) break;
			glm_vec3_scale(direction, tMax[2], rayToNextPoint);
			glm_vec3_add(pos, rayToNextPoint, nextPoint);
			tMax[2] += tDelta[2];
			collision = CollideVoxelAabbZ(world, nextPoint, width, vel[2] >= 0);
		}
		else return; // tMax is inf in all 3 directions, probably because vel = (0, 0, 0)

		// When there is a collision: zero out the velocity in that direction,
		// reset the DDA algorithm, and continue in the other non-collided dimensions.
		if (collision >= 0)
		{
			// zero out vel and get the remainder of tempVel
			if (collision == 0) tempVel[0] = vel[0] = 0.0f; else tempVel[0] -= rayToNextPoint[0];
			if (collision == 1) tempVel[1] = vel[1] = 0.0f; else tempVel[1] -= rayToNextPoint[1];
			if (collision == 2) tempVel[2] = vel[2] = 0.0f; else tempVel[2] -= rayToNextPoint[2];

			glm_vec3_copy(nextPoint, pos);
			rayLength = DdaSetup(pos, width, tempVel, direction, tMax, tDelta);
			collision = -1;
		}
	}

	// By this point tempVel is confirmed to not have any more solid voxels in its path.
	glm_vec3_add(pos, tempVel, pos);
}

// Moves a 3D axis-aligned bounding box (AABB) through a voxel world for one frame with a given velocity.
// This modifies the pos and vel vectors depending on how it collides with any solid blocks in its path.
void Physics_MoveAabbThroughVoxels(World* world, vec3 pos, vec3 width, vec3 vel)
{
	// offset position to the lower corner, assuming it initially lies in the center of the aabb
	vec3 ofsPos;
	ofsPos[0] = pos[0] - (width[0] / 2.0f);
	ofsPos[1] = pos[1] - (width[1] / 2.0f);
	ofsPos[2] = pos[2] - (width[2] / 2.0f);

	Physics_CollideVoxelAabb(world, ofsPos, width, vel, 20);

	// undo the offset and set the final position
	pos[0] = ofsPos[0] + (width[0] / 2.0f);
	pos[1] = ofsPos[1] + (width[1] / 2.0f);
	pos[2] = ofsPos[2] + (width[2] / 2.0f);
}

static void SolveElasticCollision(float ma, float mb, vec3 va, vec3 vb, vec3 vaFinal)
{
	vec3 temp;
	float coA = (ma - mb) / (ma + mb);
	float coB = (2 * mb) / (ma + mb);
	glm_vec3_scale(va, coA, temp);
	glm_vec3_scale(vb, coB, vaFinal);
	glm_vec3_add(temp, vaFinal, vaFinal);
}

static void CollideWorldBounds(float origin, float* coord, float* vel, float radius)
{
	const float bound = 600.0f; // 30
	const float maxVel = 80.0f;

	if (*vel < -maxVel)
		*vel = -maxVel;
	else if (*vel > maxVel)
		*vel = maxVel;

	if (*coord - radius < origin - bound)
	{
		*coord = origin - bound + radius;

		if (*vel < 0)
			*vel *= -1.0f;
	}
	else if (*coord + radius > origin + bound)
	{
		*coord = origin + bound - radius;

		if (*vel > 0)
			*vel *= -1.0f;
	}
	else if (isnan(*coord))
	{
		*coord = 0;
		*vel = 0;
	}
}

void Physics_Collide(Shape* shapes, int shapeC)
{
	vec3 origin;
	glm_vec3_zero(origin);

	for (int i = 0; i < shapeC; i++)
	{
		Shape* shape = shapes + i;

		for (int j = 0; j < shape->numModels; j++)
		{
			Model* model = shape->models + j;

			if (model->isFixed) continue;

			for (int k = 0; k < shapeC; k++)
			{
				Shape* otherShape = shapes + k;

				for (int l = 0; l < otherShape->numModels; l++)
				{
					Model* otherModel = otherShape->models + l;

					if (otherModel->isFixed) continue;

					if (k > i || (k == i && l > j))
					{
						float minD = model->radius + otherModel->radius;
						vec3 vec;
						glm_vec3_sub(model->pos, otherModel->pos, vec);
						float d = glm_vec3_distance(origin, vec);

						if (d < minD)
						{
							vec3* vi1 = (void*)(model->vel);
							vec3* vi2 = (void*)(otherModel->vel);
							float m1 = model->mass;
							float m2 = otherModel->mass;

							vec3 temp;
							SolveElasticCollision(model->mass, otherModel->mass, model->vel, otherModel->vel, temp);
							SolveElasticCollision(otherModel->mass, model->mass, otherModel->vel, model->vel, otherModel->vel);
							glm_vec3_copy(temp, model->vel);

							glm_vec3_normalize(vec);
							glm_vec3_scale(vec, (minD - d + 0.1f) / 2.0f, vec);
							glm_vec3_add(model->pos, vec, model->pos);
							glm_vec3_sub(otherModel->pos, vec, otherModel->pos);
						}
					}
				}
			}

			CollideWorldBounds(0, model->pos + 0, model->vel + 0, model->radius);
			CollideWorldBounds(120, model->pos + 1, model->vel + 1, model->radius);
			CollideWorldBounds(0, model->pos + 2, model->vel + 2, model->radius);

			if (isinf(model->mass) || isnan(model->mass))
				model->mass = 10.0f;
		}
	}
}
