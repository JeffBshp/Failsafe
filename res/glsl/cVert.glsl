#version 460 core

layout (location = 0) in uint quadL;
layout (location = 1) in uint quadU;

out uint quadUpperBits;
out mat4 matViewProj;

uniform mat4 ourModel;
uniform mat4 ourView;
uniform mat4 ourProj;

void main()
{
	uint z = (quadL >> 12) & 0x3f;
	uint y = (quadL >> 6) & 0x3f;
	uint x = quadL & 0x3f;
	gl_Position = vec4(x, y, z, 1.0);
	quadUpperBits = quadU;
	matViewProj = ourProj * ourView * ourModel;
}
