cmake_minimum_required(VERSION 3.24)
project(lightweaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	SDL3
	GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
	GIT_TAG release-3.2.0
	GIT_SHALLOW TRUE
)

# glm
FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG 1.0.1
	GIT_SHALLOW TRUE
)

# Vulkan
find_package(Vulkan REQUIRED)

# VMA
FetchContent_Declare(
	vma
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
	GIT_TAG v3.4.0
	GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3 glm vma)

add_executable(
	lightweaver
	src/main.cpp
	src/application.h
	src/application.cpp
	src/vk_context.h
	src/vk_context.cpp
	src/window.h
	src/window.cpp
	src/vk_swapchain.h
	src/vk_swapchain.cpp
	src/vk_renderer.cpp
	src/vk_renderer.h
	src/vma_impl.cpp
	src/fatal.h
	src/log.h
)

target_link_libraries(lightweaver PRIVATE SDL3::SDL3-static glm::glm Vulkan::Headers GPUOpen::VulkanMemoryAllocator)

target_compile_definitions(lightweaver PRIVATE
    # No link against vulkan-1, Vulkan-Hpp loads every entry point at runtime
    VK_NO_PROTOTYPES
    # Resizing the window is normal, so do not let it throw out of acquire/present
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)

# VMA is third party, so stop /W4 from shouting about its internals
get_target_property(vma_include_dirs GPUOpen::VulkanMemoryAllocator INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(lightweaver SYSTEM PRIVATE ${vma_include_dirs})

if (MSVC)
    target_compile_options(lightweaver PRIVATE /W4 /permissive-)
else ()
    target_compile_options(lightweaver PRIVATE -Wall -Wextra)
endif ()