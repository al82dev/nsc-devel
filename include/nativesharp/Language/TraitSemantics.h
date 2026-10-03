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

std::string_view operatorTrait(std::string_view spelling) noexcept;
bool operatorCanBeOverloaded(std::string_view spelling) noexcept;
}
