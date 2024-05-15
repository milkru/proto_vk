#pragma once

struct alignas(16) PerDrawData
{
	m4 model{};
	u32 meshIndex = 0;
	u32 meshletVisibilityOffset = 0;
};

struct DrawCommand
{
	u32 taskX = 0;
	u32 taskY = 0;
	u32 taskZ = 0;

	u32 drawIndex = 0;
	u32 lodIndex = 0;
};

struct DrawBuffers
{
	Buffer drawsBuffer{};
	Buffer drawCommandsBuffer{};
	Buffer drawCountBuffer{};
	Buffer meshVisibilityBuffer{};
	Buffer meshletVisibilityBuffer{};
};

DrawBuffers createDrawBuffers(
	Device& _rDevice,
	Geometry& _rGeometry,
	u32 _maxDrawCount,
	u32 _spawnCubeSize);

void destroyDrawBuffers(
	Device& _rDevice,
	DrawBuffers& _rDrawBuffers);
