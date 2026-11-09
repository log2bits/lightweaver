#include "vk_swapchain.h"
#include "vk_check.h"
#include "vk_enumerate.h"
#include <algorithm>
#include <format>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL3/SDL.h>
#include "fatal.h"
#include <string>

VulkanSwapchain::VulkanSwapchain(const Window& window, const VulkanContext& context) : m_window(window), m_context(context) {
    create();
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
        fatal(error);
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

    { // SWAPCHAIN
        const VkSwapchainCreateInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = m_context.surface(),
            .minImageCount = imageCount,
            .imageFormat = m_format,
            .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };
        vkCheck(vkCreateSwapchainKHR(m_context.device(), &info, nullptr, &m_swapchain));
    }

    // Retrieve swapchain images
    m_images = vkEnumerate<VkImage>(vkGetSwapchainImagesKHR, m_context.device(), m_swapchain);
    m_extent = extent;

    // SWAPCHAIN IMAGE VIEWS
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); i++) {
        const VkImageViewCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        vkCheck(vkCreateImageView(m_context.device(), &info, nullptr, &m_imageViews[i]));
    }

    { // DEPTH IMAGE
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = m_depthFormat,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        const VmaAllocationCreateInfo allocInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        vkCheck(vmaCreateImage(m_context.allocator(), &imageInfo, &allocInfo, &m_depthImage, &m_depthAllocation,
                               nullptr));
    }

    { // DEPTH IMAGE VIEW
        const VkImageViewCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_depthImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_depthFormat,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        vkCheck(vkCreateImageView(m_context.device(), &info, nullptr, &m_depthImageView));
    }
}

void VulkanSwapchain::recreate() {
    vkDeviceWaitIdle(m_context.device());
    destroy();
    create();
}

void VulkanSwapchain::destroy() noexcept {
    // DEPTH IMAGE VIEW
    if (m_depthImageView) {
        vkDestroyImageView(m_context.device(), m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }

    // DEPTH IMAGE
    if (m_depthImage) {
        vmaDestroyImage(m_context.allocator(), m_depthImage, m_depthAllocation);
        m_depthImage = VK_NULL_HANDLE;
        m_depthAllocation = VK_NULL_HANDLE;
    }

    // SWAPCHAIN IMAGE VIEWS
    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(m_context.device(), view, nullptr);
    }
    m_imageViews.clear();

    // SWAPCHAIN
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
