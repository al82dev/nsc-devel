#pragma once

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace nativesharp::interop
{
enum class CppLanguageMode { C, Cpp };
enum class CppIncludeKind { User, System };
enum class CppMacroAction { Define, Undefine };
enum class CppVolatileMode { Default, Microsoft, Iso };

struct CppIncludePath
{
    std::filesystem::path path;
    CppIncludeKind kind = CppIncludeKind::User;
};

struct CppMacroOption
{
    CppMacroAction action = CppMacroAction::Define;
    // A definition may include its value, e.g. NAME=VALUE.
    std::string text;
};

struct CppFileInput
{
    std::filesystem::path path;
};

struct CppBufferInput
{
    std::filesystem::path virtualPath;
    std::string contents;
};

// All strings, paths and ordered options are owned by the receiving session.
struct CppSessionOptions
{
    CppLanguageMode language = CppLanguageMode::Cpp;
    std::string languageStandard;
    std::string targetTriple;
    std::filesystem::path driverReferencePath;
    std::filesystem::path resourceDirectory;
    std::filesystem::path workingDirectory;
    std::vector<CppIncludePath> includePaths;
    std::vector<CppMacroOption> macros;
    std::variant<CppFileInput, CppBufferInput> input;
    CppVolatileMode volatileMode = CppVolatileMode::Default;
};

// Stable identity of these option values. It is not a complete cache key: file
// contents, toolchain binaries and transitive include state are not captured.
std::string cppSessionConfigurationIdentity(const CppSessionOptions& options);
}
