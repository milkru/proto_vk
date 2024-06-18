#pragma once

struct Geometry
{
	std::vector<Meshlet> meshlets;
	std::vector<u32> meshletVertices;
	std::vector<u8> meshletTriangles;

	std::vector<Vertex> vertices;
	std::vector<u32> indices;
	std::vector<Mesh> meshes;
};

struct GeometryBuffers
{
	Buffer meshletBuffer{};
	Buffer meshletVerticesBuffer{};
	Buffer meshletTrianglesBuffer{};

	Buffer vertexBuffer{};
	Buffer indexBuffer{};
	Buffer meshesBuffer{};
};

void loadMesh(
	const char* _pFilePath,
	_Out_ Geometry& _rGeometry);

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	Geometry& _rGeometry);

void destroyGeometryBuffers(
	Device& _rDevice,
	_Out_ GeometryBuffers& _rGeometryBuffer);
