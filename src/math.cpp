#include "math.h"
#include <math.h>

namespace math {
	Vector3 Vector3::add(Vector3 b) {
		return Vector3 {
			x + b.x,
			y + b.y,
			z + b.z
		};
	}

	Vector3 Vector3::sub(Vector3 b) {
		return Vector3 {
			x - b.x,
			y - b.y,
			z - b.z
		};
	}

	Vector3 Vector3::negate() {
		return Vector3 {
			-x,
			-y,
			-z
		};
	}
	Vector3 Vector3::normalize() {
		f32 dist = sqrtf(x*x + y*y + z*z);
		return Vector3{
			x/dist,
			y/dist,
			z/dist
		};
	}

	//this x b
	Vector3 Vector3::cross(Vector3 b) {
		// A = ai + bj + ck B = xi + yj + zk
		// A × B = (bz – cy)i + (cx – az)j + (ay – bx)k
		return Vector3 {
			y * b.z - z * b.y,
			z * b.x - x * b.z,
			x * b.y - y * b.x,
		};
	}

	// this projected onto vector
	Vector3 Vector3::project(Vector3 onto) {
		f32 ontoDot = onto.dot(onto);
		_assert(ontoDot > 1/1024.0f);
		f32 dot = this->dot(onto);
		return onto.scale(dot/ontoDot);
	}

	Vector3 Vector3::scale(f32 s) {
		return Vector3 {
			x * s,
			y * s,
			z * s,
		};
	}

	f32 Vector3::dot(Vector3 b) {
		return x * b.x + y * b.y + z * b.z;
	}

	f32 Vector3::length() {
		return sqrtf(x * x + y * y + z * z);
	}

	Matrix4 Matrix4::multiply(Matrix4 b) {
		Matrix4 r = {};
		const u32 squareSize = 4;
		for (u32 i = 0; i < 16; i++) {
			u32 column = i / 4;
			u32 row = i % 4;
			f32 result =
				a.m[squareSize*0 + row] * b.a.m[squareSize*column + 0] +
				a.m[squareSize*1 + row] * b.a.m[squareSize*column + 1] +
				a.m[squareSize*2 + row] * b.a.m[squareSize*column + 2] +
				a.m[squareSize*3 + row] * b.a.m[squareSize*column + 3];
			r.a.m[i] = result;
		}
		return r;
	}

	Matrix4 initIdentityMatrix() {
		Matrix4 m  = {};
		m.e.m00 = 1.0;
		m.e.m11 = 1.0;
		m.e.m22 = 1.0;
		m.e.m33 = 1.0;
		return m;
	}


	Matrix4 translateMatrix(Matrix4 m, Vector3 translate) {
		m.e.m03 += translate.x;
		m.e.m13 += translate.y;
		m.e.m23 += translate.z;
		return m;
	}

	Matrix4 scaleMatrix(Matrix4 m, Vector3 scale) {
		Matrix4 s = {};
		s.e.m00 = scale.x;
		s.e.m11 = scale.y;
		s.e.m22 = scale.z;
		s.e.m33 = 1.0f;
		return m.multiply(s);
	}

	Matrix4 scaleMatrix(Matrix4 m, f32 scalar) {
		return scaleMatrix(m, Vector3{scalar, scalar, scalar});
	}

	Matrix4 createFrustum(f32 left, f32 right, f32 bottom, f32 top, f32 zNear, f32 zFar) {
		Matrix4 m = {};
		m.e.m00 = 2*zNear/(right - left);
		m.e.m02 = (right + left) / (right - left);
		m.e.m12 = (top + bottom)  / (top - bottom);
		m.e.m11 = 2*zNear/(top - bottom);
		m.e.m22 = -(zFar+zNear)/(zFar-zNear);
		m.e.m23 = -2*zFar*zNear/(zFar-zNear);
		m.e.m32 = -1;
		return m;
	}

	Matrix4 createPerspective(f32 fovRadians, f32 aspect, f32 zNear, f32 zFar) {
		Matrix4 m = {};
		f32 tangent = tanf(fovRadians/2);
		f32 half_height = zNear * tangent;
		f32 half_width = half_height * aspect;
		return createFrustum(-half_width, half_width, -half_height, half_height, zNear, zFar);
	}

	// calculate the right, and up lookAt vectors
	void lookAtVectors(Vector3 direction, Vector3 *right, Vector3 *up) {
		math::Vector3 tmpUp = {0.0f, 1.0f, 0.0f};
		Vector3 forward = direction;
		*right = tmpUp.cross(forward).normalize();
		*up = forward.cross(*right);
	}

	Matrix4 lookAt(Vector3 from, Vector3 to, Vector3 up) {
		Vector3 forward = from.sub(to).normalize();
		Vector3 right = up.cross(forward).normalize();
		Vector3 newUp = forward.cross(right);
		Matrix4 m = {};
		m.e.m00 = right.x, m.e.m01 = right.y, m.e.m02 = right.z;
		m.e.m10 = newUp.x, m.e.m11 = newUp.y, m.e.m12 = newUp.z;
		m.e.m20 = forward.x, m.e.m21 = forward.y, m.e.m22 = forward.z;
		m.e.m03 = right.dot(from.negate()), m.e.m13 = newUp.dot(from.negate()), m.e.m23 = forward.dot(from.negate());
		m.e.m33 = 1.0f;
		return m;
	}


	Matrix4 initXAxisRotationMatrix(f32 angle) {
		Matrix4 m = {};
		m.e.m00 = 1.0f;
		m.e.m11 = cosf(angle);
		m.e.m12 = -sinf(angle);
		m.e.m21 = sinf(angle);
		m.e.m22 = cosf(angle);
		m.e.m33 = 1.0f;
		return m;
	}

	Matrix4 initYAxisRotationMatrix(f32 angle) {
		Matrix4 m = {};
		m.e.m00 = cosf(angle);
		m.e.m02 = sinf(angle);
		m.e.m11 = 1.0f;
		m.e.m20 = -sinf(angle);
		m.e.m22 = cosf(angle);
		m.e.m33 = 1.0f;
		return m;
	}

	Matrix4 initZAxisRotationMatrix(f32 angle) {
		Matrix4 m = {};
		m.e.m00 = cosf(angle);
		m.e.m01 = -sinf(angle);
		m.e.m10 = sinf(angle);
		m.e.m11 = cosf(angle);
		m.e.m22 = 1;
		m.e.m33 = 1;
		return m;
	}


	f32 calculateElementCofactor(Matrix4 m, int row, int column) {
		_assert(row >= 0 && row < 4 && column >= 0 && column < 4);
		f32 minor[9] = {};

		int minorRow = 0;
		int neg = 2 * ((1 + row + column) % 2) - 1;

		for (int i = 0; i < 4; i++) {
			if (i == row) {
				continue;
			}
			int minorColumn = 0;
			for (int j = 0; j < 4; j++) {
				if (j == column) {
					continue;
				}
				int index = 3 * minorColumn + minorRow;
				minor[index] = m.a.m[4 * j + i];
				minorColumn += 1;
			}
			minorRow += 1;
		}

		f32 sum1 = minor[0] * (minor[4] * minor[8] - minor[7] * minor[5]);
		f32 sum2 = minor[3] * (minor[1] * minor[8] - minor[7] * minor[2]);
		f32 sum3 = minor[6] * (minor[1] * minor[5] - minor[4] * minor[2]);

		return m.a.m[4 * column + row] * (sum1 - sum2 + sum3) * (f32)neg;
	}

	Matrix4 inverseMatrix(Matrix4 m) {
		Matrix4 r = {};
		return r;
	}

	f32 radians(f32 degrees) {
		return degrees * TAU32 / 360.0f;
	}

	bool isUnitVector(Vector3 a) {
		f32 epsilon = 1 / 4096.0f;
		f32 distsq = a.dot(a);
		return distsq <= 1.0f + epsilon && distsq >= 1.0f - epsilon;
	}

	// c = a * b
	Quaternion multiplyQuaternions(Quaternion a, Quaternion b) {

		Vector3 cross = a.vector.cross(b.vector);
		Vector3 bar = b.vector.scale(a.real);
		Vector3 abr = a.vector.scale(b.real);

		Quaternion quat = {
			a.real * b.real - a.vector.dot(b.vector),
			cross.add(bar).add(abr),
		};
		return quat;
	}

	Vector3 rotateVector(Vector3 a, Rotation rotation) {
		_assert(isUnitVector(rotation.unit));
		Quaternion q = {
			cosf(0.5f*rotation.angle),
			rotation.unit.scale(sinf(0.5f*rotation.angle)),
		};
		Vector3 cross = q.vector.cross(a);
		Vector3 crossV = q.vector.cross(cross);
		
		return a.add(cross.scale(2*q.real)).add(crossV.scale(2));
	}

	Matrix4 createRotationMatrix(Rotation rotation) {
		_assert(isUnitVector(rotation.unit));
		Quaternion q = {
			cosf(0.5f*rotation.angle),
			rotation.unit.scale(sinf(0.5f*rotation.angle)),
		};
		Matrix4 m = initIdentityMatrix();

		m.e.m00 = 2 * (q.real * q.real + q.vector.x * q.vector.x) - 1;
		m.e.m01 = 2 * (q.vector.x * q.vector.y - q.real * q.vector.z);
		m.e.m02 = 2 * (q.vector.x * q.vector.z + q.real * q.vector.y);

		m.e.m10 = 2 * (q.vector.x * q.vector.y + q.real * q.vector.z);
		m.e.m11 = 2 * (q.real * q.real + q.vector.y * q.vector.y) - 1;
		m.e.m12 = 2 * (q.vector.y * q.vector.z - q.real * q.vector.x);

		m.e.m20 = 2 * (q.vector.x * q.vector.z - q.real * q.vector.y);
		m.e.m21 = 2 * (q.vector.y * q.vector.z + q.real * q.vector.x);
		m.e.m22 = 2 * (q.real * q.real + q.vector.z * q.vector.z) - 1;

		return m;
	}

	bool32 isSegmentLineIntersectingPlane(Plane plane, Vector3 pointA, Vector3 pointB, Vector3* q) { 
		f32 t = (plane.d - plane.normal.dot(pointA)) / plane.normal.dot(pointB.sub(pointA));
		if (t >= 0.0f && t <= 1.0f) {
			*q = pointA.add(pointB.sub(pointA).scale(t));
			return 1;
		}
		return 0;
	}
};