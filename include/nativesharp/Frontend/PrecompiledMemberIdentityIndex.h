#pragma once

#include "nativesharp/Frontend/PrecompiledMemberIdentityMatch.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityMatchKind.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <unordered_map>

namespace nativesharp::frontend
{
// A short-lived view over one decoded NSMETA set. The caller owns the identity strings and must
// keep them alive for the lifetime of the index.
class PrecompiledMemberIdentityIndex final
{
public:
    explicit PrecompiledMemberIdentityIndex(
        std::span<const PrecompiledMemberIdentity> identities);

    PrecompiledMemberIdentityMatch find(
        ClassMemberKind kind,
        std::string_view declaringTypeKey,
        std::string_view name,
        std::string_view signatureKey) const noexcept;

    PrecompiledMemberIdentityMatch findStructurally(
        ClassMemberKind kind,
        std::string_view declaringTypeKey,
        std::string_view name,
        std::string_view signatureKey) const;

    std::size_t indexedIdentityCount() const noexcept;
    std::size_t lookupCount() const noexcept;

private:
    static std::size_t keyHash(
        ClassMemberKind kind,
        std::string_view declaringTypeKey,
        std::string_view name,
        std::string_view signatureKey) noexcept;

    std::span<const PrecompiledMemberIdentity> identities_;
    std::unordered_multimap<std::size_t, std::size_t> ordinalsByHash_;
    std::size_t indexedIdentityCount_ = 0;
    mutable std::size_t lookupCount_ = 0;
};
}
