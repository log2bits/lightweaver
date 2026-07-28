#pragma once

struct SDL_Window;

class Application {
public:
	Application() = default;
	~Application() = default;

	Application(const Application&) = delete; // copy constructor (doesnt exist yet)
	Application& operator=(const Application&) = delete; // copy assignment (getting overwritten)

	bool init();
	void run();
	void shutdown();

private:
	bool initWindow();

	SDL_Window* m_window  = nullptr;
	bool m_running = false;
};