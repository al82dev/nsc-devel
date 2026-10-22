#pragma once

#include <filesystem>

namespace nativesharp::compilation
{
// NativeSharp 1.0.1: a precompiled library is a three-part contract. `libraryPath` is either a
// static COFF archive or the import library of `runtimePath`; metadata is mandatory because a
// bare .lib cannot describe NativeSharp types, ownership, defaults, or ABI identity.
struct LibraryReference final
{
    std::filesystem::path libraryPath;
    std::filesystem::path metadataPath;
    std::filesystem::path runtimePath;
    // Optional acceleration product; absence preserves the three-part library contract above.
    std::filesystem::path productSidecarPath;
};
}
