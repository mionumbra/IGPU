#pragma once

#include "igpu_backend.h"

#include <memory>

namespace igpu
{
    bool bind_current_context();
    std::unique_ptr<Backend> make_gl_backend();
    bool gl_load();
}
