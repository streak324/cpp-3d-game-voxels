#pragma once

#ifndef VOXEL_GAME_RENDERER_H
#define VOXEL_GAME_RENDERER_H

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "math.h"

#include "memory.h"

struct PositionColorTextureVertex {
	f32 position[3];
	f32 color[4];
	f32 textureCoordinates[2];
};
struct PositionVertex {
	f32 position[3];
};
const u32 MAX_FRAMES_IN_FLIGHT = 2;

struct Swapchain {
	VkSwapchainKHR handle;
	u32 minImageCount;
	u32 imageCount;
	VkImage *images;
	VkImageView *imageViews;
	VkFramebuffer *framebuffers;
	VkExtent2D extent;
};

struct Buffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkResult createResult;
	VkDeviceSize size;
	void * mappedData;
};

struct UniformBufferData {
	math::Matrix4 view;
	math::Matrix4 projection;
};

struct GPUObjectData {
	math::Matrix4* models;
	RGBAColorF32* rgbaColors;
	u32 count;
};

struct TexturePushConstants {
	i32 imageIndex;
};


struct Image {
	VkImage image;
	VkImageView imageView;
	VkDeviceMemory memory;
	VkExtent3D extent;
};

struct Renderer {
	GLFWwindow* window;

	VkInstance instance;
	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;

	VkDevice device;

	u32 queueFamilyIndicesCount;
	u32* queueFamilyIndices;

	bool32 isUsingSameQueueForGraphicsAndPresent;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	Swapchain* swapchain;
	VkRenderPass renderPass;
	Image depthImage;

	VkDescriptorSetLayout uniformBufferDescriptorSetLayout;
	VkDescriptorSetLayout objectDataDescriptorSetLayout;
	VkDescriptorSetLayout texturesSetLayout;

	VkPipeline texturePipeline;
	VkPipelineLayout texturePipelineLayout;

	VkPipeline voxelPipeline;
	VkPipelineLayout voxelPipelineLayout;

	VkCommandPool commandPool;

	Buffer stagingBuffer;
	Buffer texturedCubeVertexBuffer;
	Buffer cubeVertexBuffer;

	VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];

	VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

	Buffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
	Buffer objectTransformBuffers[MAX_FRAMES_IN_FLIGHT];
	Buffer objectColorBuffers[MAX_FRAMES_IN_FLIGHT];

	VkDescriptorSet uniformBufferDescriptorSets[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSet objectDataDescriptorSets[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSet textureDescriptorSets[MAX_FRAMES_IN_FLIGHT];
};


VkResult createSwapchainAndRenderPass(
	GLFWwindow* window,
	VkPhysicalDevice physicalDevice,
	VkDevice device,
	VkSurfaceKHR surface,
	u32* queueFamilyIndices,
	u32 queueFamilyIndicesCount,
	bool32 isUsingSameQueueForGraphicsAndPresent,
	VkRenderPass* renderPass,
	Swapchain* swapchain,
	Image* depthImage
);


VkResult handleRenderResizing(Renderer* renderer);
void loadTextureImage(const char* filepath, Renderer* renderer, Image* textureImage);
int initRenderer(Renderer* renderer, GLFWwindow* window, MemoryAllocator* memoryAllocator);

#endif
