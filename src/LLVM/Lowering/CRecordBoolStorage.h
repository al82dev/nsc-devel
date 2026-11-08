#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"

#include <string_view>

namespace nativesharp::llvm_backend
{
// T08-006. A C _Bool stored in an imported C struct or union occupies one byte that holds 0 or 1.
// Clang accesses that byte as i8 (load + trunc to i1, zext to i8 + store); the NativeSharp expression
// value stays i1. Only the memory access type changes; layout, alignment and volatility are unchanged.
inline bool isImportedCRecordTypeKey(const frontend::ProjectNameLookup& lookup, std::string_view typeKey)
{
    if (typeKey.ends_with('*')) typeKey.remove_suffix(1);
    if (!typeKey.starts_with("type:")) return false;
    const auto ordinal = lookup.typeOrdinalForIdentity(typeKey.substr(5));
    const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
    return imported && (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
                        imported->origin == frontend::CppImportedTypeOrigin::CUnion);
}

// True for the address or load of a bool field of an imported C record, or of an element of a bool
// array field of one (any number of fixed dimensions). Bit-fields keep their own storage-unit path.
inline bool cRecordBoolStorage(const frontend::ProjectNameLookup& lookup, const frontend::ProjectOopMirNode* node)
{
    if (!node || node->typeKey != "builtin:bool") return false;
    unsigned depth = 0;
    while (node->kind == frontend::ProjectOopMirNodeKind::RawElementAddress)
    {
        if (++depth > 128 || node->children.size() != 2 || !node->children.front()) return false;
        node = node->children.front().get();
    }
    if (node->kind != frontend::ProjectOopMirNodeKind::FieldAddress &&
        node->kind != frontend::ProjectOopMirNodeKind::FieldLoad) return false;
    if (depth == 0 ? node->typeKey != "builtin:bool" : !node->typeKey.starts_with("builtin:bool[")) return false;
    return node->children.size() == 1 && node->children.front() &&
        isImportedCRecordTypeKey(lookup, node->children.front()->typeKey);
}
}
