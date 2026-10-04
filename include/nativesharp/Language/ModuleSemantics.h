#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
std::string canonicalWindowsModuleIdentity(std::string_view package, std::string_view module);

enum class ModuleDependencyKind { Binding, Layout };
struct ModuleDependency final { std::string from; std::string to; ModuleDependencyKind kind; };
bool moduleGraphResolvable(std::span<const ModuleDependency> dependencies, std::string& error);

enum class ImportKind { Module, Symbol, Trait, Extensions, RestrictedGlob };
struct Import final { ImportKind kind; std::string identity; std::string alias; };
bool normalizeImports(std::span<const Import> imports, std::vector<Import>& result, std::string& error);

enum class Visibility { Private, Module, Package, Public };
bool canAccess(
    Visibility visibility,
    bool sameModule,
    bool samePackage,
    bool explicitFriend,
    bool sameDeclaration = false) noexcept;

std::string reexportedIdentity(std::string_view originalIdentity, std::string_view facadeName);

struct Availability final
{
    int sinceEdition = 0;
    int removedEdition = 0;
    std::string architecture;
};
enum class AvailabilityStatus { Available, NotYetAvailable, Deprecated, Removed, WrongTarget };
AvailabilityStatus availabilityStatus(
    const Availability& availability,
    int edition,
    std::string_view architecture,
    bool deprecated) noexcept;

struct PublicApiSymbol final
{
    std::string identity;
    std::string signature;
    std::string effects;
    std::string layoutContract;
    bool isPublic = true;
};
std::string publicApiFingerprintInput(std::span<const PublicApiSymbol> symbols);
}
