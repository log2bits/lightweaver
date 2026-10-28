#pragma once
#include "vk_swapchain.h"
#include <vector>
#include <array>
class VulkanRenderer {
    static constexpr uint32_t kFramesInFlight = 2;

public:
    VulkanRenderer(const VulkanContext&, VulkanSwapchain&);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;

private:
    void create();
    void destroy() noexcept;

    const VulkanContext& m_context;
    VulkanSwapchain& m_swapchain;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kFramesInFlight> m_commandBuffers{};
    std::array<VkSemaphore, kFramesInFlight> m_imageAvailable{};
    std::array<VkFence, kFramesInFlight> m_inFlight{};
    std::vector<VkSemaphore> m_renderFinished;
    uint32_t m_frameIndex = 0;
};