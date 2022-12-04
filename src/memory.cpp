#include "memory.h"

void initMemoryAllocator(MemoryAllocator *allocator, u64 capacity) {
	(u8*) malloc(capacity);
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
