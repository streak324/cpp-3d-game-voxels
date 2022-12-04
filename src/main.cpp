#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "common.h"
#include "math.h"
#include "voxel.h"
#include "memory.h"
#include "collision.h"

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include "renderer.h"

Ray calculateRayFromScreenToWorld(f32 cursorX, f32 cursorY, int windowWidth, int windowHeight, UniformBufferData ub, math::Vector3 cameraPosition) {
	math::Vector4 cursorInClipSpace = math::Vector4{2 * ((f32)cursorX / (f32)windowWidth) - 1.0f, 2 * ((f32)cursorY / (f32)windowHeight) - 1.0f, -1.0f, 1.0f};

	math::Matrix4 invProjection = math::inverseMatrix(ub.projection);

	math::Vector4 cursorInViewSpace = math::multiplyMatrixVector(invProjection, cursorInClipSpace);

	cursorInViewSpace = math::Vector4{cursorInViewSpace.x, cursorInViewSpace.y, -1.0f, 0.0f};

	math::Matrix4 invView = math::inverseMatrix(ub.view);

	math::Vector4 cursorInWorldSpaceVec4 = math::multiplyMatrixVector(invView, cursorInViewSpace);

	Ray r = {};

	r.direction = math::Vector3{cursorInWorldSpaceVec4.x, cursorInWorldSpaceVec4.y, cursorInWorldSpaceVec4.z}.normalize();
	r.origin = cameraPosition;

	return r;
}

int main(void) {
	f64 loadStartTime = glfwGetTime();
#ifndef NDEBUG
	printf("IN DEBUG MODE\n");
#endif

	/* Initialize the library */
	if (!glfwInit()) {
		printf("unable to initialize glfw!\n");
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* window = glfwCreateWindow(1280, 720, "CPP Voxels!!", nil, nil);
	if (!window) {
		glfwTerminate();
		printf("unable to create the window\n");
		return 1;
	}

	//TODO: might have to memory allocate the memory allocator
	MemoryAllocator* memoryAllocator;
	{
		MemoryAllocator tmp;
		initMemoryAllocator(&tmp, gigabyte(2));
		memoryAllocator = &tmp;
	}

	Renderer* renderer = (Renderer*) allocateMemory(memoryAllocator, sizeof(renderer));
	initRenderer(renderer, window, memoryAllocator);

	const u32 maxVoxels = 2048 * 2048;
	VoxelArray voxelArray = {};
	initVoxelArray(&voxelArray, memoryAllocator, maxVoxels, maxVoxels/16);

	GPUObjectData gpuObjectData = {};
	gpuObjectData.models = (math::Matrix4*) allocateMemory(memoryAllocator, voxelArray.voxelsCapacity * sizeof(math::Matrix4));
	gpuObjectData.rgbaColors = (RGBAColorF32*) allocateMemory(memoryAllocator, voxelArray.voxelsCapacity * sizeof(RGBAColorF32));
	gpuObjectData.count = 0;

	RGBAColorF32 colorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

	i32 g1 = addVoxel(&voxelArray, colorWhite, Vector3i{ 0, 0, 0 }, Vector3ui{ 8, 8, 8 });
	i32 g2 = addVoxel(&voxelArray, colorWhite, Vector3i{ 0, 0, 0 }, Vector3ui{ 8, 8, 8 });

	addVoxelGroupFromVoxelRange(&voxelArray, g1, g1, math::Vector3{ 12, 0, -30 });
	addVoxelGroupFromVoxelRange(&voxelArray, g2, g2, math::Vector3{ -12, 0, -30 });

	addVoxel(&voxelArray, {0.0f, 1.0f, 1.0f, 0.2f}, Vector3i{0, 12, -40}, Vector3ui{8, 8, 8});
	addVoxel(&voxelArray, colorWhite, Vector3i{24, 12, -30}, Vector3ui{ 8, 8, 2 });
	addVoxel(&voxelArray, colorWhite, Vector3i{36, 12, -30}, Vector3ui{ 8, 8, 1 });
	addVoxel(&voxelArray, colorWhite, Vector3i{-10, 12, -30}, Vector3ui{ 8, 8, 8 });
	addVoxel(&voxelArray, colorWhite, Vector3i{10, 12, -30}, Vector3ui{ 8, 8, 8 });
	addVoxel(&voxelArray, colorWhite, Vector3i{-24, 12, -30}, Vector3ui{ 1, 1, 1 });

	{
		u32 start = addVoxel(&voxelArray, colorWhite, Vector3i{ 0, 0, 0 }, Vector3ui{ 2, 2, 2 });
		addVoxel(&voxelArray, colorWhite, Vector3i{ 0, 2, 0 }, Vector3ui{ 2, 2, 2 });
		addVoxel(&voxelArray, colorWhite, Vector3i{ 0, 4, 0 }, Vector3ui{ 2, 2, 2 });
		addVoxel(&voxelArray, colorWhite, Vector3i{ 0, 6, 0 }, Vector3ui{ 2, 2, 2 });
		u32 end = addVoxel(&voxelArray, colorWhite, Vector3i{ 0, 8, 0 }, Vector3ui{ 2, 2, 2 });

		addVoxelGroupFromVoxelRange(&voxelArray, start, end, math::Vector3{0, 0, -30.0f});
	}
	{
		u32 i1 = addVoxel(&voxelArray, { 0.8f, 1.0f, 0.0f, 1.0f }, Vector3i{ 0, 0, 0 }, Vector3ui{2, 2, 4});
		u32 i2 = addVoxel(&voxelArray, { 0.5f, 0.0f, 1.0f, 1.0f }, Vector3i{ 0, 0, 0 }, Vector3ui{2, 2, 4});
		i32 g1 = addVoxelGroupFromVoxelRange(&voxelArray, i1, i1, math::Vector3{ 0, 0, -50 });
		i32 g2 = addVoxelGroupFromVoxelRange(&voxelArray, i2, i2, math::Vector3{ 0, 0, -60 });
		voxelArray.groups[g1].rotation = math::createQuaternionRotation(1.604749, { 0.067773, 0.995257, -0.069782 });

		voxelArray.groups[g2].rotation = math::createQuaternionRotation(PI32 / 4.0f, { 0.0f, 1.0f, 0.0f });
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	u64 frameCounter = -1;

	int windowWidth, windowHeight;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);

	f64 lastCursorX, lastCursorY;
	f64 cursorX, cursorY;
	glfwGetCursorPos(window, &cursorX, &cursorY);
	cursorY = windowHeight - cursorY;
	lastCursorY = cursorX;
	lastCursorY = cursorY;

	bool showImGuiDemoWindow = false;
	bool wasCameraToggleKeyPressed = false;

	bool32 isMovingCamera = 0;
	if (isMovingCamera) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
	}

	bool isDisplayingGrid = true;

	i32 maxVoxelGridUnitSize = 16;
	i32 voxelGridUnitSize = 8;
	i32 voxelGridWidth = 64;
	i32 voxelGridHeight = 32;

	i32 selectedVoxelIndex = -1;
	RGBAColorF32 selectedVoxelColorBlend = { 1.0f, 1.0f, 0.0f, 0.1f };

	f32 cameraPitch = 0.0f;
	f32 cameraYaw = 0.0f;
	math::Vector3 cameraDirection = {
		cosf(cameraYaw - PI32 / 2) * cosf(cameraPitch),
		sinf(cameraPitch),
		sinf(cameraYaw - PI32 / 2) * cosf(cameraPitch),
	};

	math::Vector3 cameraRight = {};
	math::Vector3 cameraUp = {};
	math::lookAtVectors(cameraDirection, &cameraRight, &cameraUp);

	VkClearValue clearColor = {};
	clearColor.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	UniformBufferData ub = {};

	bool32 isLeftCursorPressed = false;

	bool32 wasCursorRayCasted = false;
	math::Quaternion cursorRayOrientation = math::Quaternion{};
	
	bool32 isCursorRayHit = 0;
	Ray cursorRay = {};
	math::Vector3 cursorRayHitPoint = {};
	f32 cursorRayHitDist = 100.0f;

	printf("%f seconds to bootup\n", (f32)glfwGetTime() - loadStartTime);


	const f32 max_timestep = 1.0f / 60.0f;
	math::Vector3 cameraPosition = {0.0f, 2.0f, 2.0f};
	const math::Vector3 zAxis = {0.0f, 0.0f, 1.0f};
	const math::Vector3 negativeZAxis = {0.0f, 0.0f, -1.0f};

	f64 lastFrameDelta = glfwGetTime();
	
	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window)) {
		frameCounter = (frameCounter + 1) % MAX_FRAMES_IN_FLIGHT;
		/* Poll for and process events */
		glfwPollEvents();

		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		if (windowWidth == 0 || windowHeight == 0) {
			glfwWaitEventsTimeout(0.1);
			continue;
		}

		lastCursorX = cursorX;
		lastCursorY = cursorY;
		glfwGetCursorPos(window, &cursorX, &cursorY);
		cursorY = (f32) windowHeight - cursorY;

		f32 deltaCursorX = (f32)(cursorX - lastCursorX);
		f32 deltaCursorY = (f32)(cursorY - lastCursorY);

		f32 timestep = fminf(max_timestep, (f32)(glfwGetTime() - lastFrameDelta));

		math::Vector3 cameraForwardUnitVelocity = {};
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			cameraForwardUnitVelocity.z -= 1;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			cameraForwardUnitVelocity.x -= 1;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			cameraForwardUnitVelocity.z += 1;
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			cameraForwardUnitVelocity.x += 1;
		}
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
			cameraForwardUnitVelocity.y -= 1;
		}
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
			cameraForwardUnitVelocity.y += 1;
		}
		if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
			if (!wasCameraToggleKeyPressed) {
				isMovingCamera = !isMovingCamera;
				if (isMovingCamera) {
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				}
				else {
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				}
			}
			wasCameraToggleKeyPressed = true;
		}
		else if (glfwGetKey(window, GLFW_KEY_O) == GLFW_RELEASE) {
			wasCameraToggleKeyPressed = false;
		}
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			if (!isLeftCursorPressed) {
				cursorRay = calculateRayFromScreenToWorld(cursorX, cursorY, windowWidth, windowHeight, ub, cameraPosition);

				cursorRayOrientation = math::convertEulerAnglesToQuaternionRotation(math::Vector3{ cameraPitch, -cameraYaw, 0.0f });

				f32 cursorRayAngleFromCameraDirection = math::getAngleBetweenTwoVectors(cameraDirection, cursorRay.direction);

				math::Vector3 cursorNormal = cameraDirection.cross(cursorRay.direction).normalize();

				cursorRayOrientation = math::multiplyQuaternions(cursorRayOrientation, math::createQuaternionRotation( cursorRayAngleFromCameraDirection, cursorNormal ));

				wasCursorRayCasted = 1;

				selectedVoxelIndex = -1;
				const f32 tmax = 100.0f;
				isCursorRayHit = 0;
				cursorRayHitDist = tmax;
				for (i32 i = 0; i < voxelArray.voxelsCount; i++) {
					OBB o = {};
					o.center = convertVoxelUnitsToWorldUnits(voxelArray.voxelsPosition[i]);
					o.halfExtents = convertVoxelUnitsToWorldUnits(voxelArray.voxelsScale[i]).scale(0.5f);
					o.orientation = math::createQuaternionRotation( 0.0f, {1.0f, 0.0f, 0.0f} );

					i32 voxelGroupIndex = voxelArray.voxelsGroupIndex[i];
					if (voxelGroupIndex >= 0) {
						o.center = math::rotateVector(o.center, voxelArray.groups[voxelGroupIndex].rotation).add(voxelArray.groups[voxelGroupIndex].worldPosition.scale(voxelUnitsToWorldUnits));
						o.orientation = voxelArray.groups[voxelGroupIndex].rotation;
					}
					f32 t;
					math::Vector3 q;
					if (isRayIntersectingOBB(cursorRay.origin, cursorRay.direction, o, tmax, &t, &q) && t < cursorRayHitDist) {
						cursorRayHitDist = t;
						cursorRayHitPoint = q;
						selectedVoxelIndex = i;
						isCursorRayHit = 1;
					}
				}
			}
			else if (isCursorRayHit) {
				cursorRay = calculateRayFromScreenToWorld(cursorX, cursorY, windowWidth, windowHeight, ub, cameraPosition);
			}
			isLeftCursorPressed = 1;
		}
		else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
			isLeftCursorPressed = 0;
		}

		if (isMovingCamera) {
			cameraYaw += deltaCursorX / 100.0f;
			cameraPitch += deltaCursorY / 100.0f;
			f32 minPitch = -0.5f*PI32+math::radians(1);
			f32 maxPitch = 0.5f*PI32-math::radians(1);
			if (cameraPitch < minPitch) {
				cameraPitch = minPitch;
			} else if (cameraPitch > maxPitch) {
				cameraPitch = maxPitch;
			}

			cameraDirection = {
				cosf(cameraYaw - PI32 / 2) * cosf(cameraPitch),
				sinf(cameraPitch),
				sinf(cameraYaw - PI32 / 2) * cosf(cameraPitch),
			};

			f32 cameraSpeed = 5;
			f32 distanceSquared = cameraForwardUnitVelocity.dot(cameraForwardUnitVelocity);
			if (distanceSquared > (1/1024.0f)) {
				math::Vector3 up = {0.0, 1.0, 0.0};
				math::Vector3 forward = cameraDirection;
				cameraRight = up.cross(forward).normalize();
				cameraUp = forward.cross(cameraRight);
				math::Vector3 cameraVelocity = 
					forward.scale(cameraForwardUnitVelocity.z)
					.add(cameraRight.scale(cameraForwardUnitVelocity.x)
					.add(cameraUp.scale(cameraForwardUnitVelocity.y)))
					.scale(-cameraSpeed);
				cameraPosition = cameraPosition.add(cameraVelocity.scale(timestep));
			}
		}

		vkWaitForFences(renderer->device, 1, &renderer->inFlightFences[frameCounter], VK_TRUE, UINT64_MAX);
		u32 imageIndex;
		VkResult result = vkAcquireNextImageKHR(renderer->device, renderer->swapchain->handle, UINT64_MAX, renderer->imageAvailableSemaphores[frameCounter], VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			result = handleRenderResizing(renderer);
			if (result != VK_SUCCESS) {
				printf("unable to create swapchain!\n");
				return 1;	
			}
			continue;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			printf("failed to acquire the next swapchain image!\n");
			return 1;
		}
		vkResetFences(renderer->device, 1, &renderer->inFlightFences[frameCounter]);

		vkResetCommandBuffer(renderer->commandBuffers[frameCounter], 0);

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = 0;
		commandBufferBeginInfo.pInheritanceInfo = nil;

		if (vkBeginCommandBuffer(renderer->commandBuffers[frameCounter], &commandBufferBeginInfo) != VK_SUCCESS) {
			printf("unable to begin the command buffer!\n");
			return 1;
		}

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderer->renderPass;
		renderPassBeginInfo.framebuffer = renderer->swapchain->framebuffers[imageIndex];
		renderPassBeginInfo.renderArea.offset = {0, 0};
		renderPassBeginInfo.renderArea.extent = renderer->swapchain->extent;

		VkClearValue clearDepth = {};
		clearDepth.depthStencil = {1.0f, 0};
		VkClearValue clearValues[2] = {clearColor, clearDepth};
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(renderer->commandBuffers[frameCounter], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (float) renderer->swapchain->extent.height;
		viewport.width = (float) renderer->swapchain->extent.width;
		viewport.height = -(float) renderer->swapchain->extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(renderer->commandBuffers[frameCounter], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset = {0, 0};
		scissor.extent = renderer->swapchain->extent;
		vkCmdSetScissor(renderer->commandBuffers[frameCounter], 0, 1, &scissor);

		f32 scale = 2.0f;

		ub = {};
		ub.view = math::lookAt(cameraPosition, cameraPosition.add(cameraDirection), math::Vector3{0.0f, 1.0f, 0.0f});
		ub.projection = math::createPerspective(math::radians(70.0f), (f32)renderer->swapchain->extent.width/(f32)renderer->swapchain->extent.height, 0.1f, 100.0f);

		{
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(renderer->commandBuffers[frameCounter], 0, 1, &renderer->texturedCubeVertexBuffer.buffer, offsets);
		}

		memcpy(renderer->uniformBuffers[frameCounter].mappedData, &ub, sizeof(ub));

		voxelArray.groups[0].rotation = math::createQuaternionRotation(fmodf((float)glfwGetTime(), TAU32), math::Vector3{ 1.0f, 0.0f, 0.0f });

		voxelArray.groups[1].rotation = math::createQuaternionRotation(fmodf((float)glfwGetTime(), TAU32), math::Vector3{ 0.0f, 1.0f, 0.0f });

		voxelArray.groups[2].rotation = math::createQuaternionRotation(fmodf((float) glfwGetTime(), TAU32), math::Vector3{ 1.0f, 0.0f, 1.0f }.normalize());

		gpuObjectData.count = 0;

		for (i32 i = 0; i < voxelArray.voxelsCount; i++) {
			math::Vector3 worldPosition = math::Vector3{ (f32)voxelArray.voxelsPosition[i].x, (f32)voxelArray.voxelsPosition[i].y, (f32)voxelArray.voxelsPosition[i].z }.scale(voxelUnitsToWorldUnits);

			math::Matrix4 rotationMatrix = math::initIdentityMatrix();
			RGBAColorF32 color = voxelArray.colors[i];
			if (selectedVoxelIndex == i) {
				math::Vector3 cursorRayPoint = cursorRay.origin.add(cursorRay.direction.scale(cursorRayHitDist));
				if (voxelArray.voxelsGroupIndex[i] >= 0) {
					VoxelGroup* g = &voxelArray.groups[voxelArray.voxelsGroupIndex[i]];
					g->worldPosition = g->worldPosition.add(cursorRayPoint.sub(cursorRayHitPoint).scale(1.0f / voxelUnitsToWorldUnits));

				}
				cursorRayHitPoint = cursorRayPoint;
				color.r = 0.5f * (color.r + selectedVoxelColorBlend.r);
				color.g = 0.5f * (color.g + selectedVoxelColorBlend.g);
				color.b = 0.5f * (color.b + selectedVoxelColorBlend.b);
				color.a = 0.5f * (color.a + selectedVoxelColorBlend.a);
			}

			if (voxelArray.voxelsGroupIndex[i] >= 0) {
				//if part of a group, the voxel's world position is now relative to the group's world position.
				VoxelGroup* group = &voxelArray.groups[voxelArray.voxelsGroupIndex[i]];
				worldPosition = math::rotateVector(worldPosition, group->rotation);
				worldPosition = worldPosition.add(group->worldPosition.scale(voxelUnitsToWorldUnits));
				rotationMatrix = math::createRotationMatrix(group->rotation);
			}
			math::Matrix4 model = math::translateMatrix(math::initIdentityMatrix(), worldPosition);
			model = model.multiply(rotationMatrix);

			model = math::scaleMatrix(model, math::Vector3{ (f32)voxelArray.voxelsScale[i].x, (f32)voxelArray.voxelsScale[i].y, (f32)voxelArray.voxelsScale[i].z }.scale(voxelUnitsToWorldUnits));
			gpuObjectData.models[gpuObjectData.count] = model;
			gpuObjectData.rgbaColors[gpuObjectData.count] = color;
			gpuObjectData.count += 1;
		}


		{ //handle transparent objects
			math::Matrix4 otherModel = gpuObjectData.models[gpuObjectData.count - 1];
			RGBAColorF32 otherColor = gpuObjectData.rgbaColors[gpuObjectData.count - 1];
			gpuObjectData.models[gpuObjectData.count - 1] = gpuObjectData.models[selectedVoxelIndex];
			gpuObjectData.rgbaColors[gpuObjectData.count - 1] = gpuObjectData.rgbaColors[selectedVoxelIndex];
			gpuObjectData.models[selectedVoxelIndex] = otherModel;
			gpuObjectData.rgbaColors[selectedVoxelIndex] = otherColor;
		}

		f32 lineThickness = 0.06125f;
		RGBAColorF32 gridColor = {1.0f, 0.0f, 0.0f, 0.1f};
		for (i32 i = 0; i < voxelGridWidth+1; i++) {
			f32 zLength = -(f32) (voxelGridHeight * voxelGridUnitSize) * voxelUnitsToWorldUnits;
			math::Matrix4 model = math::translateMatrix(math::initIdentityMatrix(), math::Vector3{ (f32) (voxelGridUnitSize * i) * voxelUnitsToWorldUnits , 0.0f, 0.5f * zLength });
			model = math::scaleMatrix(model, math::Vector3{ lineThickness, lineThickness, (f32) zLength });
			gpuObjectData.models[gpuObjectData.count] = model;
			gpuObjectData.rgbaColors[gpuObjectData.count] = gridColor;
			gpuObjectData.count += 1;
		}

		for (i32 i = 0; i < voxelGridHeight+1; i++) {
			f32 columnLength = (f32)(voxelGridWidth * voxelGridUnitSize) * voxelUnitsToWorldUnits;
			math::Matrix4 model = math::translateMatrix(math::initIdentityMatrix(), math::Vector3{ 0.5f * columnLength, 0.0f, -(f32)(voxelGridUnitSize * i) * voxelUnitsToWorldUnits});
			model = math::scaleMatrix(model, math::Vector3{columnLength, lineThickness, lineThickness });
			gpuObjectData.models[gpuObjectData.count] = model;
			gpuObjectData.rgbaColors[gpuObjectData.count] = gridColor;
			gpuObjectData.count += 1;
		}

		if (isCursorRayHit) {
			math::Matrix4 model = math::translateMatrix(math::initIdentityMatrix(), cursorRayHitPoint);
			model = model.multiply(math::createRotationMatrix(cursorRayOrientation));
			model = math::scaleMatrix(model, math::Vector3{0.125f, 0.125f, 0.125f});
			gpuObjectData.models[gpuObjectData.count] = model;
			gpuObjectData.rgbaColors[gpuObjectData.count] = RGBAColorF32{0.7f, 1.0f, 0.0f, 1.0f};
			gpuObjectData.count += 1;
		}


		math::Quaternion cameraOrientation = math::convertEulerAnglesToQuaternionRotation(math::Vector3{ cameraPitch, -cameraYaw, 0.0f });
		math::Matrix4 model = math::initIdentityMatrix();
		model = model.multiply(math::createRotationMatrix(cameraOrientation));
		model = math::scaleMatrix(model, math::Vector3{0.5f, 0.5f, 2.0f});
		gpuObjectData.models[gpuObjectData.count] = model;
		gpuObjectData.rgbaColors[gpuObjectData.count] = RGBAColorF32{1.0f, 0.5f, 0.0f, 1.0f};
		gpuObjectData.count += 1;

		memcpy(renderer->objectTransformBuffers[frameCounter].mappedData, gpuObjectData.models, sizeof(math::Matrix4)*gpuObjectData.count);
		memcpy(renderer->objectColorBuffers[frameCounter].mappedData, gpuObjectData.rgbaColors, sizeof(RGBAColorF32)*gpuObjectData.count);

		vkCmdBindPipeline(renderer->commandBuffers[frameCounter], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->voxelPipeline);

		vkCmdBindDescriptorSets(renderer->commandBuffers[frameCounter], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->voxelPipelineLayout, 0, 1, &renderer->uniformBufferDescriptorSets[frameCounter], 0, nil);
		vkCmdBindDescriptorSets(renderer->commandBuffers[frameCounter], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->voxelPipelineLayout, 1, 1, &renderer->objectDataDescriptorSets[frameCounter], 0, nil);

		{
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(renderer->commandBuffers[frameCounter], 0, 1, &renderer->cubeVertexBuffer.buffer, offsets);
		}

		vkCmdDraw(renderer->commandBuffers[frameCounter], 36, gpuObjectData.count, 0, 0);

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

		if (showImGuiDemoWindow) {
			ImGui::ShowDemoWindow(&showImGuiDemoWindow);
		}
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &showImGuiDemoWindow);      // Edit bools storing our window open/close state

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clearColor); // Edit 3 floats representing a color

			ImGui::InputInt("Voxel Grid Width", &voxelGridWidth);
			ImGui::InputInt("Voxel Grid Height", &voxelGridHeight);
			ImGui::InputInt("Voxel Grid Cell Size", &voxelGridUnitSize);

			voxelGridWidth = MIN(MAX(1, voxelGridWidth), 1024);
			voxelGridHeight = MIN(MAX(1, voxelGridHeight), 1024);
			voxelGridUnitSize = MIN(MAX(1, voxelGridUnitSize), maxVoxelGridUnitSize);

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(drawData, renderer->commandBuffers[frameCounter]);

		vkCmdEndRenderPass(renderer->commandBuffers[frameCounter]);

		if (vkEndCommandBuffer(renderer->commandBuffers[frameCounter]) != VK_SUCCESS) {
			printf("unable to record command buffer!\n");
			return 1;
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &renderer->imageAvailableSemaphores[frameCounter];
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &renderer->commandBuffers[frameCounter];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderer->renderFinishedSemaphores[frameCounter];

		VkResult submitResult = vkQueueSubmit(renderer->graphicsQueue, 1, &submitInfo, renderer->inFlightFences[frameCounter]);
		if (submitResult != VK_SUCCESS) {
			printf("unable to submit to queue!\n");
			return 1;
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderer->renderFinishedSemaphores[frameCounter];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &renderer->swapchain->handle;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nil;

		result = vkQueuePresentKHR(renderer->presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			result = handleRenderResizing(renderer);
			if (result != VK_SUCCESS) {
				printf("unable to create swapchain!\n");
				return 1;	
			}
			continue;
		}
	}

	vkDeviceWaitIdle(renderer->device);

	glfwTerminate();
	return 0;
}
