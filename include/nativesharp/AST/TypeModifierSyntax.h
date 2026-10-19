#pragma once

#include "nativesharp/AST/PointerNullabilitySyntaxKind.h"
#include "nativesharp/AST/TypeModifierSyntaxKind.h"
#include "nativesharp/Basic/SourceRange.h"

#include <cstddef>

namespace nativesharp::ast
{
class AstNode;

struct TypeModifierSyntax
{
    TypeModifierSyntaxKind kind = TypeModifierSyntaxKind::Pointer;
    AstNode* arrayCount = nullptr;
    SourceRange range;
    std::size_t arrayRank = 0;
    // T13-F03 semantic pointer guarantees. LLVM uses opaque pointers and Win64 layout is
    // unchanged, so these promises travel beside the physical canonical key.
    PointerNullabilitySyntaxKind pointerNullability =
        PointerNullabilitySyntaxKind::LegacyNullable;
    bool pointerHasProvenance = true;
    std::size_t pointerKnownAlignment = 1;
    unsigned pointerAddressSpace = 0;
};
}
