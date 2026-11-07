#include "vk_renderer.h"

VulkanRenderer::VulkanRenderer(const VulkanContext& context, VulkanSwapchain& swapchain) : m_context(context), m_swapchain(swapchain) {
    create();
}

VulkanRenderer::~VulkanRenderer() {
    destroy();
}

void VulkanRenderer::create() {

}

void VulkanRenderer::destroy() noexcept {

}