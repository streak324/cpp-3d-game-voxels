#include "common.h"

struct Vector3 {
	f32 x, y, z;
};

/* 
	myx: 
	- x column
	- y row
*/
struct Matrix4 {
	f32 m00, m10, m20, m30;
	f32 m01, m11, m21, m31;
	f32 m02, m12, m22, m32;
	f32 m0r, m13, m23, m33;
};

Matrix4 initIdentityMatrix();