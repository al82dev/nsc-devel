#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/AttributeRetention.h"
#include "nativesharp/Language/AttributeUse.h"
#include "nativesharp/Language/ComptimeBudget.h"
#include "nativesharp/Language/ComptimeCapability.h"
#include "nativesharp/Language/ComptimeInput.h"
#include "nativesharp/Language/HygienicName.h"
#include "nativesharp/Language/IntrospectionMember.h"

namespace nativesharp::language
{
std::vector<IntrospectionMember> visibleIntrospectionMembers(
    std::span<const IntrospectionMember> members,
    bool sameModule);
bool introspectionRequiresRuntimeMetadata() noexcept;

bool validateAttributes(std::span<const AttributeUse> attributes, std::string& error);

std::string deriveDependencyKey(
    std::string_view generatorIdentity,
    std::span<const std::string> fieldIdentities);

bool sameBinding(const HygienicName& left, const HygienicName& right) noexcept;

bool capabilityAllowedInHermeticProfile(ComptimeCapability capability) noexcept;
std::string comptimeInputIdentity(std::span<const ComptimeInput> inputs);

// NS304-T14-F06: the complete cache key of a macro evaluation — the declared capabilities, the
// declared inputs, and the source digest, without clock/network/cwd/global state, so clean and
// incremental builds share the identical key. Non-hermetic capabilities (Clock/Random/Ffi) are
// excluded from the key because they are not reproducible declared inputs.
std::string comptimeCacheKey(
    std::span<const ComptimeCapability> capabilities,
    std::span<const ComptimeInput> inputs,
    std::string_view sourceDigest);

bool validPackageDiagnosticCode(std::string_view package, std::string_view code) noexcept;

}
