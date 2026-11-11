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

    const vk::raii::SwapchainKHR& swapchain() const { return m_swapchain; }
    vk::Format format() const { return m_format; }
    vk::Extent2D extent() const { return m_extent; }
    const std::vector<vk::Image>& images() const { return m_images; }
    const std::vector<vk::raii::ImageView>& views() const { return m_imageViews; }
    vk::Image depthImage() const { return m_depthImage; }
    const vk::raii::ImageView& depthView() const { return m_depthView; }
    vk::Format depthFormat() const { return m_depthFormat; }
    bool valid() const { return static_cast<bool>(*m_swapchain); }
    void recreate();

private:
    const Window& m_window;
    const VulkanContext& m_context;

    void create();
    void destroy() noexcept;

    vk::raii::SwapchainKHR m_swapchain = nullptr;
    vk::Format m_format = vk::Format::eB8G8R8A8Srgb;
    vk::Extent2D m_extent{};
    std::vector<vk::Image> m_images;
    std::vector<vk::raii::ImageView> m_imageViews;

    vk::Image m_depthImage = nullptr;
    vk::raii::ImageView m_depthView = nullptr;
    VmaAllocation m_depthAllocation = nullptr;
    vk::Format m_depthFormat = vk::Format::eD32Sfloat;
};
