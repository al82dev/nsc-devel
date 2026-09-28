#include "nativesharp/Driver/CompilerConsole.h"

#include "nativesharp/Basic/Version.h"

#include <ostream>

namespace nativesharp::driver
{
namespace
{
thread_local CompilerLanguage currentLanguage = CompilerLanguage::English;

bool parseLanguage(std::string_view value, CompilerLanguage& language) noexcept
{
    if (value == "en")
    {
        language = CompilerLanguage::English;
        return true;
    }
    if (value == "es")
    {
        language = CompilerLanguage::Spanish;
        return true;
    }
    return false;
}
}

CompilerConsoleArguments parseCompilerConsoleArguments(
    std::span<const std::string_view> arguments, CompilerLanguage fallback)
{
    CompilerConsoleArguments result;
    result.language = fallback;
    result.arguments.reserve(arguments.size());

    bool languageSeen = false;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        std::string_view value;
        bool languageArgument = false;
        if (argument == "--language")
        {
            languageArgument = true;
            if (index + 1U >= arguments.size())
            {
                result.error = result.language == CompilerLanguage::Spanish
                    ? "la opción --language requiere en o es"
                    : "--language requires en or es";
                return result;
            }
            value = arguments[++index];
        }
        else if (argument.starts_with("--language="))
        {
            languageArgument = true;
            value = argument.substr(std::string_view("--language=").size());
        }

        if (!languageArgument)
        {
            result.arguments.push_back(argument);
            continue;
        }
        if (languageSeen)
        {
            result.error = result.language == CompilerLanguage::Spanish
                ? "la opción --language no puede repetirse"
                : "--language may not be specified more than once";
            return result;
        }
        languageSeen = true;
        CompilerLanguage parsed = result.language;
        if (!parseLanguage(value, parsed))
        {
            result.error = result.language == CompilerLanguage::Spanish
                ? "idioma no compatible '" + std::string(value) + "'; se esperaba en o es"
                : "unsupported language '" + std::string(value) + "'; expected en or es";
            return result;
        }
        result.language = parsed;
    }
    return result;
}

CompilerLanguage activeCompilerLanguage() noexcept
{
    return currentLanguage;
}

CompilerLanguageScope::CompilerLanguageScope(CompilerLanguage language) noexcept
    : previous_(currentLanguage)
{
    currentLanguage = language;
}

CompilerLanguageScope::~CompilerLanguageScope()
{
    currentLanguage = previous_;
}

std::string_view compilerText(std::string_view english, std::string_view spanish) noexcept
{
    return currentLanguage == CompilerLanguage::Spanish ? spanish : english;
}

void printCompilerBanner(std::ostream& output)
{
    if (currentLanguage == CompilerLanguage::Spanish)
    {
        output << "Compilador NativeSharp " << kCompilerReleaseVersion << " (nsc)\n"
               << "Copyright (c) 2026 Alberto Huerta Luna. Todos los derechos reservados.\n"
               << "Hash de compilación: " << kCompilerBuildId << '\n';
        return;
    }
    output << "NativeSharp Compiler " << kCompilerReleaseVersion << " (nsc)\n"
           << "Copyright (c) 2026 Alberto Huerta Luna. All rights reserved.\n"
           << "Build hash: " << kCompilerBuildId << '\n';
}
}
