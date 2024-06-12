#include "core/device.h"
#include "core/buffer.h"

#include "shader_interop.h"
#include "geometry.h"
#include "serializer.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
/*
const u32 kArchiveVersion = 0;

static void ConvertToWindowsPath(std::string& outPath)
{
	//std::for_each(outPath.begin(), outPath.end(), [](char& character)
	//	{
	//		if (character == '/')
	//		{
	//			character = '\\';
	//		}
	//	});
}

static std::string ReadFile(std::string inPath)
{
	ConvertToWindowsPath(inPath);

	FILE* file = fopen(inPath.c_str(), "r");

	if (file == nullptr)
	{
		file = fopen(inPath.c_str(), "w");
	}

	//assert(file != nullptr, "Failed opening/creating a file %s.", inPath.c_str());

	fseek(file, 0L, SEEK_END);
	const size_t fileByteCount = static_cast<size_t>(ftell(file));
	fseek(file, 0L, SEEK_SET);

	char* buffer = static_cast<char*>(alloca(fileByteCount + 1));
	const size_t bytesToRead = fread(buffer, sizeof(char), fileByteCount, file);
	fclose(file);

	buffer[bytesToRead] = '\0';

	return std::string(buffer);
}

static void WriteFile(std::string inPath, const std::string& inBuffer)
{
	ConvertToWindowsPath(inPath);

	FILE* file = fopen(inPath.c_str(), "w");
	//assert(file != nullptr, "Failed opening a file %s.", inPath.c_str());

	const size_t fileByteCount = inBuffer.size();
	const size_t bytesToWrite = fwrite(inBuffer.c_str(), sizeof(char), fileByteCount, file);
	//assert(bytesToWrite == fileByteCount, "Failed writing to a file $s.", inPath.c_str());

	fclose(file);
}

static const char* getVersionedJsonPath(
	const char* _pPath,
	u32 _version)
{
	std::string jsonPath(_pPath);
	{
		jsonPath.append(".v");
		jsonPath.append(std::to_string(_version));
		jsonPath.append(".json");
	}

	return jsonPath.c_str();
}

rapidjson::Value serializeVertex(
	Vertex& _rVertex,
	rapidjson::Document::AllocatorType& _rAllocator)
{
	rapidjson::Value json(rapidjson::kObjectType);

	json.AddMember("position0", _rVertex.position[0], _rAllocator);
	json.AddMember("position1", _rVertex.position[1], _rAllocator);
	json.AddMember("position2", _rVertex.position[2], _rAllocator);

	json.AddMember("normal0", _rVertex.normal[0], _rAllocator);
	json.AddMember("normal0", _rVertex.normal[1], _rAllocator);
	json.AddMember("normal0", _rVertex.normal[2], _rAllocator);
	json.AddMember("normal0", _rVertex.normal[3], _rAllocator);

	json.AddMember("texCoord0", _rVertex.position[0], _rAllocator);
	json.AddMember("texCoord1", _rVertex.position[1], _rAllocator);

#ifdef VERTEX_COLOR
	json.AddMember("color0", _rVertex.color[0], _rAllocator);
	json.AddMember("color1", _rVertex.color[1], _rAllocator);
	json.AddMember("color2", _rVertex.color[2], _rAllocator);
#else
	json.AddMember("color0", u16(0), _rAllocator);
	json.AddMember("color1", u16(0), _rAllocator);
	json.AddMember("color2", u16(0), _rAllocator);
#endif // VERTEX_COLOR

	return json;
}

struct Meshlet
{
	u32 vertexOffset = 0;
	u32 triangleOffset = 0;
	u32 vertexCount = 0;
	u32 triangleCount = 0;

	f32 center[3] = {};
	f32 radius = 0.0f;
	i8 coneAxis[3] = {};
	i8 coneCutoff = 0;
};

rapidjson::Value serializeMeshlet(
	Meshlet& _rMeshlet,
	rapidjson::Document::AllocatorType& _rAllocator)
{
	rapidjson::Value json(rapidjson::kObjectType);


	// vertexOffset and other offsets needs to be updated during serialization/deserializations. Leave comment for these cases to avoid confusion

	json.AddMember("position0", _vertex.position[0], _rAllocator);
	json.AddMember("position1", _vertex.position[1], _rAllocator);
	json.AddMember("position2", _vertex.position[2], _rAllocator);

	return json;
}

void serializeMesh(
	const char* _pPath,
	Mesh _mesh,
	Geometry& _rGeometry)
{
	const char* jsonPath = getVersionedJsonPath(_pPath, kArchiveVersion);

	rapidjson::Document documentJson(rapidjson::kObjectType);

	documentJson.AddMember("LodCount", _mesh.lodCount, documentJson.GetAllocator());
	documentJson.AddMember("Radius", _mesh.radius, documentJson.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	documentJson.Accept(writer);

	std::string configJson = buffer.GetString();

	WriteFile(jsonPath, configJson);
}

bool deserializeMesh(
	const char* _pPath,
	_Out_ Geometry& _rGeometry,
	_Out_ Mesh& _rMesh)
{
	std::string configJson = ReadFile(_pPath);

	if (configJson.length() == 0)
	{
		//FT_LOG("Config json file doesn't exist, new one will be created %s.\n", ConfigFilePath.c_str());
		return false;
	}

	rapidjson::Document documentJson;
	documentJson.Parse(configJson.c_str());

	_rMesh.lodCount = documentJson["LodCount"].GetUint();
	_rMesh.radius = documentJson["Radius"].GetFloat();

	return true;
}
*/
