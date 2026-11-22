#pragma once

#include "nativesharp/Frontend/ClassMemberAccessibility.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
// Isolated experimental metadata; field access and object lifetime are not enabled by this type.
struct CppImportedInstanceField final
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string typeKey;
    std::uint64_t byteOffset;
    bool isConst;
    bool isVolatile;
    bool pointeeReadonly = false;
    std::uint64_t byteSize = 0;
    std::uint64_t byteAlignment = 0;
    std::optional<bool> microsoftVolatileSemantics{};
    std::optional<std::string> clangUsr{};
    std::optional<std::uint64_t> bitOffset{};
    std::optional<std::uint64_t> bitWidth{};
    bool isAnonymousCarrier = false;
    // The field is a pointer whose pointee is `volatile` in C. / El campo es un puntero a un apuntado `volatile` en C.
    bool pointeeVolatile = false;
    // T19-038: the field is a C `_Atomic T*`. / El campo es un `_Atomic T*` de C.
    bool pointeeAtomic = false;
    // Physical element stride for each dimension of a non-contiguous C++ vector array.
    std::vector<std::uint64_t> cppArrayElementStrides{};
    // Clang type size and natural alignment for each array-view dimension.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> cppArrayPhysicalLayouts{};
    // EN: T19-012. C++ access of the field; NativeSharp checks it at each use.
    // ES: T19-012. Acceso C++ del campo; NativeSharp lo comprueba en cada uso.
    ClassMemberAccessibility accessibility = ClassMemberAccessibility::Public;
    // EN: Item 1.4. Set when the C/C++ field exists but could not be imported; the field is not a member, the reason
    //     only explains a later use.
    // ES: Item 1.4. Presente cuando el campo C/C++ existe pero no se pudo importar; no es miembro, el motivo solo
    //     explica un uso posterior.
    std::optional<std::string> notImportedReason{};
    std::uint8_t pointerLevelQualifiers = 0; // C1-02: intermediate pointer level qualifiers (Language/PointerLevelQualifiers.h).
    bool atomicStorage = false; // C3-04: a C `_Atomic T` field. / Un campo C `_Atomic T`.
    // C3-05: the Microsoft storage unit a volatile access uses (absent when it is the field window). / La
    //     unidad de Microsoft que usa un acceso volatile (ausente si es la ventana del campo).
    std::optional<std::uint64_t> volatileUnitByteOffset{};
    std::optional<std::uint64_t> volatileUnitBitOffset{};
    std::optional<std::uint64_t> volatileUnitBytes{};
};
}
