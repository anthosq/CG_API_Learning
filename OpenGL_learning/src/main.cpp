// GLRenderer - 主入口点

#include "app/SandboxApp.h"
#include <iostream>

int main() {
    try {
        GLRenderer::SandboxApp app;
        app.Run();
    }
    catch (const std::exception& e) {
        std::cerr << "[致命错误] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
