#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Interop/CppType.h"
#include "nativesharp/Interop/CppAbiPlan.h"
#include "nativesharp/Interop/CppSessionDiagnostic.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class ProjectGenericModel;
struct NamespaceFunctionSymbol final
{
    NamespaceFunctionAccessibility accessibility = NamespaceFunctionAccessibility::Internal;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    std::string sourcePath;
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    const ast::TypeSyntax* declaredTypeSyntax = nullptr;
    std::string declaredTypeKey;
    const ast::TypeSyntax* declaredErrorTypeSyntax = nullptr;
    std::string declaredErrorTypeKey;
    sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
    std::vector<MethodParameterSymbol> parameters;
    std::string signatureKey;
    std::string callableIdentityKey;
    std::string mangledName;
    // Non-empty only for a declaration imported from C/C++; its identity stays distinct from
    // the NativeSharp representation key used for value types and overload candidates.
    std::string cppCanonicalIdentity;
    std::optional<std::string> cppClangUsr;
    // T10-002: further NativeSharp namespaces where this imported C++ function is also visible, from
    // inline namespaces, namespace aliases and using-declarations; the symbol itself is shared.
    std::vector<std::string> cppAlternateNamespaces;
    std::string cppCallingConvention;
    // Original Clang types retain qualifiers at every captured level. They do
    // not, by themselves, authorize emitting noalias or changing call ABI.
    std::optional<interop::CppType> cppReturnType;
    std::vector<interop::CppType> cppParameterTypes;
    // 0 means no hidden argument; 1..4 are pass_object_size modes 0..3,
    // 5..8 are the corresponding dynamic modes.
    std::vector<std::uint8_t> cppObjectSizeModes;
    bool cppDllImport = false;
    // EN: T17-003. For a C/C++ function: its C++ declaration as text (`int A::f(double)`) and where the header
    //     declares it, for diagnostics only. A function restored from a precompiled catalog has neither.
    // ES: T17-003. En una funcion C/C++: su declaracion C++ como texto (`int A::f(double)`) y donde la declara
    //     el encabezado, solo para diagnosticos. Una funcion restaurada de un catalogo precompilado no tiene.
    std::string cppDeclarationSpelling{};
    std::optional<interop::CppSourcePoint> cppLocation;
    // T13-005: a C++ function template specialization; a function that is not one is better when the
    // conversions tie, as in C++. / Una especializacion de plantilla de funcion C++; una funcion que no lo es
    // es mejor si las conversiones empatan, como en C++.
    bool cppTemplateSpecialization = false;
    // T19-040: the C name of a macro wrapper. / El nombre C de un envoltorio de macro.
    std::string cppMacroWrapper;
    const ast::AstNode* declarationSyntax = nullptr;
    const ast::AstNode* definitionSyntax = nullptr;
    std::vector<NamespaceFunctionPart> parts;
    bool isReferenceReturn = false;
    std::string returnLifetimeName;
    std::vector<std::string> lifetimeParameters;
    std::vector<std::pair<std::string, std::string>> lifetimeOutlivesConstraints;
    std::vector<language::EscapeClass> parameterEscapeSummary;
    sema::EffectSet effects;
    bool isVariadic = false;
    InliningPolicy inliningPolicy = InliningPolicy::Automatic;
    bool isExternC = false;
    bool isExportedC = false;
    bool isPrecompiled = false;
    bool isExtension = false;
    // NS304-T08-F07: static extension (`nsc-extension-static`) has no implicit receiver; distinct
    // from `isExtension` so NSMETA can publish the "static" marker and the consumer rebuilds it
    // without discarding the first parameter as a receiver.
    bool isStaticExtension = false;
    // X2: a C++ member function template specialization, an extension of its record visible from any namespace
    //     (as the member it is). / Una especializacion de plantilla miembro C++, extension visible desde cualquier
    //     namespace.
    bool cppMemberTemplate = false;
    // NS304-T08-F04: for an extension operator, the operation token (Plus/Minus/...). Invalid for
    // non-operator extension functions.
    lexer::TokenKind operatorToken = lexer::TokenKind::Invalid;
    // The DLL identity of a restored C declaration, when its producer recorded one.
    std::string cppDllRuntimeIdentity;
    // EN: T08-D13b. Volatile mode (VolatileMode) of the C reference that declares the function; absent when its
    //     references disagree or the session does not report one. A returned `volatile T*` starts the C alias
    //     contract with this mode.
    // ES: T08-D13b. Modo volatile (VolatileMode) de la referencia C que declara la funcion; ausente si sus
    //     referencias discrepan o la sesion no lo informa. Un `volatile T*` devuelto inicia el contrato de alias
    //     C con este modo.
    std::optional<bool> cppMicrosoftVolatileSemantics;
    // Carried for future validated ABI lowering; not sufficient to enable a call.
    std::shared_ptr<const interop::abi_plan::Plan> cppAbiPlan;
    // X1-03: a C++ `= delete` function. / Una funcion C++ `= delete`.
    bool cppDeleted = false;
    // R1 (D6): an owned result released by this function. / Un resultado con dueno que libera esta funcion.
    std::string cppOwnedRelease;
    // X2-05: its call constructs the result in the destination. / Su llamada construye el resultado en el destino.
    bool cppFactoryConstruction = false;
    // EN: X2-05. A by-value adapter's original symbol, called directly when every by-value record argument is a
    //     prvalue constructed in the caller's temporary (Microsoft x64: the callee destroys it); cppByValueDirect
    //     marks that hidden variant, which no name lookup finds.
    // ES: X2-05. El simbolo original de un adaptador por valor, llamado directamente cuando cada argumento record por
    //     valor es un prvalue construido en el temporal del llamador; cppByValueDirect marca esa variante oculta.
    std::string cppByValueDirectSymbol;
    bool cppByValueDirectDllImport = false;
    bool cppByValueDirect = false;
};
}
