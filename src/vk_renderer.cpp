#include "vk_renderer.h"

#include "vk_check.h"

VulkanRenderer::VulkanRenderer(const VulkanContext& context, VulkanSwapchain& swapchain) : m_context(context), m_swapchain(swapchain) {
    create();
}

VulkanRenderer::~VulkanRenderer() {
    destroy();
}

void VulkanRenderer::create() {
    { // COMMAND POOL
        const VkCommandPoolCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_context.graphicsFamily(),
        };
        vkCheck(vkCreateCommandPool(m_context.device(), &info, nullptr, &m_commandPool));
    }

    { // COMMAND BUFFERS (1 per frame)
        const VkCommandBufferAllocateInfo info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = kFramesInFlight,
        };
        vkCheck(vkAllocateCommandBuffers(m_context.device(), &info, m_commandBuffers.data()));
    }

    { // SEMAPHORES (1 acquire per frame slot, 1 present per swapchain image)
        const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        for (VkSemaphore& semaphore : m_imageAvailable) {
            vkCheck(vkCreateSemaphore(m_context.device(), &info, nullptr, &semaphore));
        }

        m_renderFinished.resize(m_swapchain.images().size(), VK_NULL_HANDLE);
        for (VkSemaphore& semaphore : m_renderFinished) {
            vkCheck(vkCreateSemaphore(m_context.device(), &info, nullptr, &semaphore));
        }
    }

    { // FENCES (start signaled, otherwise we deadlock)
        const VkFenceCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (VkFence& fence : m_inFlight) {
            vkCheck(vkCreateFence(m_context.device(), &info, nullptr, &fence));
        }
    }
}

void VulkanRenderer::destroy() noexcept {
    vkDeviceWaitIdle(m_context.device()); // Gotta wait until nothing is in use

    // FENCES
    for (VkFence& fence : m_inFlight) {
        vkDestroyFence(m_context.device(), fence, nullptr);
        fence = VK_NULL_HANDLE;
    }

    // SEMAPHORES
    for (VkSemaphore& semaphore : m_imageAvailable) {
        vkDestroySemaphore(m_context.device(), semaphore, nullptr);
        semaphore = VK_NULL_HANDLE;
    }
    for (VkSemaphore semaphore : m_renderFinished) {
        vkDestroySemaphore(m_context.device(), semaphore, nullptr);
    }
    m_renderFinished.clear();

    // COMMAND POOL (frees its command buffers)
    if (m_commandPool) {
        vkDestroyCommandPool(m_context.device(), m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
    m_commandBuffers.fill(VK_NULL_HANDLE);
    m_frameIndex = 0;
}
