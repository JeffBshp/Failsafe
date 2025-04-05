#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in float texIndex;
layout (location = 3) in mat4 ourModel;

out vec3 textureCoord;

uniform mat4 ourView;
uniform mat4 ourProj;

void main()
{
	gl_Position = ourProj * ourView * ourModel * vec4(position, 1.0);
	textureCoord = vec3(1.0f - texCoord.x, 1.0f - texCoord.y, texIndex);
}
