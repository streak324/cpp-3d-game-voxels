#pragma once
#ifndef VOXELS_GAME_VOXEL_H
#define VOXELS_GAME_VOXEL_H
#include "common.h"
#include "math.h"
#include "memory.h"

struct VoxelMaterial {
	int topFaceTextureID;
	int sideFaceTextureID;
	int bottomFaceTextureID;
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
	math::Rotation rotation;
	u32 start;
	u32 end;
};

struct VoxelArray {
	i32 voxelsCapacity;
	i32 voxelsCount;

	VoxelMaterial* voxelsMaterial;
	Vector3i* voxelsPosition;
	Vector3ui* voxelsScale;
	i32* voxelsGroupIndex;

	i32 groupsCapacity;
	i32 groupsCount;

	VoxelGroup* groups;
};

void initVoxelArray(VoxelArray* voxelArray, MemoryAllocator* memoryAllocator, i32 voxelCapacity, i32 groupsCapacity);
i32 addVoxel(VoxelArray* voxelArray, VoxelMaterial material, Vector3i position, Vector3ui scale);
i32 addVoxelGroupFromVoxelRange(VoxelArray* voxelArray, u32 start, u32 end, math::Vector3 worldPosition);

#endif
