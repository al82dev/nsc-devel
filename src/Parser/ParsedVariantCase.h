#pragma once

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Source/SourcePosition.h"

#include <string>

namespace nativesharp::parser
{
struct ParsedVariantCase final
{
    std::string name;
    SourceRange range;
    ast::TypeSyntax* payloadType = nullptr;
};
}
