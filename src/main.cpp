#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

//#include "glm/glm.hpp"
//#include "glm/gtc/matrix_transform.hpp";
//#include "glm/gtc/type_ptr.hpp";
#include "common.h"
#include "math.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

bool framebufferResized = false;
struct PositionColorTextureVertex {
	f32 position[3];
	f32 color[4];
	f32 textureCoordinates[2];
};
const u32 MAX_FRAMES_IN_FLIGHT = 2;

struct Swapchain {
	VkSwapchainKHR handle;
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

struct GPUViewProjectionData {
	//TODO: remove model
	math::Matrix4 model;
	math::Matrix4 view;
	math::Matrix4 projection;
};

struct GPUObjectData {
	math::Matrix4 model;
};

const int MAX_OBJECTS = 10000;

inline void vkCheck(VkResult result) {
	_assert(result == VK_SUCCESS);
}

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	framebufferResized = true;
}

u32 findMemoryType(VkPhysicalDeviceMemoryProperties memoryProperties, u32 memoryTypeBits, VkMemoryPropertyFlags desiredMemoryPropertyFlags) {
	u32 memoryTypeIndex = 0;
	for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((memoryTypeBits & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & desiredMemoryPropertyFlags) == desiredMemoryPropertyFlags) {
			memoryTypeIndex = i;
			return memoryTypeIndex;
		}
	}
}

VkResult createShaderFromFile(VkDevice device , const char * shaderFilePath, VkShaderModule * shaderModule) {
	FILE * shaderFile;
	//TODO: fopen_s won't work with gcc
	if (fopen_s(&shaderFile, shaderFilePath, "rb") != 0 ) {
		if (shaderFile == nil) {
			printf("Error opening file: %s\n", shaderFilePath);
			return VK_ERROR_UNKNOWN;
		}
	}

	fseek(shaderFile, 0i64, SEEK_END);
	i64 shaderFileSize = ftell(shaderFile);
	rewind(shaderFile);
	u8 * shaderData = (u8 *) _malloca(shaderFileSize * sizeof(u8));

	i64 bytesRead = fread(shaderData, sizeof(u8), shaderFileSize, shaderFile);
	fclose(shaderFile);

	if (bytesRead != shaderFileSize) {
		printf("Error reading from file %s: read %dll bytes. expected %dll\n", shaderFilePath, bytesRead, shaderFileSize);
		return VK_ERROR_UNKNOWN;
	}

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize =  shaderFileSize;
	shaderModuleCreateInfo.pCode = (const u32*) shaderData;
	return vkCreateShaderModule(device, &shaderModuleCreateInfo, nil, shaderModule);
}

Buffer createBuffer(VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags) {
	Buffer buffer;
	buffer.size = size;
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	buffer.createResult = vkCreateBuffer(device, &bufferInfo, nil, &buffer.buffer);
	if (buffer.createResult != VK_SUCCESS) {
		printf("unable to create the buffer!\n");
		return buffer;
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer.buffer, &memoryRequirements);

	VkMemoryPropertyFlags desiredMemoryPropertyFlags = memoryPropertyFlags;
	u32 memoryTypeIndex = 0;
	for (u32 i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & desiredMemoryPropertyFlags) == desiredMemoryPropertyFlags) {
			memoryTypeIndex = i;
			break;
		}
	}

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = memoryTypeIndex;

	buffer.createResult = vkAllocateMemory(device, &memoryAllocInfo, nil, &buffer.memory);
	if (buffer.createResult != VK_SUCCESS) {
		printf("unable to allocate any memory!\n");
		return buffer;
	}

	vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);
	return buffer;
}

struct Image {
	VkImage image;
	VkImageView imageView;
	VkDeviceMemory memory;
	VkExtent3D extent;
};

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer;
	vkCheck(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	return commandBuffer;
}

void endSingleTimeCommands(VkDevice device, VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue) {
	vkCheck(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkCheck(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkDevice device, VkCommandPool commandPool, VkQueue queue)  {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else {
			printf("unsupported layout transition!\n");
			panic();
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nil,
			0, nil,
			1, &barrier
		);

		endSingleTimeCommands(device, commandBuffer, commandPool, queue);
}

VkResult createSwapchainAndRenderPass(
	GLFWwindow *window,
	VkPhysicalDevice physicalDevice,
	VkDevice device, 
	VkSurfaceKHR surface, 
	u32 *queueFamilyIndices,
	u32 queueFamilyIndicesCount,
	bool32 isUsingSameQueueForGraphicsAndPresent,
	VkRenderPass *renderPass,
	Swapchain *swapchain,
	Image *depthImage
) {
	vkDeviceWaitIdle(device);
	framebufferResized = false;
	VkResult result;

	if (swapchain == nil) {
		printf("swapchain is null\n");
		return VK_ERROR_UNKNOWN;
	}

	if (swapchain->handle != VK_NULL_HANDLE) {
		for (u32 i = 0; i < swapchain->imageCount; i++) {
			vkDestroyFramebuffer(device, swapchain->framebuffers[i], nil);
			vkDestroyImageView(device, swapchain->imageViews[i], nil);
		}
		vkDestroySwapchainKHR(device, swapchain->handle, nil);

		free(swapchain->framebuffers);
		free(swapchain->imageViews);
		free(swapchain->images);
		swapchain->handle = VK_NULL_HANDLE;
	}

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

	u32 availableSurfaceFormatsCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &availableSurfaceFormatsCount, nil);
	if (!availableSurfaceFormatsCount) {
		printf("physical device does not support any surface formats!");
		return VK_ERROR_UNKNOWN;
	}
	VkSurfaceFormatKHR * availableSurfaceFormats = (VkSurfaceFormatKHR *) _malloca(availableSurfaceFormatsCount * sizeof(VkSurfaceFormatKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &availableSurfaceFormatsCount, availableSurfaceFormats);	

	VkSurfaceFormatKHR desiredSurfaceFormat; 
	u8 foundDesiredSurfaceFormat = 0;
	for (u32 i = 0; i < availableSurfaceFormatsCount; i++) {
		if (availableSurfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB && availableSurfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			desiredSurfaceFormat = availableSurfaceFormats[i];
			foundDesiredSurfaceFormat = 1;
			break;
		}
	}
	if (!foundDesiredSurfaceFormat) {
		printf("did not find the desired surface format.\n");
		return VK_ERROR_UNKNOWN;
	}

	VkFormat swapchainImageFormat = desiredSurfaceFormat.format;

	u32 presentModesCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, nil);
	if (!presentModesCount) {
		printf("physical device does not support any present modes!");
		return VK_ERROR_UNKNOWN;
	}
	VkPresentModeKHR * presentModes = (VkPresentModeKHR *) _malloca(presentModesCount * sizeof(VkSurfaceFormatKHR));
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, nil);

	// VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available
	VkPresentModeKHR desiredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (u32 i = 0; i < presentModesCount; i++) {
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			desiredPresentMode = presentModes[i];
		}
	}

	if (surfaceCapabilities.currentExtent.width ==  UINT32_MAX) {
		i32 width, height;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}
		swapchain->extent.width = max(min((u32) width, surfaceCapabilities.maxImageExtent.width), surfaceCapabilities.minImageExtent.width);
		swapchain->extent.height = max(min((u32) height, surfaceCapabilities.maxImageExtent.height), surfaceCapabilities.minImageExtent.height);
	} else {
		swapchain->extent = surfaceCapabilities.currentExtent;
	}

	u32 minImageCount = 3;
	if (surfaceCapabilities.maxImageCount > 0) {
		minImageCount = max(minImageCount, surfaceCapabilities.maxImageCount);
	}

	if (depthImage->image != VK_NULL_HANDLE) {
		vkDestroyImageView(device, depthImage->imageView, nil);
		vkDestroyImage(device, depthImage->image, nil);
		vkFreeMemory(device, depthImage->memory, nil);
	}

	VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

	VkImageCreateInfo depthImageInfo = {};
	depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageInfo.pNext = nil;
	depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageInfo.format = depthFormat;
	depthImageInfo.extent = VkExtent3D{
		swapchain->extent.width,
		swapchain->extent.height,
		1
	};
	depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImageInfo.mipLevels = 1;
	depthImageInfo.arrayLayers = 1;
	depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	result = vkCreateImage(device, &depthImageInfo, nil, &depthImage->image);
	if (result != VK_SUCCESS) {
		printf("unable to create depth image!\n");
		return result;
	}

	VkMemoryRequirements depthImageMemoryRequirements;
	vkGetImageMemoryRequirements(device, depthImage->image, &depthImageMemoryRequirements);


	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	VkMemoryAllocateInfo depthImageMemoryAllocInfo = {};
	depthImageMemoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	depthImageMemoryAllocInfo.allocationSize = depthImageMemoryRequirements.size;
	depthImageMemoryAllocInfo.memoryTypeIndex = findMemoryType(memoryProperties, depthImageMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	result = vkAllocateMemory(device, &depthImageMemoryAllocInfo, nil, &depthImage->memory);
	if (result != VK_SUCCESS) {
		printf("unable to alloate memory for depth image!\n");
		return result;
	}

	vkBindImageMemory(device, depthImage->image, depthImage->memory, 0);

	VkImageViewCreateInfo depthImageViewInfo = {};
	depthImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthImageViewInfo.pNext = nil;
	depthImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthImageViewInfo.image = depthImage->image;
	depthImageViewInfo.format = depthFormat;
	depthImageViewInfo.subresourceRange.baseMipLevel = 0;
	depthImageViewInfo.subresourceRange.levelCount = 1;
	depthImageViewInfo.subresourceRange.baseArrayLayer = 0;
	depthImageViewInfo.subresourceRange.layerCount = 1;
	depthImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	result = vkCreateImageView(device, &depthImageViewInfo, nil, &depthImage->imageView);
	if (result != VK_SUCCESS) {
		printf("unable to create depth image view!\n");
		return result;
	}

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.flags = 0;
	depthAttachment.format = depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency subpassDependency = {};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;

	if (vkCreateRenderPass(device, &renderPassCreateInfo, nil, renderPass) != VK_SUCCESS) {
		printf("unable to create render pass!\n");
		return VK_ERROR_UNKNOWN;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = minImageCount;
	swapchainCreateInfo.imageFormat = desiredSurfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = desiredSurfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = swapchain->extent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if (isUsingSameQueueForGraphicsAndPresent) {
		//TODO: support exclusive mode
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nil;
	} else {
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = queueFamilyIndicesCount;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}

	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = desiredPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = swapchain->handle;

	result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nil, &swapchain->handle);
	if (result != VK_SUCCESS) {
		printf("unable to create swapchain!\n");
		return result;
	}

	vkGetSwapchainImagesKHR(device, swapchain->handle, &swapchain->imageCount, nil);
	swapchain->images = (VkImage *) malloc(swapchain->imageCount * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain->handle, &swapchain->imageCount, swapchain->images);

	swapchain->imageViews = (VkImageView *) malloc(swapchain->imageCount * sizeof(VkImageView));
	swapchain->framebuffers = (VkFramebuffer *) malloc(swapchain->imageCount * sizeof(VkImageView));

	for (u32 i = 0; i < swapchain->imageCount; i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchain->images[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = desiredSurfaceFormat.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		vkCreateImageView(device, &createInfo, nil, &swapchain->imageViews[i]);

		VkImageView attachments[2] = {
			swapchain->imageViews[i],
			depthImage->imageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = *renderPass;
		framebufferCreateInfo.attachmentCount = 2;
		framebufferCreateInfo.pAttachments = attachments;
		framebufferCreateInfo.width = swapchain->extent.width;
		framebufferCreateInfo.height = swapchain->extent.height;
		framebufferCreateInfo.layers = 1;

		VkResult framebufferCreateResult = vkCreateFramebuffer(device, &framebufferCreateInfo, nil, &swapchain->framebuffers[i]);
		if (framebufferCreateResult != VK_SUCCESS) {
			printf("unable to create framebuffer for index %d\n", i);
			return framebufferCreateResult;
		}
	}

	return VK_SUCCESS;
}

int main(void) {
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
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_API_VERSION_1_3;
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_API_VERSION_1_3;
	appInfo.apiVersion = VK_API_VERSION_1_3;

	u32 glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	u32 requiredExtensionCount = 0;
	const u32 requiredExtensionCapacity = 1024;
	const char * requiredExtensions [requiredExtensionCapacity] = {
		//mac os thing
		//VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
	};
	_assert(glfwExtensionCount + requiredExtensionCount < requiredExtensionCapacity);
	for (u32 i=0; i < glfwExtensionCount; i++) {
		requiredExtensions[i+requiredExtensionCount] = glfwExtensions[i];
	}
	requiredExtensionCount += glfwExtensionCount;

	u32 extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nil, &extensionCount, nil);

	VkExtensionProperties * extensions = (VkExtensionProperties*) _malloca(extensionCount * sizeof(VkExtensionProperties));
	const char* *extensionNames = (const char**) _malloca(extensionCount * sizeof(char*));
	vkEnumerateInstanceExtensionProperties(nil, &extensionCount, extensions);

	for (u32 i=0; i < extensionCount; i++) {
		printf("extension: %s\n", extensions[i].extensionName);
		extensionNames[i] = extensions[i].extensionName;
	}


	for (u32 i=0; i < requiredExtensionCount; i++) {
		printf("required extension %s\n", requiredExtensions[i]);
		u8 found = 0;
		for (u32 j=0; j < extensionCount; j++) {
			if (strcmp(requiredExtensions[i], extensionNames[j]) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			printf("extension %s is not supported in this device!\n", requiredExtensions[i]);
			return 1;
		}
	}

	u32 validationLayerCount = 1;
	const char * validationLayers [1] = {
		"VK_LAYER_KHRONOS_validation",
	};

	#ifdef NDEBUG
		const u8 enableValidationLayers = 0;
	#else
		const u8 enableValidationLayers = 1;
	#endif

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	//mac os thing
	//instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	instanceCreateInfo.flags = 0;

	instanceCreateInfo.enabledExtensionCount = extensionCount;
	instanceCreateInfo.ppEnabledExtensionNames = extensionNames;
	instanceCreateInfo.enabledLayerCount = 0;

	if (enableValidationLayers) {
		u32 layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nil);
		VkLayerProperties * availableLayers = (VkLayerProperties *) _malloca(layerCount * sizeof(VkLayerProperties));
		for (u32 i = 0; i < validationLayerCount; i++) {
			u8 found = 0;
			for (u32 j = 0; j < layerCount; j++) {
				if (strcmp(validationLayers[i], availableLayers[j].layerName) == 0) {
					printf("validation layer %s is not supported", validationLayers[i]);
					return 1;
				}
			}
		}
		instanceCreateInfo.enabledLayerCount = validationLayerCount;
		instanceCreateInfo.ppEnabledLayerNames = validationLayers;
	}

	VkInstance instance;

	if(vkCreateInstance(&instanceCreateInfo, nil, &instance) != VK_SUCCESS) {
		fprintf(stderr, "failed to create instance!\n");
		return 1;
	}

	VkSurfaceKHR surface;
	VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {};
	win32SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	win32SurfaceCreateInfo.hwnd = glfwGetWin32Window(window);
	win32SurfaceCreateInfo.hinstance = GetModuleHandle(nil);
	if (vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, nil, &surface) != VK_SUCCESS) {
		printf("failed to create win32 surface!");
		return 1;
	}

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	u32 deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nil);
	if (deviceCount == 0) {
		printf("there are no found physical devices with Vulkan support!");
		return 1;
	}

	VkPhysicalDevice * availableDevices = (VkPhysicalDevice *) _malloca(deviceCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(instance, &deviceCount, availableDevices);

	u32 pickedDeviceIndex = -1;
	for (;;) {
		printf("Pick GPU to use from %d to %d:\n", 1, deviceCount);
		for (u32 i = 0; i < deviceCount; i++) {
			VkPhysicalDeviceProperties physicalDeviceProperties;
			vkGetPhysicalDeviceProperties(availableDevices[i], &physicalDeviceProperties);
			printf("[%d] - %s\n",  i+1, physicalDeviceProperties.deviceName);
		}
		printf("Choice: ");

		const u32 userInputBufferSize = 32;
		char userInput[userInputBufferSize] = {};
		fgets(userInput, userInputBufferSize, stdin);

		pickedDeviceIndex = atoi(userInput) - 1;
		if (pickedDeviceIndex >= 0 && pickedDeviceIndex < deviceCount) {
			break;
		}
	}

	physicalDevice = availableDevices[pickedDeviceIndex];

	if (physicalDevice == VK_NULL_HANDLE) {
		printf("failed to find a suitable GPU\n");
		return 1;
	}


	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	u32	queueFamilyCount = 0;

	bool foundGraphicsQueueFamily = false;
	u32 graphicsQueueFamilyIndex = 0;

	bool foundPresentQueueFamily = 0;
	u32 presentQueueFamilyIndex = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nil);
	VkQueueFamilyProperties * queueFamilyProperties = (VkQueueFamilyProperties *) _malloca(queueFamilyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties);
	printf("%d queue families\n", queueFamilyCount);
	for (u32 i = 0; i < queueFamilyCount; i++) {
		if (!foundGraphicsQueueFamily && queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			printf("queue family %d supports graphics operations\n", i);
			graphicsQueueFamilyIndex = i;
			foundGraphicsQueueFamily = true;
		}

		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
		if (presentSupport) {
			printf("queue family %d supports presentation operations\n", i);
			if (!foundPresentQueueFamily || (foundGraphicsQueueFamily && graphicsQueueFamilyIndex != presentQueueFamilyIndex)) {
				presentQueueFamilyIndex = i;
				foundPresentQueueFamily = true;
			}
		}
	}

	if (!foundGraphicsQueueFamily) {
		printf("device doesn't support graphics operations!!!\n");
		return 1;
	}

	if (!foundPresentQueueFamily) {
		printf("device doesn't support present operations!!!\n");
		return 1;
	}

	u8 isUsingSameQueueForGraphicsAndPresent = graphicsQueueFamilyIndex == presentQueueFamilyIndex;

	VkDeviceQueueCreateInfo queueCreateInfos [3] = {};

	u32 queueCreateInfoCount = 1;
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		f32 queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos[0] = queueCreateInfo;
	}

	if (isUsingSameQueueForGraphicsAndPresent) {
		printf("queue family %d supports both graphics and presentation!\n", graphicsQueueFamilyIndex);
	} else {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = presentQueueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		f32 queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos[1] = queueCreateInfo;
		queueCreateInfoCount += 1;
	}

	//TODO: support separate transfer queues

	u32 requiredDeviceExtensionsCount = 1;
	const char * requiredDeviceExtensions [1] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	u32 availableDeviceExtensionsCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nil, &availableDeviceExtensionsCount, nil);
	VkExtensionProperties * availableDeviceExtensions = (VkExtensionProperties *) _malloca(availableDeviceExtensionsCount * sizeof(VkExtensionProperties));
	vkEnumerateDeviceExtensionProperties(physicalDevice, nil, &availableDeviceExtensionsCount, availableDeviceExtensions);

	for (u32 i = 0; i < requiredDeviceExtensionsCount; i++) {
		u8 found = 0;
		for (u32 j = 0; j < availableDeviceExtensionsCount; j++) {
			if (strcmp(requiredDeviceExtensions[i], availableDeviceExtensions[j].extensionName) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			printf("device does not support the required device extension: %s\n", requiredDeviceExtensions[i]);
			return 1;
		}
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
	deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = requiredDeviceExtensionsCount;
	deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
	deviceCreateInfo.enabledLayerCount = 0;

	if (enableValidationLayers) {
		deviceCreateInfo.enabledLayerCount = validationLayerCount;
		deviceCreateInfo.ppEnabledLayerNames = validationLayers;
	}

	VkDevice device;
	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nil, &device) != VK_SUCCESS) {
		printf("unable to create logical device!");
		return 1;
	}

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

	if (isUsingSameQueueForGraphicsAndPresent) {
		presentQueue = graphicsQueue;
	} else {
		vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
	}

	const u32 queueFamilyIndicesCount = 2;
	u32 queueFamilyIndices [queueFamilyIndicesCount] =  {
		graphicsQueueFamilyIndex,
		presentQueueFamilyIndex,
	};

	Swapchain swapchain = {}; 
	VkRenderPass renderPass;
	Image depthImage = {};

	if (createSwapchainAndRenderPass(
			window,
			physicalDevice,
			device, 
			surface, 
			queueFamilyIndices,
			queueFamilyIndicesCount,
			isUsingSameQueueForGraphicsAndPresent,
			&renderPass,
			&swapchain,
			&depthImage) != VK_SUCCESS
		) 
	{
		printf("unable to create swapchain!\n");
		return 1;	
	}

	const char * vertexShaderFilePath = "./spir-v/shader.vert.spv";
	VkShaderModule vertexShaderModule;
	if(createShaderFromFile(device , vertexShaderFilePath, &vertexShaderModule) != VK_SUCCESS) {
		printf("unable to create vertex shader module!\n");
		return 1;
	}

	const char * fragmentShaderFilePath = "./spir-v/shader.frag.spv";
	VkShaderModule fragmentShaderModule;
	if(createShaderFromFile(device , fragmentShaderFilePath, &fragmentShaderModule) != VK_SUCCESS) {
		printf("unable to create fragment shader module!\n");
		return 1;
	}

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[] = {
		vertexShaderStageCreateInfo,
		fragmentShaderStageCreateInfo
	};

	const u32 numDynamicStates = 2;
	VkDynamicState dynamicStates [numDynamicStates] =  {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = 2;
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(PositionColorTextureVertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription positionColorAttributeDescriptions[3] = {};

	positionColorAttributeDescriptions[0].binding = 0;
	positionColorAttributeDescriptions[0].location = 0;
	positionColorAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	positionColorAttributeDescriptions[0].offset = offsetof(PositionColorTextureVertex, position);

	positionColorAttributeDescriptions[1].binding = 0;
	positionColorAttributeDescriptions[1].location = 1;
	positionColorAttributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	positionColorAttributeDescriptions[1].offset = offsetof(PositionColorTextureVertex, color);

	positionColorAttributeDescriptions[2].binding = 0;
	positionColorAttributeDescriptions[2].location = 2;
	positionColorAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	positionColorAttributeDescriptions[2].offset = offsetof(PositionColorTextureVertex, textureCoordinates);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 3;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = positionColorAttributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	//rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	//rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = false;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo = {};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingStateCreateInfo.minSampleShading = 1.0f;
	multisamplingStateCreateInfo.pSampleMask = nil;
	multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendingState = {};
	colorBlendingState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingState.logicOpEnable = VK_FALSE;
	colorBlendingState.logicOp = VK_LOGIC_OP_COPY;
	colorBlendingState.attachmentCount = 1;
	colorBlendingState.pAttachments = &colorBlendAttachment;
	colorBlendingState.blendConstants[0];
	colorBlendingState.blendConstants[1];
	colorBlendingState.blendConstants[2];
	colorBlendingState.blendConstants[3];


	VkDescriptorSetLayoutBinding uniformBufferLayoutBinding = {};
	uniformBufferLayoutBinding.binding = 0;
	uniformBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferLayoutBinding.descriptorCount = 1;
	uniformBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uniformBufferLayoutBinding.pImmutableSamplers = nil;


	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nil;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings [] = { uniformBufferLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = sizeof(bindings)/sizeof(bindings[0]);
	layoutInfo.pBindings = bindings;

	VkDescriptorSetLayout uniformBufferDescriptorSetLayout;

	vkCheck(vkCreateDescriptorSetLayout(device, &layoutInfo, nil, &uniformBufferDescriptorSetLayout));

	VkDescriptorSetLayoutBinding objectDataBinding = {};
	objectDataBinding.binding = 0;
	objectDataBinding.descriptorCount = 1;
	objectDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectDataBinding.pImmutableSamplers = nil;
	objectDataBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo objectDataLayoutInfo = {};
	objectDataLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	objectDataLayoutInfo.bindingCount = 1;
	objectDataLayoutInfo.flags = 0;
	objectDataLayoutInfo.pNext = nil;
	objectDataLayoutInfo.pBindings = &objectDataBinding;

	VkDescriptorSetLayout objectDataDescriptorSetLayout;
	vkCheck(vkCreateDescriptorSetLayout(device, &objectDataLayoutInfo, nil, &objectDataDescriptorSetLayout));

	VkDescriptorSetLayout setLayouts[] = { uniformBufferDescriptorSetLayout, objectDataDescriptorSetLayout };

	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = sizeof(setLayouts)/sizeof(setLayouts[0]);
	pipelineLayoutCreateInfo.pSetLayouts = setLayouts;

	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nil, &pipelineLayout) != VK_SUCCESS) {
		printf("unable to create pipeline layout!\n");
		return 1;
	}

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilInfo = {};
	pipelineDepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilInfo.depthTestEnable = VK_TRUE;
	pipelineDepthStencilInfo.depthWriteEnable = VK_TRUE;
	pipelineDepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	pipelineDepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	pipelineDepthStencilInfo.minDepthBounds = 0.0f;
	pipelineDepthStencilInfo.maxDepthBounds = 1.0f;
	pipelineDepthStencilInfo.stencilTestEnable = VK_FALSE;
	pipelineDepthStencilInfo.front = {};
	pipelineDepthStencilInfo.back = {};


	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};

	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = pipelineShaderStageCreateInfos;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingState;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilInfo;

	VkPipeline graphicsPipeline;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nil, &graphicsPipeline) != VK_SUCCESS) {
		printf("unable to create graphics pipeline!\n");
		return 1;
	}

	VkCommandPool commandPool;

	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	if (vkCreateCommandPool(device, &commandPoolInfo, nil, &commandPool) != VK_SUCCESS) {
		printf("unable to create command pool!\n");
		return 1;
	}

	PositionColorTextureVertex cubeVertices[36] = {
		// 3D Position, 4D Color
		//{ {1.0f, 1.0f, -1.0f }, { 1.0, 1.0, 1.0, 1.0 } },
		//{ {1.0f, -1.0f, -1.0f }, { 0.0, 1.0, 0.0, 1.0 } },
		//{ {1.0f, 1.0f, 1.0f }, { 0.0, 0.0, 1.0, 1.0 } },
		//{ {1.0f, -1.0f, 1.0f }, { 1.0, 0.0, 0.0, 1.0 } },

		//{ {-1.0f, 1.0f, -1.0f }, { 1.0, 1.0, 1.0, 1.0 } },
		//{ {-1.0f, -1.0f, -1.0f }, { 0.0, 1.0, 0.0, 1.0 } },
		//{ {-1.0f, 1.0f, 1.0f }, { 0.0, 0.0, 1.0, 1.0 } },
		//{ {-1.0f, -1.0f, 1.0f }, { 1.0, 0.0, 0.0, 1.0 } }

		//front
		{ { -0.5f, -0.5f,  0.5f, }, { 1.0f, 1.0f, 1.0f, 1.0f, }, {  0.0f, 0.0f, } },
		{ { 0.5f, -0.5f,  0.5f,  }, { 1.0f, 1.0f, 1.0f, 1.0f, }, { 1.0f, 0.0f, } },
		{ { 0.5f,  0.5f,  0.5f,  }, { 1.0f, 1.0f, 1.0f, 1.0f, }, { 1.0f, 1.0f, } },
		{ { 0.5f,  0.5f,  0.5f,  }, { 1.0f, 1.0f, 1.0f, 1.0f, }, { 1.0f, 1.0f, } },
		{ { -0.5f,  0.5f,  0.5f, }, { 1.0f, 1.0f, 1.0f, 1.0f, }, {  0.0f, 1.0f, } },
		{ { -0.5f, -0.5f,  0.5f, }, { 1.0f, 1.0f, 1.0f, 1.0f, }, {  0.0f, 0.0f, } },

		//back
		{ { -0.5f, -0.5f, -0.5f, }, { 1.0f, 0.0f, 0.0f, 1.0f },  {  0.0f, 0.0f, } },
		{ { -0.5f,  0.5f, -0.5f, }, { 1.0f, 0.0f, 0.0f, 1.0f, }, {  0.0f, 1.0f, } },
		{ { 0.5f,  0.5f, -0.5f,  }, { 1.0f, 0.0f, 0.0f, 1.0f, }, { 1.0f, 1.0f, } },
		{ { 0.5f,  0.5f, -0.5f,  }, { 1.0f, 0.0f, 0.0f, 1.0f, }, { 1.0f, 1.0f, } },
		{ { 0.5f, -0.5f, -0.5f,  }, { 1.0f, 0.0f, 0.0f, 1.0f, }, { 1.0f, 0.0f, } },
		{ { -0.5f, -0.5f, -0.5f, }, { 1.0f, 0.0f, 0.0f, 1.0f, }, {  0.0f, 0.0f, } },

		//left side
		{ { -0.5f,  0.5f,  0.5f, }, { 0.0f, 1.0f, 0.0f, 1.0f, }, {  1.0f, 0.0f, } },
		{ { -0.5f,  0.5f, -0.5f, }, { 0.0f, 1.0f, 0.0f, 1.0f, }, {  1.0f, 1.0f, } },
		{ { -0.5f, -0.5f, -0.5f, }, { 0.0f, 1.0f, 0.0f, 1.0f, }, {  0.0f, 1.0f, } },
		{ { -0.5f, -0.5f, -0.5f, }, { 0.0f, 1.0f, 0.0f, 1.0f, }, {  0.0f, 1.0f, } },
		{ { -0.5f, -0.5f,  0.5f, }, { 0.0f, 1.0f, 0.0f, 1.0f, }, {  0.0f, 0.0f, } },
		{ { -0.5f,  0.5f,  0.5f, }, { 0.0f, 1.0f, 0.0f, 1.0f, }, {  1.0f, 0.0f, } },

		//right side
		{ { 0.5f,  0.5f,  0.5f,  }, { 0.0f, 0.0f, 1.0f, 1.0f, }, { 1.0f, 0.0f, } },
		{ { 0.5f, -0.5f,  0.5f,  }, { 0.0f, 0.0f, 1.0f, 1.0f, }, { 0.0f, 0.0f, } },
		{ { 0.5f, -0.5f, -0.5f,  }, { 0.0f, 0.0f, 1.0f, 1.0f, }, { 0.0f, 1.0f, } },
		{ { 0.5f, -0.5f, -0.5f,  }, { 0.0f, 0.0f, 1.0f, 1.0f, }, { 0.0f, 1.0f, } },
		{ { 0.5f,  0.5f, -0.5f,  }, { 0.0f, 0.0f, 1.0f, 1.0f, }, { 1.0f, 1.0f, } },
		{ { 0.5f,  0.5f,  0.5f,  }, { 0.0f, 0.0f, 1.0f, 1.0f, }, { 1.0f, 0.0f, } },

		//bottom side
		{ { -0.5f, -0.5f, -0.5f, }, { 1.0f, 1.0f, 0.0f, 1.0f, }, {  0.0f, 1.0f, } },
		{ { 0.5f, -0.5f, -0.5f,  }, { 1.0f, 1.0f, 0.0f, 1.0f, }, { 1.0f, 1.0f, } },
		{ { 0.5f, -0.5f,  0.5f,  }, { 1.0f, 1.0f, 0.0f, 1.0f, }, { 1.0f, 0.0f, } },
		{ { 0.5f, -0.5f,  0.5f,  }, { 1.0f, 1.0f, 0.0f, 1.0f, }, { 1.0f, 0.0f, } },
		{ { -0.5f, -0.5f,  0.5f, }, { 1.0f, 1.0f, 0.0f, 1.0f, }, {  0.0f, 0.0f, } },
		{ { -0.5f, -0.5f, -0.5f, }, { 1.0f, 1.0f, 0.0f, 1.0f, }, {  0.0f, 1.0f, } },

		//top side
		{ { -0.5f,  0.5f, -0.5f, }, { 0.0f, 1.0f, 1.0f, 1.0f, }, {  0.0f, 1.0f, } },
		{ { -0.5f,  0.5f,  0.5f, }, { 0.0f, 1.0f, 1.0f, 1.0f, }, {  0.0f, 0.0f, } },
		{ { 0.5f,  0.5f,  0.5f,  }, { 0.0f, 1.0f, 1.0f, 1.0f, }, { 1.0f, 0.0f, } },
		{ { 0.5f,  0.5f,  0.5f,  }, { 0.0f, 1.0f, 1.0f, 1.0f, }, { 1.0f, 0.0f, } },
		{ { 0.5f,  0.5f, -0.5f,  }, { 0.0f, 1.0f, 1.0f, 1.0f, }, { 1.0f, 1.0f, } },
		{ { -0.5f,  0.5f, -0.5f, }, { 0.0f, 1.0f, 1.0f, 1.0f, }, {  0.0f, 1.0f } },


	};
	u32 indices[] = {
		6, 7, 3, 3, 2, 6,
		6, 5, 7, 6, 4, 5,
		4, 0, 1, 1, 5, 4,
		1, 0, 2, 2, 3, 1,
		0, 4, 6, 6, 2, 0,
		3, 7, 5, 5, 1, 3,
	};

	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

	Buffer indexBuffer = createBuffer(physicalDeviceMemoryProperties, device, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (indexBuffer.createResult != VK_SUCCESS) {
		printf("failed to create index buffer!!!\n");
		return 1;
	}

	Buffer stagingBuffer = createBuffer(physicalDeviceMemoryProperties, device, 100*1000*1000, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (stagingBuffer.createResult != VK_SUCCESS) {
		printf("failed to create staging buffer!!!\n");
		return 1;
	}

	Buffer vertexBuffer = createBuffer(physicalDeviceMemoryProperties, device, sizeof(cubeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (vertexBuffer.createResult != VK_SUCCESS) {
		printf("failed to create vertex buffer!!!\n");
		return 1;
	}

	VkCommandBufferAllocateInfo copyCmdBufferAllocInfo = {};
	copyCmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	copyCmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	copyCmdBufferAllocInfo.commandPool = commandPool;
	copyCmdBufferAllocInfo.commandBufferCount = 1;
	VkCommandBuffer copyDataCmdBuffer;
	if (vkAllocateCommandBuffers(device, &copyCmdBufferAllocInfo, &copyDataCmdBuffer) != VK_SUCCESS) {
		printf("unable to allocate a copy buffer command buffer!!!\n");
		return 1;
	}

	{
		vkResetCommandBuffer(copyDataCmdBuffer, 0);

		void *data;
		vkMapMemory(device, stagingBuffer.memory, 0, stagingBuffer.size, 0, &data);
		memcpy(data, cubeVertices, sizeof(cubeVertices));
		vkUnmapMemory(device, stagingBuffer.memory);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(copyDataCmdBuffer, &beginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = sizeof(cubeVertices);
		vkCmdCopyBuffer(copyDataCmdBuffer, stagingBuffer.buffer, vertexBuffer.buffer, 1, &copyRegion);
		vkEndCommandBuffer(copyDataCmdBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyDataCmdBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);
	}

	{
		vkResetCommandBuffer(copyDataCmdBuffer, 0);

		void *data;
		vkMapMemory(device, stagingBuffer.memory, 0, stagingBuffer.size, 0, &data);
		memcpy(data, indices, sizeof(indices));
		vkUnmapMemory(device, stagingBuffer.memory);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(copyDataCmdBuffer, &beginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = sizeof(indices);
		vkCmdCopyBuffer(copyDataCmdBuffer, stagingBuffer.buffer, indexBuffer.buffer, 1, &copyRegion);
		vkEndCommandBuffer(copyDataCmdBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyDataCmdBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);
	}

	Buffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
	Buffer objectBuffers[MAX_FRAMES_IN_FLIGHT];
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		uniformBuffers[i] = createBuffer(physicalDeviceMemoryProperties, device, sizeof(GPUViewProjectionData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkCheck(uniformBuffers[i].createResult);
		vkMapMemory(device, uniformBuffers[i].memory, 0, sizeof(GPUViewProjectionData), 0, &uniformBuffers[i].mappedData);

		objectBuffers[i] = createBuffer(physicalDeviceMemoryProperties, device, MAX_OBJECTS*sizeof(GPUObjectData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkCheck(objectBuffers[i].createResult);
		vkMapMemory(device, objectBuffers[i].memory, 0, MAX_OBJECTS*sizeof(GPUObjectData), 0, &objectBuffers[i].mappedData);
	}

	Image textureImage;
	{ //load and create texture image

		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("./assets/textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize=  texWidth * texHeight * 4;
		if (!pixels) {
			printf("failed to load texture image!");
			return 1;
		}

		{
			void *data;
			vkMapMemory(device, stagingBuffer.memory, 0, imageSize, 0, &data);
			memcpy(data, pixels, imageSize);
			vkUnmapMemory(device, stagingBuffer.memory);
			stbi_image_free(pixels);
		}

		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = texWidth;
		imageInfo.extent.height = texHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0;

		vkCheck(vkCreateImage(device, &imageInfo, nil, &textureImage.image));

		textureImage.extent = imageInfo.extent;

		VkMemoryRequirements textureImageMemoryRequirements = {};
		vkGetImageMemoryRequirements(device, textureImage.image, &textureImageMemoryRequirements);
		VkMemoryAllocateInfo textureImageMemoryAllocateInfo = {};
		textureImageMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		textureImageMemoryAllocateInfo.allocationSize = textureImageMemoryRequirements.size;
		textureImageMemoryAllocateInfo.memoryTypeIndex = findMemoryType(physicalDeviceMemoryProperties, textureImageMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkCheck(vkAllocateMemory(device, &textureImageMemoryAllocateInfo, nil, &textureImage.memory));

		vkBindImageMemory(device, textureImage.image, textureImage.memory, 0);

		//TODO: run copy command buffer, and transition layouts asynchronously

		transitionImageLayout(textureImage.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, device, commandPool, graphicsQueue);
		VkCommandBuffer copyCommand = beginSingleTimeCommands(device, commandPool);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0, };
		region.imageExtent = {
			textureImage.extent.width, textureImage.extent.height,
			1
		};

		vkCmdCopyBufferToImage(
			copyCommand,
			stagingBuffer.buffer,
			textureImage.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		endSingleTimeCommands(device, copyCommand, commandPool, graphicsQueue);

		transitionImageLayout(textureImage.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, device, commandPool, graphicsQueue);

		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = textureImage.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		vkCheck(vkCreateImageView(device, &viewInfo, nil, &textureImage.imageView));
	}

	VkSamplerCreateInfo linearFilterSamplerInfo = {};
	linearFilterSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	linearFilterSamplerInfo.magFilter = VK_FILTER_LINEAR; //TODO: make it an option to specify which filter to use
	linearFilterSamplerInfo.minFilter = VK_FILTER_LINEAR;
	linearFilterSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	linearFilterSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	linearFilterSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	linearFilterSamplerInfo.anisotropyEnable = VK_TRUE;
	linearFilterSamplerInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;
	linearFilterSamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	linearFilterSamplerInfo.unnormalizedCoordinates = VK_FALSE;
	linearFilterSamplerInfo.compareEnable = VK_FALSE;
	linearFilterSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	linearFilterSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	linearFilterSamplerInfo.mipLodBias = 0.0f;
	linearFilterSamplerInfo.minLod = 0.0f;
	linearFilterSamplerInfo.maxLod = 0.0f;
	
	VkSampler linearFilterSampler = {};
	vkCheck(vkCreateSampler(device, &linearFilterSamplerInfo, nil, &linearFilterSampler));


	VkDescriptorPoolSize poolSizes [3] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.poolSizeCount = sizeof(poolSizes)/sizeof(poolSizes[0]);
	descriptorPoolInfo.pPoolSizes = poolSizes;
	descriptorPoolInfo.maxSets = 4;//2 for view projection data (uniform buffer), 2 for object data (storage buffer)

	VkDescriptorPool descriptorPool;
	vkCheck(vkCreateDescriptorPool(device, &descriptorPoolInfo, nil, &descriptorPool));


	VkDescriptorSet viewProjectionDataDescriptorSets[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSet objectDataDescriptorSets[MAX_FRAMES_IN_FLIGHT];
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
			descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocateInfo.descriptorPool = descriptorPool;
			descriptorSetAllocateInfo.descriptorSetCount = 1;
			descriptorSetAllocateInfo.pSetLayouts = &uniformBufferDescriptorSetLayout;;
			vkCheck(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &viewProjectionDataDescriptorSets[i]));
		}
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
			descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocateInfo.descriptorPool = descriptorPool;
			descriptorSetAllocateInfo.descriptorSetCount = 1;
			descriptorSetAllocateInfo.pSetLayouts = &objectDataDescriptorSetLayout;
			vkCheck(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &objectDataDescriptorSets[i]));
		}

		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniformBuffers[i].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(GPUViewProjectionData);

		VkWriteDescriptorSet descriptorWrites[3] = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = viewProjectionDataDescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nil;
		descriptorWrites[0].pTexelBufferView = nil;

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImage.imageView;
		imageInfo.sampler = linearFilterSampler;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = viewProjectionDataDescriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		VkDescriptorBufferInfo objectBufferInfo = {};
		objectBufferInfo.buffer = objectBuffers[i].buffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;
		
		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = objectDataDescriptorSets[i];
		descriptorWrites[2].dstBinding = 0;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pBufferInfo = &objectBufferInfo;

		vkUpdateDescriptorSets(device, sizeof(descriptorWrites)/sizeof(descriptorWrites[0]), descriptorWrites, 0, nil);
	}

	VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT] = {};
	VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
	commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocInfo.commandPool = commandPool;
	commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	if (vkAllocateCommandBuffers(device, &commandBufferAllocInfo, commandBuffers) != VK_SUCCESS) {
		printf("unable to allocate command buffers\n");
		return 1;
	}

	VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = {};
	VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT] = {};
	VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT] = {};

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (
			vkCreateSemaphore(device, &semaphoreInfo, nil, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nil, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nil, &inFlightFences[i]) != VK_SUCCESS)
		{
			printf("unable to create semaphore and fences!\n");
			return 1;
		}
	}


	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	u64 frameCounter = -1;
	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window)) {
		frameCounter = (frameCounter + 1) % MAX_FRAMES_IN_FLIGHT;
		/* Poll for and process events */
		glfwPollEvents();


		vkWaitForFences(device, 1, &inFlightFences[frameCounter], VK_TRUE, UINT64_MAX);
		u32 imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapchain.handle, UINT64_MAX, imageAvailableSemaphores[frameCounter], VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			result = createSwapchainAndRenderPass(
				window,
				physicalDevice,
				device, 
				surface, 
				queueFamilyIndices,
				queueFamilyIndicesCount,
				isUsingSameQueueForGraphicsAndPresent,
				&renderPass,
				&swapchain,
				&depthImage);
			if (result != VK_SUCCESS) {
				printf("unable to create swapchain!\n");
				return 1;	
			}
			continue;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			printf("failed to acquire the next swapchain image!\n");
			return 1;
		}
		vkResetFences(device, 1, &inFlightFences[frameCounter]);

		vkResetCommandBuffer(commandBuffers[frameCounter], 0);

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = 0;
		commandBufferBeginInfo.pInheritanceInfo = nil;

		if (vkBeginCommandBuffer(commandBuffers[frameCounter], &commandBufferBeginInfo) != VK_SUCCESS) {
			printf("unable to begin the command buffer!\n");
			return 1;
		}

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
		renderPassBeginInfo.renderArea.offset = {0, 0};
		renderPassBeginInfo.renderArea.extent = swapchain.extent;

		VkClearValue clearColor = {};
		clearColor.color = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkClearValue clearDepth = {};
		clearDepth.depthStencil = {1.0f, 0};
		VkClearValue clearValues[2] = {clearColor, clearDepth};
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(commandBuffers[frameCounter], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[frameCounter], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = swapchain.extent.height;
		viewport.width = (float) swapchain.extent.width;
		viewport.height = -(float) swapchain.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[frameCounter], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset = {0, 0};
		scissor.extent = swapchain.extent;
		vkCmdSetScissor(commandBuffers[frameCounter], 0, 1, &scissor);

		GPUViewProjectionData mvp = {};
		math::Matrix4 modelRotation = math::initYAxisRotationMatrix(fmodf(glfwGetTime(), TAU32));
		mvp.model = math::translateMatrix(math::initIdentityMatrix(), math::Vector3{ 0.0f, 0.0f, -8.0f });
		mvp.model = math::scaleMatrix(mvp.model, 5.0f);
		mvp.model = mvp.model.multiply(modelRotation);

		f32 scale = 2.0f;

		f32 camX = cosf(glfwGetTime());
		f32 camZ = sinf(glfwGetTime());

		mvp.view = math::initIdentityMatrix();//math::lookAt(math::Vector3{1.0f, 0.0f, 3.0f}, math::Vector3{0.0f, 0.0f, -2.0f}, math::Vector3{0.0f, 1.0f, 0.0f});
		mvp.projection = math::initPerspectiveMatrix((f32)swapchain.extent.width/(f32)swapchain.extent.height, 1.0f, 100.0f, 0.1f);

		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffers[frameCounter], 0, 1, &vertexBuffer.buffer, offsets);

		memcpy(uniformBuffers[frameCounter].mappedData, &mvp, sizeof(mvp));

		vkCmdBindDescriptorSets(commandBuffers[frameCounter], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &viewProjectionDataDescriptorSets[frameCounter], 0, nil);
		vkCmdDraw(commandBuffers[frameCounter], 36, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[frameCounter]);

		if (vkEndCommandBuffer(commandBuffers[frameCounter]) != VK_SUCCESS) {
			printf("unable to record command buffer!\n");
			return 1;
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageAvailableSemaphores[frameCounter];
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[frameCounter];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderFinishedSemaphores[frameCounter];

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[frameCounter]) != VK_SUCCESS) {
			printf("unable to submit to queue!\n");
			return 1;
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphores[frameCounter];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain.handle;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nil;

		result = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			result = createSwapchainAndRenderPass(
				window,
				physicalDevice,
				device, 
				surface, 
				queueFamilyIndices,
				queueFamilyIndicesCount,
				isUsingSameQueueForGraphicsAndPresent,
				&renderPass,
				&swapchain,
				&depthImage);
			if (result != VK_SUCCESS) {
				printf("unable to create swapchain!\n");
				return 1;	
			}
			continue;
		}
	}

	vkDeviceWaitIdle(device);

	glfwTerminate();
	return 0;
}
