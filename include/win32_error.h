#pragma once

#include "WindowsStub.h"

#include <string>
#include <system_error>

std::string GetWin32ErrorStr(DWORD error_code = 0, bool english = false);


class win32_category_impl : public std::error_category
{
public:
    const char* name() const noexcept override { return "win32"; };
    std::string message(int error_code) const override { return GetWin32ErrorStr(error_code); }
};

inline const std::error_category& win32_category()
{
    static win32_category_impl category;
    return category;
}

class win32_error : public std::system_error
{
public:
    win32_error(DWORD error_code, const char* message)
        : std::system_error(error_code, win32_category(), message) {}
};
