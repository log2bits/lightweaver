#include "application.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_log.h>
#include <cstdlib>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        Application app;
        app.run();
    } catch (const std::exception& e) {
        SDL_Log("fatal: %s", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
