#version 460 core

layout(std430, binding = 3) buffer someLayoutName
{
	uvec4 blockData[64 * 64 * 64 / 16];
};

in flat uint dir;
in flat vec3 voxelOffset;
in vec3 voxelCoord;

out vec4 color;

uniform sampler2DArray ourTexture;

void main()
{
	uint x = uint(floor(voxelCoord.x + voxelOffset.x));
	uint y = uint(floor(voxelCoord.y + voxelOffset.y));
	uint z = uint(floor(voxelCoord.z + voxelOffset.z));
	uint i = (z * 4096) + (y * 64) + x;
	
	uint bytePosition = i % 4;
	uint uintPosition = (i / 4) % 4;
	uint vecIndex = i / 16;
	uint bytes = blockData[vecIndex][uintPosition];

	// (4 - bytePosition) for little-endian
	// I don't know why this is big-endian
	uint blockType = (bytes >> (bytePosition * 8)) & 0xFF;
	vec3 blockCoord = vec3(voxelCoord.x - float(x), voxelCoord.y - float(y), voxelCoord.z - float(z));
	vec2 textureCoord;

	switch (dir)
	{
	case 0:
		textureCoord = vec2(blockCoord.z, 1.0 - blockCoord.y);
		break;
	case 1:
		textureCoord = vec2(1.0 - blockCoord.z, 1.0 - blockCoord.y);
		break;
	case 2:
		textureCoord = vec2(1.0 - blockCoord.x, blockCoord.z);
		break;
	case 3:
		textureCoord = vec2(1.0 - blockCoord.x, 1.0 - blockCoord.z);
		break;
	case 4:
		textureCoord = vec2(1.0 - blockCoord.x, 1.0 - blockCoord.y);
		break;
	case 5:
		textureCoord = vec2(blockCoord.x, 1.0 - blockCoord.y);
		break;
	default:
		textureCoord = vec2(0.0, 0.0);
		return;
	}
	
	color = texture(ourTexture, vec3(textureCoord, blockType + 576));
}
