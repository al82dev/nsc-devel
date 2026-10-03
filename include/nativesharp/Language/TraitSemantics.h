#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct TraitMethod final
{
    std::string signature;
    bool hasDefault = false;
    bool isGeneric = false;
    bool returnsSelfByValue = false;
};

struct TraitContract final
{
    std::string qualifiedName;
    std::vector<TraitMethod> methods;
    bool declaresInstanceStorage = false;
};

bool validateTrait(const TraitContract& trait, std::string& error);

struct ImplCandidate final
{
    std::string traitIdentity;
    std::string typeIdentity;
    std::string moduleIdentity;
    bool traitOwnedByModule = false;
    bool typeOwnedByModule = false;
};

bool validateImplCoherence(std::span<const ImplCandidate> implementations, std::string& error);

struct AssociatedProjection final
{
    std::string projection;
    std::string replacement;
};

bool normalizeAssociatedType(
    std::string input,
    std::span<const AssociatedProjection> projections,
    std::string& result,
    std::string& error);

bool validateOpaqueResultTypes(std::span<const std::string> concreteReturns, std::string& error);
bool isObjectSafe(std::span<const TraitMethod> methods, std::string& error);

enum class DispatchKind { Static, Dynamic };
struct TraitObjectLayout final
{
    std::size_t size;
    std::size_t alignment;
    bool descriptorInterfaceMap;
    bool nullIsZero;
    bool conversionAllocates;
};
TraitObjectLayout windowsX64TraitObjectLayout() noexcept;
std::string dispatchMetadata(DispatchKind kind, std::string_view traitIdentity);

enum class ExtensionResolution { InstanceMember, UniqueExtension, Missing, Ambiguous };
ExtensionResolution resolveExtension(bool hasInstanceMember, std::size_t visibleExtensions) noexcept;

// NativeSharp 3.0 T05-F07. Operators are language items, not ad-hoc method-name conventions.
// The spelling+arity pair is the source identity; langItem is the stable semantic identity used
// by trait requirements and witnesses; methodName is only the existing ABI spelling. Keeping the
// three in one table prevents parser, Sema and metadata reconstruction from inventing different
// mappings for the same operator.
struct OperatorTraitContract final
{
    std::string_view spelling;
    std::size_t arity = 0;
    std::string_view langItem;
    std::string_view methodName;
};

std::span<const OperatorTraitContract> operatorTraitContracts() noexcept;
const OperatorTraitContract* operatorTraitContract(
    std::string_view spelling,
    std::size_t arity) noexcept;
const OperatorTraitContract* operatorTraitContractForMethod(
    std::string_view methodName,
    std::size_t arity) noexcept;
std::string_view operatorTrait(std::string_view spelling, std::size_t arity) noexcept;
std::string_view operatorMethodName(std::string_view spelling, std::size_t arity) noexcept;

// Compatibility projection retained for the original T05 helper. Ambiguous unary/binary
// spellings select their binary contract, matching the historical `+ -> lang:Add` behavior.
std::string_view operatorTrait(std::string_view spelling) noexcept;
bool operatorCanBeOverloaded(std::string_view spelling) noexcept;
}
