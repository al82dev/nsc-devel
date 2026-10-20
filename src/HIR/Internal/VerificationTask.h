#pragma once

#include "VerificationTaskKind.h"

#include <cstddef>

namespace nativesharp::sema
{
class Type;
}

namespace nativesharp::hir
{
class HirNode;
}

namespace nativesharp::hir::internal
{
struct VerificationTask final
{
    using Kind = VerificationTaskKind;

    Kind kind = Kind::Node;
    const HirNode* node = nullptr;
    sema::Type* functionReturnType = nullptr;
    int loopDepth = 0;
    std::size_t childIndex = 0;
};
}
