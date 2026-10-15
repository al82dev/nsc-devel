#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct IntrospectionMember final
{
    std::string name;
    std::string typeKey;
    bool isPublic = false;
    std::size_t sourceOrdinal = 0;
};
std::vector<IntrospectionMember> visibleIntrospectionMembers(
    std::span<const IntrospectionMember> members,
    bool sameModule);
bool introspectionRequiresRuntimeMetadata() noexcept;

enum class AttributeRetention { CompileTime, NativeRuntimeSection };
struct AttributeUse final
{
    std::string identity;
    std::string target;
    bool repeatable = false;
    AttributeRetention retention = AttributeRetention::CompileTime;
};
bool validateAttributes(std::span<const AttributeUse> attributes, std::string& error);

std::string deriveDependencyKey(
    std::string_view generatorIdentity,
    std::span<const std::string> fieldIdentities);

struct HygienicName final { std::string spelling; std::size_t scopeId = 0; };
bool sameBinding(const HygienicName& left, const HygienicName& right) noexcept;

enum class ComptimeCapability { ReadDeclaredFile, ReadDeclaredEnvironment, Clock, Random, Ffi };
struct ComptimeInput final { std::string identity; std::string contentDigest; };
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

class ComptimeBudget final
{
public:
    ComptimeBudget(std::size_t steps, std::size_t bytes, std::size_t depth) noexcept;
    bool consume(std::size_t steps, std::size_t bytes, std::size_t depth, std::string& error);
private:
    std::size_t steps_;
    std::size_t bytes_;
    std::size_t depth_;
};
}
