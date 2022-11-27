#include "../src/math.h"
#include "stdio.h"

int main() {
	{
		math::Plane plane{};
		plane.normal = math::Vector3{ 0.0f, 0.0f, 1.0f };
		plane.d = 0.5f;

		math::Vector3 pointA = { 0.0f, 0.2f, 0.0625f };

		math::Vector3 pointB = { 0.0f, 0.2f, 100.0f };

		bool32 want = 1;
		math::Vector3 wantPoint = { 0.0f, 0.2f, 0.5f };

		math::Vector3 q = {};
		bool32 got = math::isSegmentLineIntersectingPlane(plane, pointA, pointB, &q);
		if (got != want) {
			printf("want %d, got %d", want, got);
			return 1;
		}

		if (q.x != wantPoint.x || q.y != wantPoint.y || q.z != wantPoint.z) {
			printf("want (%f, %f, %f). got (%f, %f, %f)", wantPoint.x, wantPoint.y, wantPoint.z, q.x, q.y, q.z);
			return -1;
		}
	}

	{

		math::Matrix4 m = {};

		m.e.m00 = -1.0f, m.e.m01 = -2.0f, m.e.m02 = 2.0f, m.e.m03 = 1.0f;
		m.e.m10 = 2.0f, m.e.m11 = 1.0f, m.e.m12 = 1.0f, m.e.m13 = 1.0f;
		m.e.m20 = 3.0f, m.e.m21 = 4.0f, m.e.m22 = 5.0f, m.e.m23 = 1.0f;
		m.e.m30 = 1.0f, m.e.m31 = 1.0f, m.e.m32 = 1.0f, m.e.m33 = 1.0f;

		struct testCase {
			int row;
			int col;
			math::Matrix4 m;
			f32 want;
		};

		testCase testCases[] = {
			{
				0, 0,
				m,
				0.0f,
			},
			{
				0, 1,
				m,
				-4.0f,
			},
			{
				0, 2,
				m, 
				3.0f,
			},
			{
				0, 3,
				m,
				1.0f
			},
			{
				2, 3,
				m,
				-4.0f,
			},
		};

		for (int i = 0; i < sizeof(testCases) / sizeof(testCases[0]); i++) {
			f32 got = math::calculateElementCofactor(testCases[i].m, testCases[i].row, testCases[i].col);
			if (got != testCases[i].want) {
				printf("calculate element cofactor failed at test case %d. want: %f. got %f", i, testCases[i].want, got);
				return 1;
			}
		}
	}
	{

		struct testCase {
			math::Matrix4 m;
			f32 want;
		};

		testCase testCases[] = {
			{
				math::scaleMatrix(math::initIdentityMatrix(), 2.0f),
				8.0f,
			},
		};

		for (int i = 0; i < sizeof(testCases) / sizeof(testCases[0]); i++) {
			f32 got = math::calculateDeterminant(testCases[i].m);
			if (got != testCases[i].want) {
				printf("calculating determinant failed at test case %d. want: %f. got %f", i, testCases[i].want, got);
				return 1;
			}
		}
	}
	{
		math::Matrix4 m = {};

		m.e.m00 = -1.0f, m.e.m01 = -2.0f, m.e.m02 = 2.0f, m.e.m03 = 1.0f;
		m.e.m10 = 2.0f, m.e.m11 = 1.0f, m.e.m12 = 1.0f, m.e.m13 = 1.0f;
		m.e.m20 = 3.0f, m.e.m21 = 4.0f, m.e.m22 = 5.0f, m.e.m23 = 1.0f;
		m.e.m30 = 1.0f, m.e.m31 = 1.0f, m.e.m32 = 1.0f, m.e.m33 = 1.0f;

		math::Matrix4 w = {};

		w.e.m00 = -1.0f, w.e.m10 = -2.0f, w.e.m20 = 2.0f, w.e.m30 = 1.0f;
		w.e.m01 = 2.0f, w.e.m11 = 1.0f, w.e.m21 = 1.0f, w.e.m31 = 1.0f;
		w.e.m02 = 3.0f, w.e.m12 = 4.0f, w.e.m22 = 5.0f, w.e.m32 = 1.0f;
		w.e.m03 = 1.0f, w.e.m13 = 1.0f, w.e.m23 = 1.0f, w.e.m33 = 1.0f;

		math::Matrix4 g = math::transposeMatrix(m);

		for (int i = 0; i < 16; i++) {
			if (g.a.m[i] != w.a.m[i]) {
				printf("fail on index %d. want %f, got %f\n", i, w.a.m[i], g.a.m[i]);
				return 1;
			}
		}
	}
	{
		const f32 tolerance = 1.0f / (1024.0f * 16.0f);
		struct testCase {
			math::Matrix4 m;
			math::Matrix4 want;
		};

		testCase testCases[] = {
			{
				math::initIdentityMatrix(),
				math::initIdentityMatrix(),
			},
			{
				math::scaleMatrix(math::initIdentityMatrix(), 2.0f),
				math::scaleMatrix(math::initIdentityMatrix(), 0.5f),
			},
			{
				math::transposeMatrix(math::Matrix4{
					1, 4, 5, -1,
					-2, 3, -1, 0,
					2, 1, 1, 0,
					3, -1, 2, 1
				}),
				math::transposeMatrix(math::Matrix4{
					-0.1f, -0.1f, 0.6f, -0.1f,
					0.0f, 0.25f, 0.25f, 0.0f,
					0.2f, -0.05f, -0.45f, 0.2f,
					-0.1f, 0.65f, -0.65f, 0.9f,
				}),
			},
		};
		for (int i = 0; i < sizeof(testCases) / sizeof(testCases[0]); i++) {
			math::Matrix4 got = math::inverseMatrix(testCases[i].m);

			for (int j = 0; j < 16; j++) {
				if (!math::isWithinTolerance(got.a.m[j], testCases[i].want.a.m[j], tolerance)) {
					printf("failed on test case %d. element %d. want %f, got %f\n", i, j, testCases[i].want.a.m[j], got.a.m[j]);
					return 1;
				}
			}
		}
	}
	{
		struct testCase {
			math::Matrix4 m;
			math::Vector4 v;
			math::Vector4 want;
		};

		testCase testCases[] = {
			{
				math::initIdentityMatrix(),
				math::Vector4 {1.0f, 2.0f, 3.0f, 1.0f},
				math::Vector4 {1.0f, 2.0f, 3.0f, 1.0f},
			},
			{
				math::scaleMatrix(math::initIdentityMatrix(), 2.0f),
				math::Vector4 {1.0f, 2.0f, 3.0f, 1.0f},
				math::Vector4 {2.0f, 4.0f, 6.0f, 1.0f},
			},
		};

		for (int i = 0; i < sizeof(testCases) / sizeof(testCases[0]); i++) {
			math::Vector4 got = math::multiplyMatrixVector(testCases[i].m, testCases[i].v);
			math::Vector4 want = testCases[i].want;
			if (got.x != want.x || got.y != want.y || got.z != want.z) {
				printf("multiplying matrices and vectors failed at test case %d.\n\twanted (%f, %f, %f). got (%f, %f, %f)\n", i, want.x, want.y, want.z, got.x, got.y, got.z);
				return 1;
			}
		}
	}
	{
		math::Quaternion wq = math::Quaternion{0.6f, {0.38, 0.65, 0.27}};
		math::Rotation wr = math::Rotation{math::radians(-106.0f), {-0.47, -0.81, -0.33} };
		math::Rotation wr1 = math::Rotation{ math::radians(106.0f), {0.47, 0.81, 0.33} };

		const f32 rotationToQuaternionTolerance = 1.0f / 128.0f;

		math::Quaternion gq = math::convertRotationToQuaternion(wr);
		if (!math::isWithinTolerance(gq.real, wq.real, rotationToQuaternionTolerance) || !math::isVectorWithinTolerance(gq.vector, wq.vector, rotationToQuaternionTolerance)) {
			printf("quaternions do not match.\n");
			return 1;
		}

		const f32 quaternionToRotationTolerance = 1.0f / 128.0f;
		math::Rotation gr = math::convertQuaternionToRotation(wq);
		if ((!math::isWithinTolerance(gr.angle, wr.angle, quaternionToRotationTolerance) || !math::isVectorWithinTolerance(gr.axis, wr.axis, quaternionToRotationTolerance)) &&
			(!math::isWithinTolerance(gr.angle, wr1.angle, quaternionToRotationTolerance) || !math::isVectorWithinTolerance(gr.axis, wr1.axis, quaternionToRotationTolerance))) {
			printf("rotations do not match.\n");
			return 1;
		}
	}

	printf("Successfully completed the tests!!!\n");

	return 0;
}