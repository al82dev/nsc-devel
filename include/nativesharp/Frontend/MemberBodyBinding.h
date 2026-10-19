#pragma once

#include "nativesharp/Frontend/MemberBodyBindingKind.h"

#include "nativesharp/AST/AstNode.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct MemberBodyBinding final
{
    MemberBodyBindingKind kind = MemberBodyBindingKind::ThisReference;
    const ast::AstNode* syntax = nullptr;
    std::size_t declaringTypeOrdinal = 0;
    std::size_t methodMemberOrdinal = 0;
    std::string memberName;
    std::vector<std::size_t> candidateMemberOrdinals;
};
}
