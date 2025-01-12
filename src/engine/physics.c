#include "SDL2/SDL.h"
#include "cglm/cglm.h"
#include "shape.h"
#include "physics.h"

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
	const float bound = 30.0f;
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
