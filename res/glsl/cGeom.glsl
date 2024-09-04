#version 460 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in uint quadUpperBits[];
in mat4 matViewProj[];

out flat uint dir;
out flat vec3 voxelOffset;
out vec3 voxelCoord;

void GenVertex(float x, float y, float z)
{
	vec4 position = gl_in[0].gl_Position + vec4(x, y, z, 0.0);
	gl_Position = matViewProj[0] * position;
	voxelCoord = position.xyz;
	EmitVertex();
}

void main()
{
	float height = 1.0 + ((quadUpperBits[0] >> 6) & 0x3f);
	float width = 1.0 + (quadUpperBits[0] & 0x3f);
	dir = (quadUpperBits[0] >> 29) & 0x7;
	voxelOffset = vec3(0.0, 0.0, 0.0);

	switch (dir)
	{
	case 0:
		voxelOffset.x = 0.5;
		GenVertex(0.0,		0.0,		0.0);
		GenVertex(0.0,		0.0,		width);
		GenVertex(0.0,		height,		0.0);
		GenVertex(0.0,		height,		width);
		break;
	case 1:
		voxelOffset.x = -0.5;
		GenVertex(1.0,		0.0,		0.0);
		GenVertex(1.0,		height,		0.0);
		GenVertex(1.0,		0.0,		width);
		GenVertex(1.0,		height,		width);
		break;
	case 2:
		voxelOffset.y = 0.5;
		GenVertex(0.0,		0.0,		0.0);
		GenVertex(height,	0.0,		0.0);
		GenVertex(0.0,		0.0,		width);
		GenVertex(height,	0.0,		width);
		break;
	case 3:
		voxelOffset.y = -0.5;
		GenVertex(0.0,		1.0,		0.0);
		GenVertex(0.0,		1.0,		width);
		GenVertex(height,	1.0,		0.0);
		GenVertex(height,	1.0,		width);
		break;
	case 4:
		voxelOffset.z = 0.5;
		GenVertex(0.0,		0.0,		0.0);
		GenVertex(0.0,		width,		0.0);
		GenVertex(height,	0.0,		0.0);
		GenVertex(height,	width,		0.0);
		break;
	case 5:
		voxelOffset.z = -0.5;
		GenVertex(0.0,		0.0,		1.0);
		GenVertex(height,	0.0,		1.0);
		GenVertex(0.0,		width,		1.0);
		GenVertex(height,	width,		1.0);
		break;
	default:
		break;
	}

	EndPrimitive();
}
