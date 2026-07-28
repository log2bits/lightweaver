#include "application.h"
#include <volk.h>
#include <SDL3/SDL.h>

bool Application::init() {
	if (!initWindow()) {
		return false;
	}
	// Future stuff here
	return true;
}

bool Application::initWindow() {
	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed - is the Vulkan runtime installed?");
		return false;
	}
	SDL_Log("volk initialized, instance version: %u", volkGetInstanceVersion());

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return false;
	}

	m_window = SDL_CreateWindow(
		"Lightweaver", 1280, 720,
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
	);

	if (!m_window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return false;
	}

	return true;
}

void Application::run() {
	m_running = true;
	SDL_Event event;

	while (m_running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				m_running = false;
			}
		}
		SDL_Delay(16);
	}
}

void Application::shutdown() {
	if (m_window) {
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
	SDL_Quit();
}