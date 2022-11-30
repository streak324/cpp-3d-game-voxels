#include "collision.h"
#include <math.h>

bool32 isRayIntersectingAABB(math::Vector3 rayOrigin, math::Vector3 rayDirection, AABB a, f32 tmax, f32* tmin, math::Vector3* q) {
	*tmin = 0.0f;
	f32 tolerance = 1.0f / 1024.0f;
	for (i32 i = 0; i < 3; i++) {
		if (math::isWithinTolerance(rayDirection.v[i], 0.0f, tolerance)) {
			if (rayOrigin.v[i] < a.min.v[i] || rayOrigin.v[i] > a.max.v[i]) {
				return 0;
			}
		}
		else {
			f32 ood = 1.0f / rayDirection.v[i];
			f32 t1 = (a.min.v[i] - rayOrigin.v[i]) * ood;
			f32 t2 = (a.max.v[i] - rayOrigin.v[i]) * ood;
			if (t1 > t2) {
				f32 t3 = t1;
				t1 = t2;
				t2 = t3;
			}
			*tmin = fmaxf(*tmin, t1);
			tmax = fminf(tmax, t2);

			if (*tmin > tmax) {
				return 0;
			}
		}
	}
	*q = rayOrigin.add(rayDirection.scale(*tmin));
	return 1;
}

bool32 isRayIntersectingOBB(math::Vector3 rayOrigin, math::Vector3 rayDirection, OBB o, f32 tmax, f32* tmin, math::Vector3 *q) {
	math::Quaternion opp = math::createQuaternionRotation(-math::getRotationAngle(o.orientation), math::getRotationAxis(o.orientation));
	math::Vector3 rayOriginRelative = rotateVector(rayOrigin.sub(o.center), opp);
	math::Vector3 rayDirectionRelative = rotateVector(rayDirection, opp);
	AABB a = {};
	a.min = o.halfExtents.scale(-1);
	a.max = o.halfExtents;
	if (isRayIntersectingAABB(rayOriginRelative, rayDirectionRelative, a, tmax, tmin, q)) {
		*q = rayOrigin.add(rayDirection.scale(*tmin));
		return 1;
	}
	return 0;
}

