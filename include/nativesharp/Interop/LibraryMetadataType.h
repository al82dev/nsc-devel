#pragma once

#include "nativesharp/Interop/LibraryMetadataAssociatedItem.h"
#include "nativesharp/Interop/LibraryMetadataField.h"
#include "nativesharp/Interop/LibraryMetadataParameterPassKind.h"
#include "nativesharp/Interop/LibraryMetadataTypeKind.h"
#include "nativesharp/Interop/LibraryMetadataVariantCase.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataType final
{
    LibraryMetadataTypeKind kind = LibraryMetadataTypeKind::Struct;
    std::uint8_t accessibility = 0;
    std::uint16_t flags = 0;
    std::string identityKey;
    std::string fullyQualifiedName;
    std::string baseTypeKey;
    std::vector<std::string> interfaceTypeKeys;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint64_t headerSize = 0;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    std::uint64_t stableTypeId = 0;
    // EL TIPO ENTERO SUBYACENTE DE UN ENUM ("builtin:byte", "builtin:sbyte"...). Vacio para todo
    // lo que no sea un enum.
    //
    // POR QUE NO BASTA CON `size`. `enum : byte` y `enum : sbyte` miden LOS DOS un byte, asi que
    // un consumidor que solo lea el tamano no puede distinguirlos -- y un generador de host que
    // eligiera el signo contrario convertiria un 200 en un -56. El signo es parte del ABI igual
    // que el ancho, y estos metadatos son lo que los generadores de host leen.
    //
    // Se guarda la CLAVE y no un par (ancho, signo) porque la clave es lo que ya circula por todo
    // el compilador, y porque lleva las dos respuestas sin que nadie tenga que recomponerlas.
    std::string enumUnderlyingTypeKey;
    // NSMETA 1.5. Delegate declarations are types, not ordinary member records. Without their
    // return and parameter keys a precompiled consumer can know that `Transform` exists but not
    // reconstruct `int Transform(int)`. Bit 0 marks a raw-function-pointer extern delegate and
    // bit 1 selects Microsoft x64 vectorcall rather than the ordinary C convention.
    std::uint8_t delegateFlags = 0;
    std::string delegateReturnTypeKey;
    std::string delegateErrorTypeKey;
    std::vector<std::string> delegateParameterTypeKeys;
    // NSMETA 1.31. `dyn` is a source-level trait-object contract. It uses the same
    // physical reference ABI as the nominal interface key, but must survive source
    // materialization and API-diff round trips.
    bool delegateReturnDynamicTrait = false;
    std::vector<std::uint8_t> delegateParameterDynamicTraits;
    // NSMETA 1.27. Declaration order is deliberately retained because it defines CaseTag().
    std::vector<LibraryMetadataVariantCase> variantCases;
    // NSMETA 1.28. Effective instance-method signatures in their normative trait-dispatch order.
    // Default method bodies compiled by the producer use these ordinals, so a consumer must not
    // reconstruct the trait from the signature-sorted public member catalogue.
    std::vector<std::string> traitMethodSignatures;
    // NSMETA 1.29. Ordered as declared. These are logical compile-time items and never fields,
    // vtable slots, or exported storage.
    std::vector<LibraryMetadataAssociatedItem> associatedItems;
    // NSMETA 1.31. Zero for non-traits. NativeSharp's Windows x64 representation is one
    // object-reference word; dispatch is selected through the object's descriptor/interface map.
    std::uint8_t traitObjectFlags = 0;
    std::uint8_t traitObjectWordCount = 0;
    std::vector<LibraryMetadataField> fields;
    std::vector<std::string> lifetimeParameters;
    std::vector<std::pair<std::string, std::string>> lifetimeOutlivesConstraints;
    // NSMETA 1.36. Empty for trivially destructible values and all reference-shaped types.
    // Consumers call this producer-owned thunk instead of rebuilding private layout/destructors.
    std::string dropGlueEntryName;
    bool delegateReferenceReturn = false;
    std::string delegateReturnLifetimeName;
    std::vector<LibraryMetadataParameterPassKind> delegateParameterPassKinds;
    std::vector<std::string> delegateParameterLifetimeNames;
};
}
