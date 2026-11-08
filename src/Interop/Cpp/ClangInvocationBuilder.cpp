#include "ClangSessionSupport.h"

#include <filesystem>

namespace nativesharp::interop::detail
{
namespace
{
std::string utf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
}

std::vector<std::string> buildClangInvocationArguments(const CppSessionOptions& options)
{
    std::vector<std::string> args = {
        "-target", options.targetTriple, "-x",
        options.language == CppLanguageMode::C ? "c" : "c++",
        "-std=" + options.languageStandard, "-resource-dir", utf8(options.resourceDirectory),
        "-nostdinc", "-isystem", utf8(options.resourceDirectory / "include")};
    if (options.volatileMode == CppVolatileMode::Microsoft) args.push_back("-fms-volatile");
    else if (options.volatileMode == CppVolatileMode::Iso) args.push_back("-fno-ms-volatile");
    for (const auto& path : options.includePaths)
    {
        args.push_back(path.kind == CppIncludeKind::System ? "-isystem" : "-I");
        args.push_back(utf8(path.path));
    }
    for (const auto& macro : options.macros)
        args.push_back((macro.action == CppMacroAction::Define ? "-D" : "-U") + macro.text);
    return args;
}
}
