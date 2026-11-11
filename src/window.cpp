#include "window.h"
#include "fatal.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <format>

Window::Window(const char* title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fatal(std::format("SDL_Init failed: {}", SDL_GetError()));
    }

    m_window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!m_window) {
        fatal(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
    }
}

Window::~Window() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

std::vector<const char*> Window::vulkanExtensions() const {
    uint32_t count = 0;
    const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);

    if (!names) {
        fatal(std::format("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError()));
    }

    return std::vector<const char*>(names, names + count);
}

vk::raii::SurfaceKHR Window::createSurface(const vk::raii::Instance& instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    if (!SDL_Vulkan_CreateSurface(m_window, *instance, nullptr, &surface)) {
        fatal(std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError()));
    }

    // SDL made the handle, so hand it to a raii object that will clean it up
    return vk::raii::SurfaceKHR(instance, surface);
}

vk::Extent2D Window::pixelSize() const {
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);

    return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}
