#version 450

#include "common.h"

layout(binding = 1) uniform sampler2D fontSampler;

layout(location = 0) in v2 inUV;
layout(location = 1) in v4 inColor;

layout(location = 0) out v4 outColor;

void main() 
{
	outColor = inColor * texture(fontSampler, inUV);
}
