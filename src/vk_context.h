#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>
class Window;

class VulkanContext {
public:
    explicit VulkanContext(const Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    const vk::raii::Instance& instance() const { return m_instance; }
    const vk::raii::SurfaceKHR& surface() const { return m_surface; }
    const vk::raii::PhysicalDevice& physicalDevice() const { return m_physicalDevice; }
    const vk::raii::Device& device() const { return m_device; }
    const vk::raii::Queue& graphicsQueue() const { return m_graphicsQueue; }
    uint32_t graphicsFamily() const { return m_graphicsQueueFamily; }
    VmaAllocator allocator() const { return m_allocator; }

private:
    void createInstance(const Window& window);
    void pickPhysicalDevice();
    bool isDeviceSuitable(const vk::raii::PhysicalDevice&, uint32_t& outFamily) const;
    void createDevice();
    void createAllocator();

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    // Declaration order is destruction order reversed, so these read top down as creation order
    vk::raii::Context m_loader;
    vk::raii::Instance m_instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_messenger = nullptr;
    vk::raii::SurfaceKHR m_surface = nullptr;
    vk::raii::PhysicalDevice m_physicalDevice = nullptr;
    vk::raii::Device m_device = nullptr;
    vk::raii::Queue m_graphicsQueue = nullptr;
    uint32_t m_graphicsQueueFamily = UINT32_MAX;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};
