#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef glm::vec2 v2;
typedef glm::vec3 v3;
typedef glm::vec4 v4;

typedef glm::ivec2 iv2;
typedef glm::ivec3 iv3;
typedef glm::ivec4 iv4;

typedef glm::uvec2 uv2;
typedef glm::uvec3 uv3;
typedef glm::uvec4 uv4;

typedef glm::mat2 m2;
typedef glm::mat3 m3;
typedef glm::mat4 m4;
#else // GLSL
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_16bit_storage : require

#define i8 int8_t
#define i16 int16_t
#define i32 int

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint

#define f16 float16_t
#define f32 float
#define f64 double

#define v2 vec2
#define v3 vec3
#define v4 vec4

#define iv2 ivec2
#define iv3 ivec3
#define iv4 ivec4

#define uv2 uvec2
#define uv3 uvec3
#define uv4 uvec4

#define m2 mat2
#define m3 mat3
#define m4 mat4
#endif // __cplusplus

#endif // TYPES_H
