#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
// Experimental declaration of external C++ storage; not an initialized NativeSharp field.
struct CppImportedStaticField final
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string externalSymbol;
    std::string typeKey;
    bool isConst;
    bool isVolatile;
    bool dllImport = false;
    bool pointeeReadonly = false;
};
}
