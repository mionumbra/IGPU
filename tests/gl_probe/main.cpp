#include "wgl_host.h"

#include <Windows.h>
#include <GL/gl.h>

#include <cstdio>
#include <string>

int main()
{
    std::string error;
    if (!gl_probe_begin(error))
    {
        std::fprintf(stderr, "gl host failed: %s\n", error.c_str());
        return 1;
    }
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (version == nullptr || renderer == nullptr)
    {
        std::fprintf(stderr, "glGetString returned null\n");
        gl_probe_end();
        return 1;
    }
    std::printf("gl version  : %s\n", version);
    std::printf("gl renderer : %s\n", renderer);
    void* context = gl_probe_context();
    gl_probe_end();
    if (gl_probe_context() != nullptr)
    {
        std::fprintf(stderr, "context survived gl_probe_end\n");
        return 1;
    }
    if (context == nullptr)
    {
        std::fprintf(stderr, "context was null while current\n");
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
