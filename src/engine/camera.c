#include <math.h>
#include "camera.h"

void Camera_Init(Camera* c)
{
	c->pos[0] = -50.0f;
	c->pos[1] = 130.0f;
	c->pos[2] = 0.0f;
	glm_vec3_zero(c->vel);
	glm_vec3_zero(c->look);
	glm_vec3_zero(c->up);
	glm_vec3_zero(c->front);
	glm_vec3_zero(c->right);
	glm_vec3_zero(c->rot);
}

void Camera_GetViewMatrix(Camera* c, mat4 m)
{
	glm_lookat(c->pos, c->look, c->up, m);
}

inline void Camera_Move(Camera* c, vec3 move)
{
	glm_vec3_add(c->pos, move, c->pos);
}

void Camera_UpdateVectors(Camera* c)
{
	if (c->rot[0] > 180.0f) c->rot[0] -= 360.0f;
	else if (c->rot[0] <= -180.0f) c->rot[0] += 360.0f;

	c->rot[1] = glm_clamp(c->rot[1], -89.0f, 89.0f);
	c->rot[2] = glm_clamp(c->rot[2], -30.0f, 30.0f);

	// calculate the front facing unit vector by yaw and pitch
	c->front[0] = cos(glm_rad(c->rot[0])) * cos(glm_rad(c->rot[1]));
	c->front[1] = sin(glm_rad(c->rot[1]));
	c->front[2] = sin(glm_rad(c->rot[0])) * cos(glm_rad(c->rot[1]));
	
	// the look vector is the position + front
	glm_vec3_add(c->pos, c->front, c->look);
	
	// reset the up vector
	c->up[0] = 0.0f;
	c->up[1] = 1.0f;
	c->up[2] = 0.0f;
	// cross product points to the right, perpendicular to both front and up
	glm_vec3_cross(c->front, c->up, c->right);
	// pitch up or down, around the axis of the cross product
	glm_vec3_rotate(c->up, glm_rad(c->rot[1]), c->right);
	// roll around the axis of the front vector
	glm_vec3_rotate(c->up, glm_rad(c->rot[2]), c->front);
	// recalculate right vector after rolling
	glm_vec3_cross(c->front, c->up, c->right);
}
