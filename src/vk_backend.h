#pragma once
#include <volk.h>
struct SDL_Window;

class VulkanBackend {
public:
    bool init(SDL_Window* window);
    void shutdown();

    VkInstance       instance()       const { return m_instance; }
    VkSurfaceKHR     surface()        const { return m_surface; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice         device()         const { return m_device; }
    VkQueue          graphicsQueue()  const { return m_graphicsQueue; }
    uint32_t         graphicsFamily() const { return m_graphicsQueueFamily; }

private:
    bool createInstance();
    bool createSurface(SDL_Window* window);
    bool pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice, uint32_t& outFamily) const;
    bool createDevice();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    VkInstance               m_instance            = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_messenger           = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface             = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physicalDevice      = VK_NULL_HANDLE;
    VkDevice                 m_device              = VK_NULL_HANDLE;
    VkQueue                  m_graphicsQueue       = VK_NULL_HANDLE;
    uint32_t                 m_graphicsQueueFamily = UINT32_MAX;
};