#pragma once

#include "cglm/cglm.h"
#include "world.h"

void Physics_MoveAabbThroughVoxels(World* world, vec3 pos, vec3 width, vec3 vel);
void Physics_Collide(Shape* shapes, int shapeC);
