#include "voxel.h"

void addVoxel(VoxelArray* voxelArray, VoxelMaterial material, Vector3i position, Vector3ui scale) {
	_assert(voxelArray->size < voxelArray->capacity);
	voxelArray->voxels[voxelArray->size] = {
		material,
		position,
		scale,
	};
	voxelArray->size += 1;
}
