#pragma once

#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Parser/KnownTypeNameSet.h"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
// Immutable after construction. Each interval is half-open and belongs to one source file.
// An enclosing interval remains visible in a nested interval; a sibling never contributes.
class ScopedKnownTypeNames final
{
public:
    struct Scope final
    {
        FileId file;
        std::uint32_t begin;
        std::uint32_t end;
        KnownTypeNameSet names;
    };

    explicit ScopedKnownTypeNames(std::vector<Scope> scopes, bool effectiveScopes = false)
        : scopes_(std::move(scopes)), effectiveScopes_(effectiveScopes) {}

    bool contains(SourceLocation location, std::string_view name) const noexcept
    {
        if (!location.isValid() || name.empty()) return false;
        const Scope* innermost = nullptr;
        for (const auto& scope : scopes_)
            if (scope.file == location.fileId() && scope.begin <= location.offset() &&
                location.offset() < scope.end)
            {
                if (!effectiveScopes_ && scope.names.contains(name)) return true;
                if (effectiveScopes_) innermost = &scope;
            }
        return innermost != nullptr && innermost->names.contains(name);
    }

private:
    std::vector<Scope> scopes_;
    bool effectiveScopes_ = false;
};
}
