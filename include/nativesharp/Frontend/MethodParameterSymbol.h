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
    bool pointerPointeeVolatile = false;
    // EN: T19-007. A C++ `T&&` parameter: a ref (or, for `const T&&`, in) parameter that also binds a value written
    //     without the keyword, which the call materializes in a temporary, as C++ binds a prvalue; `ref x` passes x
    //     itself, as `std::move(x)` does. An lvalue of the parameter's type without the keyword is rejected, as C++
    //     rejects binding it.
    // ES: T19-007. Un parametro C++ `T&&`: un parametro ref (o in, para `const T&&`) que tambien enlaza un valor
    //     escrito sin la palabra clave, que la llamada materializa en un temporal, como C++ enlaza un prvalue;
    //     `ref x` pasa x mismo, como `std::move(x)`. Un lvalue del tipo del parametro sin la palabra se rechaza,
    //     como C++ rechaza enlazarlo.
    bool cppRvalueReference = false;
    // EN: T19-007. The object operand of a C++ member operator, which a Clang adapter takes as its first parameter
    //     (T12-004). A non-const operator takes it as `T&`, yet it also binds a temporary, materialized for the call,
    //     as C++ binds the implicit object parameter of a member without a ref-qualifier ([over.match.funcs]).
    // ES: T19-007. El operando objeto de un operador miembro C++, que un adaptador de Clang recibe como su primer
    //     parametro (T12-004). Un operador no const lo recibe como `T&`, pero tambien enlaza un temporal,
    //     materializado para la llamada, como C++ enlaza el parametro objeto implicito de un miembro sin
    //     calificador de referencia ([over.match.funcs]).
    bool cppImplicitObject = false;
    // EN: T19-002. The immediate pointee is C `_Atomic` storage (a C `_Atomic T*` parameter): every
    //     access through the pointer is a sequentially consistent atomic access, as in C. Not user syntax.
    // ES: T19-002. El dato apuntado inmediato es almacenamiento C `_Atomic` (un parametro `_Atomic T*` de
    //     C): cada acceso por el puntero es atomico y secuencialmente consistente, como en C. No es
    //     sintaxis de usuario.
    bool cppPointerPointeeAtomic = false;
};
}
