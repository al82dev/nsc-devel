#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
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
};
}
