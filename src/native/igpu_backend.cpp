#include "igpu_backend.h"

namespace igpu
{
    namespace
    {
        std::unique_ptr<Backend> g_backend;
    }

    Backend* active_backend()
    {
        return g_backend.get();
    }

    void set_active_backend(std::unique_ptr<Backend> backend)
    {
        g_backend = std::move(backend);
    }
}
