#pragma once

#include <vector>
#include "vk_context.h"
#include "window.h"

class VulkanSwapchain {
public:
    VulkanSwapchain(const Window&, const VulkanContext&);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    VkSwapchainKHR swapchain() const { return m_swapchain; }
    VkFormat format() const { return m_format; }
    VkExtent2D extent() const { return m_extent; }
    const std::vector<VkImage>& images() const { return m_images; }
    const std::vector<VkImageView>& views() const { return m_imageViews; }
    VkImage depthImage() const { return m_depthImage; }
    VkImageView depthImageView() const { return m_depthImageView; }
    VkFormat depthFormat() const { return m_depthFormat; }
    bool valid() const { return m_swapchain != VK_NULL_HANDLE; }
    void recreate();

private:
    const Window& m_window;
    const VulkanContext& m_context;

    void create();
    void destroy() noexcept;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
};
