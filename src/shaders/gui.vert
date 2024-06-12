#version 450

#include "common.h"

struct VertexGUI
{
	f32 position[2];
	f32 uv[2];
	u8 color[4];
};

layout(binding = 0) readonly buffer Vertices { VertexGUI vertices[]; };

layout(push_constant) uniform block
{
	v2 scale;
	v2 translate;
};

layout(location = 0) out v2 outUV;
layout(location = 1) out v4 outColor;

void main() 
{
	outUV = v2(
		vertices[gl_VertexIndex].uv[0],
		vertices[gl_VertexIndex].uv[1]);

	outColor = v4(
		u32(vertices[gl_VertexIndex].color[0]),
		u32(vertices[gl_VertexIndex].color[1]),
		u32(vertices[gl_VertexIndex].color[2]),
		u32(vertices[gl_VertexIndex].color[3])) / 256.0;

	gl_Position = v4(v2(
			vertices[gl_VertexIndex].position[0],
			vertices[gl_VertexIndex].position[1]) * scale + translate, 0.0, 1.0);
}
