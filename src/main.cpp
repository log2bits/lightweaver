#include "application.h"
#include "fatal.h"
#include <SDL3/SDL_main.h>
#include <cstdlib>
#include <exception>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Vulkan-Hpp reports failures by throwing and there is nothing to recover to, so die right here
    try {
        Application app;
        app.run();
    } catch (const std::exception& e) {
        fatal(e.what());
    }

    return EXIT_SUCCESS;
}
