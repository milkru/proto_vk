#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

#include "types.h"

// Used mostly for debugging
#define VERTEX_COLOR

const i32 kShaderGroupSize = 32;
const i32 kMaxVerticesPerMeshlet = 64;
const i32 kMaxTrianglesPerMeshlet = 124;
const i32 kMaxMeshLods = 16;
const i32 kFrustumPlaneCount = 5;

// A structure has a scalar alignment equal to the largest scalar alignment of any of its members
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout

#ifdef __cplusplus
struct Vertex
{
	u16 position[3];
	u8 normal[4];
	u16 texCoord[2];

#ifdef VERTEX_COLOR
	u16 color[3];
#endif // VERTEX_COLOR
};
#else
struct Vertex
{
	f16 position[3];
	u8 normal[4];
	f16 texCoord[2];

#ifdef VERTEX_COLOR
	f16 color[3];
#endif // VERTEX_COLOR
};
#endif // __cplusplus

struct Meshlet
{
	u32 vertexOffset;
	u32 triangleOffset;
	u32 vertexCount;
	u32 triangleCount;

	// TODO-MILKRU: Quantize `center` to half vector3 by using meshopt_quantizeHalf. What about `radius`?
	f32 center[3];
	f32 radius;
	i8 coneAxis[3];
	i8 coneCutoff;
};

struct MeshLod
{
	u32 firstIndex;
	u32 indexCount;
	u32 meshletOffset;
	u32 meshletCount;
};

struct Mesh
{
	u32 vertexOffset;
	u32 vertexCount;
	f32 center[3];
	f32 radius;
	u32 lodCount;
	MeshLod lods[kMaxMeshLods];
};

struct DrawCommand
{
	u32 taskX;
	u32 taskY;
	u32 taskZ;

	// TODO-MILKRU: Move this to a separate buffer instead
	u32 drawIndex;
	u32 lodIndex;
	u32 meshVisibility;
};

#ifdef __cplusplus
struct alignas(16) PerDrawData
#else
struct PerDrawData
#endif // __cplusplus
{
	m4 model;
	u32 meshIndex;
	u32 meshletVisibilityOffset;
};

#ifdef __cplusplus
struct alignas(16) PerPassData
#else
struct PerPassData
#endif // __cplusplus
{
	i32 bPrepass;
};

#ifdef __cplusplus
struct alignas(16) PerFrameData
#else
struct PerFrameData
#endif // __cplusplus
{
	m4 view;
	m4 freezeView;
	m4 projection;
	v4 freezeFrustumPlanes[kFrustumPlaneCount];
	v4 cameraPosition;
	v4 freezeCameraPosition;
	u32 screenWidth;
	u32 screenHeight;
	u32 maxDrawCount;
	f32 lodTransitionBase;
	f32 lodTransitionStep;
	i32 forcedLod;
	u32 hzbSize;
	i32 bMeshFrustumCullingEnabled;
	i32 bMeshOcclusionCullingEnabled;
	i32 bMeshletConeCullingEnabled;
	i32 bMeshletFrustumCullingEnabled;
	i32 bMeshletOcclusionCullingEnabled;
	i32 bSmallTriangleCullingEnabled;
	i32 bTriangleBackfaceCullingEnabled;
};

#endif // SHADER_INTEROP_H
