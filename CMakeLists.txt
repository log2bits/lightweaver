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
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
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
FetchContent_Declare(
	volk
	GIT_REPOSITORY https://github.com/zeux/volk.git
	GIT_TAG 1.4.350
	GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3 glm volk)

add_executable(
	lightweaver src/main.cpp
	src/application.h
	src/application.cpp
)

target_link_libraries(lightweaver PRIVATE SDL3::SDL3-static glm::glm volk::volk)
target_compile_definitions(lightweaver PRIVATE VK_NO_PROTOTYPES)

if (MSVC)
	target_compile_options(lightweaver PRIVATE /W4 /permissive-)
else ()
	target_compile_options(lightweaver PRIVATE -Wall -Wextra)
endif ()