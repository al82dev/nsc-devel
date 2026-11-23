#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"
#include "nativesharp/Interop/CppAbiPlan.h"

#include <optional>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::interop
{
enum class CppLanguageLinkage { None, C, Cpp };

// T10-010: how a default argument is known. Unevaluated means Clang did not reduce it to one of the
// constant kinds below, so a NativeSharp call keeps supplying that argument.
enum class CppDefaultArgumentKind : std::uint8_t { None, Unevaluated, Signed, Unsigned, Floating, Boolean, NullPointer };

// Textual spellings support identification and diagnostics; they are not an ABI classification.
struct CppFunctionParameter
{
    std::string name;
    std::string typeSpelling;
    std::string canonicalTypeSpelling;
    CppType type;
    bool hasDefaultArgument = false;
    // EN: T19-003. A pointer parameter of a macro wrapper that stands for an lvalue argument of the macro: it is
    //     imported as a `ref` parameter, so `M(ref x)` lets the macro assign x.
    // ES: T19-003. Un parametro puntero de un envoltorio de macro que representa un argumento lvalue de la macro:
    //     se importa como parametro `ref`, asi `M(ref x)` deja que la macro asigne x.
    bool cMacroReference = false;
    // EN: Item 8.3. The pointer is declared non-null (`nonnull`, `_Nonnull`): a null argument is rejected.
    // ES: Item 8.3. El puntero se declara no nulo (`nonnull`, `_Nonnull`): un argumento nulo se rechaza.
    bool nonNull = false;
    // Clang adds a size_t argument immediately after this pointer parameter.
    std::optional<std::uint8_t> objectSizeMode{};
    bool objectSizeDynamic = false;
    // T10-010: the constant Clang evaluated for the default argument; integers keep their two's
    // complement bits and a floating value its IEEE binary64 bits.
    CppDefaultArgumentKind defaultArgumentKind = CppDefaultArgumentKind::None;
    std::uint64_t defaultArgumentBits = 0;
};

struct CppFunctionDeclaration
{
    std::string name;
    std::string qualifiedName;
    std::string canonicalFunctionType;
    std::string callingConvention;
    CppLanguageLinkage languageLinkage = CppLanguageLinkage::None;
    std::string returnTypeSpelling;
    std::string canonicalReturnTypeSpelling;
    CppType returnType;
    std::vector<CppFunctionParameter> parameters;
    bool variadic = false;
    bool externalLinkage = false;
    // True only if Clang resolved a non-throwing exception specification.
    bool isNonThrowing = false;
    bool dllImport = false;
    // EN: Item 8.7. `returns_twice` (setjmp and similar): the call may return again through longjmp.
    // ES: Item 8.7. `returns_twice` (setjmp y similares): la llamada puede volver otra vez por longjmp.
    bool returnsTwice = false;
    // An external spelling does not guarantee that a particular binary defines the symbol.
    std::optional<std::string> externalSymbol;
    std::optional<CppSourcePoint> location;
    // T10-002: other qualified names that denote this same function in C++: its path through inline
    // namespaces, namespace aliases that reach it and using-declarations in a namespace.
    std::vector<std::string> alternateQualifiedNames;
    // Opaque Clang USR; absence means Clang could not produce one.
    std::optional<std::string> clangUsr{};
    // A C declaration f() has no prototype; f(void) does.
    bool hasPrototype = true;
    // Definition owned by this C translation unit; not an external ABI symbol.
    bool isCStaticInlineDefinition = false;
    // EN: Item 15.6. A C++ function with internal linkage defined in the header (`static inline int f(...)`): NSC
    //     emits its own weak_odr copy under the mangled name, as for an inline function.
    // ES: Item 15.6. Una funcion C++ con enlace interno definida en el encabezado: NSC emite su propia copia weak_odr
    //     con el nombre decorado, como una funcion inline.
    bool cppInternalDefinition = false;
    // T13-005: a function template specialization NativeSharp requested. / Una especializacion pedida.
    bool templateSpecialization = false;
    // EN: T19-003. For a C function-like macro called from NativeSharp, the C name of the static inline wrapper
    //     that expands it with the types of one call; `name` is the macro's. Empty for any other function.
    // ES: T19-003. Para una macro de funcion C llamada desde NativeSharp, el nombre C del envoltorio static inline
    //     que la expande con los tipos de una llamada; `name` es el de la macro. Vacio para cualquier otra funcion.
    std::string cMacroWrapper;
    // Optional experimental ABI observation; absence never implies a Win64 fallback.
    std::shared_ptr<const abi_plan::Plan> abiPlan;
    // X1-03: a `= delete` function: it takes part in overload selection and a call that selects it is rejected.
    //     / Una funcion `= delete`: participa en la seleccion y una llamada que la elige se rechaza.
    bool isDeleted = false;
    // EN: X2-05. A C++ function that returns a non-trivial record by value, published as its in-place adapter
    //     `T* adapter(T* self, ...)`: the result is constructed in the destination, as a constructor would.
    // ES: X2-05. Una funcion C++ que devuelve un record no trivial por valor, publicada como su adaptador en sitio:
    //     el resultado se construye en el destino, como lo haria un constructor.
    bool cppFactoryResult = false;
};
}
