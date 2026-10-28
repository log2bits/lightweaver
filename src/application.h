#pragma once

#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_renderer.h"
#include "window.h"

class Application {
public:
    Application();
    ~Application() = default;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

private:
    Window m_window;
    VulkanContext m_context;
    VulkanSwapchain m_swapchain;
    VulkanRenderer m_renderer;
    bool m_running = false;
};
