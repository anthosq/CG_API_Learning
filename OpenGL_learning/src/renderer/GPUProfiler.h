#pragma once
#include <glad/glad.h>

// Double-buffered GPU timestamp pair.
//
// GL_TIMESTAMP inserts a counter into the GPU command stream.
// Reading GL_QUERY_RESULT blocks until the GPU reaches that point.
// By maintaining two sets of query objects and reading the PREVIOUS
// frame's set, we avoid stalling: the GPU has a full frame to finish
// before we ask for its result.
//
// Usage (once per frame):
//   timer.Resolve();     // non-blocking: reads the frame-before-last result
//   GPU_TIMER_BEGIN(timer);
//   ... GPU commands ...
//   GPU_TIMER_END(timer);
//   double ms = timer.GetMs();  // returns the result resolved above
struct GPUTimer {
    GLuint q[2][2] = {};   // [doubleBuffer][0=begin, 1=end]
    double ms      = 0.0;
    int    cur     = 0;
    bool   valid   = false;
    bool   enabled = false;

    void Init() {
        glGenQueries(2, q[0]);
        glGenQueries(2, q[1]);
        // Prime both sets so the first Resolve() reads a well-defined value.
        glQueryCounter(q[0][0], GL_TIMESTAMP);
        glQueryCounter(q[0][1], GL_TIMESTAMP);
        glQueryCounter(q[1][0], GL_TIMESTAMP);
        glQueryCounter(q[1][1], GL_TIMESTAMP);
    }

    void Shutdown() {
        for (auto& pair : q) {
            if (pair[0]) { glDeleteQueries(2, pair); pair[0] = pair[1] = 0; }
        }
        ms    = 0.0;
        valid = false;
    }

    void Begin() { if (enabled) glQueryCounter(q[cur][0], GL_TIMESTAMP); }
    void End()   { if (enabled) glQueryCounter(q[cur][1], GL_TIMESTAMP); }

    // Non-blocking resolve: reads previous frame's result only if the GPU
    // has finished (GL_QUERY_RESULT_AVAILABLE), so heavy frames never stall.
    void Resolve() {
        if (!enabled) return;
        const int prev = cur ^ 1;
        GLint available = 0;
        glGetQueryObjectiv(q[prev][1], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLuint64 t0 = 0, t1 = 0;
            glGetQueryObjectui64v(q[prev][0], GL_QUERY_RESULT, &t0);
            glGetQueryObjectui64v(q[prev][1], GL_QUERY_RESULT, &t1);
            ms    = static_cast<double>(t1 > t0 ? t1 - t0 : 0) * 1e-6;
            valid = true;
        }
        cur ^= 1;
    }

    double GetMs()    const { return ms; }
    bool   IsValid()  const { return valid; }
    bool   IsEnabled() const { return enabled; }
};

// RAII wrapper: calls Begin() on construction, End() on destruction.
// Use GPU_PROFILE_SCOPE macro for convenient block-scoped timing.
struct GPUTimerScope {
    GPUTimer& t;
    explicit GPUTimerScope(GPUTimer& t_) : t(t_) { t.Begin(); }
    ~GPUTimerScope()                             { t.End();   }
};

// Macros
#define GPU_TIMER_BEGIN(t)   (t).Begin()
#define GPU_TIMER_END(t)     (t).End()
#define GPU_PROFILE_SCOPE(t) GPUTimerScope _gts_##__LINE__(t)
