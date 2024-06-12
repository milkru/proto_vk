#version 450

#include "common.h"

layout(location = 0) in v3 inColor;

layout(location = 0) out v4 outColor;

void main()
{
    outColor = v4(inColor, 1.0);
}
