#pragma once

void trySerializeMesh(
	const char* _pPath,
	Mesh& _rMesh,
	Geometry& _rGeometry);

bool tryDeserializeMesh(
	const char* _pPath,
	_Out_ Geometry& _rGeometry);
