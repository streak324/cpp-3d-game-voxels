#include "common.h"
#include <stdlib.h>

#define gigabyte(x) (x*1000ull*1000ull*1000ull)

struct MemoryAllocator {
	u64 byteCapacity;
	u64 byteOffset;
	u8* memory;
};

void initMemoryAllocator(MemoryAllocator *allocator, u64 capacity) {
	allocator->memory = (u8*) malloc(capacity);
	allocator->byteOffset = 0;
	allocator->byteCapacity = capacity;
	_assert(allocator->memory != 0);
}

void* allocateMemory(MemoryAllocator *allocator, u64 byteAllocation) {
	_assert(allocator->byteOffset + byteAllocation <= allocator->byteCapacity);
	void* ptr = (void*)(allocator->memory + allocator->byteOffset + byteAllocation);
	allocator->byteOffset += byteAllocation;
	return ptr;
}