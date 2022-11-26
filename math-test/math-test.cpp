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
				8.0f,
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
				printf("want: %f. got %f", testCases[i].want, got);
				return 1;
			}
		}
	}

	printf("Successfully completed the tests!!!\n");

	return 0;
}