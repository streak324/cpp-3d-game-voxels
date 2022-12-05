#include "voxel.h"
#include "memory.h"

void initVoxelArray(VoxelArray* voxelArray, MemoryAllocator* memoryAllocator, i32 voxelCapacity, i32 groupsCapacity) {
	voxelArray->voxelsCount = 0;
	voxelArray->voxelsCapacity = voxelCapacity;
	voxelArray->colors = (RGBAColorF32*) allocateMemory(memoryAllocator, voxelCapacity*sizeof(RGBAColorF32));
	voxelArray->voxelsPosition = (Vector3i*) allocateMemory(memoryAllocator, voxelCapacity*sizeof(Vector3i));
	voxelArray->voxelsScale = (Vector3ui*) allocateMemory(memoryAllocator, voxelCapacity*sizeof(Vector3ui));
	voxelArray->voxelsGroupIndex = (i32*) allocateMemory(memoryAllocator, voxelCapacity*sizeof(i32));

	voxelArray->groupsCount = 0;
	voxelArray->groupsCapacity = groupsCapacity;
	voxelArray->groups = (VoxelGroup*) allocateMemory(memoryAllocator, voxelCapacity * sizeof(VoxelGroup));
}

i32 addStandaloneVoxel(VoxelArray* voxelArray, RGBAColorF32 color, Vector3i position, Vector3ui scale) {
	_assert(voxelArray->voxelsCount < voxelArray->voxelsCapacity);
	voxelArray->colors[voxelArray->voxelsCount] = color;
	voxelArray->voxelsPosition[voxelArray->voxelsCount] = Vector3i{};
	voxelArray->voxelsScale[voxelArray->voxelsCount] = scale;
	voxelArray->voxelsGroupIndex[voxelArray->voxelsCount] = -1;

	VoxelGroup* group = &voxelArray->groups[voxelArray->groupsCount];
	group->rotation = math::Quaternion{
		0.0f,
		math::Vector3{1.0f, 0.0f, 0.0f},
	};
	group->position = math::Vector3{ (f32)position.x, (f32)position.y, (f32)position.z };
	group->voxelsCount += 1;

	voxelArray->voxelsGroupIndex[voxelArray->voxelsCount] = voxelArray->groupsCount;
	voxelArray->groupsCount += 1;
	voxelArray->voxelsCount += 1;
	return voxelArray->voxelsCount-1;
}

i32 addVoxelToGroup(VoxelArray* voxelArray, RGBAColorF32 color, Vector3i position, Vector3ui scale, i32 groupIndex) {
	_assert(voxelArray->voxelsCount < voxelArray->voxelsCapacity);
	voxelArray->colors[voxelArray->voxelsCount] = color;
	voxelArray->voxelsPosition[voxelArray->voxelsCount] = position;
	voxelArray->voxelsScale[voxelArray->voxelsCount] = scale;
	voxelArray->voxelsGroupIndex[voxelArray->voxelsCount] = groupIndex;

	voxelArray->groups[groupIndex].voxelsCount += 1;

	voxelArray->voxelsCount += 1;

	return voxelArray->voxelsCount-1;
}

i32 addEmptyVoxelGroup(VoxelArray* voxelArray, math::Vector3 position) {
	_assert(voxelArray->groupsCount < voxelArray->groupsCapacity);
	VoxelGroup* group = &voxelArray->groups[voxelArray->groupsCount];
	group->rotation = math::Quaternion {
		1.0f,
		math::Vector3{0.0f, 0.0f, 0.0f},
	};
	group->position = position;
	group->voxelsCount = 0;

	voxelArray->groupsCount += 1;

	return voxelArray->groupsCount-1;
}

math::Vector3 convertVoxelUnitsToWorldUnits(Vector3i v) {
	return math::Vector3{ (f32)v.x, (f32)v.y, (f32) v.z }.scale(voxelUnitsToWorldUnits);
}

math::Vector3 convertVoxelUnitsToWorldUnits(Vector3ui v) {
	return math::Vector3{ (f32)v.x, (f32)v.y, (f32) v.z }.scale(voxelUnitsToWorldUnits);
}
