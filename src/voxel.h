#pragma once
#include "common.h"

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

struct Voxel {
	VoxelMaterial material;
	Vector3i position;
	Vector3ui scale;
};

struct VoxelArray {
	u32 capacity;
	u32 size;
	Voxel* voxels;
};

void addVoxel(VoxelArray* voxelArray, VoxelMaterial material, Vector3i position, Vector3ui scale);
