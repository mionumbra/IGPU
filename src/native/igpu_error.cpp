#include "igpu_error.h"

#include <Windows.h>

namespace igpu
{
    namespace
    {
        std::string g_last_error;
    }

    void set_last_error(std::string message)
    {
        g_last_error = std::move(message);
    }

    void clear_last_error()
    {
        g_last_error.clear();
    }

    const std::string& last_error()
    {
        return g_last_error;
    }

    std::string narrow(const wchar_t* text)
    {
        if (text == nullptr)
        {
            return {};
        }

        const int length = ::WideCharToMultiByte(
            CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (length <= 1)
        {
            return {};
        }

        std::string result(static_cast<std::size_t>(length - 1), '\0');
        ::WideCharToMultiByte(
            CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
        return result;
    }
}
