#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <malloc.h>
#include <string.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "common.h"
#include "math.h"

bool framebufferResized = false;
struct PositionColorVertex {
	f32 x, y, z;
	f32 r, g, b, a;
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

struct ModelViewProjection {
	math::Matrix4 model;
	math::Matrix4 view;
	math::Matrix4 projection;
};

inline void vkCheck(VkResult result) {
	assert(result == VK_SUCCESS);
}

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	framebufferResized = true;
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

Buffer createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags) {
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

	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	VkMemoryPropertyFlags desiredMemoryPropertyFlags = memoryPropertyFlags;
	u32 memoryTypeIndex = 0;
	for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & desiredMemoryPropertyFlags) == desiredMemoryPropertyFlags) {
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

VkResult createSwapchainAndRenderPass(
	GLFWwindow *window,
	VkPhysicalDevice physicalDevice,
	VkDevice device, 
	VkSurfaceKHR surface, 
	u32 *queueFamilyIndices,
	u32 queueFamilyIndicesCount,
	bool32 isUsingSameQueueForGraphicsAndPresent,
	VkRenderPass *renderPass,
	Swapchain *swapchain
) {
	vkDeviceWaitIdle(device);
	framebufferResized = false;

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


	VkSubpassDependency subpassDependency = {};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
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

	VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nil, &swapchain->handle);
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

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = *renderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = &swapchain->imageViews[i];
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
	assert(glfwExtensionCount + requiredExtensionCount < requiredExtensionCapacity);
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

	if (createSwapchainAndRenderPass(
			window,
			physicalDevice,
			device, 
			surface, 
			queueFamilyIndices,
			queueFamilyIndicesCount,
			isUsingSameQueueForGraphicsAndPresent,
			&renderPass,
			&swapchain) != VK_SUCCESS
		) 
	{
		printf("unable to create swapchain!\n");
		return 1;	
	}

	const char * vertexShaderFilePath = "./spir-v/mvp-input-position-color.vert.spv";
	VkShaderModule vertexShaderModule;
	if(createShaderFromFile(device , vertexShaderFilePath, &vertexShaderModule) != VK_SUCCESS) {
		printf("unable to create vertex shader module!\n");
		return 1;
	}

	const char * fragmentShaderFilePath = "./spir-v/simple-input-color.frag.spv";
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
	bindingDescription.stride = sizeof(PositionColorVertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription positionColorAttributeDescriptions[2] = {};
	positionColorAttributeDescriptions[0].binding = 0;
	positionColorAttributeDescriptions[0].location = 0;
	positionColorAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	positionColorAttributeDescriptions[0].offset = 0;
	VkVertexInputAttributeDescription positionAttributeDescription = {};
	positionColorAttributeDescriptions[1].binding = 0;
	positionColorAttributeDescriptions[1].location = 1;
	positionColorAttributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	positionColorAttributeDescriptions[1].offset = 12;


	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;
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
	//TODO: use cull mode back and frontface counter clockwise when using persptive matrix and inverting y coordinate
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

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uniformBufferLayoutBinding;

	VkDescriptorSetLayout uniformBufferDescriptorSetLayout;
	vkCheck(vkCreateDescriptorSetLayout(device, &layoutInfo, nil, &uniformBufferDescriptorSetLayout));

	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &uniformBufferDescriptorSetLayout;

	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nil, &pipelineLayout) != VK_SUCCESS) {
		printf("unable to create pipeline layout!\n");
		return 1;
	}

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

	f32 vertices[] = {
		0.5, -0.5, 0.0,	1.0, 1.0, 1.0, 1.0,
		0.5, 0.5, 0.0, 0.0, 1.0, 0.0, 1.0,
		-0.5, 0.5, 0.0, 0.0, 0.0, 1.0, 1.0,
		-0.5, -0.5, 0.0, 1.0, 0.0, 0.0, 1.0
	};
	u32 indices[] = {
		0, 1, 2, 2, 3, 0	
	};

	Buffer indexBuffer = createBuffer(physicalDevice, device, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (indexBuffer.createResult != VK_SUCCESS) {
		printf("failed to create index buffer!!!\n");
		return 1;
	}

	Buffer stagingBuffer = createBuffer(physicalDevice, device, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (stagingBuffer.createResult != VK_SUCCESS) {
		printf("failed to create staging buffer!!!\n");
		return 1;
	}

	Buffer vertexBuffer = createBuffer(physicalDevice, device, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
		memcpy(data, vertices, sizeof(vertices));
		vkUnmapMemory(device, stagingBuffer.memory);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(copyDataCmdBuffer, &beginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = sizeof(vertices);
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
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		uniformBuffers[i] = createBuffer(physicalDevice, device, sizeof(ModelViewProjection), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkMapMemory(device, uniformBuffers[i].memory, 0, sizeof(ModelViewProjection), 0, &uniformBuffers[i].mappedData);
	}

	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.poolSizeCount = 1;
	descriptorPoolInfo.pPoolSizes = &poolSize;
	descriptorPoolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPool descriptorPool;
	vkCheck(vkCreateDescriptorPool(device, &descriptorPoolInfo, nil, &descriptorPool));

	VkDescriptorSetLayout descriptorSetLayoutFrames[MAX_FRAMES_IN_FLIGHT];
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		descriptorSetLayoutFrames[i] = uniformBufferDescriptorSetLayout;
	}
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayoutFrames;

	VkDescriptorSet descriptorSetFrames[MAX_FRAMES_IN_FLIGHT];

	vkCheck(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, descriptorSetFrames));

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

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

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
				&swapchain);
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
		VkClearValue clearColor = {{{ 0.0f, 0.0f, 0.0f, 11.0f }}};
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffers[frameCounter], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[frameCounter], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) swapchain.extent.width;
		viewport.height = (float) swapchain.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[frameCounter], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset = {0, 0};
		scissor.extent = swapchain.extent;
		vkCmdSetScissor(commandBuffers[frameCounter], 0, 1, &scissor);

		ModelViewProjection mvp = {};
		//TODO: add some dynamic rotation to the model matrix
		mvp.model = math::initTranslationMatrix(math::Vector3{0.0f, 0.0f, -10.0f}).multiply(math::initScaleMatrix(200.0f));
		mvp.view = math::lookAt(math::Vector3{1.0f, 1.0f, 1.0f}, math::Vector3{0.0f, 0.0f, 0.0f}, math::Vector3{0.0f, 1.0f, 0.0f});
		mvp.projection = math::initPerspectiveMatrix((f32)swapchain.extent.width, -(f32)swapchain.extent.height, 1000.0f, 0.1f);

		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffers[frameCounter], 0, 1, &vertexBuffer.buffer, offsets);

		vkCmdBindIndexBuffer(commandBuffers[frameCounter], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		memcpy(uniformBuffers[frameCounter].mappedData, &mvp, sizeof(mvp));

		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniformBuffers[frameCounter].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(ModelViewProjection);

		VkWriteDescriptorSet descriptorWrite = {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSetFrames[frameCounter];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;
		descriptorWrite.pImageInfo = nil;
		descriptorWrite.pTexelBufferView = nil;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nil);

		vkCmdBindDescriptorSets(commandBuffers[frameCounter], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetFrames[frameCounter], 0, nil);
		vkCmdDrawIndexed(commandBuffers[frameCounter], sizeof(indices)/sizeof(indices[0]), 1, 0, 0, 0);

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
				&swapchain);
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
