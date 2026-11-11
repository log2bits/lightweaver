#include "vk_swapchain.h"
#include <algorithm>
#include <format>
#include <string>
#include "fatal.h"
#include "log.h"

VulkanSwapchain::VulkanSwapchain(const Window& window, const VulkanContext& context) : m_window(window), m_context(context) {
    create();
}

void VulkanSwapchain::create() {
    // Query surface capabilities
    const vk::SurfaceCapabilitiesKHR capabilities = m_context.physicalDevice().getSurfaceCapabilitiesKHR(*m_context.surface());

    // Choose image count
    uint32_t imageCount = std::max(3u, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    // Query available formats
    const std::vector<vk::SurfaceFormatKHR> formats = m_context.physicalDevice().getSurfaceFormatsKHR(*m_context.surface());

    // Check format is supported
    bool formatSupported = false;
    for (const vk::SurfaceFormatKHR& surfaceFormat : formats) {
        if (surfaceFormat.format == m_format && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            formatSupported = true;
            break;
        }
    }

    // Log alternative formats upon failure
    if (!formatSupported) {
        std::string error = "VK_FORMAT_B8G8R8A8_SRGB / SRGB_NONLINEAR not supported. Available:\n";
        for (const vk::SurfaceFormatKHR& surfaceFormat : formats) {
            error += std::format("  {} / {}\n", vk::to_string(surfaceFormat.format), vk::to_string(surfaceFormat.colorSpace));
        }
        fatal(error);
    }

    // Determine extent (window size/resolution)
    vk::Extent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent = m_window.pixelSize();
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    // having no swapchain is fine, the render loop just has to skip frames if it is null
    if (extent.width == 0 || extent.height == 0) {
        logInfo("window has zero size, skipping swapchain creation");
        return;
    }
    logInfo(std::format("swapchain: {} x {}, {} images", extent.width, extent.height, imageCount));

    // SWAPCHAIN
    const auto swapchainInfo = vk::SwapchainCreateInfoKHR()
        .setSurface(*m_context.surface())
        .setMinImageCount(imageCount)
        .setImageFormat(m_format)
        .setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
        .setImageExtent(extent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setPreTransform(capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(vk::PresentModeKHR::eFifo)
        .setClipped(vk::True);

    m_swapchain = vk::raii::SwapchainKHR(m_context.device(), swapchainInfo);

    // Retrieve swapchain images
    m_images = m_swapchain.getImages();
    m_extent = extent;

    // SWAPCHAIN IMAGE VIEWS (the mip and layer bases default to zero)
    const auto colorRange = vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setLevelCount(1)
        .setLayerCount(1);

    m_imageViews.reserve(m_images.size());
    for (const vk::Image& image : m_images) {
        const auto viewInfo = vk::ImageViewCreateInfo()
            .setImage(image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(m_format)
            .setSubresourceRange(colorRange);

        m_imageViews.emplace_back(m_context.device(), viewInfo);
    }

    // DEPTH IMAGE
    const auto depthImageInfo = vk::ImageCreateInfo()
        .setImageType(vk::ImageType::e2D)
        .setFormat(m_depthFormat)
        .setExtent({extent.width, extent.height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    const VmaAllocationCreateInfo depthAllocInfo{
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    // VMA is a C library, so the C++ struct goes over the wall as its C counterpart
    const VkImageCreateInfo& cDepthImageInfo = depthImageInfo;
    VkImage depthImage = VK_NULL_HANDLE;

    const VkResult depthResult = vmaCreateImage(m_context.allocator(), &cDepthImageInfo, &depthAllocInfo, &depthImage, &m_depthAllocation, nullptr);
    if (depthResult != VK_SUCCESS) {
        fatal(std::format("vmaCreateImage failed: {}", vk::to_string(static_cast<vk::Result>(depthResult))));
    }

    m_depthImage = depthImage;

    // DEPTH IMAGE VIEW
    const auto depthRange = vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eDepth)
        .setLevelCount(1)
        .setLayerCount(1);

    const auto depthViewInfo = vk::ImageViewCreateInfo()
        .setImage(m_depthImage)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(m_depthFormat)
        .setSubresourceRange(depthRange);

    m_depthView = vk::raii::ImageView(m_context.device(), depthViewInfo);
}

void VulkanSwapchain::recreate() {
    m_context.device().waitIdle();
    destroy();
    create();
}

void VulkanSwapchain::destroy() noexcept {
    // DEPTH IMAGE VIEW (explicit, because the image below is not raii and would otherwise go first)
    m_depthView = nullptr;

    // DEPTH IMAGE
    if (m_depthImage) {
        vmaDestroyImage(m_context.allocator(), m_depthImage, m_depthAllocation);
        m_depthImage = nullptr;
        m_depthAllocation = nullptr;
    }

    // SWAPCHAIN IMAGE VIEWS
    m_imageViews.clear();

    // SWAPCHAIN
    m_swapchain = nullptr;
    m_images.clear();
    m_extent = vk::Extent2D{};
}

VulkanSwapchain::~VulkanSwapchain() {
    destroy();
}
