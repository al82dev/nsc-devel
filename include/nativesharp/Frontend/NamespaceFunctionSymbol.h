#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Interop/CppType.h"
#include "nativesharp/Interop/CppAbiPlan.h"

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
    std::string cppCallingConvention;
    // Original Clang types retain qualifiers at every captured level. They do
    // not, by themselves, authorize emitting noalias or changing call ABI.
    std::optional<interop::CppType> cppReturnType;
    std::vector<interop::CppType> cppParameterTypes;
    // 0 means no hidden argument; 1..4 are pass_object_size modes 0..3,
    // 5..8 are the corresponding dynamic modes.
    std::vector<std::uint8_t> cppObjectSizeModes;
    bool cppDllImport = false;
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
    // NS304-T08-F07: static extension (`ns3-extension-static`) has no implicit receiver; distinct
    // from `isExtension` so NSMETA can publish the "static" marker and the consumer rebuilds it
    // without discarding the first parameter as a receiver.
    bool isStaticExtension = false;
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
};
}
