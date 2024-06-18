#pragma once

bool fileExists(
	const char* _pPath);

std::vector<char> readFile(
	const char* _pFilePath);

void writeFile(
	const char* _pPath,
	_Out_ std::string& _rContents);

m4 getInfinitePerspectiveMatrix(
	f32 _fov,
	f32 _aspect,
	f32 _near);

u32 roundUpToPowerOfTwo(
	f32 _value);
