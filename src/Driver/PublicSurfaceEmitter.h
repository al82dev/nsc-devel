#pragma once

#include <string>

namespace nativesharp::interop
{
struct LibraryMetadata;
}

namespace nativesharp::driver
{
/// Renders and validates the NativeSharp source projection of a library's public metadata surface.
class PublicSurfaceEmitter final
{
public:
    /// Renders the public source projection while preserving canonical metadata order.
    static bool build(
        const interop::LibraryMetadata& metadata,
        std::string& source,
        std::string& error);

    /// Parses the rendered projection and reports the established metadata diagnostic on failure.
    static bool verify(
        const interop::LibraryMetadata& metadata,
        std::string& error);
};
} // namespace nativesharp::driver
