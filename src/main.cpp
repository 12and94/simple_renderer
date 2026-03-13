#include "app/Application.h"

#include "core/Log.h"

#include <exception>

int main() {
    try {
        vr::Application app;
        app.run();
    } catch (const std::exception& e) {
        vr::log::error(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

