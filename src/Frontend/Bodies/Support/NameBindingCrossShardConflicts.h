#pragma once
#include "nativesharp/Frontend/MethodNameBinding.h"
#include <cstddef>
#include <span>
#include <unordered_map>
#include <unordered_set>
namespace nativesharp::frontend::detail
{
// Metadata only: this does not alter syntax, binding order or canonical lookup.
inline void invalidateCrossShardNameContexts(std::span<MethodNameBinding> destination,
    std::span<MethodNameBinding> source)
{
    std::unordered_map<std::size_t, std::unordered_set<const ast::AstNode*>> incoming;
    for (const auto& binding : source)
        if (binding.syntax) incoming[binding.methodMemberOrdinal].insert(binding.syntax);
    std::unordered_map<std::size_t, std::unordered_set<const ast::AstNode*>> duplicated;
    for (auto& binding : destination)
    {
        const auto owner = incoming.find(binding.methodMemberOrdinal);
        if (!binding.syntax || owner == incoming.end() || !owner->second.contains(binding.syntax)) continue;
        duplicated[binding.methodMemberOrdinal].insert(binding.syntax);
        binding.analysisContext = MethodNameBindingContext::Unknown;
        binding.recordedLambdaOrdinal.reset();
        binding.analysisContextRecorded = false;
    }
    for (auto& binding : source)
    {
        const auto owner = duplicated.find(binding.methodMemberOrdinal);
        if (!binding.syntax || owner == duplicated.end() || !owner->second.contains(binding.syntax)) continue;
        binding.analysisContext = MethodNameBindingContext::Unknown;
        binding.recordedLambdaOrdinal.reset();
        binding.analysisContextRecorded = false;
    }
}
}
