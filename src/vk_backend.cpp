#include "vk_backend.h"
#include <cstring>
#include <vector>
#include "vk_check.h"
#include "vk_enumerate.h"
#include <volk.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_vulkan.h>
#include <vk_mem_alloc.h>


bool VulkanBackend::init(SDL_Window *window) {
    if (volkInitialize() != VK_SUCCESS) {
        SDL_Log("volkInitialize failed, is the Vulkan runtime installed?");
        return false;
    }

    uint32_t const v = volkGetInstanceVersion();
    SDL_Log("volk initialized, instance version: %u.%u.%u",
            VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));

    if (!createInstance()) return false;
    if (!createSurface(window)) return false;
    if (!pickPhysicalDevice()) return false;
    if (!createDevice()) return false;
    if (!createAllocator()) return false;
    return true;
}

void VulkanBackend::shutdown() {
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

bool VulkanBackend::createInstance() {
    // Describe application
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Lightweaver";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "Lightweaver";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Build extension list
    uint32_t sdlExtCount = 0;
    const char *const*sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExts) {
        SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
        return false;
    }

    std::vector<const char *> extensions(sdlExts, sdlExts + sdlExtCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    SDL_Log("found %zu instance extensions:", extensions.size());
    for (const char *ext: extensions) {
        SDL_Log("  %s", ext);
    }

    // Build layer list
    std::vector<const char *> valid_layers = {"VK_LAYER_KHRONOS_validation"};
    std::vector<VkLayerProperties> layers = vkEnumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

    SDL_Log("found %zu instance layers:", layers.size());
    bool validationFound = false;
    for (const VkLayerProperties &layerProperties: layers) {
        SDL_Log("  %s", layerProperties.layerName);
        if (std::strcmp(layerProperties.layerName, valid_layers[0]) == 0) {
            validationFound = true;
        }
    }

    if (!validationFound) {
        SDL_Log("validation layer unavailable, continuing without it");
        valid_layers.clear();
    }

    // Describe the debugger
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;
    debugInfo.pUserData = this;

    // Describe the instance
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = &debugInfo;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(valid_layers.size());
    createInfo.ppEnabledLayerNames = valid_layers.data();

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    // Create it
    vkCheck(vkCreateInstance(&createInfo, nullptr, &m_instance));

    // Load every function pointer from the new instance
    volkLoadInstance(m_instance);

    // Create the persistent messenger
    vkCheck(vkCreateDebugUtilsMessengerEXT(m_instance, &debugInfo, nullptr, &m_messenger));

    return true;
}

bool VulkanBackend::createSurface(SDL_Window *window) {
    if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &m_surface)) {
        SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool VulkanBackend::pickPhysicalDevice() {
    std::vector<VkPhysicalDevice> physicalDevices = vkEnumerate<VkPhysicalDevice>(
        vkEnumeratePhysicalDevices, m_instance);

    SDL_Log("found %zu physical devices:", physicalDevices.size());

    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    uint32_t fallbackFamily = UINT32_MAX;

    for (const VkPhysicalDevice &physicalDevice: physicalDevices) {
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
        SDL_Log("no suitable physical device found");
        return false;
    }

    VkPhysicalDeviceProperties selected;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &selected);
    SDL_Log("using GPU: %s (queue family %u)", selected.deviceName, m_graphicsQueueFamily);

    return true;
}

bool VulkanBackend::isDeviceSuitable(VkPhysicalDevice physicalDevice, uint32_t &outGraphicsFamily) const {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    // 1. API version
    if (properties.apiVersion < VK_API_VERSION_1_3) {
        SDL_Log("    rejected: Vulkan version too old");
        return false;
    }

    // 2. A queue family supporting both graphics and presentation
    std::vector<VkQueueFamilyProperties> queueFamilies =
            vkEnumerate<VkQueueFamilyProperties>(
                vkGetPhysicalDeviceQueueFamilyProperties, physicalDevice);

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
            vkEnumerate<VkExtensionProperties>(
                vkEnumerateDeviceExtensionProperties, physicalDevice, nullptr);

    bool hasSwapchain = false;
    for (const VkExtensionProperties &extension: extensions) {
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
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;

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

bool VulkanBackend::createDevice() {
    // Features to enable
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.timelineSemaphore = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.pNext = &features13;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;

    // One graphics queue
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    // Device extensions
    std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef __APPLE__
    std::vector<VkExtensionProperties> available =
            vkEnumerate<VkExtensionProperties>(
                vkEnumerateDeviceExtensionProperties, m_physicalDevice, nullptr);

    for (const VkExtensionProperties &extension: available) {
        if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
            deviceExtensions.push_back("VK_KHR_portability_subset");
            break;
        }
    }
#endif

    // Create the device
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features2;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceInfo.pEnabledFeatures = nullptr;

    vkCheck(vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device));

    volkLoadDevice(m_device);
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);

    SDL_Log("logical device created, graphics queue from family %u", m_graphicsQueueFamily);
    return true;
}

bool VulkanBackend::createAllocator() {
    VmaVulkanFunctions functions{};

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &functions);
    allocatorInfo.pVulkanFunctions = &functions;

    vkCheck(vmaCreateAllocator(&allocatorInfo, &m_allocator));

    SDL_Log("VMA allocator created");
    return true;
}


VKAPI_ATTR VkBool32 VKAPI_CALL VulkanBackend::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
    (void) type;
    (void) pUserData;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[vk] %s", pCallbackData->pMessage);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[vk] %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}
