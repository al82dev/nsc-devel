#pragma once

#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
enum class GeneratedDeclarationKind { Member, Impl, Helper };

// A validated typed declaration produced by a metaprogramming generator (derive/mixin).
// Every generated declaration carries its origin (source path), its hygiene scope (so an
// introduced name can never capture a same-spelled caller binding), and a stable ID derived
// from spelling + scope, so the downstream binder binds it exactly like a hand-written
// declaration. `valid()` is false (with a causal `error`) whenever the builder rejected it.
struct GeneratedDeclaration final
{
    GeneratedDeclarationKind kind = GeneratedDeclarationKind::Member;
    HygienicName name;
    std::string typeKey;
    std::string originPath;
    bool isPublic = false;
    bool isStatic = false;
    bool isReadonly = false;
    std::string error;

    bool valid() const noexcept { return error.empty(); }
};

// Builds and validates generated declarations. This is the AST-builder front half of the
// T14-F02 typed-generation vertical: it owns the validation that a generated member/impl/helper
// is well-formed before it is handed to the ordinary binder.
class MetaprogrammingAstBuilder final
{
public:
    GeneratedDeclaration buildField(
        std::string name,
        std::string typeKey,
        std::size_t hygieneScope,
        std::string originPath,
        bool isPublic = false,
        bool isStatic = false,
        bool isReadonly = false) const;

    GeneratedDeclaration buildMethod(
        std::string name,
        std::string typeKey,
        std::size_t hygieneScope,
        std::string originPath,
        bool isPublic = false,
        bool isStatic = false) const;

    GeneratedDeclaration buildImpl(
        std::string traitName,
        std::string targetTypeKey,
        std::size_t hygieneScope,
        std::string originPath) const;

    GeneratedDeclaration buildHelper(
        std::string name,
        std::string typeKey,
        std::size_t hygieneScope,
        std::string originPath,
        bool isPublic = false) const;

    // A generated declaration must not collide with an existing member of the same
    // spelling+scope; this is the "same binder" gate exercised before binding.
    static bool wouldCollide(
        const std::vector<HygienicName>& existing,
        const GeneratedDeclaration& declaration) noexcept;
};

// NS304-T14-F05: the unified identity of a generated member — the same fully-qualified form as a
// source member's identity.key(), so a generated member participates in the partial merge, the
// extension lookup, and associated items by the identical identity (deterministic, so cycle
// resolution compares the same stable string).
std::string generatedMemberIdentity(
    std::string_view enclosingTypeIdentity,
    std::string_view memberName);
bool sameGeneratedIdentity(std::string_view left, std::string_view right) noexcept;
}
