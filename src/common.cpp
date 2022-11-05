#include "common.h"

void assert(u8 b) {
	#ifndef NDEBUG
		if (!b) {
			*((char*) 0) = 0;
		}
	#endif
}