#include "common.h"

struct Vector3 {
	f32 x, y, z;
};

union Matrix4 {
	struct array
	{
		//ordered by row first, then column. i.e. first element is column 0, row 1, second element is column 0, row 1, last element is column 3, row 3.
		f32 m[16];
	};
	struct elements{
		/* 
			myx: 
			- x column
			- y row
		*/
		f32 m00, m10, m20, m30;
		f32 m01, m11, m21, m31;
		f32 m02, m12, m22, m32;
		f32 m03, m13, m23, m33;
	};
	array a;
	elements e;
};

Matrix4 initIdentityMatrix();
Matrix4 scaleMatrix(Matrix4 m, Vector3 scale);
Matrix4 scaleMatrix(Matrix4 m, f32 scalar);
Matrix4 multiplyMatrices(Matrix4 a, Matrix4 b);
