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

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t u16;

typedef float f32;
typedef double f64;

#define nil nullptr

#define min(a, b) (a < b ? a : b)
#define max(a, b) (a > b ? a : b)

inline void assert(u8 b) {
	#ifndef NDEBUG
		if (!b) {
			*((char*) 0) = 0;
		}
	#endif
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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(1280, 720, "CPP Voxels!!", nil, nil);
	if (!window) {
		glfwTerminate();
		printf("unable to create the window\n");
		return 1;
	}

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

	u8 foundGraphicsQueueFamily = 0;
	u32 graphicsQueueFamilyIndex = 0;

	u8 foundPresentQueueFamily = 0;
	u32 presentQueueFamilyIndex = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nil);
	VkQueueFamilyProperties * queueFamilyProperties = (VkQueueFamilyProperties *) _malloca(queueFamilyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties);
	printf("%d queue families\n", queueFamilyCount);
	for (u32 i = 0; i < queueFamilyCount; i++) {
		if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			printf("queue family %d supports graphics operations\n", i);
			graphicsQueueFamilyIndex = i;
			foundGraphicsQueueFamily = 1;
		}

		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
		if (presentSupport) {
			printf("queue family %d supports presentation operations\n", i);
			if (!foundPresentQueueFamily || (foundGraphicsQueueFamily && graphicsQueueFamilyIndex != presentQueueFamilyIndex)) {
				presentQueueFamilyIndex = i;
				foundPresentQueueFamily = 1;
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

	VkDeviceQueueCreateInfo queueCreateInfos [2] = {};

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
		queueCreateInfoCount = 2;
	}

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

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

	u32 availableSurfaceFormatsCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &availableSurfaceFormatsCount, nil);
	if (!availableSurfaceFormatsCount) {
		printf("physical device does not support any surface formats!");
		return 1;
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
		return 1;
	}

	u32 presentModesCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, nil);
	if (!presentModesCount) {
		printf("physical device does not support any present modes!");
		return 1;
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

	VkExtent2D swapchainExtent = {};

	if (surfaceCapabilities.currentExtent.width ==  UINT32_MAX) {
		i32 width, height;
		glfwGetFramebufferSize(window, &width, &height);
		swapchainExtent.width = max(min((u32) width, surfaceCapabilities.maxImageExtent.width), surfaceCapabilities.minImageExtent.width);
		swapchainExtent.height = max(min((u32) height, surfaceCapabilities.maxImageExtent.height), surfaceCapabilities.minImageExtent.height);
	} else {
		swapchainExtent = surfaceCapabilities.currentExtent;
	}

	u32 imageCount = 3;
	if (surfaceCapabilities.maxImageCount > 0) {
		imageCount = max(imageCount, surfaceCapabilities.maxImageCount);
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = imageCount;
	swapchainCreateInfo.imageFormat = desiredSurfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = desiredSurfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = swapchainExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if (isUsingSameQueueForGraphicsAndPresent) {
		//TODO: support exclusive mode
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nil;
	} else {
		u32 queueFamilyIndices [] =  {
			graphicsQueueFamilyIndex,
			presentQueueFamilyIndex,
		};
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}

	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = desiredPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain;
	if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nil, &swapchain) != VK_SUCCESS) {
		printf("unable to create swapchain!\n");
		return 1;
	}

	u32 swapchainImagesCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImagesCount, nil);
	VkImage * swapchainImages = (VkImage *) _malloca(swapchainImagesCount * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImagesCount, swapchainImages);

	VkImageView * swapchainImageViews = (VkImageView *) _malloca(swapchainImagesCount * sizeof(VkImageView));

	for (u32 i = 0; i < swapchainImagesCount; i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = desiredSurfaceFormat.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_A;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_A;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_A;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		vkCreateImageView(device, &createInfo, nil, &swapchainImageViews[i]);
	}

	const char * vertexShaderFilePath = "./spir-v/triangle.vert.spv";
	VkShaderModule vertexShaderModule;
	if(createShaderFromFile(device , vertexShaderFilePath, &vertexShaderModule) != VK_SUCCESS) {
		printf("unable to create vertex shader module!\n");
		return 1;
	}

	const char * fragmentShaderFilePath = "./spir-v/triangle.frag.spv";
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

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nil;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nil;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) swapchainExtent.width;
	viewport.height = (float) swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = swapchainExtent;

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
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	//TODO: empty pipeline layout. use push constants in here for the view projection matrix
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nil, &pipelineLayout) != VK_SUCCESS) {
		printf("unable to create pipeline layout!\n");
		return 1;
	}


	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window)) {
		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}