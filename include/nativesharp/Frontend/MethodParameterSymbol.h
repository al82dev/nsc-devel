#pragma once

#include "nativesharp/Frontend/ParameterPassKind.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Sema/OwnershipKind.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct MethodParameterSymbol final
{
    std::string name;
    // C4: Value for every ordinary parameter. For the other three, `typeKey` is the POINTER type
    // (`T*`) -- the parser adds that modifier -- because a by-reference parameter genuinely
    // receives an address; `pointeeTypeKey` is what the programmer wrote and what a use of the
    // parameter inside the body evaluates to.
    ParameterPassKind passKind = ParameterPassKind::Value;
    std::string pointeeTypeKey;
    const ast::TypeSyntax* typeSyntax = nullptr;
    std::string typeKey;
    bool pointerPointeeReadonly = false;
    bool pointerNullable = true;
    bool pointerHasProvenance = true;
    std::size_t pointerKnownAlignment = 1;
    unsigned pointerAddressSpace = 0;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    // T06-F04. Empty for value/out parameters. Elided ref/in parameters receive a stable
    // declaration-local synthetic name so a ref return can still bind to exactly one input.
    std::string lifetimeName;
    // NativeSharp 3.0 T01. A typed pack is always the final ReadOnlySpan<T> parameter. This bit
    // changes call-site binding only; the physical ABI remains one ordinary span value.
    bool isParameterPack = false;
    // Canonical T from ReadOnlySpan<T>. The closed span identity itself is mangled and therefore
    // intentionally does not need to encode or be reparsed to recover its element type.
    std::string parameterPackElementTypeKey;
    // NativeSharp 3.0 T13-F06. These are semantic C-boundary contracts. The linked length
    // ordinal is resolved after collecting the complete parameter list and is serialized in
    // NSMETA so a consumer validates the same declaration reconstructed from LibraryReference.
    std::string ffiLengthParameterName;
    std::optional<std::size_t> ffiLengthParameterOrdinal;
    bool ffiCallback = false;
    bool ffiCallbackMayEscape = false;
    // NSC-LIMIT-020. Defaults remain attached to the declaration AST so ProjectReference calls
    // materialize the value fixed by the callee's contract, not a caller-side reparse.
    const ast::AstNode* defaultValueSyntax = nullptr;
};
}
