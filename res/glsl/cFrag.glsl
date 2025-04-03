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

uint splitBits(uint x)
{
	x = (x | (x << 8)) & 0x300F; // 0011 0000 0000 1111
	x = (x | (x << 4)) & 0x30C3; // 0011 0000 1100 0011
	x = (x | (x << 2)) & 0x9249; // 1001 0010 0100 1001

	return x;
}

uint getMortonCode(uint x, uint y, uint z)
{
	x = splitBits(x);
	y = splitBits(y) << 1;
	z = splitBits(z) << 2;

	return z | y | x;
}

void main()
{
	uint x = uint(floor(voxelCoord.x + voxelOffset.x));
	uint y = uint(floor(voxelCoord.y + voxelOffset.y));
	uint z = uint(floor(voxelCoord.z + voxelOffset.z));
	uint m = getMortonCode(x, y, z);

	uint bytePosition = m % 4;
	uint uintPosition = (m / 4) % 4;
	uint vecIndex = m / 16;
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
