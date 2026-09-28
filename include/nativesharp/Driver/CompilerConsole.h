#pragma once

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
enum class CompilerLanguage
{
    English,
    Spanish
};

struct CompilerConsoleArguments final
{
    CompilerLanguage language = CompilerLanguage::English;
    std::vector<std::string_view> arguments;
    std::string error;
};

CompilerConsoleArguments parseCompilerConsoleArguments(
    std::span<const std::string_view> arguments,
    CompilerLanguage fallback = CompilerLanguage::English);

CompilerLanguage activeCompilerLanguage() noexcept;

class CompilerLanguageScope final
{
public:
    explicit CompilerLanguageScope(CompilerLanguage language) noexcept;
    ~CompilerLanguageScope();

    CompilerLanguageScope(const CompilerLanguageScope&) = delete;
    CompilerLanguageScope& operator=(const CompilerLanguageScope&) = delete;

private:
    CompilerLanguage previous_;
};

std::string_view compilerText(std::string_view english, std::string_view spanish) noexcept;
std::string localizeCompilerMessage(std::string_view english);
void printCompilerBanner(std::ostream& output);
}
