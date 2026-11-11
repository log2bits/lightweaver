#include "vk_renderer.h"
#include "fatal.h"

VulkanRenderer::VulkanRenderer(const VulkanContext& context, VulkanSwapchain& swapchain) : m_context(context), m_swapchain(swapchain) {
    create();
}

VulkanRenderer::~VulkanRenderer() {
    // Gotta wait until nothing is in use, then raii handles the rest. waitIdle throws on device
    // loss, and a throw out of a destructor terminates, so swallow it. We are shutting down anyway
    try {
        m_context.device().waitIdle();
    } catch (...) {
    }
}

void VulkanRenderer::create() {
    // COMMAND POOL
    const auto poolInfo = vk::CommandPoolCreateInfo()
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(m_context.graphicsFamily());

    m_commandPool = vk::raii::CommandPool(m_context.device(), poolInfo);

    // COMMAND BUFFERS (1 per frame)
    const auto bufferInfo = vk::CommandBufferAllocateInfo()
        .setCommandPool(*m_commandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(kFramesInFlight);

    m_commandBuffers = m_context.device().allocateCommandBuffers(bufferInfo);

    // SEMAPHORES AND FENCES (1 of each per frame slot)
    const auto semaphoreInfo = vk::SemaphoreCreateInfo();

    // start signaled, otherwise we deadlock
    const auto fenceInfo = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);

    m_imageAvailable.reserve(kFramesInFlight);
    m_inFlight.reserve(kFramesInFlight);

    for (uint32_t i = 0; i < kFramesInFlight; i++) {
        m_imageAvailable.emplace_back(m_context.device(), semaphoreInfo);
        m_inFlight.emplace_back(m_context.device(), fenceInfo);
    }

    createPresentSemaphores();
}

void VulkanRenderer::createPresentSemaphores() {
    // PRESENT SEMAPHORES (1 per swapchain image, not per frame slot)
    const auto semaphoreInfo = vk::SemaphoreCreateInfo();

    m_renderFinished.clear();
    m_renderFinished.reserve(m_swapchain.images().size());

    for (size_t i = 0; i < m_swapchain.images().size(); i++) {
        m_renderFinished.emplace_back(m_context.device(), semaphoreInfo);
    }
}

void VulkanRenderer::recreateSwapchain() {
    // recreate() waits for the device to go idle, so the old semaphores are free to drop
    m_swapchain.recreate();
    createPresentSemaphores();
    m_frameIndex = 0;
}

void VulkanRenderer::drawFrame() {
    // A zero sized window leaves us with no swapchain, so keep trying until it comes back
    if (!m_swapchain.valid()) {
        recreateSwapchain();
        return;
    }

    const vk::raii::Device& device = m_context.device();
    const vk::raii::Fence& inFlight = m_inFlight[m_frameIndex];

    // The whole colour image, which is all either barrier below touches
    const auto wholeColor = vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setLevelCount(1)
        .setLayerCount(1);

    // WAIT FOR THIS FRAME SLOT TO FINISH ITS LAST FRAME
    if (device.waitForFences(*inFlight, vk::True, UINT64_MAX) != vk::Result::eSuccess) {
        fatal("timed out waiting on an in-flight fence");
    }

    // ACQUIRE AN IMAGE (the index comes back now, the semaphore is signaled later)
    const auto [acquireResult, imageIndex] = m_swapchain.swapchain().acquireNextImage(UINT64_MAX, *m_imageAvailable[m_frameIndex]);

    if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapchain();
        return;
    }

    // Reset after the acquire, so bailing out above cannot leave an unsignaled fence
    device.resetFences(*inFlight);

    const vk::raii::CommandBuffer& cmd = m_commandBuffers[m_frameIndex];
    cmd.reset();
    cmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // UNDEFINED -> COLOR ATTACHMENT (undefined as the old layout throws the old pixels away)
    // The src stage has to match the stage imageAvailable is waited at, or the transition
    // is not ordered against the acquire and can clobber an image still being read
    const auto toColorAttachment = vk::ImageMemoryBarrier2()
        .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setImage(m_swapchain.images()[imageIndex])
        .setSubresourceRange(wholeColor);

    cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(toColorAttachment));

    // CLEAR THE SCREEN (the clear is the load op, not a command)
    const auto colorAttachment = vk::RenderingAttachmentInfo()
        .setImageView(*m_swapchain.views()[imageIndex])
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.02f, 0.02f, 0.05f, 1.0f});

    const auto renderInfo = vk::RenderingInfo()
        .setRenderArea({{0, 0}, m_swapchain.extent()})
        .setLayerCount(1)
        .setColorAttachments(colorAttachment);

    cmd.beginRendering(renderInfo);

    // Nothing drawn yet, the triangle goes here

    cmd.endRendering();

    // COLOR ATTACHMENT -> PRESENT SRC
    const auto toPresentSrc = vk::ImageMemoryBarrier2()
        .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
        .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setImage(m_swapchain.images()[imageIndex])
        .setSubresourceRange(wholeColor);

    cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(toPresentSrc));

    cmd.end();

    // SUBMIT. Waiting at colour output, not top of pipe, lets vertex work start early
    const auto waitInfo = vk::SemaphoreSubmitInfo()
        .setSemaphore(*m_imageAvailable[m_frameIndex])
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    const auto signalInfo = vk::SemaphoreSubmitInfo()
        .setSemaphore(*m_renderFinished[imageIndex])
        .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    const auto cmdInfo = vk::CommandBufferSubmitInfo().setCommandBuffer(*cmd);

    const auto submitInfo = vk::SubmitInfo2()
        .setWaitSemaphoreInfos(waitInfo)
        .setCommandBufferInfos(cmdInfo)
        .setSignalSemaphoreInfos(signalInfo);

    m_context.graphicsQueue().submit2(submitInfo, *inFlight);

    // PRESENT
    const vk::Semaphore renderFinished = *m_renderFinished[imageIndex];
    const vk::SwapchainKHR swapchain = *m_swapchain.swapchain();

    const auto presentInfo = vk::PresentInfoKHR()
        .setWaitSemaphores(renderFinished)
        .setSwapchains(swapchain)
        .setImageIndices(imageIndex);

    // Suboptimal is a success code, so testing for an error here would miss it
    const vk::Result presentResult = m_context.graphicsQueue().presentKHR(presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        recreateSwapchain();
        return;
    }

    m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
}
