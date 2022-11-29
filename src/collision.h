#pragma once

#ifndef VOXEL_GAME_COLLISION_H
#define VOXEL_GAME_COLLISION_H

#include "math.h"

//axis aligned bounding box
struct AABB {
	math::Vector3 min;
	math::Vector3 max;
};

//oriented bounding box
struct OBB {
	math::Vector3 center;
	math::Vector3 halfExtents;
	math::Rotation orientation;
};

bool32 isRayIntersectingAABB(math::Vector3 rayOrigin, math::Vector3 rayDirection, AABB a, f32 tmax, f32* tmin, math::Vector3 *q);
bool32 isRayIntersectingOBB(math::Vector3 rayOrigin, math::Vector3 rayDirection, OBB o, f32 tmax, f32* tmin, math::Vector3 *q);

#endif
