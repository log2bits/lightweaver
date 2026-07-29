#include "application.h"
#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include <cstring>
#include <cstdlib>

#define VK_CHECK(x)                         \
    do {                                    \
        VkResult err_ = (x);                \
        if (err_ != VK_SUCCESS) {           \
            SDL_Log(                        \
				"Vulkan error %s at %s:%d", \
				string_VkResult(err_),      \
				__FILE__,                   \
				__LINE__                    \
			);                              \
            return false;                   \
        }                                   \
    } while (0)

bool Application::init() {
	if (!initWindow())  return false;
	if (!initVulkan())  return false;
	return true;
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

	SDL_Log("Found %zu instance extensions:", extensions.size());
	for (const char* ext : extensions) {
		SDL_Log("  %s", ext);
	}

	// Build layer list
	std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
	uint32_t availableCount = 0;
	VK_CHECK(vkEnumerateInstanceLayerProperties(&availableCount, nullptr));
	std::vector<VkLayerProperties> available(availableCount);
	VK_CHECK(vkEnumerateInstanceLayerProperties(&availableCount, available.data()));

	SDL_Log("Found %u instance layers:", availableCount);
	bool validationFound = false;
	for (const auto& layer : available) {
		SDL_Log("  %s", layer.layerName);
		if (std::strcmp(layer.layerName, layers[0]) == 0) {
			validationFound = true;
			break;
		}
	}

	if (!validationFound) {
		SDL_Log("Validation layer unavailable, continuing without it");
		layers.clear();
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
	createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames     = layers.data();

	#ifdef __APPLE__
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	#endif

	// Create it
	VK_CHECK(
		vkCreateInstance(&createInfo, nullptr, &m_instance)
	);

	// Load every function pointer from the new instance
	volkLoadInstance(m_instance);

	// Create the persistent messenger
	VK_CHECK(
		vkCreateDebugUtilsMessengerEXT(m_instance, &debugInfo, nullptr, &m_messenger)
	);

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
	return createInstance();
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