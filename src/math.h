#include "common.h"

#define PI32 3.1415926f
#define TAU32 6.283185f

namespace math {
	struct Vector3 {
		f32 x, y, z;
		Vector3 sub(Vector3 b);
		Vector3 negate();
		Vector3 normalize();
		Vector3 cross(Vector3 b);
	};

	union Matrix4 {
		struct array
		{
			//ordered by then column first, then row. i.e. first element is column 0, row 1, second element is column 0, row 1, last element is column 3, row 3.
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

		Matrix4 multiply(Matrix4 b);
	};

	Matrix4 initIdentityMatrix();
	Matrix4 translateMatrix(Matrix4 m, Vector3 translate);
	Matrix4 scaleMatrix(Matrix4 m, Vector3 scale);
	Matrix4 scaleMatrix(Matrix4 m, f32 scalar);
	Matrix4 initPerspectiveMatrix(f32 width, f32 height, f32 zFar, f32 zNear);
	Matrix4 lookAt(Vector3 from, Vector3 to, Vector3 up);
	Matrix4 initYAxisRotationMatrix(f32 angle);
	Matrix4 initZAxisRotationMatrix(f32 angle);
}