#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

// Used mostly for debugging
#define VERTEX_COLOR

// TODO-MILKRU: Thread Group Size can be reflected
const int kShaderGroupSize = 32;
const int kMaxVerticesPerMeshlet = 64;
const int kMaxTrianglesPerMeshlet = 124;
const int kMaxMeshLods = 16;
const int kFrustumPlaneCount = 5;

#endif // SHADER_INTEROP_H
