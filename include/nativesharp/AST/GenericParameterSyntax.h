#pragma once

#include "nativesharp/AST/GenericParameterSyntaxKind.h"
#include "nativesharp/Basic/SourceRange.h"

#include <string>

namespace nativesharp::ast
{
class TypeSyntax;

struct GenericParameterSyntax
{
    std::string name;
    GenericParameterSyntaxKind kind = GenericParameterSyntaxKind::Type;
    TypeSyntax* constantType = nullptr;
    SourceRange range;
};
}
