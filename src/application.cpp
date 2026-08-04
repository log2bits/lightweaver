#include "application.h"
#include <algorithm>
#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include <cstring>
#include "vk_check.h"
#include "vk_enumerate.h"

bool Application::init() {
	if (!initWindow())  return false;
	if (!initVulkan())  return false;
	return true;
}

void Application::run() {
	m_running = true;
	SDL_Event event;

	while (m_running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				m_running = false;
			}
		}
		SDL_Delay(16);
	}
}

void Application::shutdown() {
	destroySwapchain();

	if (m_device) {
		vkDestroyDevice(m_device, nullptr);
		m_device = VK_NULL_HANDLE;
	}
	if (m_surface) {
		SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
		m_surface = VK_NULL_HANDLE;
	}
	if (m_messenger) {
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
		m_messenger = VK_NULL_HANDLE;
	}
	if (m_instance) {
		vkDestroyInstance(m_instance, nullptr);
		m_instance = VK_NULL_HANDLE;
	}
	if (m_window) {
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
	SDL_Quit();
}

bool Application::initWindow() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return false;
	}

	m_window = SDL_CreateWindow(
		"Lightweaver", 1280, 720,
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
	);

	if (!m_window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return false;
	}

	return true;
}


bool Application::initVulkan() {
	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed, is the Vulkan runtime installed?");
		return false;
	}

	uint32_t const v = volkGetInstanceVersion();
	SDL_Log("volk initialized, instance version: %u.%u.%u",
		VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));

	if (!createInstance()) {
		SDL_Log("createInstance failed");
		return false;
	}

	if (!createSurface()) {
		SDL_Log("createSurface failed");
		return false;
	}

	if (!pickPhysicalDevice()) {
		SDL_Log("pickPhysicalDevice failed");
		return false;
	}

	if (!createDevice()) {
		SDL_Log("createDevice failed");
		return false;
	}

	if (!createSwapchain()) {
		SDL_Log("createSwapchain failed");
		return false;
	}

	return true;
}

bool Application::createInstance() {
	// Describe application
	VkApplicationInfo appInfo{};
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName   = "Lightweaver";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	appInfo.pEngineName        = "Lightweaver";
	appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 0, 1, 0);
	appInfo.apiVersion         = VK_API_VERSION_1_3;

	// Build extension list
	uint32_t sdlExtCount = 0;
	const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
	if (!sdlExts) {
		SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		return false;
	}

	std::vector<const char*> extensions(sdlExts, sdlExts + sdlExtCount);
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	#ifdef __APPLE__
		extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	#endif

	SDL_Log("found %zu instance extensions:", extensions.size());
	for (const char* ext : extensions) {
		SDL_Log("  %s", ext);
	}

	// Build layer list
	std::vector<const char*> valid_layers = { "VK_LAYER_KHRONOS_validation" };
	std::vector<VkLayerProperties> layers = vkEnumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

	SDL_Log("found %zu instance layers:", layers.size());
	bool validationFound = false;
	for (const VkLayerProperties& layerProperties : layers) {
		SDL_Log("  %s", layerProperties.layerName);
		if (std::strcmp(layerProperties.layerName, valid_layers[0]) == 0) {
			validationFound = true;
		}
	}

	if (!validationFound) {
		SDL_Log("validation layer unavailable, continuing without it");
		valid_layers.clear();
	}

	// Describe the debugger
	VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
	debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugInfo.pfnUserCallback = debugCallback;
	debugInfo.pUserData       = this;

	// Describe the instance
	VkInstanceCreateInfo createInfo{};
	createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext                   = &debugInfo;
	createInfo.pApplicationInfo        = &appInfo;
	createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledLayerCount       = static_cast<uint32_t>(valid_layers.size());
	createInfo.ppEnabledLayerNames     = valid_layers.data();

	#ifdef __APPLE__
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	#endif

	// Create it
	vkCheck(vkCreateInstance(&createInfo, nullptr, &m_instance));

	// Load every function pointer from the new instance
	volkLoadInstance(m_instance);

	// Create the persistent messenger
	vkCheck(vkCreateDebugUtilsMessengerEXT(m_instance, &debugInfo, nullptr, &m_messenger));

	return true;
}

bool Application::createSurface() {
	if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		return false;
	}
	return true;
}

bool Application::pickPhysicalDevice() {
	std::vector<VkPhysicalDevice> physicalDevices = vkEnumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, m_instance);

	SDL_Log("found %zu physical devices:", physicalDevices.size());

	VkPhysicalDevice fallback = VK_NULL_HANDLE;
	uint32_t fallbackFamily = UINT32_MAX;

	for (const VkPhysicalDevice& physicalDevice : physicalDevices) {
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		SDL_Log("  %s", properties.deviceName);

		uint32_t graphicsFamily = UINT32_MAX;
		if (!isDeviceSuitable(physicalDevice, graphicsFamily)) {
			continue;
		}

		// Prefer discrete GPU
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			m_physicalDevice = physicalDevice;
			m_graphicsQueueFamily = graphicsFamily;
			SDL_Log("    suitable (discrete) selected");
			continue;
		}

		// Prepare to fallback to integrated GPU
		SDL_Log("    suitable (non-discrete) keeping as fallback");
		if (fallback == VK_NULL_HANDLE) {
			fallback = physicalDevice;
			fallbackFamily = graphicsFamily;
		}
	}

	// Fallback to integrated GPU
	if (m_physicalDevice == VK_NULL_HANDLE) {
		m_physicalDevice = fallback;
		m_graphicsQueueFamily = fallbackFamily;
	}

	// No GPU found
	if (m_physicalDevice == VK_NULL_HANDLE) {
		SDL_Log("no suitable physical device found");
		return false;
	}

	VkPhysicalDeviceProperties selected;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &selected);
	SDL_Log("using GPU: %s (queue family %u)", selected.deviceName, m_graphicsQueueFamily);

	return true;
}

bool Application::isDeviceSuitable(VkPhysicalDevice physicalDevice, uint32_t& outGraphicsFamily) const {
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

	// 1. API version
	if (properties.apiVersion < VK_API_VERSION_1_3) {
		SDL_Log("    rejected: Vulkan version too old");
		return false;
	}

	// 2. A queue family supporting both graphics and presentation
	std::vector<VkQueueFamilyProperties> queueFamilies =
		vkEnumerate<VkQueueFamilyProperties>(
			vkGetPhysicalDeviceQueueFamilyProperties, physicalDevice);

	uint32_t graphicsFamily = UINT32_MAX;
	for (uint32_t i = 0; i < queueFamilies.size(); i++) {
		VkBool32 canPresent = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_surface, &canPresent);

		if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent) {
			graphicsFamily = i;
			break;
		}
	}
	if (graphicsFamily == UINT32_MAX) {
		SDL_Log("    rejected: no graphics+present queue family");
		return false;
	}

	// 3. Swapchain extension
	std::vector<VkExtensionProperties> extensions =
		vkEnumerate<VkExtensionProperties>(
			vkEnumerateDeviceExtensionProperties, physicalDevice, nullptr);

	bool hasSwapchain = false;
	for (const VkExtensionProperties& extension : extensions) {
		if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
			hasSwapchain = true;
			break;
		}
	}
	if (!hasSwapchain) {
		SDL_Log("    rejected: no swapchain extension");
		return false;
	}

	// 4. Required Vulkan 1.2 / 1.3 features
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.pNext = &features13;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features12;

	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

	if (!features13.dynamicRendering || !features13.synchronization2) {
		SDL_Log("    rejected: missing dynamicRendering or synchronization2");
		return false;
	}
	if (!features12.timelineSemaphore || !features12.bufferDeviceAddress) {
		SDL_Log("    rejected: missing timelineSemaphore or bufferDeviceAddress");
		return false;
	}

	outGraphicsFamily = graphicsFamily;
	return true;
}

bool Application::createDevice() {
	// Features to enable
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.timelineSemaphore   = VK_TRUE;
	features12.bufferDeviceAddress = VK_TRUE;
	features12.pNext               = &features13;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features12;

	// One graphics queue
	float queuePriority = 1.0f;

	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = m_graphicsQueueFamily;
	queueInfo.queueCount       = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	// Device extensions
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	#ifdef __APPLE__
	std::vector<VkExtensionProperties> available =
		vkEnumerate<VkExtensionProperties>(
			vkEnumerateDeviceExtensionProperties, m_physicalDevice, nullptr);

	for (const VkExtensionProperties& extension : available) {
		if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
			deviceExtensions.push_back("VK_KHR_portability_subset");
			break;
		}
	}
	#endif

	// Create the device
	VkDeviceCreateInfo deviceInfo{};
	deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext                   = &features2;
	deviceInfo.queueCreateInfoCount    = 1;
	deviceInfo.pQueueCreateInfos       = &queueInfo;
	deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
	deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceInfo.pEnabledFeatures        = nullptr;

	vkCheck(vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device));

	volkLoadDevice(m_device);
	vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);

	SDL_Log("logical device created, graphics queue from family %u", m_graphicsQueueFamily);
	return true;
}

bool Application::createSwapchain() {
	// Query surface capabilities
	VkSurfaceCapabilitiesKHR capabilities{};
	vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities));

	// Choose image count
	uint32_t imageCount = std::max(3u, capabilities.minImageCount);
	if (capabilities.maxImageCount > 0) {
		imageCount = std::min(imageCount, capabilities.maxImageCount);
	}
	SDL_Log("swapchain image count: %d", imageCount);

	// Query available formats
	std::vector<VkSurfaceFormatKHR> formats = vkEnumerate<VkSurfaceFormatKHR>(
		vkGetPhysicalDeviceSurfaceFormatsKHR, m_physicalDevice, m_surface
	);

	// Check format is supported
	bool formatSupported = false;
	for (const VkSurfaceFormatKHR& surfaceFormat : formats) {
		if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			formatSupported = true;
			break;
		}
	}

	// Log alternative formats upon failure
	if (!formatSupported) {
		SDL_Log("VK_FORMAT_B8G8R8A8_SRGB / SRGB_NONLINEAR not supported. Available:");
		for (const VkSurfaceFormatKHR& surfaceFormat : formats) {
			SDL_Log("  format %d, colorSpace %d", surfaceFormat.format, surfaceFormat.colorSpace);
		}
		return false;
	}

	// Determine extent (window size/resolution)
	VkExtent2D extent;
	if (capabilities.currentExtent.width != UINT32_MAX) {
		extent = capabilities.currentExtent;
	} else {
		int pixelWidth = 0, pixelHeight = 0;
		SDL_GetWindowSizeInPixels(m_window, &pixelWidth, &pixelHeight);
		if (pixelWidth <= 0 || pixelHeight <= 0) {
			SDL_Log("window has zero size, skipping swapchain creation");
			return false;
		}
		extent.width = std::clamp(static_cast<uint32_t>(pixelWidth), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp(static_cast<uint32_t>(pixelHeight), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}
	SDL_Log("swapchain extent size: %d x %d", extent.width, extent.height);

	// Fill swapchain info
	VkSwapchainCreateInfoKHR info{};
	info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface          = m_surface;
	info.minImageCount    = imageCount;
	info.imageFormat      = m_swapchainFormat;
	info.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	info.imageExtent      = extent;
	info.imageArrayLayers = 1;
	info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform     = capabilities.currentTransform;
	info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
	info.clipped          = VK_TRUE;
	info.oldSwapchain     = VK_NULL_HANDLE;

	// Create swapchain
	vkCheck(vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain));

	// Retrieve swapchain images
	m_swapchainImages = vkEnumerate<VkImage>(vkGetSwapchainImagesKHR, m_device, m_swapchain);
	m_swapchainExtent = extent;

	// Create an image view per image
	m_swapchainImageViews.resize(m_swapchainImages.size());
	for (size_t i = 0; i < m_swapchainImages.size(); i++) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image    = m_swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format   = m_swapchainFormat;
		viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel   = 0;
		viewInfo.subresourceRange.levelCount     = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount     = 1;

		vkCheck(vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]));
	}
}

void Application::destroySwapchain() {
	// Destroy image views
	for (VkImageView view : m_swapchainImageViews) {
		vkDestroyImageView(m_device, view, nullptr);
	}
	m_swapchainImageViews.clear();

	// Destroy swapchain
	if (m_swapchain) {
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}
	m_swapchainImages.clear();
}

VKAPI_ATTR VkBool32 VKAPI_CALL Application::debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	(void)type;
	(void)pUserData;

	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[vk] %s", pCallbackData->pMessage);
	} else {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[vk] %s", pCallbackData->pMessage);
	}

	return VK_FALSE;
}