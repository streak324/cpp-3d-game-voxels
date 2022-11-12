#include "common.h"

void _assert(u8 b) {
	#ifndef NDEBUG
		if (!b) {
			*((char*) 0) = 0;
		}
	#endif
}

void panic() {
	#ifndef NDEBUG
		*((char*) 0) = 0;
	#endif
}