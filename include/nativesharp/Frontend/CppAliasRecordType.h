#pragma once

#include "nativesharp/Interop/CppType.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
struct CppAliasRecordType
{
    std::string name;
    std::string canonicalName;
    std::size_t pointerDepth;
    bool pointeeConst;
    std::optional<std::string> clangUsr = std::nullopt;
};

// This is a source name to resolve against the imported record index, not proof
// that a record can be copied, constructed, or passed by value.
inline std::optional<CppAliasRecordType> cppAliasRecordType(const interop::CppType& type)
{
    const interop::CppType* current = &type;
    std::size_t pointers = 0;
    while (current->kind == interop::CppTypeKind::Pointer)
    {
        if (current->isConst || current->isVolatile || current->sizeBits != 64 || !current->element)
            return std::nullopt;
        current = current->element.get();
        ++pointers;
    }
    if (current->kind != interop::CppTypeKind::Record || current->isVolatile ||
        (current->isConst && pointers != 1) || !current->recordIdentity) return std::nullopt;
    const auto& identity = *current->recordIdentity;
    if (identity.name.empty() || identity.anonymousContext || identity.specializationContext) return std::nullopt;
    std::string name;
    const auto append = [&](const std::string& part) {
        if (!name.empty()) name.push_back('.');
        name += part;
    };
    for (const auto& part : identity.namespaceSegments) append(part);
    for (const auto& part : identity.containingRecords) append(part);
    append(identity.name);
    std::string canonical;
    for (const auto& part : identity.namespaceSegments)
    {
        if (!canonical.empty()) canonical.push_back('.');
        canonical += part;
    }
    bool first = true;
    for (const auto& part : identity.containingRecords)
    {
        if (!canonical.empty()) canonical.push_back(first ? '.' : '+');
        canonical += part;
        first = false;
    }
    if (!canonical.empty()) canonical.push_back(identity.containingRecords.empty() ? '.' : '+');
    canonical += identity.name;
    return CppAliasRecordType{std::move(name), std::move(canonical), pointers, current->isConst, identity.clangUsr};
}
}
