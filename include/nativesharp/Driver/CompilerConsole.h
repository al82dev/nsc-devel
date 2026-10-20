#pragma once

#include "nativesharp/Driver/CompilerConsoleArguments.h"
#include "nativesharp/Driver/CompilerLanguage.h"
#include "nativesharp/Driver/CompilerLanguageScope.h"

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
CompilerConsoleArguments parseCompilerConsoleArguments(
    std::span<const std::string_view> arguments,
    CompilerLanguage fallback = CompilerLanguage::English);

CompilerLanguage activeCompilerLanguage() noexcept;

std::string_view compilerText(std::string_view english, std::string_view spanish) noexcept;
std::string localizeCompilerMessage(std::string_view english);
void printCompilerBanner(std::ostream& output);
}
