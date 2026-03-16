// ============================================================================
// editor_main.cpp - GLRenderer Editor Entry Point
// ============================================================================

#include "editor/EditorApp.h"
#include <iostream>

int main() {
    try {
        std::cout << "  GLRenderer Editor" << std::endl;

        GLRenderer::EditorApp app;
        app.Run();
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
