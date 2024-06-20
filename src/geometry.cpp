#include "core/device.h"
#include "core/buffer.h"

#include "shader_interop.h"
#include "geometry.h"
#include "serializer.h"

#include <fast_obj.h>
#include <meshoptimizer.h>

// TODO-MILKRU: Dynamic LOD system
// TODO-MILKRU: Task command submission?
// TODO-MILKRU: Emulate task shaders in a compute shader? https://themaister.net/blog/2024/01/17/modernizing-granites-mesh-rendering/
// TODO-MILKRU: Implement Forward rendering with bindless, since overdraw with all of these geometry optimizations should be minimal. Check NSight overdraw view
// TODO-MILKRU: Clustered light culling and binning. Check ACU, Doom and Doom Eternal implementations

struct RawVertex
{
	f32 position[3] = {};
	f32 normal[3] = {};
	f32 texCoord[2] = {};

#ifdef VERTEX_COLOR
	f32 color[3] = {};
#endif // VERTEX_COLOR
};

// TODO-MILKRU: Implement `Welzl Algorithm` for more conservative bounding sphere?
static v4 calculateMeshBounds(
	std::vector<RawVertex>& _rVertices)
{
	v4 meshBounds(0.0f);
	for (RawVertex& rVertex : _rVertices)
	{
		meshBounds += v4(rVertex.position[0], rVertex.position[1], rVertex.position[2], 0.0f);
	}
	meshBounds /= f32(_rVertices.size());

	for (RawVertex& rVertex : _rVertices)
	{
		meshBounds.w = glm::max(meshBounds.w, glm::distance(v3(meshBounds),
			v3(rVertex.position[0], rVertex.position[1], rVertex.position[2])));
	}

	return meshBounds;
}

static Vertex quantizeVertex(
	RawVertex& _rRawVertex)
{
	return {
		.position = {
			meshopt_quantizeHalf(_rRawVertex.position[0]),
			meshopt_quantizeHalf(_rRawVertex.position[1]),
			meshopt_quantizeHalf(_rRawVertex.position[2]) },

		// TODO-MILKRU: To snorm
		.normal = {
			u8(meshopt_quantizeUnorm(_rRawVertex.normal[0], 8)),
			u8(meshopt_quantizeUnorm(_rRawVertex.normal[1], 8)),
			u8(meshopt_quantizeUnorm(_rRawVertex.normal[2], 8)),
			u8(0) }, // Free 8 bits for custom use

		// TODO-MILKRU: To unorm
		.texCoord = {
			meshopt_quantizeHalf(_rRawVertex.texCoord[0]),
			meshopt_quantizeHalf(_rRawVertex.texCoord[1]) },

#ifdef VERTEX_COLOR
		.color = {
			meshopt_quantizeHalf(1.0f),
			meshopt_quantizeHalf(0.0f),
			meshopt_quantizeHalf(1.0f) }
#endif // VERTEX_COLOR
	};
}

static Meshlet buildMeshlet(
	meshopt_Meshlet _meshlet,
	meshopt_Bounds _bounds)
{
	return {
		.vertexOffset = _meshlet.vertex_offset,
		.triangleOffset = _meshlet.triangle_offset,
		.vertexCount = _meshlet.vertex_count,
		.triangleCount = _meshlet.triangle_count,

		.center = {
			_bounds.center[0],
			_bounds.center[1],
			_bounds.center[2] },

		.radius = _bounds.radius,

		.coneAxis = {
			_bounds.cone_axis_s8[0],
			_bounds.cone_axis_s8[1],
			_bounds.cone_axis_s8[2] },

		.coneCutoff = _bounds.cone_cutoff_s8 };
}

void loadMesh(
	const char* _pFilePath,
	_Out_ Geometry& _rGeometry)
{
	if (tryDeserializeMesh(_pFilePath, _rGeometry))
	{
		return;
	}

	fastObjMesh* objMesh = fast_obj_read(_pFilePath);
	assert(objMesh);

	std::vector<RawVertex> vertices;
	vertices.reserve(objMesh->index_count);

	for (u32 i = 0; i < objMesh->index_count; ++i)
	{
		fastObjIndex vertexIndex = objMesh->indices[i];
		vertices.push_back({
			.position = {
				objMesh->positions[3 * size_t(vertexIndex.p) + 0],
				objMesh->positions[3 * size_t(vertexIndex.p) + 1],
				objMesh->positions[3 * size_t(vertexIndex.p) + 2] },

			.normal = {
				0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 0],
				0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 1],
				0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 2] },

			.texCoord = {
				objMesh->texcoords[2 * size_t(vertexIndex.t) + 0],
				objMesh->texcoords[2 * size_t(vertexIndex.t) + 1] } });
	}
	
	std::vector<u32> remapTable(objMesh->index_count);
	size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, objMesh->index_count,
		vertices.data(), vertices.size(), sizeof(RawVertex));

	vertices.resize(vertexCount);
	std::vector<u32> indices(objMesh->index_count);

	meshopt_remapVertexBuffer(vertices.data(), vertices.data(), indices.size(), sizeof(RawVertex), remapTable.data());
	meshopt_remapIndexBuffer(indices.data(), nullptr, indices.size(), remapTable.data());

	meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
	meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &vertices[0].position[0], vertices.size(), sizeof(RawVertex), /*threshold*/ 1.01f);
	meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(RawVertex));

	v4 meshBounds = calculateMeshBounds(vertices);
	Mesh mesh = {
		.vertexOffset = u32(_rGeometry.vertices.size()),
		.vertexCount = u32(vertices.size()),
		.center = { meshBounds.x, meshBounds.y, meshBounds.z },
		.radius = meshBounds.w,
		.lodCount = 0 };

	_rGeometry.vertices.reserve(_rGeometry.vertices.size() + vertices.size());
	for (RawVertex& rVertex : vertices)
	{
		_rGeometry.vertices.push_back(quantizeVertex(rVertex));
	}

	for (u32 lodIndex = 0u; lodIndex < kMaxMeshLods; ++lodIndex)
	{
		mesh.lods[lodIndex].firstIndex = u32(_rGeometry.indices.size());
		mesh.lods[lodIndex].indexCount = u32(indices.size());
		_rGeometry.indices.insert(_rGeometry.indices.end(), indices.begin(), indices.end());

		size_t maxMeshlets = meshopt_buildMeshletsBound(indices.size(), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet);
		std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
		std::vector<u32> meshletVertices(maxMeshlets * kMaxVerticesPerMeshlet);
		std::vector<u8> meshletTriangles(maxMeshlets * kMaxTrianglesPerMeshlet * 3);

		// TODO-MILKRU: After per-meshlet frustum/occlusion culling gets implemented, try playing around with cone_weight. You might get better performance
		size_t meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(), indices.data(), indices.size(),
			&vertices[0].position[0], vertices.size(), sizeof(RawVertex), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet, /*cone_weight*/ 0.7f);

		meshopt_Meshlet& rLastMeshlet = meshlets[meshletCount - 1];

		meshletVertices.resize(rLastMeshlet.vertex_offset + size_t(rLastMeshlet.vertex_count));
		meshletTriangles.resize(rLastMeshlet.triangle_offset + ((size_t(rLastMeshlet.triangle_count) * 3 + 3) & ~3));
		meshlets.resize(meshletCount);

		mesh.lods[lodIndex].meshletOffset = _rGeometry.meshlets.size();
		mesh.lods[lodIndex].meshletCount = meshletCount;

		u32 globalMeshletVerticesOffset = _rGeometry.meshletVertices.size();
		u32 globalMeshletTrianglesOffset = _rGeometry.meshletTriangles.size();

		_rGeometry.meshlets.reserve(_rGeometry.meshlets.size() + meshletCount);
		for (u32 meshletIndex = 0; meshletIndex < meshlets.size(); ++meshletIndex)
		{
			meshopt_Meshlet meshlet = meshlets[meshletIndex];
			meshopt_optimizeMeshlet(meshletVertices.data() + meshlet.vertex_offset, meshletTriangles.data() + meshlet.triangle_offset,
				meshlet.triangle_count, meshlet.vertex_count);

			meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[meshlet.vertex_offset], &meshletTriangles[meshlet.triangle_offset],
				meshlet.triangle_count, &vertices[0].position[0], vertices.size(), sizeof(RawVertex));

			meshlet.vertex_offset += globalMeshletVerticesOffset;
			meshlet.triangle_offset += globalMeshletTrianglesOffset;

			_rGeometry.meshlets.push_back(buildMeshlet(meshlet, bounds));
		}

		_rGeometry.meshletVertices.insert(_rGeometry.meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
		_rGeometry.meshletTriangles.insert(_rGeometry.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());

		++mesh.lodCount;
		if (lodIndex >= kMaxMeshLods - 1)
		{
			break;
		}

		f32 threshold = 0.6f;
		size_t targetIndexCount = size_t(indices.size() * threshold);
		f32 targetError = 1e-2f;

		size_t newIndexCount = meshopt_simplify(indices.data(), indices.data(), indices.size(),
			&vertices[0].position[0], vertices.size(), sizeof(RawVertex), targetIndexCount, targetError);

		if (newIndexCount >= indices.size())
		{
			break;
		}

		indices.resize(newIndexCount);
		meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
	}

	_rGeometry.meshes.push_back(mesh);
	trySerializeMesh(_pFilePath, mesh, _rGeometry);
}

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	Geometry& _rGeometry)
{
	return {
		.meshletBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(Meshlet) * _rGeometry.meshlets.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = _rGeometry.meshlets.data() }),

		.meshletVerticesBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u32) * _rGeometry.meshletVertices.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = _rGeometry.meshletVertices.data() }),

		.meshletTrianglesBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u8) * _rGeometry.meshletTriangles.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = _rGeometry.meshletTriangles.data() }),

		.vertexBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(Vertex) * _rGeometry.vertices.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = _rGeometry.vertices.data() }),

		.indexBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u32) * _rGeometry.indices.size(),
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			.pContents = _rGeometry.indices.data() }),

		.meshesBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(Mesh) * _rGeometry.meshes.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = _rGeometry.meshes.data() }) };
}

void destroyGeometryBuffers(
	Device& _rDevice,
	_Out_ GeometryBuffers& _rGeometryBuffer)
{
	destroyBuffer(_rDevice, _rGeometryBuffer.meshletBuffer);
	destroyBuffer(_rDevice, _rGeometryBuffer.meshletVerticesBuffer);
	destroyBuffer(_rDevice, _rGeometryBuffer.meshletTrianglesBuffer);
	destroyBuffer(_rDevice, _rGeometryBuffer.vertexBuffer);
	destroyBuffer(_rDevice, _rGeometryBuffer.indexBuffer);
	destroyBuffer(_rDevice, _rGeometryBuffer.meshesBuffer);
}

#if 0
if (_bMeshShadingSupported)
{
	auto meshlets = generateMeshlets(mesh);

	// And put them as leaf nodes in the DAG
	dag.add_leafs(meshlets);

	// Iteratively merge-simplify-split
	while (we can group meshlets)
	{
		for (group in partition(meshlets))
		{
			// Merge the meshlets in the group
			auto meshlet = merge(group);

			// Track all the borders between the meshlets
			find_borders(meshlets);

			// Simplify the merged meshlet
			simplify(meshlet); // TODO-MILKRU: Use `meshopt_SimplifyLockBorder`

			// Split the simplified meshlet
			auto parts = split(meshlet);

			// Write the result to the DAG
			dag.add_parents(group, parts);
		}
	}
}
#endif
