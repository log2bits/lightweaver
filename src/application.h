#pragma once

#include <volk.h>

struct SDL_Window;

class Application {
public:
	Application() = default;
	~Application() = default;

	Application(const Application&) = delete; // copy constructor (doesnt exist yet)
	Application& operator=(const Application&) = delete; // copy assignment (getting overwritten)

	bool init();
	void run();
	void shutdown();

private:
	bool initWindow();
	bool initVulkan();
	bool createInstance();
	bool createSurface();
	bool pickPhysicalDevice();
	bool createDevice();
	bool isDeviceSuitable(VkPhysicalDevice physicalDevice, uint32_t& outGraphicsFamily) const;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	);

	SDL_Window* m_window  = nullptr;
	bool m_running = false;

	VkInstance               m_instance            = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_messenger           = VK_NULL_HANDLE;
	VkSurfaceKHR             m_surface             = VK_NULL_HANDLE;
	VkPhysicalDevice         m_physicalDevice      = VK_NULL_HANDLE;
	VkDevice                 m_device              = VK_NULL_HANDLE;
	VkQueue                  m_graphicsQueue       = VK_NULL_HANDLE;

	uint32_t                 m_graphicsQueueFamily = UINT32_MAX;
};