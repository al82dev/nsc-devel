#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"

#include "nativesharp/AST/TypeSyntax.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectNameLookup::LookupEnvironment final
{
    std::vector<std::string> importedNamespaces;
    std::unordered_map<std::string, std::string> aliases;
    // C3 "FFI aliases": a `using` alias whose target is a TYPE rather than a namespace, kept as
    // the parsed type syntax so the expansion can carry pointers, fixed arrays and builtins --
    // everything a C typedef says -- and not just a name. `aliases` above stays the namespace
    // half; a given alias name is in exactly one of the two maps.
    std::unordered_map<std::string, const ast::TypeSyntax*> typeAliases;
    // NSC-LIMIT-009. Los tipos dentro de los que se esta escribiendo, del mas exterior al mas
    // interior. Es lo que convierte `Item` en `ns.Outer.Item` sin que nadie lo escriba, y lo que
    // permite decidir si un anidado `private` se ve desde aqui.
    std::vector<std::size_t> enclosingTypeOrdinals;
};
}
