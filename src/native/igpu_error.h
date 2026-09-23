#pragma once

#include <cstdint>
#include <string>

namespace igpu
{
    void set_last_error(std::string message);
    void clear_last_error();
    const std::string& last_error();

    std::string narrow(const wchar_t* text);
}
