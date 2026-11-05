#pragma once

#include "nativesharp/Frontend/CppAliasBuiltinType.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct CppAliasArrayType
{
    CppAliasBuiltinType element;
    // C order: outermost dimension first.
    std::vector<std::uint64_t> dimensions;
};

// Only fixed, nonempty arrays whose element has a representable builtin type.
// An incomplete or variable bound remains distinct and must not become zero.
inline std::optional<CppAliasArrayType> cppAliasArrayType(const interop::CppType& type)
{
    const interop::CppType* current = &type;
    std::vector<std::uint64_t> dimensions;
    while (current->kind == interop::CppTypeKind::Array)
    {
        if (current->isConst || current->isVolatile || !current->arrayCount ||
            *current->arrayCount == 0 || !current->element) return std::nullopt;
        dimensions.push_back(*current->arrayCount);
        current = current->element.get();
    }
    if (dimensions.empty()) return std::nullopt;
    const auto element = cppAliasBuiltinType(*current);
    if (!element) return std::nullopt;
    return CppAliasArrayType{*element, std::move(dimensions)};
}
}
