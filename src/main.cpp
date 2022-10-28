#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>

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

#define GIGABYTE(a) a * 1024*1024*1024

int main() {
	const i64 heap_allocation_size = (i64) GIGABYTE(1);
	u8 * data = (u8*) malloc(heap_allocation_size);
	i64 i = 0;
	printf("hello world!\nsize of i64: %lld", sizeof(i64));
	for (;;) {
		Sleep(1);
	}
}