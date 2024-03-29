#pragma once
#ifndef VOXELS_GAME_COMMON_H
#define VOXELS_GAME_COMMON_H

#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t u16;

typedef float f32;
typedef double f64;

typedef u32 bool32;

#define nil nullptr

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)


struct RGBAColorF32 {
	f32 r, b, g, a;
};

void _assert(u8 b);
void panic();

#endif