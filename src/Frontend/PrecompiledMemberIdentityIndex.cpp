#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"

#include <functional>

namespace nativesharp::frontend
{
PrecompiledMemberIdentityIndex::PrecompiledMemberIdentityIndex(
    std::span<const PrecompiledMemberIdentity> identities)
    : identities_(identities)
{
    ordinalsByHash_.reserve(identities.size());
    for (std::size_t ordinal = 0; ordinal < identities.size(); ++ordinal)
    {
        const PrecompiledMemberIdentity& identity = identities[ordinal];
        if (identity.mangledName.empty()) continue;
        ordinalsByHash_.emplace(keyHash(
            identity.kind,
            identity.declaringTypeKey,
            identity.name,
            identity.signatureKey), ordinal);
        ++indexedIdentityCount_;
    }
}

PrecompiledMemberIdentityMatch PrecompiledMemberIdentityIndex::find(
    ClassMemberKind kind,
    std::string_view declaringTypeKey,
    std::string_view name,
    std::string_view signatureKey) const noexcept
{
    ++lookupCount_;
    const auto [begin, end] = ordinalsByHash_.equal_range(
        keyHash(kind, declaringTypeKey, name, signatureKey));
    const PrecompiledMemberIdentity* match = nullptr;
    for (auto iterator = begin; iterator != end; ++iterator)
    {
        const PrecompiledMemberIdentity& candidate = identities_[iterator->second];
        if (candidate.kind != kind || candidate.declaringTypeKey != declaringTypeKey ||
            candidate.name != name || candidate.signatureKey != signatureKey)
            continue;
        if (match != nullptr)
            return {PrecompiledMemberIdentityMatchKind::Ambiguous, nullptr};
        match = &candidate;
    }
    return match != nullptr
        ? PrecompiledMemberIdentityMatch{
              PrecompiledMemberIdentityMatchKind::Unique, match}
        : PrecompiledMemberIdentityMatch{};
}

std::size_t PrecompiledMemberIdentityIndex::indexedIdentityCount() const noexcept
{
    return indexedIdentityCount_;
}

std::size_t PrecompiledMemberIdentityIndex::lookupCount() const noexcept
{
    return lookupCount_;
}

std::size_t PrecompiledMemberIdentityIndex::keyHash(
    ClassMemberKind kind,
    std::string_view declaringTypeKey,
    std::string_view name,
    std::string_view signatureKey) noexcept
{
    std::size_t hash = static_cast<std::size_t>(kind);
    const auto combine = [&hash](std::string_view value) {
        const std::size_t next = std::hash<std::string_view>{}(value);
        hash ^= next + static_cast<std::size_t>(0x9e3779b9U) +
            (hash << 6U) + (hash >> 2U);
    };
    combine(declaringTypeKey);
    combine(name);
    combine(signatureKey);
    return hash;
}
}
