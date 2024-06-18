#pragma once

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
	_Out_ DrawBuffers& _rDrawBuffers);
