#pragma once
#include <vector>
#include <volk.h>
#include "vk_backend.h"

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
	bool initBackend();
	bool createSwapchain();
	void destroySwapchain();

	SDL_Window* m_window  = nullptr;
	bool m_running = false;


	VulkanBackend            m_backend = VulkanBackend();
	VkSwapchainKHR           m_swapchain = VK_NULL_HANDLE;
	VkFormat                 m_swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
	VkExtent2D               m_swapchainExtent{};
	std::vector<VkImage>     m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
};