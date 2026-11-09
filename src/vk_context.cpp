#include "vk_context.h"
#include <cstring>
#include <vector>
#include "vk_check.h"
#include "vk_enumerate.h"
#include <volk.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_vulkan.h>
#include <vk_mem_alloc.h>
#include "fatal.h"

VulkanContext::VulkanContext(SDL_Window* window) {
    if (volkInitialize() != VK_SUCCESS) {
        fatal("volkInitialize failed, is the Vulkan runtime installed?");
    }

    uint32_t const v = volkGetInstanceVersion();
    SDL_Log("volk initialized, instance version: %u.%u.%u", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v),
            VK_API_VERSION_PATCH(v));

    createInstance();
    createSurface(window);
    pickPhysicalDevice();
    createDevice();
    createAllocator();
}

VulkanContext::~VulkanContext() {
    destroy();
}

void VulkanContext::destroy() noexcept {
    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }
    if (m_device) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface) {
        SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_messenger) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
        m_messenger = VK_NULL_HANDLE;
    }
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanContext::createInstance() {
    // Describe application
    const VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Lightweaver",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .pEngineName = "Lightweaver",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    // Build extension list
    uint32_t extensionCount = 0;
    const char* const* extensionsBegin = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionsBegin) {
        fatal(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }

    std::vector<const char*> extensions(extensionsBegin, extensionsBegin + extensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateFlags instanceFlags = 0;

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    instanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    SDL_Log("found %zu instance extensions:", extensions.size());
    for (const char* extension : extensions) {
        SDL_Log("  %s", extension);
    }

    // Build layer list
    std::vector<const char*> requestedLayers = {"VK_LAYER_KHRONOS_validation"};
    std::vector<VkLayerProperties> layers = vkEnumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

    SDL_Log("found %zu instance layers:", layers.size());
    bool validationFound = false;
    for (const VkLayerProperties& layerProperties : layers) {
        SDL_Log("  %s", layerProperties.layerName);
        if (std::strcmp(layerProperties.layerName, requestedLayers[0]) == 0) {
            validationFound = true;
        }
    }

    if (!validationFound) {
        SDL_Log("validation layer unavailable, continuing without it");
        requestedLayers.clear();
    }

    // Describe the debugger
    const VkDebugUtilsMessengerCreateInfoEXT debugInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
        .pUserData = this,
    };

    { // INSTANCE
        const VkInstanceCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = &debugInfo,
            .flags = instanceFlags,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
            .ppEnabledLayerNames = requestedLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };
        vkCheck(vkCreateInstance(&info, nullptr, &m_instance));
    }

    // Load every function pointer from the new instance
    volkLoadInstance(m_instance);

    // Create the persistent messenger
    vkCheck(vkCreateDebugUtilsMessengerEXT(m_instance, &debugInfo, nullptr, &m_messenger));
}

void VulkanContext::createSurface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &m_surface)) {
        fatal(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
}

void VulkanContext::pickPhysicalDevice() {
    std::vector<VkPhysicalDevice> physicalDevices =
        vkEnumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, m_instance);

    SDL_Log("found %zu physical devices:", physicalDevices.size());

    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    uint32_t fallbackFamily = UINT32_MAX;

    for (const VkPhysicalDevice& physicalDevice : physicalDevices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        SDL_Log("  %s", properties.deviceName);

        uint32_t graphicsFamily = UINT32_MAX;
        if (!isDeviceSuitable(physicalDevice, graphicsFamily)) {
            continue;
        }

        // Prefer discrete GPU
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physicalDevice = physicalDevice;
            m_graphicsQueueFamily = graphicsFamily;
            SDL_Log("    suitable (discrete) selected");
            continue;
        }

        // Prepare to fallback to integrated GPU
        SDL_Log("    suitable (non-discrete) keeping as fallback");
        if (fallback == VK_NULL_HANDLE) {
            fallback = physicalDevice;
            fallbackFamily = graphicsFamily;
        }
    }

    // Fallback to integrated GPU
    if (m_physicalDevice == VK_NULL_HANDLE) {
        m_physicalDevice = fallback;
        m_graphicsQueueFamily = fallbackFamily;
    }

    // No GPU found
    if (m_physicalDevice == VK_NULL_HANDLE) {
        fatal("no suitable physical device found");
    }

    VkPhysicalDeviceProperties selected;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &selected);
    SDL_Log("using GPU: %s (queue family %u)", selected.deviceName, m_graphicsQueueFamily);
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice physicalDevice, uint32_t& outGraphicsFamily) const {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    // 1. API version
    if (properties.apiVersion < VK_API_VERSION_1_3) {
        SDL_Log("    rejected: Vulkan version too old");
        return false;
    }

    // 2. A queue family supporting both graphics and presentation
    std::vector<VkQueueFamilyProperties> queueFamilies =
        vkEnumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, physicalDevice);

    uint32_t graphicsFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        VkBool32 canPresent = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_surface, &canPresent);

        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent) {
            graphicsFamily = i;
            break;
        }
    }
    if (graphicsFamily == UINT32_MAX) {
        SDL_Log("    rejected: no graphics+present queue family");
        return false;
    }

    // 3. Swapchain extension
    std::vector<VkExtensionProperties> extensions =
        vkEnumerate<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, physicalDevice, nullptr);

    bool hasSwapchain = false;
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            break;
        }
    }
    if (!hasSwapchain) {
        SDL_Log("    rejected: no swapchain extension");
        return false;
    }

    // 4. Required Vulkan 1.2 / 1.3 features
    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };

    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12,
    };

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    if (!features13.dynamicRendering || !features13.synchronization2) {
        SDL_Log("    rejected: missing dynamicRendering or synchronization2");
        return false;
    }
    if (!features12.timelineSemaphore || !features12.bufferDeviceAddress) {
        SDL_Log("    rejected: missing timelineSemaphore or bufferDeviceAddress");
        return false;
    }

    outGraphicsFamily = graphicsFamily;
    return true;
}

void VulkanContext::createDevice() {
    // Features to enable
    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
        .timelineSemaphore = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12,
    };

    // One graphics queue
    const float queuePriority = 1.0f;

    const VkDeviceQueueCreateInfo queueInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_graphicsQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    // Device extensions
    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef __APPLE__
    std::vector<VkExtensionProperties> available =
        vkEnumerate<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, m_physicalDevice, nullptr);

    for (const VkExtensionProperties& extension : available) {
        if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
            deviceExtensions.push_back("VK_KHR_portability_subset");
            break;
        }
    }
#endif

    { // DEVICE
        const VkDeviceCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &features2,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
        };
        vkCheck(vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device));
    }

    volkLoadDevice(m_device);
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);

    SDL_Log("logical device created, graphics queue from family %u", m_graphicsQueueFamily);
}

void VulkanContext::createAllocator() {
    VmaVulkanFunctions functions{};

    VmaAllocatorCreateInfo info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m_physicalDevice,
        .device = m_device,
        .instance = m_instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    vmaImportVulkanFunctionsFromVolk(&info, &functions);
    info.pVulkanFunctions = &functions;

    vkCheck(vmaCreateAllocator(&info, &m_allocator));

    SDL_Log("VMA allocator created");
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                            VkDebugUtilsMessageTypeFlagsEXT type,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                            void* pUserData) {
    (void)type;
    (void)pUserData;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[vk] %s", pCallbackData->pMessage);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[vk] %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}
