#pragma once
#ifndef VOXELS_GAME_VOXEL_H
#define VOXELS_GAME_VOXEL_H
#include "common.h"
#include "math.h"
#include "memory.h"

const f32 voxelUnitsToWorldUnits = 0.25f;

struct RGBAColorF32 {
	f32 r, b, g, a;
};

struct Vector3i {
	i32 x;
	i32 y;
	i32 z;
};


struct Vector3ui {
	u32 x;
	u32 y;
	u32 z;
};

struct VoxelGroup {
	math::Vector3 worldPosition;
	math::Quaternion rotation;
	u32 start;
	u32 end;
};

struct VoxelArray {
	i32 voxelsCapacity;
	i32 voxelsCount;

	RGBAColorF32* colors;
	Vector3i* voxelsPosition;
	//TODO: rename to be voxelsHalfExtents
	Vector3ui* voxelsScale;
	i32* voxelsGroupIndex;

	i32 groupsCapacity;
	i32 groupsCount;

	VoxelGroup* groups;
};

void initVoxelArray(VoxelArray* voxelArray, MemoryAllocator* memoryAllocator, i32 voxelCapacity, i32 groupsCapacity);
i32 addVoxel(VoxelArray* voxelArray, RGBAColorF32 color, Vector3i position, Vector3ui scale);
i32 addVoxelGroupFromVoxelRange(VoxelArray* voxelArray, u32 start, u32 end, math::Vector3 worldPosition);

math::Vector3 convertVoxelUnitsToWorldUnits(Vector3i v);
math::Vector3 convertVoxelUnitsToWorldUnits(Vector3ui v);

#endif
