#pragma once

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL3/SDL.h>

#include <cstdlib>
#include <source_location>

inline void vkCheck(VkResult r, std::source_location loc = std::source_location::current()) {
	if (r != VK_SUCCESS) {
		SDL_Log("Vulkan error %s at %s:%u",
				string_VkResult(r), loc.file_name(),
				static_cast<unsigned>(loc.line()));
		std::abort();
	}
}