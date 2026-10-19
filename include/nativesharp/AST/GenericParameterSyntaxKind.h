#pragma once

namespace nativesharp::ast
{
enum class GenericParameterSyntaxKind
{
    Type,
    Constant,
    TypePack,
    ConstantPack
};
}
