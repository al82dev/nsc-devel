#include "nativesharp/Interop/CppSessionOptions.h"
#include "nativesharp/Basic/Sha256.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace nativesharp::interop
{
std::string cppSessionConfigurationIdentity(const CppSessionOptions& options)
{
    std::vector<std::uint8_t> bytes;
    const auto number = [&](std::uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    };
    const auto text = [&](std::string_view value) {
        number(value.size());
        for (const unsigned char c : value) bytes.push_back(c);
    };
    const auto path = [&](const std::filesystem::path& value) {
        const auto encoded = value.generic_u8string();
        text(std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
    };
    text("NativeSharp.CppSessionConfiguration.v2");
    number(static_cast<std::uint64_t>(options.language));
    number(static_cast<std::uint64_t>(options.volatileMode));
    text(options.languageStandard);
    text(options.targetTriple);
    path(options.driverReferencePath);
    path(options.resourceDirectory);
    path(options.workingDirectory);
    number(options.includePaths.size());
    for (const auto& include : options.includePaths)
    {
        number(static_cast<std::uint64_t>(include.kind));
        path(include.path);
    }
    number(options.macros.size());
    for (const auto& macro : options.macros)
    {
        number(static_cast<std::uint64_t>(macro.action));
        text(macro.text);
    }
    number(options.input.index());
    if (const auto* file = std::get_if<CppFileInput>(&options.input)) path(file->path);
    else
    {
        const auto& buffer = std::get<CppBufferInput>(options.input);
        path(buffer.virtualPath);
        text(buffer.contents);
    }
    return basic::sha256Hex(basic::sha256(bytes));
}
}
