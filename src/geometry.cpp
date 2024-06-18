#include "core/device.h"
#include "core/buffer.h"

#include "shader_interop.h"
#include "geometry.h"
#include "serializer.h"

#include <fast_obj.h>
#include <meshoptimizer.h>

// TODO-MILKRU: Dynamic LOD system
// TODO-MILKRU: Task command submission
// 
// TODO-MILKRU: Emulate task shaders in a compute shader? https://themaister.net/blog/2024/01/17/modernizing-granites-mesh-rendering/
// TODO-MILKRU: Use meshopt_optimizeMeshlet once new meshoptimizer version is merged to vertex-lock branch
// 
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

#if 1
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
#else
static v4 calculateMeshBounds(
	std::vector<RawVertex>& _rVertices)
{
	v3 _min = v3(FLT_MAX);
	v3 _max = v3(FLT_MIN);

	for (RawVertex& rVertex : _rVertices)
	{
		v3 position = v3(rVertex.position[0], rVertex.position[1], rVertex.position[2]);

		_min = glm::min(_min, position);
		_max = glm::max(_max, position);
	}

	v3 center = v3((_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f, (_min.z + _max.z) * 0.5f);
	v3 halfWidth = v3(abs(_max.x - center.x), abs(_max.y - center.y), abs(_max.z - center.z));
	f32 radius = std::sqrt(std::pow(std::sqrt(std::pow(halfWidth.x, 2.0f) + std::pow(halfWidth.y, 2.0f)), 2.0f) + std::pow(halfWidth.z, 2.0f));

	return v4(center, radius);
}
#endif

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

		RawVertex vertex{};

		vertex.position[0] = objMesh->positions[3 * size_t(vertexIndex.p) + 0];
		vertex.position[1] = objMesh->positions[3 * size_t(vertexIndex.p) + 1];
		vertex.position[2] = objMesh->positions[3 * size_t(vertexIndex.p) + 2];

		vertex.normal[0] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 0];
		vertex.normal[1] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 1];
		vertex.normal[2] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 2];

		vertex.texCoord[0] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 0];
		vertex.texCoord[1] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 1];

		vertices.push_back(vertex);
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

	Mesh mesh = {};
	mesh.center[0] = meshBounds.x;
	mesh.center[1] = meshBounds.y;
	mesh.center[2] = meshBounds.z;
	mesh.radius = meshBounds.w;

	mesh.vertexOffset = u32(_rGeometry.vertices.size());
	mesh.vertexCount = u32(vertices.size());
	_rGeometry.vertices.reserve(_rGeometry.vertices.size() + vertices.size());

	for (RawVertex& rVertex : vertices)
	{
		_rGeometry.vertices.push_back(quantizeVertex(rVertex));
	}

	mesh.lodCount = 0;

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

		_rGeometry.meshletVertices.insert(_rGeometry.meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
		_rGeometry.meshletTriangles.insert(_rGeometry.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());
		_rGeometry.meshlets.reserve(_rGeometry.meshlets.size() + meshletCount);

		for (u32 meshletIndex = 0; meshletIndex < meshlets.size(); ++meshletIndex)
		{
			meshopt_Meshlet& rMeshlet = meshlets[meshletIndex];

			meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[rMeshlet.vertex_offset], &meshletTriangles[rMeshlet.triangle_offset],
				rMeshlet.triangle_count, &vertices[0].position[0], vertices.size(), sizeof(RawVertex));

			rMeshlet.vertex_offset += globalMeshletVerticesOffset;
			rMeshlet.triangle_offset += globalMeshletTrianglesOffset;

			_rGeometry.meshlets.push_back(buildMeshlet(rMeshlet, bounds));
		}

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

	// and put them as leaf nodes in the dag
	dag.add_leafs(meshlets);

	// iteratively merge-simplify-split
	while (we can group meshlets)
	{
		for (group in partition(meshlets))
		{
			// merge the meshlets in the group
			auto meshlet = merge(group);

			// track all the borders between the meshlets
			find_borders(meshlets);

			// simplify the merged meshlet
			simplify(meshlet);

			// split the simplified meshlet
			auto parts = split(meshlet);

			// write the result to the dag
			dag.add_parents(group, parts);
		}
	}
}
#endif
