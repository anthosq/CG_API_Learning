#pragma once
#include "gl_common.h"

class Renderer {
public:
    void Clear() const {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void Render() const {
        // Rendering code would go here
    }


};