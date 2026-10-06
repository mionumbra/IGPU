#include "wgl_host.h"

#include <Windows.h>

#include <string>

namespace
{
    HWND g_window = nullptr;
    HDC g_dc = nullptr;
    HGLRC g_context = nullptr;

    void release_window()
    {
        if (g_dc != nullptr && g_window != nullptr)
        {
            ReleaseDC(g_window, g_dc);
        }
        g_dc = nullptr;
        if (g_window != nullptr)
        {
            DestroyWindow(g_window);
            g_window = nullptr;
        }
    }
}

bool gl_probe_begin(std::string& error)
{
    gl_probe_end();

    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"IGPUGLProbe";
    RegisterClassW(&window_class);

    g_window = CreateWindowW(
        L"IGPUGLProbe",
        L"IGPUGLProbe",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        64,
        64,
        nullptr,
        nullptr,
        window_class.hInstance,
        nullptr);
    if (g_window == nullptr)
    {
        error = "CreateWindowW failed " + std::to_string(GetLastError());
        return false;
    }

    g_dc = GetDC(g_window);
    if (g_dc == nullptr)
    {
        error = "GetDC failed " + std::to_string(GetLastError());
        release_window();
        return false;
    }

    PIXELFORMATDESCRIPTOR format = {};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;

    const int chosen = ChoosePixelFormat(g_dc, &format);
    if (chosen == 0 || !SetPixelFormat(g_dc, chosen, &format))
    {
        error = "pixel format failed " + std::to_string(GetLastError());
        release_window();
        return false;
    }

    g_context = wglCreateContext(g_dc);
    if (g_context == nullptr || !wglMakeCurrent(g_dc, g_context))
    {
        error = "wglCreateContext failed " + std::to_string(GetLastError());
        gl_probe_end();
        return false;
    }

    error.clear();
    return true;
}

void gl_probe_end()
{
    if (g_context != nullptr)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_context);
        g_context = nullptr;
    }
    release_window();
}

void* gl_probe_context()
{
    return g_context;
}
