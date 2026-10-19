#pragma once

#include "nativesharp/Interop/LibraryMetadataEscapeClass.h"
#include "nativesharp/Interop/LibraryMetadataOwnershipKind.h"
#include "nativesharp/Interop/LibraryMetadataParameter.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataMember final
{
    std::uint8_t kind = 0;
    std::uint8_t accessibility = 0;
    std::uint32_t flags = 0;
    std::string declaringTypeKey;
    std::string name;
    std::string signatureKey;
    std::string declaredTypeKey;
    std::string declaredErrorTypeKey;
    LibraryMetadataOwnershipKind declaredOwnership = LibraryMetadataOwnershipKind::None;
    std::string cAbiName;
    std::string mangledName;
    std::vector<LibraryMetadataParameter> parameters;
    std::string returnLifetimeName;
    std::vector<std::string> lifetimeParameters;
    std::vector<std::pair<std::string, std::string>> lifetimeOutlivesConstraints;
    // NSMETA 1.35. One entry per parameter. Missing contracts in older metadata are treated as
    // Unknown rather than silently assuming that a borrow cannot escape.
    std::vector<LibraryMetadataEscapeClass> parameterEscapeSummary;
    std::uint8_t constantKind = 0;
    std::uint64_t constantBits = 0;
    // NSMETA 1.9. Payload UTF-8 de una constante string; vacio para los demas kinds.
    std::string constantText;
    // NSMETA 1.10. DLL de origen para `extern C from`; vacia para imports estaticos y miembros
    // NativeSharp ordinarios. El bit ImportedC de flags distingue los dos ultimos casos.
    std::string importDllName;
    // NSMETA 1.13. Declaracion completa, incluido el cuerpo, de un metodo ordinario `inline` o
    // `forceinline`. Vacio para contratos, imports, miembros no inline y especializaciones cuya
    // plantilla reutilizable ya viaja en `genericTemplates`.
    std::string inlineSourceText;
    // NSMETA 1.38: 0 implicit/default, 1 forbidden, 2 abort-on-violation.
    std::uint8_t ffiUnwindPolicy = 0;
};
}
