#pragma once
#include "nativesharp/Frontend/MethodNameBinding.h"
#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>
namespace nativesharp::frontend::detail
{
// Contexts describe analysis visits, not execution. Conflicting visits to the
// same syntax/owner cannot establish a unique context for any of those visits.
inline std::vector<unsigned char> nameBindingContextConflicts(std::span<const MethodNameBinding> bindings,
    std::span<const unsigned char> eligible)
{
    std::vector<unsigned char> conflicts(bindings.size(), 0);
    if (eligible.size() != bindings.size()) return std::vector<unsigned char>(bindings.size(), 1);
    std::unordered_map<std::size_t, std::unordered_map<const ast::AstNode*, std::size_t>> first;
    for (std::size_t i = 0; i < bindings.size(); ++i)
    {
        const auto& binding = bindings[i];
        if (!binding.syntax) { conflicts[i] = 1; continue; }
        auto& sites = first[binding.methodMemberOrdinal];
        const auto [entry, inserted] = sites.emplace(binding.syntax, i);
        if (inserted) { if (!eligible[i]) conflicts[i] = 1; continue; }
        const auto& original = bindings[entry->second];
        if (!eligible[i] || !eligible[entry->second] || binding.scopeOrdinal != original.scopeOrdinal ||
            !binding.analysisContextRecorded || !original.analysisContextRecorded ||
            binding.recordedLambdaOrdinal != original.recordedLambdaOrdinal)
            conflicts[entry->second] = 1;
    }
    for (std::size_t i = 0; i < bindings.size(); ++i)
    {
        const auto& binding = bindings[i];
        if (binding.syntax) conflicts[i] = conflicts[first.at(binding.methodMemberOrdinal).at(binding.syntax)];
    }
    return conflicts;
}
}
