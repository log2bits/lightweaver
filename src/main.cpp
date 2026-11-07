#include "application.h"
#include <SDL3/SDL_main.h>
#include <cstdlib>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Application app;
    app.run();

    return EXIT_SUCCESS;
}
