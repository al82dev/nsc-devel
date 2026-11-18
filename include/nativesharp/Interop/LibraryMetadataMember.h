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
    // NSMETA. One entry per parameter. Missing contracts in older metadata are treated as
    // Unknown rather than silently assuming that a borrow cannot escape.
    std::vector<LibraryMetadataEscapeClass> parameterEscapeSummary;
    // Observed source-object parameters for each pointer output; this is a
    // lower bound, never a guarantee that no other source exists.
    std::vector<std::vector<std::uint32_t>> pointerOutputObjectOrigins;
    std::uint8_t constantKind = 0;
    std::uint64_t constantBits = 0;
    // NSMETA. Payload UTF-8 de una constante string; vacio para los demas kinds.
    std::string constantText;
    // NSMETA. DLL de origen para `extern C from`; vacia para imports estaticos y miembros
    // NativeSharp ordinarios. El bit ImportedC de flags distingue los dos ultimos casos.
    std::string importDllName;
    // NSMETA. Declaracion completa, incluido el cuerpo, de un metodo ordinario `inline` o
    // `forceinline`. Vacio para contratos, imports, miembros no inline y especializaciones cuya
    // plantilla reutilizable ya viaja en `genericTemplates`.
    std::string inlineSourceText;
    // NSMETA: 0 implicit/default, 1 forbidden, 2 abort-on-violation.
    std::uint8_t ffiUnwindPolicy = 0;
    // EN: T08-D02. Qualifiers of the pointee of a pointer-typed declared type (result, field or property),
    //     which the type key does not carry: kMetadataPointeeConst and kMetadataPointeeVolatile. Zero for every
    //     other type.
    // ES: T08-D02. Calificadores del dato apuntado de un tipo declarado puntero (resultado, campo o propiedad),
    //     que la clave de tipo no lleva: kMetadataPointeeConst y kMetadataPointeeVolatile. Cero para cualquier
    //     otro tipo.
    std::uint8_t declaredPointeeQualifiers = 0;
};
}
