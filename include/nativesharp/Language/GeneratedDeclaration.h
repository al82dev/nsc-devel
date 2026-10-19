#pragma once

#include "nativesharp/Language/GeneratedDeclarationKind.h"
#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include <string>

namespace nativesharp::language
{
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
}
