#include "application.h"
#include <SDL3/SDL.h>

Application::Application() : m_window("Lightweaver", 1280, 720), m_context(m_window), m_swapchain(m_window, m_context), m_renderer(m_context, m_swapchain) {}

void Application::run() {
    m_running = true;
    SDL_Event event;

    while (m_running) {
        // A minimized window has no pixels, so block on an event instead of spinning
        if (m_minimized && SDL_WaitEvent(&event)) {
            handleEvent(event);
        }

        while (SDL_PollEvent(&event)) {
            handleEvent(event);
        }

        if (!m_minimized) {
            m_renderer.drawFrame();
        }
    }
}

void Application::handleEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        m_minimized = true;
        break;
    case SDL_EVENT_WINDOW_RESTORED:
        m_minimized = false;
        break;
    default:
        break;
    }
}
