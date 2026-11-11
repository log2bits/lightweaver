#include "vk_context.h"
#include <cstring>
#include <format>
#include <vector>
#include <vk_mem_alloc.h>
#include "window.h"
#include "fatal.h"
#include "log.h"

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
constexpr uint32_t kVulkanVersion = vk::ApiVersion13;
constexpr uint32_t kEngineVersion = vk::makeVersion(0, 1, 0);

} // namespace

VulkanContext::VulkanContext(const Window& window) {
    const uint32_t vulkanLoaderVersion = m_loader.enumerateInstanceVersion();
    logInfo(std::format("Vulkan loader found, instance version: {}.{}.{}", vk::apiVersionMajor(vulkanLoaderVersion), vk::apiVersionMinor(vulkanLoaderVersion), vk::apiVersionPatch(vulkanLoaderVersion)));

    createInstance(window);
    m_surface = window.createSurface(m_instance);
    pickPhysicalDevice();
    createDevice();
    createAllocator();
}

VulkanContext::~VulkanContext() {
    // VMA is a C library, so this handle needs to be cleaned manually. Everything else is raii
    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
    }
}

void VulkanContext::createInstance(const Window& window) {
    // Describe application
    const auto appInfo = vk::ApplicationInfo()
        .setPApplicationName("Lightweaver")
        .setApplicationVersion(kEngineVersion)
        .setPEngineName("Lightweaver")
        .setEngineVersion(kEngineVersion)
        .setApiVersion(kVulkanVersion);

    // Build extension list
    std::vector<const char*> extensions = window.vulkanExtensions();
    extensions.push_back(vk::EXTDebugUtilsExtensionName);

    vk::InstanceCreateFlags instanceFlags{};

#ifdef __APPLE__
    extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
    instanceFlags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

    logInfo(std::format("found {} instance extensions:", extensions.size()));
    for (const char* extension : extensions) {
        logInfo(std::format("  {}", extension));
    }

    // Build layer list
    std::vector<const char*> requestedLayers = {kValidationLayer};
    const std::vector<vk::LayerProperties> layers = m_loader.enumerateInstanceLayerProperties();

    logInfo(std::format("found {} instance layers:", layers.size()));
    bool validationFound = false;
    for (const vk::LayerProperties& layerProperties : layers) {
        logInfo(std::format("  {}", layerProperties.layerName.data()));
        if (std::strcmp(layerProperties.layerName, kValidationLayer) == 0) {
            validationFound = true;
        }
    }

    if (!validationFound) {
        logWarn("validation layer unavailable, continuing without it");
        requestedLayers.clear();
    }

    // Describe the debugger
    using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MsgType = vk::DebugUtilsMessageTypeFlagBitsEXT;

    const auto debugInfo = vk::DebugUtilsMessengerCreateInfoEXT()
        .setMessageSeverity(Severity::eWarning | Severity::eError)
        .setMessageType(MsgType::eGeneral | MsgType::eValidation | MsgType::ePerformance)
        .setPfnUserCallback(debugCallback)
        .setPUserData(this);

    const auto instanceInfo = vk::InstanceCreateInfo()
        .setPNext(&debugInfo)
        .setFlags(instanceFlags)
        .setPApplicationInfo(&appInfo)
        .setPEnabledLayerNames(requestedLayers)
        .setPEnabledExtensionNames(extensions);

    m_instance = vk::raii::Instance(m_loader, instanceInfo);

    // Create the persistent messenger
    m_messenger = vk::raii::DebugUtilsMessengerEXT(m_instance, debugInfo);
}

void VulkanContext::pickPhysicalDevice() {
    const std::vector<vk::raii::PhysicalDevice> physicalDevices = m_instance.enumeratePhysicalDevices();

    logInfo(std::format("found {} physical devices:", physicalDevices.size()));

    size_t chosen = physicalDevices.size();
    size_t fallback = physicalDevices.size();
    uint32_t chosenFamily = UINT32_MAX;
    uint32_t fallbackFamily = UINT32_MAX;

    for (size_t i = 0; i < physicalDevices.size(); i++) {
        const vk::PhysicalDeviceProperties properties = physicalDevices[i].getProperties();
        logInfo(std::format("  {}", properties.deviceName.data()));

        uint32_t graphicsFamily = UINT32_MAX;
        if (!isDeviceSuitable(physicalDevices[i], graphicsFamily)) {
            continue;
        }

        // Prefer discrete GPU
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            chosen = i;
            chosenFamily = graphicsFamily;
            logInfo("    suitable (discrete) selected");
            continue;
        }

        // Prepare to fallback to integrated GPU
        logInfo("    suitable (non-discrete) keeping as fallback");
        if (fallback == physicalDevices.size()) {
            fallback = i;
            fallbackFamily = graphicsFamily;
        }
    }

    // Fallback to integrated GPU
    if (chosen == physicalDevices.size()) {
        chosen = fallback;
        chosenFamily = fallbackFamily;
    }

    // No GPU found
    if (chosen == physicalDevices.size()) {
        fatal("no suitable physical device found");
    }

    m_physicalDevice = physicalDevices[chosen];
    m_graphicsQueueFamily = chosenFamily;

    const vk::PhysicalDeviceProperties selected = m_physicalDevice.getProperties();
    logInfo(std::format("using GPU: {} (queue family {})", selected.deviceName.data(), m_graphicsQueueFamily));
}

bool VulkanContext::isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, uint32_t& outGraphicsFamily) const {
    const vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

    // 1. API version
    if (properties.apiVersion < kVulkanVersion) {
        logInfo("    rejected: Vulkan version too old");
        return false;
    }

    // 2. A queue family supporting both graphics and presentation
    const std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();

    uint32_t graphicsFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        const bool canPresent = physicalDevice.getSurfaceSupportKHR(i, *m_surface) == vk::True;
        if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) && canPresent) {
            graphicsFamily = i;
            break;
        }
    }
    if (graphicsFamily == UINT32_MAX) {
        logInfo("    rejected: no graphics+present queue family");
        return false;
    }

    // 3. Swapchain extension
    const std::vector<vk::ExtensionProperties> extensions = physicalDevice.enumerateDeviceExtensionProperties();

    bool hasSwapchain = false;
    for (const vk::ExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, vk::KHRSwapchainExtensionName) == 0) {
            hasSwapchain = true;
            break;
        }
    }
    if (!hasSwapchain) {
        logInfo("    rejected: no swapchain extension");
        return false;
    }

    // 4. Required Vulkan 1.2 / 1.3 features. StructureChain wires up the pNext links for us
    const auto chain = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>();
    const vk::PhysicalDeviceVulkan12Features& features12 = chain.get<vk::PhysicalDeviceVulkan12Features>();
    const vk::PhysicalDeviceVulkan13Features& features13 = chain.get<vk::PhysicalDeviceVulkan13Features>();

    if (!features13.dynamicRendering || !features13.synchronization2) {
        logInfo("    rejected: missing dynamicRendering or synchronization2");
        return false;
    }
    if (!features12.timelineSemaphore || !features12.bufferDeviceAddress) {
        logInfo("    rejected: missing timelineSemaphore or bufferDeviceAddress");
        return false;
    }

    outGraphicsFamily = graphicsFamily;
    return true;
}

void VulkanContext::createDevice() {
    // One graphics queue. setQueuePriorities fills in queueCount from the size
    const float queuePriority = 1.0f;

    const auto queueInfo = vk::DeviceQueueCreateInfo()
        .setQueueFamilyIndex(m_graphicsQueueFamily)
        .setQueuePriorities(queuePriority);

    // Device extensions
    std::vector<const char*> deviceExtensions = {vk::KHRSwapchainExtensionName};

#ifdef __APPLE__
    const std::vector<vk::ExtensionProperties> available = m_physicalDevice.enumerateDeviceExtensionProperties();

    for (const vk::ExtensionProperties& extension : available) {
        if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
            deviceExtensions.push_back("VK_KHR_portability_subset");
            break;
        }
    }
#endif

    // DEVICE (the features to enable hang off the pNext chain)
    const auto deviceInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfos(queueInfo)
        .setPEnabledExtensionNames(deviceExtensions);

    const auto features12 = vk::PhysicalDeviceVulkan12Features()
        .setTimelineSemaphore(vk::True)
        .setBufferDeviceAddress(vk::True);

    const auto features13 = vk::PhysicalDeviceVulkan13Features()
        .setSynchronization2(vk::True)
        .setDynamicRendering(vk::True);

    // StructureChain copies these and wires their pNext pointers together
    vk::StructureChain deviceChain{deviceInfo, features12, features13};

    m_device = vk::raii::Device(m_physicalDevice, deviceChain.get<vk::DeviceCreateInfo>());

    m_graphicsQueue = m_device.getQueue(m_graphicsQueueFamily, 0);

    logInfo(std::format("logical device created, graphics queue from family {}", m_graphicsQueueFamily));
}

void VulkanContext::createAllocator() {
    // Hand VMA the two lookup functions and it loads the rest of what it needs itself
    const VmaVulkanFunctions functions{
        .vkGetInstanceProcAddr = m_instance.getDispatcher()->vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = m_device.getDispatcher()->vkGetDeviceProcAddr,
    };

    // VMA is a C struct with no setters, so this one stays a designated initializer
    const VmaAllocatorCreateInfo info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = *m_physicalDevice,
        .device = *m_device,
        .pVulkanFunctions = &functions,
        .instance = *m_instance,
        .vulkanApiVersion = kVulkanVersion,
    };

    const VkResult r = vmaCreateAllocator(&info, &m_allocator);
    if (r != VK_SUCCESS) {
        fatal(std::format("vmaCreateAllocator failed: {}", vk::to_string(static_cast<vk::Result>(r))));
    }

    logInfo("VMA allocator created");
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanContext::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    (void)type;
    (void)pUserData;

    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        logError(std::format("[vk] {}", pCallbackData->pMessage));
    } else {
        logWarn(std::format("[vk] {}", pCallbackData->pMessage));
    }

    return VK_FALSE;
}
