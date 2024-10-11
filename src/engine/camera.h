#pragma once

#include "cglm/cglm.h"

typedef struct
{
	vec3 pos;
	vec3 vel;
	vec3 look;
	vec3 up;
	vec3 front;
	vec3 right;
	vec3 rot;
} Camera;

void Camera_Init(Camera* c);

void Camera_GetViewMatrix(Camera* c, mat4 m);

void Camera_Move(Camera* c, vec3 move);

void Camera_UpdateVectors(Camera* c);
