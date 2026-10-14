#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
sema::OwnershipKind ownershipKindFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    if (syntax == nullptr) return sema::OwnershipKind::None;
    switch (syntax->ownershipQualifier())
    {
        case ast::OwnershipQualifierSyntaxKind::None: return sema::OwnershipKind::None;
        case ast::OwnershipQualifierSyntaxKind::Owned: return sema::OwnershipKind::Owned;
        case ast::OwnershipQualifierSyntaxKind::Borrowed: return sema::OwnershipKind::Borrowed;
    }
    return sema::OwnershipKind::None;
}

std::string ownershipSignaturePrefix(sema::OwnershipKind ownership)
{
    switch (ownership)
    {
        case sema::OwnershipKind::None: return {};
        case sema::OwnershipKind::Owned: return "owned ";
        case sema::OwnershipKind::Borrowed: return "borrowed ";
    }
    return {};
}

ParameterPassKind parameterPassKind(const ast::AstNode& parameter) noexcept
{
    if (!parameter.auxiliaryText().starts_with("pass:")) return ParameterPassKind::Value;
    const std::string_view spelling(parameter.auxiliaryText().data() + 5,
                                    parameter.auxiliaryText().size() - 5);
    if (spelling == "ref") return ParameterPassKind::Ref;
    if (spelling == "out") return ParameterPassKind::Out;
    return ParameterPassKind::In;
}

bool declaresLifetime(const ast::AstNode& declaration, std::string_view name) noexcept
{
    return std::ranges::any_of(
        declaration.lifetimeParameters(),
        [&](const ast::LifetimeParameterSyntax& parameter)
        {
            return parameter.name == name;
        });
}

bool lifetimeOutlives(
    std::span<const std::pair<std::string, std::string>> constraints,
    std::string_view longer,
    std::string_view shorter)
{
    if (longer == shorter || longer == "static") return true;
    std::unordered_set<std::string> visited;
    std::vector<std::string> pending{std::string(longer)};
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(current).second) continue;
        for (const auto& [declaredLonger, declaredShorter] : constraints)
        {
            if (declaredLonger != current) continue;
            if (declaredShorter == shorter) return true;
            pending.push_back(declaredShorter);
        }
    }
    return false;
}

std::optional<std::pair<std::string, std::string>> lifetimeCycle(
    std::span<const std::pair<std::string, std::string>> constraints)
{
    for (const auto& [longer, shorter] : constraints)
        if (longer == shorter || lifetimeOutlives(constraints, shorter, longer))
            return std::pair{longer, shorter};
    return std::nullopt;
}

std::string nameLookupKey(
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    std::string_view name)
{
    return std::to_string(projectOrdinal) + "\x1f" + std::string(namespaceName) +
        "\x1f" + std::string(name);
}

std::string sanitizeSymbol(std::string value)
{
    for (char& character : value)
    {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) == 0 && character != '_') character = '_';
    }
    return value;
}

std::string namespaceFunctionMangledName(const NamespaceFunctionSymbol& symbol)
{
    std::string result = "__nsc_function_" +
        compilation::symbolFragmentForIdentityKey(
            symbol.moduleName + "::" + symbol.namespaceName) +
        "__" + sanitizeSymbol(symbol.name);
    for (const MethodParameterSymbol& parameter : symbol.parameters)
    {
        if (parameter.ownership == sema::OwnershipKind::Owned) result += "__owned";
        else if (parameter.ownership == sema::OwnershipKind::Borrowed) result += "__borrowed";
        if (parameter.passKind != ParameterPassKind::Value)
            result += "__" + std::string(parameterPassKindKeyword(parameter.passKind));
        result += "__" + compilation::symbolFragmentForIdentityKey(parameter.typeKey);
    }
    if (symbol.isVariadic) result += "__variadic";
    if (symbol.isExternC) result += "__extern_c";
    return result;
}

std::string namespaceStateStorageName(const NamespaceStateSymbol& symbol)
{
    return "__nsc_state_" +
        compilation::symbolFragmentForIdentityKey(
            symbol.moduleName + "::" + symbol.namespaceName) +
        "__" + sanitizeSymbol(symbol.name);
}

bool isReferenceLikeStateType(std::string_view typeKey) noexcept
{
    return typeKey == "builtin:string" ||
        ProjectTypeConversion::isPointer(typeKey) ||
        ProjectTypeConversion::isNativeArray(typeKey);
}

bool initializeNamespaceStateConstant(NamespaceStateSymbol& symbol)
{
    const ast::AstNode* initializer = symbol.initializerSyntax;
    if (initializer == nullptr)
    {
        symbol.initializerKind = NamespaceStateInitializerKind::Zero;
        return !symbol.isConst;
    }

    ProjectIntegerEvaluator integerEvaluator(nullptr);
    if (const std::optional<ProjectIntegerValue> value = integerEvaluator.evaluate(initializer);
        value.has_value() && ProjectTypeConversion::isIntegral(symbol.declaredTypeKey))
    {
        symbol.initializerKind = NamespaceStateInitializerKind::Integer;
        symbol.integerValue = ProjectIntegerEvaluator::normalize(
            value->bits, symbol.declaredTypeKey);
        return true;
    }

    switch (initializer->kind())
    {
        case ast::AstNodeKind::FloatingLiteralExpression:
            if (symbol.declaredTypeKey != "builtin:float" &&
                symbol.declaredTypeKey != "builtin:double") return false;
            symbol.initializerKind = NamespaceStateInitializerKind::Floating;
            symbol.floatingValue = initializer->floatingValue();
            return true;
        case ast::AstNodeKind::BooleanLiteralExpression:
            if (symbol.declaredTypeKey != "builtin:bool") return false;
            symbol.initializerKind = NamespaceStateInitializerKind::Boolean;
            symbol.booleanValue = initializer->tokenKind() == lexer::TokenKind::KwTrue;
            return true;
        case ast::AstNodeKind::StringLiteralExpression:
            if (symbol.declaredTypeKey != "builtin:string" &&
                symbol.declaredTypeKey != "builtin:char*") return false;
            symbol.initializerKind = NamespaceStateInitializerKind::String;
            symbol.stringValue = initializer->text();
            return true;
        case ast::AstNodeKind::NullLiteralExpression:
            if (!isReferenceLikeStateType(symbol.declaredTypeKey)) return false;
            symbol.initializerKind = NamespaceStateInitializerKind::Null;
            return true;
        case ast::AstNodeKind::DefaultValueExpression:
            symbol.initializerKind = NamespaceStateInitializerKind::Zero;
            return true;
        default:
            return false;
    }
}

std::string mergeKey(const NamespaceFunctionSymbol& symbol)
{
    return symbol.moduleName + "\x1f" + symbol.namespaceName + "\x1f" + symbol.signatureKey;
}

void reportPrevious(
    const NamespaceFunctionSymbol& previous,
    diagnostics::DiagnosticEngine& diagnostics)
{
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        previous.range,
        "previous declaration of namespace function '" + previous.signatureKey + "' is here");
}

bool isExtensionDeclaration(const ast::AstNode& syntax) noexcept
{
    return syntax.auxiliaryText() == "ns3-extension" ||
        syntax.auxiliaryText() == "ns3-extension-static" ||
        syntax.auxiliaryText().find("|ns3-extension-static") != std::string::npos ||
        syntax.auxiliaryText().find("nsc-meta:ns3-extension") != std::string::npos;
}

bool isStaticExtensionDeclaration(const ast::AstNode& syntax) noexcept
{
    return syntax.auxiliaryText() == "ns3-extension-static" ||
        syntax.auxiliaryText().find("|ns3-extension-static") != std::string::npos;
}

bool compatibleDeclarations(
    const NamespaceFunctionSymbol& symbol,
    const NamespaceFunctionDeclaration& declaration,
    std::string_view returnTypeKey,
    sema::OwnershipKind returnOwnership,
    std::string_view errorTypeKey) noexcept
{
    const ast::AstNode* syntax = declaration.syntax;
    return symbol.accessibility == declaration.accessibility &&
        symbol.declaredTypeKey == returnTypeKey &&
        symbol.declaredOwnership == returnOwnership &&
        symbol.declaredErrorTypeKey == errorTypeKey &&
        symbol.isReferenceReturn == syntax->isReference() &&
        symbol.isVariadic == syntax->isVariadic() &&
        symbol.isExternC ==
            (syntax->flag() || syntax->hasModifier(lexer::TokenKind::KwExtern)) &&
        symbol.isExportedC == syntax->hasModifier(lexer::TokenKind::KwExport) &&
        // Extension-ness is part of the declaration contract: merging an ordinary free
        // function with an extension declaration would make lookup depend on source order.
        symbol.isExtension == isExtensionDeclaration(*syntax);
}

struct FunctionLookupEnvironment final
{
    std::vector<std::string> importedNamespaces;
    std::unordered_map<std::string, std::string> aliases;
};

std::vector<std::string> splitName(std::string_view name)
{
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin < name.size())
    {
        const std::size_t dot = name.find('.', begin);
        const std::size_t end = dot == std::string_view::npos ? name.size() : dot;
        if (end != begin) segments.emplace_back(name.substr(begin, end - begin));
        if (dot == std::string_view::npos) break;
        begin = dot + 1;
    }
    return segments;
}

std::string joinName(std::span<const std::string> segments)
{
    std::string result;
    for (const std::string& segment : segments)
    {
        if (!result.empty()) result.push_back('.');
        result += segment;
    }
    return result;
}

std::optional<std::string> expressionName(const ast::AstNode* expression)
{
    if (expression == nullptr) return std::nullopt;
    if (expression->kind() == ast::AstNodeKind::IdentifierExpression)
        return expression->text().empty() ? std::nullopt
                                        : std::optional<std::string>(expression->text());
    if (expression->kind() != ast::AstNodeKind::MemberExpression ||
        expression->first() == nullptr || expression->text().empty()) return std::nullopt;
    std::optional<std::string> prefix = expressionName(expression->first());
    if (!prefix.has_value()) return std::nullopt;
    prefix->push_back('.');
    *prefix += expression->text();
    return prefix;
}
}

bool ProjectNamespaceFunctionModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics,
    const ProjectGenericModel* genericModel)
{
    functions_.clear();
    states_.clear();
    stateByName_.clear();
    functionsByName_.clear();
    referenceBindings_.clear();
    referenceBindingBySyntax_.clear();
    extensionBindings_.clear();
    extensionBindingBySyntax_.clear();
    built_ = false;
    const std::size_t initialErrorCount = diagnostics.errorCount();
    std::unordered_map<std::string, std::size_t> symbolByIdentity;
    std::unordered_map<std::string, std::size_t> exportedCByName;

    for (const NamespaceFunctionDeclaration& declaration : nameLookup.namespaceFunctions())
    {
        if (declaration.syntax == nullptr) continue;
        const ast::AstNode& syntax = *declaration.syntax;

        NamespaceFunctionSymbol candidate;
        candidate.accessibility = declaration.accessibility;
        candidate.projectOrdinal = declaration.projectOrdinal;
        candidate.sourceOrdinal = declaration.sourceOrdinal;
        candidate.fileId = declaration.fileId;
        candidate.range = declaration.range;
        candidate.sourcePath = declaration.sourcePath;
        candidate.moduleName = declaration.moduleName;
        candidate.namespaceName = declaration.namespaceName;
        candidate.name = declaration.name;
        candidate.declaredTypeSyntax = syntax.typeSyntax();
        candidate.declaredTypeKey = memberModel.canonicalTypeKey(syntax.typeSyntax(), nameLookup);
        candidate.declaredErrorTypeSyntax = syntax.throwsTypeSyntax();
        candidate.declaredErrorTypeKey = syntax.throwsTypeSyntax() == nullptr
            ? std::string{}
            : memberModel.canonicalTypeKey(syntax.throwsTypeSyntax(), nameLookup);
        if (!candidate.declaredErrorTypeKey.empty())
            candidate.effects = candidate.effects.with(sema::Effect::Throws);
        candidate.declaredOwnership = ownershipKindFromSyntax(syntax.typeSyntax());
        candidate.declarationSyntax = declaration.syntax;
        candidate.definitionSyntax = declaration.hasBody ? declaration.syntax : nullptr;
        candidate.isReferenceReturn = syntax.isReference();
        candidate.returnLifetimeName = syntax.lifetimeName();
        for (const ast::LifetimeParameterSyntax& parameter : syntax.lifetimeParameters())
            candidate.lifetimeParameters.push_back(parameter.name);
        for (const ast::LifetimeOutlivesConstraint& constraint :
             syntax.lifetimeOutlivesConstraints())
        {
            candidate.lifetimeOutlivesConstraints.emplace_back(
                constraint.longer, constraint.shorter);
        }
        candidate.isVariadic = syntax.isVariadic();
        candidate.inliningPolicy = syntax.hasModifier(lexer::TokenKind::KwForceInline)
            ? InliningPolicy::Required
            : syntax.hasModifier(lexer::TokenKind::KwNoInline)
                ? InliningPolicy::Forbidden
                : syntax.hasModifier(lexer::TokenKind::KwInline)
                    ? InliningPolicy::Preferred
                    : InliningPolicy::Automatic;
        candidate.isExternC =
            syntax.flag() || syntax.hasModifier(lexer::TokenKind::KwExtern);
        if (candidate.isExternC && !candidate.declaredErrorTypeKey.empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidExternCSignature,
                diagnostics::DiagnosticSeverity::Error,
                candidate.range,
                "extern C namespace function '" + candidate.name +
                    "' cannot declare throws; catch and translate the error in an explicit non-throwing adapter");
        }
        candidate.isExportedC = syntax.hasModifier(lexer::TokenKind::KwExport);
        candidate.isPrecompiled = declaration.isPrecompiled;
        candidate.isExtension = isExtensionDeclaration(syntax);
        candidate.isStaticExtension = isStaticExtensionDeclaration(syntax);
        candidate.operatorToken = lexer::TokenKind::Invalid;
        if (candidate.isExtension)
        {
            // NS304-T08-F04: recover the operator token from the operator name (operator+ -> +).
            const std::string_view name = candidate.name;
            constexpr std::string_view prefix = "operator";
            if (name.starts_with(prefix))
            {
                const std::string_view spelling = name.substr(prefix.size());
                if (spelling == "+") candidate.operatorToken = lexer::TokenKind::Plus;
                else if (spelling == "-") candidate.operatorToken = lexer::TokenKind::Minus;
                else if (spelling == "*") candidate.operatorToken = lexer::TokenKind::Star;
                else if (spelling == "/") candidate.operatorToken = lexer::TokenKind::Slash;
                else if (spelling == "%") candidate.operatorToken = lexer::TokenKind::Percent;
                else if (spelling == "!") candidate.operatorToken = lexer::TokenKind::Bang;
                else if (spelling == "~") candidate.operatorToken = lexer::TokenKind::Tilde;
                else if (spelling == "&") candidate.operatorToken = lexer::TokenKind::Amp;
                else if (spelling == "|") candidate.operatorToken = lexer::TokenKind::Pipe;
                else if (spelling == "^") candidate.operatorToken = lexer::TokenKind::Caret;
                else if (spelling == "<<") candidate.operatorToken = lexer::TokenKind::LessLess;
                else if (spelling == ">>") candidate.operatorToken = lexer::TokenKind::GreaterGreater;
                else if (spelling == "==") candidate.operatorToken = lexer::TokenKind::EqualEqual;
                else if (spelling == "!=") candidate.operatorToken = lexer::TokenKind::BangEqual;
                else if (spelling == "<=") candidate.operatorToken = lexer::TokenKind::LessEqual;
                else if (spelling == ">=") candidate.operatorToken = lexer::TokenKind::GreaterEqual;
                else if (spelling == "<") candidate.operatorToken = lexer::TokenKind::Less;
                else if (spelling == ">") candidate.operatorToken = lexer::TokenKind::Greater;
            }
        }
        candidate.parts.push_back(NamespaceFunctionPart{
            declaration.accessibility,
            declaration.sourceOrdinal,
            declaration.fileId,
            declaration.range,
            declaration.sourcePath,
            declaration.syntax,
            declaration.hasBody});

        std::unordered_map<std::string, const ast::AstNode*> parameterNames;
        bool sawParameterPack = false;
        for (const ast::AstNode* parameterNode : syntax.children())
        {
            if (parameterNode == nullptr ||
                parameterNode->kind() != ast::AstNodeKind::ParameterDeclaration)
            {
                continue;
            }

            MethodParameterSymbol parameter;
            parameter.name = parameterNode->text();
            parameter.passKind = parameterPassKind(*parameterNode);
            parameter.typeSyntax = parameterNode->typeSyntax();
            parameter.typeKey = memberModel.canonicalTypeKey(parameter.typeSyntax, nameLookup);
            parameter.pointerPointeeReadonly = parameter.typeSyntax != nullptr &&
                parameter.typeSyntax->isPointeeConst();
            parameter.ownership = ownershipKindFromSyntax(parameter.typeSyntax);
            parameter.fileId = declaration.fileId;
            parameter.range = parameterNode->range();
            parameter.syntax = parameterNode;
            parameter.defaultValueSyntax = parameterNode->first();
            parameter.isParameterPack = parameterNode->auxiliaryText() == "params";
            if (sawParameterPack)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter.range,
                    "a typed parameter pack must be the final parameter");
            }
            if (parameter.isParameterPack)
            {
                sawParameterPack = true;
                const bool validPackSyntax = parameter.typeSyntax != nullptr &&
                    (parameter.typeSyntax->baseName() == "ReadOnlySpan" ||
                     parameter.typeSyntax->baseName().ends_with(".ReadOnlySpan")) &&
                    parameter.typeSyntax->typeArguments().size() == 1U;
                if (validPackSyntax)
                {
                    parameter.parameterPackElementTypeKey = memberModel.canonicalTypeKey(
                        parameter.typeSyntax->typeArguments().front(), nameLookup);
                }
                else if (genericModel != nullptr)
                {
                    const GenericSpecializationRecord* specialization =
                        genericModel->specializationForTypeKey(parameter.typeKey);
                    if (specialization != nullptr &&
                        specialization->templateKey.find("ReadOnlySpan`1") !=
                            std::string::npos &&
                        specialization->typeArguments.size() == 1U)
                    {
                        parameter.parameterPackElementTypeKey =
                            ProjectMemberModel::canonicalGenericArgumentKey(
                                specialization->typeArguments.front(), nameLookup);
                    }
                }
                if (parameter.parameterPackElementTypeKey.empty())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        parameter.range,
                        "params requires ReadOnlySpan<T>; implicit heap arrays are forbidden (resolved '" +
                            parameter.typeKey + "')");
                }
                if (parameter.defaultValueSyntax != nullptr)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        parameter.range,
                        "a typed parameter pack cannot declare a default value");
                }
            }
            if (parameter.passKind != ParameterPassKind::Value)
            {
                parameter.pointeeTypeKey = parameter.typeKey.empty() || parameter.typeKey.back() != '*'
                    ? parameter.typeKey
                    : parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
            }
            if (!parameterNode->lifetimeName().empty())
            {
                parameter.lifetimeName = parameterNode->lifetimeName();
                if (parameter.passKind == ParameterPassKind::Out)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        parameter.range,
                        "out parameter '" + parameter.name +
                            "' initializes storage and cannot carry an input lifetime");
                }
                else if (parameter.passKind == ParameterPassKind::Value)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        parameter.range,
                        "only ref or in parameters may carry a named lifetime");
                }
                else if (!declaresLifetime(syntax, parameter.lifetimeName))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::UnknownIdentifier,
                        diagnostics::DiagnosticSeverity::Error,
                        parameter.range,
                        "parameter '" + parameter.name + "' names undeclared lifetime '" +
                            parameter.lifetimeName + "'");
                }
            }
            else if (parameter.passKind == ParameterPassKind::Ref ||
                     parameter.passKind == ParameterPassKind::In)
            {
                parameter.lifetimeName = "$p" + std::to_string(candidate.parameters.size());
            }

            const auto [previous, inserted] = parameterNames.emplace(parameter.name, parameterNode);
            if (!inserted)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter.range,
                    "duplicate parameter '" + parameter.name + "' in namespace function '" +
                        declaration.name + "'");
                diagnostics.report(
                    diagnostics::DiagnosticId::PreviousDeclaration,
                    diagnostics::DiagnosticSeverity::Note,
                    previous->second->range(),
                    "previous parameter declaration is here");
            }
            candidate.parameters.push_back(std::move(parameter));
        }

        if (candidate.isReferenceReturn)
        {
            if (candidate.returnLifetimeName.empty())
            {
                std::unordered_set<std::string> inputs;
                for (const MethodParameterSymbol& parameter : candidate.parameters)
                {
                    if (parameter.passKind == ParameterPassKind::Ref ||
                        parameter.passKind == ParameterPassKind::In)
                        inputs.insert(parameter.lifetimeName);
                }
                if (inputs.size() == 1U)
                    candidate.returnLifetimeName = *inputs.begin();
                else if (inputs.empty())
                    candidate.returnLifetimeName = "static";
                else
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        candidate.range,
                        "ref-returning function '" + candidate.name +
                            "' has multiple input lifetimes; write ref[a] and bind the "
                            "intended ref/in parameters to lifetime a");
                }
            }
            else if (candidate.returnLifetimeName == "this")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    candidate.range,
                    "lifetime 'this' is unavailable on a namespace function");
            }
            else if (candidate.returnLifetimeName != "static" &&
                     !declaresLifetime(syntax, candidate.returnLifetimeName))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::UnknownIdentifier,
                    diagnostics::DiagnosticSeverity::Error,
                    candidate.range,
                    "ref return of '" + candidate.name + "' names undeclared lifetime '" +
                        candidate.returnLifetimeName + "'");
            }
            if (candidate.returnLifetimeName != "static" &&
                candidate.returnLifetimeName != "this" &&
                !candidate.returnLifetimeName.empty())
            {
                const bool hasSource = std::ranges::any_of(
                    candidate.parameters,
                    [&](const MethodParameterSymbol& parameter)
                    {
                        return (parameter.passKind == ParameterPassKind::Ref ||
                                parameter.passKind == ParameterPassKind::In) &&
                            lifetimeOutlives(
                                candidate.lifetimeOutlivesConstraints,
                                parameter.lifetimeName,
                                candidate.returnLifetimeName);
                    });
                if (!hasSource)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        candidate.range,
                        "ref return lifetime '" + candidate.returnLifetimeName +
                            "' has no ref/in parameter whose lifetime outlives it");
                }
            }
            if (const auto cycle = lifetimeCycle(candidate.lifetimeOutlivesConstraints))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    candidate.range,
                    "lifetime constraints contain a cycle between '" + cycle->first +
                        "' and '" + cycle->second + "'");
            }
        }
        else if (const auto cycle = lifetimeCycle(candidate.lifetimeOutlivesConstraints))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                candidate.range,
                "lifetime constraints contain a cycle between '" + cycle->first +
                    "' and '" + cycle->second + "'");
        }

        candidate.signatureKey = candidate.name + '(';
        for (std::size_t index = 0; index < candidate.parameters.size(); ++index)
        {
            if (index != 0) candidate.signatureKey.push_back(',');
            const MethodParameterSymbol& parameter = candidate.parameters[index];
            candidate.signatureKey += ownershipSignaturePrefix(parameter.ownership);
            if (parameter.passKind != ParameterPassKind::Value)
            {
                candidate.signatureKey += parameterPassKindKeyword(parameter.passKind);
                candidate.signatureKey.push_back(' ');
            }
            candidate.signatureKey += parameter.typeKey;
        }
        candidate.signatureKey.push_back(')');
        candidate.callableIdentityKey = "function:" + candidate.moduleName + "::" +
            candidate.namespaceName + "." + candidate.signatureKey +
            (candidate.isExternC ? "|extern-c" : "|native") +
            (candidate.isVariadic ? "|variadic" : "");
        candidate.mangledName = namespaceFunctionMangledName(candidate);
        if (candidate.isExportedC &&
            candidate.accessibility != NamespaceFunctionAccessibility::Public)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidExternCSignature,
                diagnostics::DiagnosticSeverity::Error,
                candidate.range,
                "exported namespace function '" + candidate.name +
                    "' must be public");
        }

        const std::string identity = mergeKey(candidate);
        const auto existing = symbolByIdentity.find(identity);
        if (existing == symbolByIdentity.end())
        {
            const std::size_t ordinal = functions_.size();
            if (candidate.isExportedC)
            {
                const auto [previousExport, insertedExport] =
                    exportedCByName.emplace(candidate.name, ordinal);
                if (!insertedExport)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        candidate.range,
                        "C ABI symbol '" + candidate.name +
                            "' is exported by more than one namespace function");
                    reportPrevious(functions_[previousExport->second], diagnostics);
                }
            }
            symbolByIdentity.emplace(identity, ordinal);
            functionsByName_[nameLookupKey(
                candidate.projectOrdinal, candidate.namespaceName, candidate.name)].push_back(ordinal);
            functions_.push_back(std::move(candidate));
            continue;
        }

        NamespaceFunctionSymbol& symbol = functions_[existing->second];
        const bool compatible = compatibleDeclarations(
            symbol,
            declaration,
            candidate.declaredTypeKey,
            candidate.declaredOwnership,
            candidate.declaredErrorTypeKey);
        if (!compatible)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ConflictingFunctionDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration.range,
                "declaration of namespace function '" + candidate.signatureKey +
                    "' conflicts with its previous declaration");
            reportPrevious(symbol, diagnostics);
            continue;
        }
        if (declaration.hasBody && symbol.definitionSyntax != nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateFunctionDefinition,
                diagnostics::DiagnosticSeverity::Error,
                declaration.range,
                "namespace function '" + candidate.signatureKey + "' already has a definition");
            reportPrevious(symbol, diagnostics);
            continue;
        }

        symbol.parts.push_back(candidate.parts.front());
        if (declaration.hasBody)
        {
            symbol.definitionSyntax = declaration.syntax;
            symbol.inliningPolicy = candidate.inliningPolicy;
        }
    }

    for (const NamespaceStateDeclaration& declaration : nameLookup.namespaceStates())
    {
        if (declaration.syntax == nullptr) continue;
        const ast::AstNode& syntax = *declaration.syntax;

        NamespaceStateSymbol state;
        state.accessibility = declaration.accessibility;
        state.projectOrdinal = declaration.projectOrdinal;
        state.sourceOrdinal = declaration.sourceOrdinal;
        state.fileId = declaration.fileId;
        state.range = declaration.range;
        state.sourcePath = declaration.sourcePath;
        state.moduleName = declaration.moduleName;
        state.namespaceName = declaration.namespaceName;
        state.name = declaration.name;
        state.declaredTypeSyntax = syntax.typeSyntax();
        state.declaredTypeKey = memberModel.canonicalTypeKey(syntax.typeSyntax(), nameLookup);
        state.identityKey = "state:" + state.moduleName + "::" +
            state.namespaceName + "." + state.name;
        state.declarationSyntax = declaration.syntax;
        state.initializerSyntax = syntax.first();
        state.isConst = declaration.isConst;
        state.isPrecompiled = declaration.isPrecompiled;
        state.storageName = namespaceStateStorageName(state);

        const std::string lookupKey =
            nameLookupKey(state.projectOrdinal, state.namespaceName, state.name);
        const auto previous = stateByName_.find(lookupKey);
        if (previous != stateByName_.end())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                state.range,
                "duplicate namespace state '" + state.namespaceName + "." + state.name + "'");
            diagnostics.report(
                diagnostics::DiagnosticId::PreviousDeclaration,
                diagnostics::DiagnosticSeverity::Note,
                states_[previous->second].range,
                "previous namespace state declaration is here");
            continue;
        }

        if (!initializeNamespaceStateConstant(state))
        {
            diagnostics.report(
                state.isConst
                    ? diagnostics::DiagnosticId::ConstRequiresConstant
                    : diagnostics::DiagnosticId::GlobalRequiresConstant,
                diagnostics::DiagnosticSeverity::Error,
                state.initializerSyntax != nullptr ? state.initializerSyntax->range() : state.range,
                "namespace state '" + state.namespaceName + "." + state.name +
                    "' requires a constant initializer; dynamic initialization is not supported");
        }

        stateByName_.emplace(lookupKey, states_.size());
        states_.push_back(std::move(state));
    }

    auto visibleProjects = [&](std::size_t projectOrdinal) -> std::span<const std::size_t>
    {
        if (projectOrdinal >= nameLookup.modules().size()) return {};
        return nameLookup.modules()[projectOrdinal]->visibleProjectOrdinals();
    };
    auto namespaceExists = [&](std::size_t projectOrdinal, std::string_view namespaceName)
    {
        const std::vector<std::string> segments = splitName(namespaceName);
        if (projectOrdinal < nameLookup.modules().size() &&
            nameLookup.modules()[projectOrdinal]->globalNamespace().findNamespace(segments) != nullptr)
        {
            return true;
        }
        for (const std::size_t visibleProject : visibleProjects(projectOrdinal))
        {
            if (visibleProject < nameLookup.modules().size() &&
                nameLookup.modules()[visibleProject]->globalNamespace().findNamespace(segments) != nullptr)
            {
                return true;
            }
        }
        return false;
    };
    auto resolveNamespace = [&](std::size_t projectOrdinal,
                                std::string_view lexicalNamespace,
                                const FunctionLookupEnvironment& environment,
                                std::string written) -> std::optional<std::string>
    {
        const std::size_t firstDot = written.find('.');
        const std::string first = written.substr(0, firstDot);
        const auto alias = environment.aliases.find(first);
        if (alias != environment.aliases.end())
        {
            written = alias->second +
                (firstDot == std::string::npos ? std::string{} : written.substr(firstDot));
            return namespaceExists(projectOrdinal, written)
                ? std::optional<std::string>(std::move(written)) : std::nullopt;
        }

        std::vector<std::string> lexicalSegments = splitName(lexicalNamespace);
        while (true)
        {
            const std::string prefix = joinName(lexicalSegments);
            const std::string candidate = prefix.empty() ? written : prefix + "." + written;
            if (namespaceExists(projectOrdinal, candidate)) return candidate;
            if (lexicalSegments.empty()) break;
            lexicalSegments.pop_back();
        }
        return std::nullopt;
    };
    auto candidatesInNamespace = [&](std::size_t projectOrdinal,
                                     std::string_view namespaceName,
                                     std::string_view functionName)
    {
        std::vector<std::size_t> candidates;
        const auto appendProject = [&](std::size_t visibleProject)
        {
            for (const std::size_t ordinal :
                 findFunctions(visibleProject, namespaceName, functionName))
            {
                if (ordinal >= functions_.size()) continue;
                if (visibleProject == projectOrdinal ||
                    functions_[ordinal].accessibility == NamespaceFunctionAccessibility::Public)
                {
                    candidates.push_back(ordinal);
                }
            }
        };
        appendProject(projectOrdinal);
        for (const std::size_t visibleProject : visibleProjects(projectOrdinal))
            if (visibleProject != projectOrdinal) appendProject(visibleProject);
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        return candidates;
    };
    auto extensionCandidatesInNamespace = [&](std::size_t projectOrdinal,
                                               std::string_view namespaceName,
                                               std::string_view functionName)
    {
        std::vector<std::size_t> candidates = candidatesInNamespace(
            projectOrdinal, namespaceName, functionName);
        std::erase_if(candidates, [&](std::size_t ordinal)
        {
            return ordinal >= functions_.size() || !functions_[ordinal].isExtension;
        });
        return candidates;
    };
    auto stateCandidatesInNamespace = [&](std::size_t projectOrdinal,
                                          std::string_view namespaceName,
                                          std::string_view stateName)
    {
        std::vector<std::size_t> candidates;
        const auto appendProject = [&](std::size_t visibleProject)
        {
            const auto found = stateByName_.find(
                nameLookupKey(visibleProject, namespaceName, stateName));
            if (found == stateByName_.end() || found->second >= states_.size()) return;
            if (visibleProject == projectOrdinal ||
                states_[found->second].accessibility == NamespaceFunctionAccessibility::Public)
            {
                candidates.push_back(found->second);
            }
        };
        appendProject(projectOrdinal);
        for (const std::size_t visibleProject : visibleProjects(projectOrdinal))
            if (visibleProject != projectOrdinal) appendProject(visibleProject);
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        return candidates;
    };

    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPointer : units)
    {
        if (unitPointer == nullptr || unitPointer->syntaxTree() == nullptr) continue;
        const ProjectSyntaxUnit& unit = *unitPointer;
        std::unordered_set<const ast::AstNode*> visited;
        std::unordered_set<const ast::AstNode*> directCallCallees;
        std::function<void(const ast::AstNode*, const std::string&, const FunctionLookupEnvironment&)>
            walkNode;
        std::function<void(const ast::AstNode*, const std::string&, const FunctionLookupEnvironment&)>
            walkScope;

        walkScope = [&](const ast::AstNode* scopeNode,
                        const std::string& lexicalNamespace,
                        const FunctionLookupEnvironment& inherited)
        {
            if (scopeNode == nullptr) return;
            FunctionLookupEnvironment environment = inherited;
            for (const ast::AstNode* child : scopeNode->children())
            {
                if (child == nullptr) continue;
                if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
                {
                    const std::optional<std::string> resolved = resolveNamespace(
                        unit.projectOrdinal(), lexicalNamespace, environment, child->text());
                    if (resolved.has_value() &&
                        std::find(environment.importedNamespaces.begin(),
                                  environment.importedNamespaces.end(), *resolved) ==
                            environment.importedNamespaces.end())
                    {
                        environment.importedNamespaces.push_back(*resolved);
                    }
                }
                else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                         !child->auxiliaryText().empty())
                {
                    const std::optional<std::string> resolved = resolveNamespace(
                        unit.projectOrdinal(), lexicalNamespace, environment,
                        child->auxiliaryText());
                    if (resolved.has_value()) environment.aliases[child->text()] = *resolved;
                }
            }

            for (const ast::AstNode* child : scopeNode->children())
            {
                if (child == nullptr ||
                    child->kind() == ast::AstNodeKind::UsingNamespaceDirective ||
                    child->kind() == ast::AstNodeKind::UsingAliasDirective)
                {
                    continue;
                }
                if (child->kind() == ast::AstNodeKind::NamespaceDeclaration)
                {
                    const std::vector<std::string> segments = splitName(child->text());
                    std::string nestedNamespace = lexicalNamespace;
                    for (const std::string& segment : segments)
                    {
                        if (!nestedNamespace.empty()) nestedNamespace.push_back('.');
                        nestedNamespace += segment;
                    }
                    walkScope(child, nestedNamespace, environment);
                }
                else
                {
                    walkNode(child, lexicalNamespace, environment);
                }
            }
        };

        walkNode = [&](const ast::AstNode* node,
                       const std::string& lexicalNamespace,
                       const FunctionLookupEnvironment& environment)
        {
            if (node == nullptr || !visited.insert(node).second) return;
            if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
            {
                std::string nestedNamespace = lexicalNamespace;
                for (const std::string& segment : splitName(node->text()))
                {
                    if (!nestedNamespace.empty()) nestedNamespace.push_back('.');
                    nestedNamespace += segment;
                }
                walkScope(node, nestedNamespace, environment);
                return;
            }

            if (node->kind() == ast::AstNodeKind::MemberExpression &&
                node->first() != nullptr && !node->text().empty())
            {
                std::vector<std::size_t> candidates = extensionCandidatesInNamespace(
                    unit.projectOrdinal(), lexicalNamespace, node->text());
                for (const std::string& imported : environment.importedNamespaces)
                {
                    std::vector<std::size_t> importedCandidates =
                        extensionCandidatesInNamespace(
                            unit.projectOrdinal(), imported, node->text());
                    candidates.insert(candidates.end(), importedCandidates.begin(),
                                      importedCandidates.end());
                }
                std::sort(candidates.begin(), candidates.end());
                candidates.erase(
                    std::unique(candidates.begin(), candidates.end()), candidates.end());
                if (!candidates.empty())
                {
                    NamespaceFunctionReferenceBinding binding;
                    binding.calleeSyntax = node;
                    binding.projectOrdinal = unit.projectOrdinal();
                    binding.fileId = unit.fileId();
                    binding.lexicalNamespace = lexicalNamespace;
                    binding.writtenName = node->text();
                    binding.candidateFunctionOrdinals = std::move(candidates);
                    binding.extension = true;
                    binding.receiverSyntax = node->first();
                    extensionBindingBySyntax_.emplace(node, extensionBindings_.size());
                    extensionBindings_.push_back(std::move(binding));
                }
            }

            if (node->kind() == ast::AstNodeKind::CallExpression && node->first() != nullptr)
            {
                // A direct callee already receives a binding keyed by the CallExpression below.
                // Do not register the same spelling again as a first-class callable value when
                // the recursive walk reaches its Identifier/MemberExpression node. Its receiver
                // is still walked normally, so namespace-state/object expressions are preserved.
                directCallCallees.insert(node->first());
                const std::optional<std::string> written = expressionName(node->first());
                if (written.has_value())
                {
                    const std::size_t lastDot = written->find_last_of('.');
                    const bool qualified = lastDot != std::string::npos;
                    const std::string functionName = qualified
                        ? written->substr(lastDot + 1) : *written;
                    std::vector<std::size_t> candidates;
                    if (qualified)
                    {
                        const std::optional<std::string> resolvedNamespace = resolveNamespace(
                            unit.projectOrdinal(), lexicalNamespace, environment,
                            written->substr(0, lastDot));
                        if (resolvedNamespace.has_value())
                        {
                            candidates = candidatesInNamespace(
                                unit.projectOrdinal(), *resolvedNamespace, functionName);
                        }
                    }
                    else
                    {
                        candidates = candidatesInNamespace(
                            unit.projectOrdinal(), lexicalNamespace, functionName);
                        if (candidates.empty())
                        {
                            for (const std::string& imported : environment.importedNamespaces)
                            {
                                std::vector<std::size_t> importedCandidates = candidatesInNamespace(
                                    unit.projectOrdinal(), imported, functionName);
                                candidates.insert(candidates.end(),
                                                  importedCandidates.begin(), importedCandidates.end());
                            }
                            std::sort(candidates.begin(), candidates.end());
                            candidates.erase(
                                std::unique(candidates.begin(), candidates.end()), candidates.end());
                        }
                    }

                    if (!candidates.empty())
                    {
                        NamespaceFunctionReferenceBinding binding;
                        binding.callSyntax = node;
                        binding.calleeSyntax = node->first();
                        binding.projectOrdinal = unit.projectOrdinal();
                        binding.fileId = unit.fileId();
                        binding.lexicalNamespace = lexicalNamespace;
                        binding.writtenName = *written;
                        binding.qualified = qualified;
                        binding.candidateFunctionOrdinals = std::move(candidates);
                        referenceBindingBySyntax_.emplace(node, referenceBindings_.size());
                        referenceBindings_.push_back(std::move(binding));
                    }
                }
            }

            if (node->kind() == ast::AstNodeKind::IdentifierExpression ||
                node->kind() == ast::AstNodeKind::MemberExpression)
            {
                const std::optional<std::string> written = expressionName(node);
                if (written.has_value())
                {
                    // Calls are not the only consumers of a function name: assignment to a
                    // delegate and a target-typed argument both need the same overload set. A
                    // direct call was already bound by its parent CallExpression.
                    if (!directCallCallees.contains(node))
                    {
                        const std::size_t lastDot = written->find_last_of('.');
                        const bool qualified = lastDot != std::string::npos;
                        const std::string functionName = qualified
                            ? written->substr(lastDot + 1) : *written;
                        std::vector<std::size_t> candidates;
                        if (qualified)
                        {
                            const std::optional<std::string> resolvedNamespace = resolveNamespace(
                                unit.projectOrdinal(), lexicalNamespace, environment,
                                written->substr(0, lastDot));
                            if (resolvedNamespace.has_value())
                            {
                                candidates = candidatesInNamespace(
                                    unit.projectOrdinal(), *resolvedNamespace, functionName);
                            }
                        }
                        else
                        {
                            candidates = candidatesInNamespace(
                                unit.projectOrdinal(), lexicalNamespace, functionName);
                            if (candidates.empty())
                            {
                                for (const std::string& imported : environment.importedNamespaces)
                                {
                                    std::vector<std::size_t> importedCandidates = candidatesInNamespace(
                                        unit.projectOrdinal(), imported, functionName);
                                    candidates.insert(candidates.end(), importedCandidates.begin(),
                                        importedCandidates.end());
                                }
                                std::sort(candidates.begin(), candidates.end());
                                candidates.erase(
                                    std::unique(candidates.begin(), candidates.end()), candidates.end());
                            }
                        }
                        if (!candidates.empty())
                        {
                            NamespaceFunctionReferenceBinding binding;
                            binding.calleeSyntax = node;
                            binding.projectOrdinal = unit.projectOrdinal();
                            binding.fileId = unit.fileId();
                            binding.lexicalNamespace = lexicalNamespace;
                            binding.writtenName = *written;
                            binding.qualified = qualified;
                            binding.candidateFunctionOrdinals = std::move(candidates);
                            referenceBindingBySyntax_.emplace(node, referenceBindings_.size());
                            referenceBindings_.push_back(std::move(binding));
                        }
                    }
                    const std::size_t lastDot = written->find_last_of('.');
                    const bool qualified = lastDot != std::string::npos;
                    const std::string stateName = qualified
                        ? written->substr(lastDot + 1) : *written;
                    std::vector<std::size_t> candidates;
                    if (qualified)
                    {
                        const std::optional<std::string> resolvedNamespace = resolveNamespace(
                            unit.projectOrdinal(), lexicalNamespace, environment,
                            written->substr(0, lastDot));
                        if (resolvedNamespace.has_value())
                        {
                            candidates = stateCandidatesInNamespace(
                                unit.projectOrdinal(), *resolvedNamespace, stateName);
                        }
                    }
                    else
                    {
                        candidates = stateCandidatesInNamespace(
                            unit.projectOrdinal(), lexicalNamespace, stateName);
                        if (candidates.empty())
                        {
                            for (const std::string& imported : environment.importedNamespaces)
                            {
                                std::vector<std::size_t> importedCandidates =
                                    stateCandidatesInNamespace(
                                        unit.projectOrdinal(), imported, stateName);
                                candidates.insert(candidates.end(),
                                    importedCandidates.begin(), importedCandidates.end());
                            }
                            std::sort(candidates.begin(), candidates.end());
                            candidates.erase(
                                std::unique(candidates.begin(), candidates.end()), candidates.end());
                        }
                    }

                    if (!candidates.empty())
                    {
                        NamespaceStateReferenceBinding binding;
                        binding.expressionSyntax = node;
                        binding.projectOrdinal = unit.projectOrdinal();
                        binding.fileId = unit.fileId();
                        binding.lexicalNamespace = lexicalNamespace;
                        binding.writtenName = *written;
                        binding.qualified = qualified;
                        binding.candidateStateOrdinals = std::move(candidates);
                        stateReferenceBindingBySyntax_.emplace(
                            node, stateReferenceBindings_.size());
                        stateReferenceBindings_.push_back(std::move(binding));
                    }
                }
            }
            walkNode(node->first(), lexicalNamespace, environment);
            walkNode(node->second(), lexicalNamespace, environment);
            walkNode(node->third(), lexicalNamespace, environment);
            for (const ast::AstNode* child : node->children())
                walkNode(child, lexicalNamespace, environment);
        };

        walkScope(unit.syntaxTree(), {}, FunctionLookupEnvironment{});
    }

    built_ = diagnostics.errorCount() == initialErrorCount;
    return built_;
}

bool ProjectNamespaceFunctionModel::bindPrecompiledEscapeSummaries(
    std::span<const PrecompiledNamespaceFunctionIdentity> identities,
    std::string& error)
{
    error.clear();
    if (!built_)
    {
        error = "precompiled namespace-function escape summaries require a completed model";
        return false;
    }
    for (NamespaceFunctionSymbol& function : functions_)
    {
        if (!function.isPrecompiled) continue;
        const auto match = std::ranges::find_if(identities, [&](const auto& identity)
        {
            return identity.moduleName == function.moduleName &&
                identity.namespaceName == function.namespaceName &&
                identity.name == function.name &&
                identity.signatureKey == function.signatureKey;
        });
        if (match == identities.end())
        {
            error = "precompiled namespace function '" + function.moduleName + "::" +
                function.namespaceName + "." + function.signatureKey +
                "' has no escape contract in NSMETA";
            return false;
        }
        function.parameterEscapeSummary = match->parameterEscapeSummary;
        if (!function.parameterEscapeSummary.empty() &&
            function.parameterEscapeSummary.size() != function.parameters.size())
        {
            error = "precompiled namespace function '" + function.moduleName + "::" +
                function.namespaceName + "." + function.signatureKey +
                "' has an invalid escape-summary parameter count";
            return false;
        }
    }
    return true;
}

std::span<const NamespaceFunctionSymbol> ProjectNamespaceFunctionModel::functions() const noexcept
{
    return functions_;
}

std::span<const NamespaceStateSymbol> ProjectNamespaceFunctionModel::states() const noexcept
{
    return states_;
}

const NamespaceStateSymbol* ProjectNamespaceFunctionModel::findState(
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    std::string_view name) const noexcept
{
    const auto found = stateByName_.find(nameLookupKey(projectOrdinal, namespaceName, name));
    if (found == stateByName_.end() || found->second >= states_.size()) return nullptr;
    return &states_[found->second];
}

std::span<const std::size_t> ProjectNamespaceFunctionModel::findFunctions(
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    std::string_view name) const noexcept
{
    const auto found = functionsByName_.find(nameLookupKey(projectOrdinal, namespaceName, name));
    if (found == functionsByName_.end()) return {};
    return found->second;
}

const NamespaceFunctionReferenceBinding* ProjectNamespaceFunctionModel::referenceBinding(
    const ast::AstNode* callSyntax) const noexcept
{
    const auto found = referenceBindingBySyntax_.find(callSyntax);
    if (found == referenceBindingBySyntax_.end()) return nullptr;
    return &referenceBindings_[found->second];
}

const NamespaceFunctionReferenceBinding* ProjectNamespaceFunctionModel::extensionBinding(
    const ast::AstNode* memberSyntax) const noexcept
{
    const auto found = extensionBindingBySyntax_.find(memberSyntax);
    if (found == extensionBindingBySyntax_.end()) return nullptr;
    return &extensionBindings_[found->second];
}

std::span<const NamespaceFunctionReferenceBinding>
ProjectNamespaceFunctionModel::referenceBindings() const noexcept
{
    return referenceBindings_;
}

const NamespaceStateReferenceBinding* ProjectNamespaceFunctionModel::stateReferenceBinding(
    const ast::AstNode* expressionSyntax) const noexcept
{
    const auto found = stateReferenceBindingBySyntax_.find(expressionSyntax);
    if (found == stateReferenceBindingBySyntax_.end()) return nullptr;
    return &stateReferenceBindings_[found->second];
}

std::span<const NamespaceStateReferenceBinding>
ProjectNamespaceFunctionModel::stateReferenceBindings() const noexcept
{
    return stateReferenceBindings_;
}

bool ProjectNamespaceFunctionModel::isBuilt() const noexcept
{
    return built_;
}
}
