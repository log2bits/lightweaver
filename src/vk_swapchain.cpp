#include "vk_swapchain.h"
#include "vk_check.h"
#include "vk_enumerate.h"
#include <algorithm>
#include <format>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>

VulkanSwapchain::VulkanSwapchain(const Window& window, const VulkanContext& context) : m_window(window), m_context(context) {
    try {
        create();
    } catch (...) {
        destroy();
        throw;
    }
}

void VulkanSwapchain::create() {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities{};
    vkCheck(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context.physicalDevice(), m_context.surface(), &capabilities));

    // Choose image count
    uint32_t imageCount = std::max(3u, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }
    SDL_Log("swapchain image count: %u", imageCount);

    // Query available formats
    std::vector<VkSurfaceFormatKHR> formats = vkEnumerate<VkSurfaceFormatKHR>(
        vkGetPhysicalDeviceSurfaceFormatsKHR, m_context.physicalDevice(), m_context.surface());

    // Check format is supported
    bool formatSupported = false;
    for (const VkSurfaceFormatKHR& surfaceFormat : formats) {
        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            formatSupported = true;
            break;
        }
    }

    // Log alternative formats upon failure
    if (!formatSupported) {
        std::string error = "VK_FORMAT_B8G8R8A8_SRGB / SRGB_NONLINEAR not supported. Available:\n";
        for (const VkSurfaceFormatKHR& surfaceFormat : formats) {
            error += std::format("  {} / {}\n", string_VkFormat(surfaceFormat.format), string_VkColorSpaceKHR(surfaceFormat.colorSpace));
        }
        throw std::runtime_error(error);
    }

    // Determine extent (window size/resolution)
    VkExtent2D extent{};
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        int pixelWidth = 0, pixelHeight = 0;
        SDL_GetWindowSizeInPixels(m_window.handle(), &pixelWidth, &pixelHeight);
        if (pixelWidth <= 0 || pixelHeight <= 0) {
            // having no swapchain is fine, the render loop just has to skip frames if it's null
            SDL_Log("window has zero size, skipping swapchain creation");
            return;
        }
        extent.width = std::clamp(static_cast<uint32_t>(pixelWidth), capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(pixelHeight), capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }
    SDL_Log("swapchain extent size: %u x %u", extent.width, extent.height);

    // Fill swapchain info
    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_context.surface();
    info.minImageCount = imageCount;
    info.imageFormat = m_format;
    info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    // Create swapchain
    vkCheck(vkCreateSwapchainKHR(m_context.device(), &info, nullptr, &m_swapchain));

    // Retrieve swapchain images
    m_images = vkEnumerate<VkImage>(vkGetSwapchainImagesKHR, m_context.device(), m_swapchain);
    m_extent = extent;

    // Create an image view per image
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkCheck(vkCreateImageView(m_context.device(), &viewInfo, nullptr, &m_imageViews[i]));
    }

    VkImageCreateInfo depthImageInfo{};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = m_depthFormat;
    depthImageInfo.extent = {extent.width, extent.height, 1};
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    vkCheck(vmaCreateImage(m_context.allocator(), &depthImageInfo, &allocInfo, &m_depthImage, &m_depthAllocation,
                           nullptr));

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = m_depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = m_depthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    vkCheck(vkCreateImageView(m_context.device(), &depthViewInfo, nullptr, &m_depthImageView));
}

void VulkanSwapchain::recreate() {
    vkDeviceWaitIdle(m_context.device());
    destroy();
    create();
}

void VulkanSwapchain::destroy() noexcept {
    // Depth buffer
    if (m_depthImageView) {
        vkDestroyImageView(m_context.device(), m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_depthImage) {
        vmaDestroyImage(m_context.allocator(), m_depthImage, m_depthAllocation);
        m_depthImage = VK_NULL_HANDLE;
        m_depthAllocation = VK_NULL_HANDLE;
    }

    // Swapchain image views
    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(m_context.device(), view, nullptr);
    }
    m_imageViews.clear();

    // Swapchain
    if (m_swapchain) {
        vkDestroySwapchainKHR(m_context.device(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_images.clear();
    m_extent = VkExtent2D();
}

VulkanSwapchain::~VulkanSwapchain() {
    destroy();
}