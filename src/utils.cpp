#include "utils.h"

#include <stdio.h>

bool fileExists(
	const char* _pPath) {
	if (FILE* file = fopen(_pPath, "r"))
	{
		fclose(file);
		return true;
	}

	return false;
}

std::vector<char> readFile(
	const char* _pFilePath)
{
	FILE* file = fopen(_pFilePath, "rb");
	if (file == nullptr)
	{
		return {};
	}

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	std::vector<char> fileContents(fileSize);
	size_t bytesToRead = fread(fileContents.data(), sizeof(char), fileSize, file);
	fclose(file);

	return fileContents;
}

void writeFile(
	const char* _pPath,
	_Out_ std::string& _rContents)
{
	FILE* file = fopen(_pPath, "w");
	assert(file != nullptr);

	size_t fileByteCount = _rContents.size();
	size_t bytesToWrite = fwrite(_rContents.c_str(), sizeof(char), fileByteCount, file);
	assert(bytesToWrite == fileByteCount);

	fclose(file);
}

m4 getInfinitePerspectiveMatrix(
	f32 _fov,
	f32 _aspect,
	f32 _near)
{
	f32 f = 1.0f / tanf(_fov / 2.0f);
	return m4(
		f / _aspect, 0.0f, 0.0f, 0.0f,
		0.0f, -f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, _near, 0.0f);
}

u32 roundUpToPowerOfTwo(
	f32 _value)
{
	f32 valueBase = glm::log2(_value);

	if (glm::fract(valueBase) == 0.0f)
	{
		return _value;
	}

	return glm::pow(2.0f, glm::trunc(valueBase + 1.0f));
}
