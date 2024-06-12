#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

#include "../shader_interop.h"

vec3 getRandomColor(
	uint _seed)
{
	uint hash = (_seed ^ 61) ^ (_seed >> 16);
	hash = hash + (hash << 3);
	hash = hash ^ (hash >> 4);
	hash = hash * 0x27d4eb2d;
	hash = hash ^ (hash >> 15);
	return vec3(
		float(hash & 255),
		float((hash >> 8) & 255),
		float((hash >> 16) & 255)) / 255.0;
}

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere
// https://jcgt.org/published/0002/02/05/
bool tryCalculateSphereBounds(
	vec3 _center,
	float _radius,
	float _zNear,
	float _P00,
	float _P11,
	out vec4 _AABB)
{
	if (-_center.z < _radius + _zNear)
	{
		return false;
	}

	vec2 centerXZ = -_center.xz;
	vec2 vX = vec2(sqrt(dot(centerXZ, centerXZ) - _radius * _radius), _radius);
	vec2 minX = mat2(vX.x, vX.y, -vX.y, vX.x) * centerXZ;
	vec2 maxX = mat2(vX.x, -vX.y, vX.y, vX.x) * centerXZ;

	vec2 centerYZ = -_center.yz;
	vec2 vY = vec2(sqrt(dot(centerYZ, centerYZ) - _radius * _radius), _radius);
	vec2 minY = mat2(vY.x, vY.y, -vY.y, vY.x) * centerYZ;
	vec2 maxY = mat2(vY.x, -vY.y, vY.y, vY.x) * centerYZ;

	_AABB = 0.5 - 0.5 * vec4(
		minX.x / minX.y * _P00, minY.x / minY.y * _P11,
		maxX.x / maxX.y * _P00, maxY.x / maxY.y * _P11);

	return true;
}

#endif // SHADER_COMMON_H
