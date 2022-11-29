#pragma once
#ifndef VOXELS_GAME_MATH_H
#define VOXELS_GAME_MATH_H

#include "common.h"

#define PI32  3.1415926535f
#define TAU32 6.2831853071f

namespace math {
	union Vector3 {
		struct {
			f32 v[3];
		};
		struct {
			f32 x, y, z;
		};

		Vector3 add(Vector3 b);
		Vector3 sub(Vector3 b);
		Vector3 negate();
		Vector3 normalize();
		Vector3 cross(Vector3 b);
		f32 dot(Vector3 b);
		f32 length();
		Vector3 scale(f32 s);
		Vector3 project(Vector3 onto);
	};

	struct Vector4 {
		f32 x, y, z, w;
	};

	union Matrix4 {
		struct array
		{
			//ordered by column first, then row. i.e. first element is column 0, row 1, second element is column 0, row 1, last element is column 3, row 3.
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

	/*
		Quaternion, but in a slightly different representation.
		[cos(angle/2), sinf(angle) * unit] 
	*/
	struct Rotation {
		//must be a unit vector
		f32 angle;
		Vector3 axis;
	};

	struct Quaternion {
		f32 real;
		Vector3 vector;
	};

	Matrix4 initIdentityMatrix();
	Matrix4 translateMatrix(Matrix4 m, Vector3 translate);
	Matrix4 scaleMatrix(Matrix4 m, Vector3 scale);
	Matrix4 scaleMatrix(Matrix4 m, f32 scalar);
	Matrix4 createFrustum(f32 left, f32 right, f32 bottom, f32 top, f32 zNear, f32 zFar);
	Matrix4 createPerspective(f32 fov, f32 aspect, f32 zNear, f32 zFar);
	Matrix4 lookAt(Vector3 from, Vector3 to, Vector3 up);
	Matrix4 initXAxisRotationMatrix(f32 angle);
	Matrix4 initYAxisRotationMatrix(f32 angle);
	Matrix4 initZAxisRotationMatrix(f32 angle);
	Matrix4 transposeMatrix(Matrix4 m);
	f32 calculateElementCofactor(Matrix4 m, int row, int column);
	f32 calculateDeterminant(Matrix4 m);
	Matrix4 inverseMatrix(Matrix4 m);

	Vector4 multiplyMatrixVector(Matrix4 m, Vector4 v);

	f32 radians(f32 degrees);

	Quaternion normalizeQuaternion(Quaternion q);
	Vector3 rotateVector(Vector3 a, Rotation rotation);
	Matrix4 createRotationMatrix(Rotation rotation);
	Quaternion convertRotationToQuaternion(Rotation r);
	Rotation convertQuaternionToRotation(Quaternion q);
	Rotation multiplyRotations(Rotation a, Rotation b);
	Rotation convertEulerAnglesToRotation(math::Vector3 euler);

	void lookAtVectors(Vector3 direction, Vector3* right, Vector3* up);

	bool32 isWithinTolerance(f32 got, f32 want, f32 tolerance);
	bool32 isVectorWithinTolerance(Vector3 got, Vector3 want, f32 tolerance);

	f32 getAngleBetweenTwoVectors(Vector3 a, Vector3 b);

}

#endif