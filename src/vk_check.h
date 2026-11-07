#pragma once

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <source_location>
#include "fatal.h"

inline void vkCheck(VkResult r, std::source_location loc = std::source_location::current()) {
    if (r != VK_SUCCESS) {
        fatal(string_VkResult(r), loc);
    }
}
