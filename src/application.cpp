#include "application.h"
#include <SDL3/SDL.h>

Application::Application() : m_window("Lightweaver", 1280, 720), m_context(m_window.handle()), m_swapchain(m_window, m_context), m_renderer(m_context, m_swapchain) {}

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