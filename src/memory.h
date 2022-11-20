#pragma once
#ifndef VOXELS_GAME_MEMORY_H
#define VOXELS_GAME_MEMORY_H

#include "common.h"
#include <stdlib.h>

#define gigabyte(x) (x*1000ull*1000ull*1000ull)

struct MemoryAllocator {
	u64 byteCapacity;
	u64 byteOffset;
	u8* memory;
};

void initMemoryAllocator(MemoryAllocator* allocator, u64 capacity);

void* allocateMemory(MemoryAllocator* allocator, u64 byteAllocation);

#endif