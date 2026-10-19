#pragma once

#include <cstdint>
#include <string_view>

namespace nativesharp::basic
{
enum class OutputFailureKind : std::uint8_t
{
    None,
    PathLimit,
    MissingComponent,
    PermissionDenied,
    FileLocked,
    InsufficientSpace,
    IoError
};

std::string_view outputFailureKindName(OutputFailureKind kind) noexcept;

#if defined(_WIN32)
OutputFailureKind classifyWindowsOutputError(unsigned long code) noexcept;
#endif
}
