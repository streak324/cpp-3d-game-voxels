#include "math.h"

Matrix4 initIdentityMatrix() {
	Matrix4 m  = {};
	m.e.m00 = 1.0;
	m.e.m11 = 1.0;
	m.e.m22 = 1.0;
	m.e.m33 = 1.0;
	return m;
}

Matrix4 scaleMatrix(Matrix4 m, Vector3 scale) {
	Matrix4 s  = {};
	s.e.m00 = scale.x;
	s.e.m11 = scale.y;
	s.e.m22 = scale.z;
	s.e.m33 = 1.0;
	return multiplyMatrices(m, s);
}

Matrix4 scaleMatrix(Matrix4 m, f32 scalar) {
	return scaleMatrix(m, Vector3{scalar, scalar, scalar});
}

Matrix4 multiplyMatrices(Matrix4 a, Matrix4 b) {
	Matrix4 r;
	const u32 squareSize = 4;
	for (u32 i = 0; i < 16; i++) {
		u32 column = i / 4;
		u32 row = i % 4;
		f32 result =
			a.a.m[squareSize*0 + row] * b.a.m[squareSize*column + 0] +
			a.a.m[squareSize*1 + row] * b.a.m[squareSize*column + 1] +
			a.a.m[squareSize*2 + row] * b.a.m[squareSize*column + 2] +
			a.a.m[squareSize*3 + row] * b.a.m[squareSize*column + 3];
		r.a.m[i] = result;
	}
	return r;
}
