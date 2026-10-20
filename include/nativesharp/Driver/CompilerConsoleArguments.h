#pragma once

#include "nativesharp/Driver/CompilerLanguage.h"

#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
struct CompilerConsoleArguments final
{
    CompilerLanguage language = CompilerLanguage::English;
    std::vector<std::string_view> arguments;
    std::string error;
};
} // namespace nativesharp::driver
