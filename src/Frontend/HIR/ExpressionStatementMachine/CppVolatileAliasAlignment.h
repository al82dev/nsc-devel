#pragma once

#include "nativesharp/Frontend/CppImportedEnumNumeric.h"
#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/MethodLocalSymbol.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nativesharp::frontend::internal
{
// EN: Natural width of a scalar that C volatile accesses may carry atomically; zero otherwise.
// ES: Ancho natural de un escalar que los accesos volatile de C pueden llevar atomicamente; cero si no.
inline std::size_t cVolatileScalarWidth(std::string_view elementType) noexcept
{
    return (elementType == "builtin:bool" || elementType == "builtin:sbyte" || elementType == "builtin:byte") ? 1 :
        (elementType == "builtin:short" || elementType == "builtin:ushort") ? 2 :
        (elementType == "builtin:int" || elementType == "builtin:uint" || elementType == "builtin:float") ? 4 :
        (elementType == "builtin:long" || elementType == "builtin:ulong" || elementType == "builtin:double") ? 8 : 0;
}

// EN: T19-002. Width of an atomic access through a pointer to C atomic storage: the natural width of a scalar, of a
//     pointer or of a C enum's underlying integer, which is also its alignment, as Clang gives `_Atomic T`
//     storage; zero for a type the atomic access does not support.
// ES: T19-002. Ancho de un acceso atomico por un puntero a almacenamiento C atomico: el ancho natural de un escalar,
//     de un puntero o del entero subyacente de un enum C, que tambien es su alineacion, como Clang dispone el
//     almacenamiento `_Atomic T`; cero para un tipo que el acceso atomico no admite.
inline std::size_t cAtomicPointeeWidth(const ProjectNameLookup* lookup, std::string_view type)
{
    if (type.ends_with('*')) return 8;
    if (const std::size_t width = cVolatileScalarWidth(type)) return width;
    if (const auto underlying = cppImportedCEnumUnderlying(lookup, type)) return cVolatileScalarWidth(*underlying);
    // EN: An atomic C record of 16 bytes or less is widened to the next power of two; a larger one keeps its size
    //     (T08-D07, Clang's policy). / ES: Un record C atomico de 16 bytes o menos se amplia a la siguiente
    //     potencia de dos; uno mayor conserva su tamano (T08-D07, la politica de Clang).
    if (!lookup || !type.starts_with("type:")) return 0;
    const auto ordinal = lookup->typeOrdinalForIdentity(type.substr(5));
    const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
    if (!imported || !imported->objectLayout || imported->isIncomplete ||
        (imported->origin != CppImportedTypeOrigin::CStruct && imported->origin != CppImportedTypeOrigin::CUnion))
        return 0;
    const std::size_t size = imported->objectLayout->sizeBytes;
    if (size == 0 || size > 16) return size;
    std::size_t width = 1;
    while (width < size) width *= 2;
    return width;
}

inline std::optional<std::size_t> cppVolatileAliasAlignment(
    const MethodExpressionTypeBinding* pointer, std::string_view elementType) noexcept
{
    if (!pointer || !pointer->pointerPointeeVolatile || !pointer->cppVolatilePointeeContract)
        return std::nullopt;
    const std::size_t width = cVolatileScalarWidth(elementType);
    if (!width || pointer->pointerKnownAlignment < width || pointer->pointerKnownAlignment % width != 0)
        return std::nullopt;
    return width;
}

// EN: T08-D13. Mode and atomic width of an access through a C alias: the volatile mode of its C origin (record,
//     global or header reference), never a mode of the consumer. A local operand uses its contract joined over
//     every assignment; any other operand uses its expression's. No value when the mode is unknown or mixed: the
//     access is plain volatile, as a direct access to such storage. The width is set only when the contract
//     proves (or C assumes) natural alignment.
// ES: T08-D13. Modo y ancho atomico de un acceso por un alias C: el modo volatile de su origen C (record, global
//     o referencia del encabezado), nunca un modo del consumidor. Un operando local usa su contrato unido en cada
//     asignacion; otro operando, el de su expresion. Sin valor si el modo se desconoce o se mezcla: el acceso es
//     volatile simple, como el acceso directo a ese almacenamiento. El ancho solo se fija si el contrato demuestra
//     (o C supone) alineacion natural.
// EN: The local an alias access reads its pointer from, when the operand is that local's value.
// ES: El local del que un acceso por alias lee su puntero, si el operando es el valor de ese local.
template <typename Locals>
inline const MethodLocalSymbol* cAliasLocal(const Locals& locals, const ProjectOopHirNode* node) noexcept
{
    if (!node || node->kind != ProjectOopHirNodeKind::LocalValue || !node->localOrdinal ||
        *node->localOrdinal >= locals.size()) return nullptr;
    return &locals[*node->localOrdinal];
}

struct CVolatileAliasAccess
{
    bool microsoft = false;
    std::optional<std::size_t> atomicWidth;
};

inline std::optional<CVolatileAliasAccess> cVolatileAliasAccess(
    const MethodExpressionTypeBinding* pointer, const MethodLocalSymbol* local, std::string_view elementType) noexcept
{
    if (!pointer || !pointer->pointerPointeeVolatile || !pointer->cppVolatilePointeeContract) return std::nullopt;
    std::optional<bool> mode = pointer->cppVolatilePointeeMicrosoft;
    std::size_t alignment = pointer->cppVolatilePointeeAlignment;
    if (local && local->cppVolatilePointeeContract)
    {
        mode = local->cppVolatilePointeeMicrosoft;
        alignment = local->cppVolatilePointeeAlignment;
    }
    if (!mode) return std::nullopt;
    CVolatileAliasAccess access;
    access.microsoft = *mode;
    const std::size_t width = cVolatileScalarWidth(elementType);
    if (width && alignment >= width && alignment % width == 0) access.atomicWidth = width;
    return access;
}

// EN: Volatile mode of the record that owns a C field holding a pointer to a volatile scalar.
//     Returns no value when the field value is not such a pointer or the record mode is unknown.
// ES: Modo volatile del record que posee un campo C con puntero a escalar volatile.
//     No devuelve valor si el campo no es ese puntero o si el modo del record se desconoce.
inline std::optional<bool> cFieldVolatilePointeeMode(
    const ProjectMemberModel* members, const ProjectOopHirNode& field, std::string_view elementType) noexcept
{
    if (!members || field.kind != ProjectOopHirNodeKind::FieldValue || !field.memberOrdinal ||
        *field.memberOrdinal >= members->members().size() || !cVolatileScalarWidth(elementType))
        return std::nullopt;
    const auto& member = members->members()[*field.memberOrdinal];
    const auto& declared = member.declaredTypeKey;
    if (!member.isCppExternal || !member.declaredPointerPointeeVolatile ||
        declared.size() != elementType.size() + 1 || !declared.starts_with(elementType) || !declared.ends_with('*'))
        return std::nullopt;
    return member.cppMicrosoftVolatileSemantics;
}
}
