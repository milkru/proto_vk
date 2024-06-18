#include "core/device.h"
#include "core/buffer.h"

#include "shader_interop.h"
#include "geometry.h"
#include "serializer.h"
#include "utils.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

const u32 kArchiveVersion = 0;

typedef rapidjson::GenericArray<false, rapidjson::Value> JsonArray;
typedef rapidjson::Document::AllocatorType JsonAllocator;

static std::string getMetaFilePath(
	const char* _pPath,
	u32 _version)
{
	std::string jsonPath(_pPath);
	{
		jsonPath.append(".v");
		jsonPath.append(std::to_string(_version));
		jsonPath.append(".meta");
	}
	return jsonPath;
}

static rapidjson::Value serializeVertex(
	Vertex& _rVertex,
	JsonAllocator& _rAllocator)
{
	rapidjson::Value json(rapidjson::kObjectType);

	json.AddMember("position_0", u32(_rVertex.position[0]), _rAllocator);
	json.AddMember("position_1", u32(_rVertex.position[1]), _rAllocator);
	json.AddMember("position_2", u32(_rVertex.position[2]), _rAllocator);

	json.AddMember("normal_0", u32(_rVertex.normal[0]), _rAllocator);
	json.AddMember("normal_1", u32(_rVertex.normal[1]), _rAllocator);
	json.AddMember("normal_2", u32(_rVertex.normal[2]), _rAllocator);
	json.AddMember("normal_3", u32(_rVertex.normal[3]), _rAllocator);

	json.AddMember("texCoord_0", u32(_rVertex.texCoord[0]), _rAllocator);
	json.AddMember("texCoord_1", u32(_rVertex.texCoord[1]), _rAllocator);

#ifdef VERTEX_COLOR
	json.AddMember("color_0", u32(_rVertex.color[0]), _rAllocator);
	json.AddMember("color_1", u32(_rVertex.color[1]), _rAllocator);
	json.AddMember("color_2", u32(_rVertex.color[2]), _rAllocator);
#else
	json.AddMember("color_0", u32(0), _rAllocator);
	json.AddMember("color_1", u32(0), _rAllocator);
	json.AddMember("color_2", u32(0), _rAllocator);
#endif // VERTEX_COLOR

	return json;
}

static Vertex deserializeVertex(
	JsonArray& _rVerticesJson,
	u32 _vertexIndex)
{
	return {
		.position = {
			u16(_rVerticesJson[_vertexIndex]["position_0"].GetUint()),
			u16(_rVerticesJson[_vertexIndex]["position_1"].GetUint()),
			u16(_rVerticesJson[_vertexIndex]["position_2"].GetUint()) },

		.normal = {
			u8(_rVerticesJson[_vertexIndex]["normal_0"].GetUint()),
			u8(_rVerticesJson[_vertexIndex]["normal_1"].GetUint()),
			u8(_rVerticesJson[_vertexIndex]["normal_2"].GetUint()),
			u8(_rVerticesJson[_vertexIndex]["normal_3"].GetUint()) },

		.texCoord = {
			u16(_rVerticesJson[_vertexIndex]["texCoord_0"].GetUint()),
			u16(_rVerticesJson[_vertexIndex]["texCoord_1"].GetUint()) },

#ifdef VERTEX_COLOR
		.color = {
			u16(_rVerticesJson[_vertexIndex]["color_0"].GetUint()),
			u16(_rVerticesJson[_vertexIndex]["color_1"].GetUint()),
			u16(_rVerticesJson[_vertexIndex]["color_2"].GetUint()) }
#endif // VERTEX_COLOR
	};
}

static rapidjson::Value serializeMeshlet(
	Meshlet& _rMeshlet,
	JsonAllocator& _rAllocator)
{
	rapidjson::Value json(rapidjson::kObjectType);

	json.AddMember("vertexOffset", _rMeshlet.vertexOffset, _rAllocator);
	json.AddMember("triangleOffset", _rMeshlet.triangleOffset, _rAllocator);
	json.AddMember("vertexCount", _rMeshlet.vertexCount, _rAllocator);
	json.AddMember("triangleCount", _rMeshlet.triangleCount, _rAllocator);

	json.AddMember("center_0", _rMeshlet.center[0], _rAllocator);
	json.AddMember("center_1", _rMeshlet.center[1], _rAllocator);
	json.AddMember("center_2", _rMeshlet.center[2], _rAllocator);

	json.AddMember("radius", _rMeshlet.radius, _rAllocator);

	json.AddMember("coneAxis_0", i32(_rMeshlet.coneAxis[0]), _rAllocator);
	json.AddMember("coneAxis_1", i32(_rMeshlet.coneAxis[1]), _rAllocator);
	json.AddMember("coneAxis_2", i32(_rMeshlet.coneAxis[2]), _rAllocator);

	json.AddMember("coneCutoff", i32(_rMeshlet.coneCutoff), _rAllocator);

	return json;
}

static Meshlet deserializeMeshlet(
	JsonArray& _rMeshletsJson,
	u32 _meshletIndex)
{
	return {
		.vertexOffset = _rMeshletsJson[_meshletIndex]["vertexOffset"].GetUint(),
		.triangleOffset = _rMeshletsJson[_meshletIndex]["triangleOffset"].GetUint(),
		.vertexCount = _rMeshletsJson[_meshletIndex]["vertexCount"].GetUint(),
		.triangleCount = _rMeshletsJson[_meshletIndex]["triangleCount"].GetUint(),

		.center = {
			_rMeshletsJson[_meshletIndex]["center_0"].GetFloat(),
			_rMeshletsJson[_meshletIndex]["center_1"].GetFloat(),
			_rMeshletsJson[_meshletIndex]["center_2"].GetFloat() },

		.radius = _rMeshletsJson[_meshletIndex]["radius"].GetFloat(),

		.coneAxis = {
			i8(_rMeshletsJson[_meshletIndex]["coneAxis_0"].GetInt()),
			i8(_rMeshletsJson[_meshletIndex]["coneAxis_1"].GetInt()),
			i8(_rMeshletsJson[_meshletIndex]["coneAxis_2"].GetInt()) },

		.coneCutoff = i8(_rMeshletsJson[_meshletIndex]["coneCutoff"].GetInt()) };
}

static rapidjson::Value serializeMeshLod(
	MeshLod& _rMeshLod,
	JsonAllocator& _rAllocator)
{
	rapidjson::Value json(rapidjson::kObjectType);

	json.AddMember("firstIndex", _rMeshLod.firstIndex, _rAllocator);
	json.AddMember("indexCount", _rMeshLod.indexCount, _rAllocator);
	json.AddMember("meshletOffset", _rMeshLod.meshletOffset, _rAllocator);
	json.AddMember("meshletCount", _rMeshLod.meshletCount, _rAllocator);

	return json;
}

static MeshLod deserializeMeshLod(
	JsonArray& _rLodsJson,
	u32 _lodIndex)
{
	return {
		.firstIndex = _rLodsJson[_lodIndex]["firstIndex"].GetUint(),
		.indexCount = _rLodsJson[_lodIndex]["indexCount"].GetUint(),
		.meshletOffset = _rLodsJson[_lodIndex]["meshletOffset"].GetUint(),
		.meshletCount = _rLodsJson[_lodIndex]["meshletCount"].GetUint() };
}

void trySerializeMesh(
	const char* _pPath,
	Mesh& _rMesh,
	Geometry& _rGeometry)
{
	std::string pJsonPath = getMetaFilePath(_pPath, kArchiveVersion);
	if (fileExists(pJsonPath.c_str()))
	{
		return;
	}

	rapidjson::Document jsonDocument(rapidjson::kObjectType);
	JsonAllocator& rAllocator = jsonDocument.GetAllocator();

	// Mesh
	jsonDocument.AddMember("vertexCount", _rMesh.vertexCount, rAllocator);

	jsonDocument.AddMember("center_0", _rMesh.center[0], rAllocator);
	jsonDocument.AddMember("center_1", _rMesh.center[1], rAllocator);
	jsonDocument.AddMember("center_2", _rMesh.center[2], rAllocator);

	jsonDocument.AddMember("radius", _rMesh.radius, rAllocator);
	jsonDocument.AddMember("lodCount", _rMesh.lodCount, rAllocator);

	// Vertices
	rapidjson::Value verticesJson(rapidjson::kArrayType);
	for (u32 vertexIndex = _rMesh.vertexOffset; vertexIndex < _rMesh.vertexOffset + _rMesh.vertexCount; ++vertexIndex)
	{
		rapidjson::Value vertexJson = serializeVertex(_rGeometry.vertices[vertexIndex], rAllocator);
		verticesJson.PushBack(vertexJson, rAllocator);
	}
	jsonDocument.AddMember("vertices", verticesJson, rAllocator);

	// Lods
	u32 baseIndexOffset = _rMesh.lods[0].firstIndex;
	u32 baseMeshletOffset = _rMesh.lods[0].meshletOffset;

	rapidjson::Value lodsJson(rapidjson::kArrayType);
	for (u32 lodIndex = 0; lodIndex < _rMesh.lodCount; ++lodIndex)
	{
		MeshLod lod = _rMesh.lods[lodIndex];
		lod.firstIndex -= baseIndexOffset;
		lod.meshletOffset -= baseMeshletOffset;

		rapidjson::Value lodJson = serializeMeshLod(lod, rAllocator);
		lodsJson.PushBack(lodJson, rAllocator);
	}
	jsonDocument.AddMember("lods", lodsJson, rAllocator);

	// Meshlets
	u32 baseVertexOffset = _rGeometry.meshlets[baseMeshletOffset].vertexOffset;
	u32 baseTriangleOffset = _rGeometry.meshlets[baseMeshletOffset].triangleOffset;

	MeshLod& rFirstLod = _rMesh.lods[0];
	MeshLod& rLastLod = _rMesh.lods[_rMesh.lodCount - 1];

	u32 firstMeshletIndex = rFirstLod.meshletOffset;
	u32 lastMeshletIndex = rLastLod.meshletOffset + rLastLod.meshletCount - 1;
	rapidjson::Value meshletsJson(rapidjson::kArrayType);
	for (u32 meshletIndex = firstMeshletIndex; meshletIndex <= lastMeshletIndex; ++meshletIndex)
	{
		Meshlet meshlet = _rGeometry.meshlets[meshletIndex];
		meshlet.vertexOffset -= baseVertexOffset;
		meshlet.triangleOffset -= baseTriangleOffset;

		rapidjson::Value meshletJson = serializeMeshlet(meshlet, rAllocator);
		meshletsJson.PushBack(meshletJson, rAllocator);
	}
	jsonDocument.AddMember("meshlets", meshletsJson, rAllocator);

	// Indices
	u32 firstIndex = rFirstLod.firstIndex;
	u32 lastIndex = rLastLod.firstIndex + rLastLod.indexCount - 1;
	rapidjson::Value indicesJson(rapidjson::kArrayType);
	for (u32 index = firstIndex; index <= lastIndex; ++index)
	{
		indicesJson.PushBack(_rGeometry.indices[index], rAllocator);
	}
	jsonDocument.AddMember("indices", indicesJson, rAllocator);

	// Meshlet vertices
	Meshlet& rFirstMeshlet = _rGeometry.meshlets[firstMeshletIndex];
	Meshlet& rLastMeshlet = _rGeometry.meshlets[lastMeshletIndex];

	u32 firstVertexIndex = rFirstMeshlet.vertexOffset;
	u32 lastVertexIndex = rLastMeshlet.vertexOffset + rLastMeshlet.vertexCount - 1;
	rapidjson::Value meshletVerticesJson(rapidjson::kArrayType);
	for (u32 vertexIndex = firstVertexIndex; vertexIndex <= lastVertexIndex; ++vertexIndex)
	{
		meshletVerticesJson.PushBack(_rGeometry.meshletVertices[vertexIndex], rAllocator);
	}
	jsonDocument.AddMember("meshletVertices", meshletVerticesJson, rAllocator);

	// Meshlet triangles
	u32 firstTriangleIndex = rFirstMeshlet.triangleOffset;
	u32 lastTriangleIndex = rLastMeshlet.triangleOffset + 3 * rLastMeshlet.triangleCount - 1;
	rapidjson::Value meshletTrianglesJson(rapidjson::kArrayType);
	for (u32 triangleIndex = firstTriangleIndex; triangleIndex <= lastTriangleIndex; ++triangleIndex)
	{
		meshletTrianglesJson.PushBack(u32(_rGeometry.meshletTriangles[triangleIndex]), rAllocator);
	}
	jsonDocument.AddMember("meshletTriangles", meshletTrianglesJson, rAllocator);

	// Write
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	jsonDocument.Accept(writer);

	std::string configJson = buffer.GetString();
	writeFile(pJsonPath.c_str(), configJson);
}

bool tryDeserializeMesh(
	const char* _pPath,
	_Out_ Geometry& _rGeometry)
{
	std::string pJsonPath = getMetaFilePath(_pPath, kArchiveVersion);
	std::vector<char> rawJson = readFile(pJsonPath.c_str());

	if (rawJson.size() == 0)
	{
		return false;
	}

	std::string metaDataJson(rawJson.data(), rawJson.size());
	rapidjson::Document documentJson;
	documentJson.Parse(metaDataJson.c_str());

	// Mesh
	Mesh mesh = {
		.vertexOffset = u32(_rGeometry.vertices.size()),
		.vertexCount = documentJson["vertexCount"].GetUint(),

		.center = {
			documentJson["center_0"].GetFloat(),
			documentJson["center_1"].GetFloat(),
			documentJson["center_2"].GetFloat() },

		.radius = documentJson["radius"].GetFloat(),
		.lodCount = documentJson["lodCount"].GetUint() };

	// Vertices
	JsonArray verticesJson = documentJson["vertices"].GetArray();
	assert(mesh.vertexCount == verticesJson.Size());
	_rGeometry.vertices.reserve(_rGeometry.vertices.size() + verticesJson.Size());
	for (u32 vertexIndex = 0; vertexIndex < verticesJson.Size(); ++vertexIndex)
	{
		_rGeometry.vertices.push_back(deserializeVertex(verticesJson, vertexIndex));
	}

	// Lods
	u32 baseIndexOffset = _rGeometry.indices.size();
	u32 baseMeshletOffset = _rGeometry.meshlets.size();

	JsonArray lodsJson = documentJson["lods"].GetArray();
	for (u32 lodIndex = 0; lodIndex < mesh.lodCount; ++lodIndex)
	{
		MeshLod lod = deserializeMeshLod(lodsJson, lodIndex);

		lod.firstIndex += baseIndexOffset;
		lod.meshletOffset += baseMeshletOffset;
		mesh.lods[lodIndex] = lod;
	}

	// Meshlets
	u32 baseVertexOffset = _rGeometry.meshletVertices.size();
	u32 baseTriangleOffset = _rGeometry.meshletTriangles.size();

	JsonArray meshletsJson = documentJson["meshlets"].GetArray();
	_rGeometry.meshlets.reserve(baseMeshletOffset + meshletsJson.Size());
	for (u32 meshletIndex = 0; meshletIndex < meshletsJson.Size(); ++meshletIndex)
	{
		Meshlet meshlet = deserializeMeshlet(meshletsJson, meshletIndex);
		meshlet.vertexOffset += baseVertexOffset;
		meshlet.triangleOffset += baseTriangleOffset;

		_rGeometry.meshlets.push_back(meshlet);
	}

	// Indices
	JsonArray indicesJson = documentJson["indices"].GetArray();
	_rGeometry.indices.reserve(baseIndexOffset + indicesJson.Size());
	for (u32 index = 0; index < indicesJson.Size(); ++index)
	{
		_rGeometry.indices.push_back(indicesJson[index].GetUint());
	}

	// Meshlet vertices
	JsonArray meshletVerticesJson = documentJson["meshletVertices"].GetArray();
	_rGeometry.meshletVertices.reserve(baseVertexOffset + meshletVerticesJson.Size());
	for (u32 vertexIndex = 0; vertexIndex < meshletVerticesJson.Size(); ++vertexIndex)
	{
		_rGeometry.meshletVertices.push_back(meshletVerticesJson[vertexIndex].GetUint());
	}

	// Meshlet triangles
	JsonArray meshletTrianglesJson = documentJson["meshletTriangles"].GetArray();
	_rGeometry.meshletTriangles.reserve(baseTriangleOffset + meshletTrianglesJson.Size());
	for (u32 triangleIndex = 0; triangleIndex < meshletTrianglesJson.Size(); ++triangleIndex)
	{
		_rGeometry.meshletTriangles.push_back(u8(meshletTrianglesJson[triangleIndex].GetUint()));
	}

	_rGeometry.meshes.push_back(mesh);
	return true;
}
