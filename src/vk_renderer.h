#pragma once
#include "vk_swapchain.h"
#include <vector>
class VulkanRenderer {
    static constexpr uint32_t kFramesInFlight = 2;

public:
    VulkanRenderer(const VulkanContext&, VulkanSwapchain&);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;

    void drawFrame();

private:
    void create();
    void createPresentSemaphores();
    void recreateSwapchain();

    const VulkanContext& m_context;
    VulkanSwapchain& m_swapchain;

    // The buffers are declared after the pool so they are destroyed before it
    vk::raii::CommandPool m_commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> m_commandBuffers;
    std::vector<vk::raii::Semaphore> m_imageAvailable;
    std::vector<vk::raii::Fence> m_inFlight;
    std::vector<vk::raii::Semaphore> m_renderFinished;
    uint32_t m_frameIndex = 0;
};
