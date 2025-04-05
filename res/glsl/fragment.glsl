#version 450 core

in vec3 textureCoord;

out vec4 color;

uniform sampler2DArray textureSampler;

void main()
{
	color = texture(textureSampler, textureCoord);

	// TODO: transparency sorting
	if (color.w < 0.1) discard;
}
