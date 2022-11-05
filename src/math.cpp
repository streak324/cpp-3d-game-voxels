#include "math.h"

Matrix4 initIdentityMatrix() {
	Matrix4 m  = {};
	m.m00 = 1.0;
	m.m11 = 1.0;
	m.m22 = 1.0;
	m.m33 = 1.0;
	return m;
}