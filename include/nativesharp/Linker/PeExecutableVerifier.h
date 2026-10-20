#pragma once

#include "nativesharp/Linker/PeExecutableInfo.h"
#include "nativesharp/Linker/PeImportedSymbol.h"

#include <filesystem>
#include <string>

namespace nativesharp::linker
{
class PeExecutableVerifier final
{
public:
    bool verify(const std::filesystem::path& path);
    bool verifyDynamicLibrary(const std::filesystem::path& path);
    const PeExecutableInfo& info() const noexcept;
    const std::string& error() const noexcept;

private:
    bool verifyImage(const std::filesystem::path& path, bool dynamicLibrary);

    PeExecutableInfo info_;
    std::string error_;
};
}
