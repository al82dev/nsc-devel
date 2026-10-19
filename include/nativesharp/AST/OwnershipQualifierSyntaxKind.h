#pragma once

#include <string_view>

namespace nativesharp::ast
{
enum class OwnershipQualifierSyntaxKind
{
    None,
    Owned,
    Borrowed
};

std::string_view ownershipQualifierSyntaxKindName(OwnershipQualifierSyntaxKind kind) noexcept;
}
