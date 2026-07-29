#pragma once

#include <volk.h>
#include <vector>
#include <type_traits>

#include "vk_check.h"

template <typename T, typename Fn, typename... Args>
std::vector<T> vkEnumerate(Fn fn, Args... args) {
	uint32_t count = 0;
	std::vector<T> out;

	if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args..., uint32_t*, T*>>) {
		fn(args..., &count, nullptr);
		out.resize(count);
		fn(args..., &count, out.data());
	} else {
		VkResult r = fn(args..., &count, nullptr);
		if (r < 0) vkCheck(r);

		out.resize(count);
		if (count > 0) {
			r = fn(args..., &count, out.data());
			if (r < 0) vkCheck(r);
		}
	}

	return out;
}