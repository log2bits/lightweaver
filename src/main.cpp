#include "application.h"
#include <SDL3/SDL_main.h>
#include <cstdlib>

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	Application app;

	if (!app.init()) {
		app.shutdown();
		return EXIT_FAILURE;
	}

	app.run();
	app.shutdown();

	return EXIT_SUCCESS;
}