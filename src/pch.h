#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>
#include <string>
#include <functional>
#include <map>

#include "types.h"

#ifndef LAMBDA
#define LAMBDA(...) std::function<void(__VA_ARGS__)> const&
#endif // LAMBDA

#ifndef VK_CALL
#define VK_CALL(_call) \
	do { \
		VkResult result_ = _call; \
		assert(result_ == VK_SUCCESS); \
	} \
	while (0)
#endif // VK_CALL

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_arr) ((i32)(sizeof(_arr) / sizeof(_arr[0])))
#endif // ARRAY_SIZE

#ifndef TOKEN_JOIN
#define TOKEN_JOIN(_x, _y) _x ## _y
#endif // TOKEN_JOIN

#ifndef GPU_QUERY_PROFILING
#define GPU_QUERY_PROFILING 1
#endif // GPU_QUERY_PROFILING
