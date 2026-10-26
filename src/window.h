#pragma once

struct SDL_Window;

class Window {
public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    SDL_Window* handle() const { return m_window; }

private:
    SDL_Window* m_window = nullptr;
};