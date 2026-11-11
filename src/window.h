#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vector>

struct SDL_Window;

class Window {
public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    SDL_Window* handle() const { return m_window; }

    // The three places Vulkan and SDL have to meet
    std::vector<const char*> vulkanExtensions() const;
    vk::raii::SurfaceKHR createSurface(const vk::raii::Instance& instance) const;
    vk::Extent2D pixelSize() const;

private:
    SDL_Window* m_window = nullptr;
};
