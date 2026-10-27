#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>

namespace nativesharp::frontend
{
namespace
{
std::string structuralGenericKey(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (std::size_t cursor = 0; cursor < value.size();)
    {
        constexpr std::size_t kHashDigits = 16U;
        if (value.substr(cursor, 3U) == "__h" && cursor + 3U + kHashDigits <= value.size())
        {
            const std::string_view digits = value.substr(cursor + 3U, kHashDigits);
            if (std::ranges::all_of(digits, [](const char character) {
                    return std::isxdigit(static_cast<unsigned char>(character)) != 0;
                }))
            {
                result.append("__h");
                cursor += 3U + kHashDigits;
                continue;
            }
        }
        result.push_back(value[cursor]);
        ++cursor;
    }
    return result;
}
}

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

PrecompiledMemberIdentityMatch PrecompiledMemberIdentityIndex::findStructurally(
    ClassMemberKind kind,
    std::string_view declaringTypeKey,
    std::string_view name,
    std::string_view signatureKey) const
{
    ++lookupCount_;
    const std::string structuralDeclaringType = structuralGenericKey(declaringTypeKey);
    const std::string structuralSignature = structuralGenericKey(signatureKey);
    const PrecompiledMemberIdentity* match = nullptr;
    for (const PrecompiledMemberIdentity& candidate : identities_)
    {
        if (candidate.mangledName.empty() || candidate.kind != kind || candidate.name != name ||
            structuralGenericKey(candidate.declaringTypeKey) != structuralDeclaringType ||
            structuralGenericKey(candidate.signatureKey) != structuralSignature)
        {
            continue;
        }
        if (match != nullptr)
            return {PrecompiledMemberIdentityMatchKind::Ambiguous, nullptr};
        match = &candidate;
    }
    return match != nullptr
        ? PrecompiledMemberIdentityMatch{PrecompiledMemberIdentityMatchKind::Unique, match}
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
