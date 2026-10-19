#pragma once

#include "nativesharp/Language/GeneratedDeclaration.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::language
{
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
}
