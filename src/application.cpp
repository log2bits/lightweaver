#include "application.h"
#include <algorithm>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <cstring>
#include "vk_check.h"
#include "vk_enumerate.h"

bool Application::init() {
    if (!initWindow()) return false;
    if (!initBackend()) return false;
    if (!createSwapchain()) return false;
    return true;
}

void Application::shutdown() {
    destroySwapchain();
    m_backend.shutdown();
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool Application::initBackend() {
    return m_backend.init(m_window);
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

bool Application::createSwapchain() {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities{};
    vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_backend.physicalDevice(), m_backend.surface(), &capabilities));

    // Choose image count
    uint32_t imageCount = std::max(3u, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }
    SDL_Log("swapchain image count: %d", imageCount);

    // Query available formats
    std::vector<VkSurfaceFormatKHR> formats = vkEnumerate<VkSurfaceFormatKHR>(
        vkGetPhysicalDeviceSurfaceFormatsKHR, m_backend.physicalDevice(), m_backend.surface()
    );

    // Check format is supported
    bool formatSupported = false;
    for (const VkSurfaceFormatKHR &surfaceFormat: formats) {
        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace ==
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            formatSupported = true;
            break;
        }
    }

    // Log alternative formats upon failure
    if (!formatSupported) {
        SDL_Log("VK_FORMAT_B8G8R8A8_SRGB / SRGB_NONLINEAR not supported. Available:");
        for (const VkSurfaceFormatKHR &surfaceFormat: formats) {
            SDL_Log("  format %d, colorSpace %d", surfaceFormat.format, surfaceFormat.colorSpace);
        }
        return false;
    }

    // Determine extent (window size/resolution)
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        int pixelWidth = 0, pixelHeight = 0;
        SDL_GetWindowSizeInPixels(m_window, &pixelWidth, &pixelHeight);
        if (pixelWidth <= 0 || pixelHeight <= 0) {
            SDL_Log("window has zero size, skipping swapchain creation");
            return false;
        }
        extent.width = std::clamp(static_cast<uint32_t>(pixelWidth), capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(pixelHeight), capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }
    SDL_Log("swapchain extent size: %d x %d", extent.width, extent.height);

    // Fill swapchain info
    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_backend.surface();
    info.minImageCount = imageCount;
    info.imageFormat = m_swapchainFormat;
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
    vkCheck(vkCreateSwapchainKHR(m_backend.device(), &info, nullptr, &m_swapchain));

    // Retrieve swapchain images
    m_swapchainImages = vkEnumerate<VkImage>(vkGetSwapchainImagesKHR, m_backend.device(), m_swapchain);
    m_swapchainExtent = extent;

    // Create an image view per image
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkCheck(vkCreateImageView(m_backend.device(), &viewInfo, nullptr, &m_swapchainImageViews[i]));
    }

    return true;
}

void Application::destroySwapchain() {
    // Destroy image views
    for (VkImageView view: m_swapchainImageViews) {
        vkDestroyImageView(m_backend.device(), view, nullptr);
    }
    m_swapchainImageViews.clear();

    // Destroy swapchain
    if (m_swapchain) {
        vkDestroySwapchainKHR(m_backend.device(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_swapchainImages.clear();
}
