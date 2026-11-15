#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"

#include <optional>
#include <cstdint>
#include <string>

namespace nativesharp::interop
{
struct CppGlobalDeclaration
{
    std::string name;
    std::string qualifiedName;
    CppType type;
    bool externalLinkage = false;
    bool threadLocal = false;
    // A C++ thread_local that Clang reaches through __tls_guard and __dyn_tls_on_demand_init, because its
    // declaration is neither constinit nor a visible constant-initialized definition.
    bool cppTlsOnDemand = false;
    bool dllImport = false;
    // A name supplied by Clang does not prove that a linked library defines it.
    std::optional<std::string> externalSymbol;
    std::optional<CppSourcePoint> location;
    // Effective alignment of the declaration in bits, absent for incomplete types.
    std::optional<std::uint64_t> declarationAlignmentBits;
    std::optional<std::string> clangUsr;
};
}
