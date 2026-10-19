#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{

void ProjectMemberModel::validateParameterPack(
    MethodParameterSymbol& parameter,
    const ProjectNameLookup& nameLookup,
    bool& sawParameterPack,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (sawParameterPack)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "a typed parameter pack must be the final parameter");
    }
    if (!parameter.isParameterPack) return;
    sawParameterPack = true;
    const bool validPackSyntax = parameter.typeSyntax != nullptr &&
        (parameter.typeSyntax->baseName() == "ReadOnlySpan" ||
         parameter.typeSyntax->baseName().ends_with(".ReadOnlySpan")) &&
        parameter.typeSyntax->typeArguments().size() == 1U;
    if (validPackSyntax)
    {
        parameter.parameterPackElementTypeKey = canonicalTypeKey(
            parameter.typeSyntax->typeArguments().front(), nameLookup);
    }
    else if (genericModel_ != nullptr)
    {
        const GenericSpecializationRecord* specialization =
            genericModel_->specializationForTypeKey(parameter.typeKey);
        if (specialization != nullptr &&
            specialization->templateKey.find("ReadOnlySpan`1") != std::string::npos &&
            specialization->typeArguments.size() == 1U)
            parameter.parameterPackElementTypeKey = canonicalGenericArgumentKey(
                specialization->typeArguments.front(), nameLookup);
    }
    if (parameter.parameterPackElementTypeKey.empty())
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "params requires ReadOnlySpan<T>; implicit heap arrays are forbidden (resolved '" +
                parameter.typeKey + "')");
    if (parameter.defaultValueSyntax != nullptr)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "a typed parameter pack cannot declare a default value");
}

void ProjectMemberModel::applyParameterPassKind(
    MethodParameterSymbol& parameter,
    const ast::AstNode* parameterNode)
{
    // C4 "language references": the parser already turned the written type into a
    // pointer, so `typeKey` is `T*`. `pointeeTypeKey` is what the programmer
    // wrote, and it is what a USE of the parameter inside the body evaluates to --
    // that implicit dereference is the whole ergonomic difference between `ref int
    // x` and `int* x`.
    if (!parameterNode->auxiliaryText().starts_with("pass:")) return;
    const std::string_view marker = parameterNode->auxiliaryText();
    const std::size_t markerEnd = marker.find('|');
    const std::string spelling = std::string(marker.substr(
        std::string("pass:").size(),
        markerEnd == std::string_view::npos
            ? marker.size() - std::string("pass:").size()
            : markerEnd - std::string("pass:").size()));
    parameter.passKind = spelling == "ref"
        ? ParameterPassKind::Ref
        : (spelling == "out" ? ParameterPassKind::Out : ParameterPassKind::In);
    parameter.pointeeTypeKey = parameter.typeKey.empty() || parameter.typeKey.back() != '*'
        ? parameter.typeKey
        : parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
}

void ProjectMemberModel::validateDefaultParameter(
    const MethodParameterSymbol& parameter,
    bool& sawOptionalParameter,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (parameter.defaultValueSyntax == nullptr)
    {
        if (sawOptionalParameter)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "required parameter '" + parameter.name +
                    "' cannot follow a parameter with a default value");
        return;
    }
    sawOptionalParameter = true;
    const ast::AstNodeKind defaultKind = parameter.defaultValueSyntax->kind();
    const bool literal = defaultKind == ast::AstNodeKind::IntegerLiteralExpression ||
        defaultKind == ast::AstNodeKind::FloatingLiteralExpression ||
        defaultKind == ast::AstNodeKind::CharacterLiteralExpression ||
        defaultKind == ast::AstNodeKind::StringLiteralExpression ||
        defaultKind == ast::AstNodeKind::BooleanLiteralExpression ||
        defaultKind == ast::AstNodeKind::NullLiteralExpression;
    const bool signedLiteral = defaultKind == ast::AstNodeKind::UnaryExpression &&
        (parameter.defaultValueSyntax->tokenKind() == lexer::TokenKind::Plus ||
         parameter.defaultValueSyntax->tokenKind() == lexer::TokenKind::Minus) &&
        parameter.defaultValueSyntax->first() != nullptr &&
        (parameter.defaultValueSyntax->first()->kind() ==
             ast::AstNodeKind::IntegerLiteralExpression ||
         parameter.defaultValueSyntax->first()->kind() ==
             ast::AstNodeKind::FloatingLiteralExpression);
    if (!literal && !signedLiteral)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            parameter.defaultValueSyntax->range(),
            "default parameter value must be a literal constant");
    else
    {
        const ast::AstNode* valueSyntax = signedLiteral
            ? parameter.defaultValueSyntax->first()
            : parameter.defaultValueSyntax;
        if (!defaultParameterValueIsCompatible(parameter, valueSyntax, nameLookup))
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                parameter.defaultValueSyntax->range(),
                "default value cannot convert to parameter type '" +
                    parameter.typeKey + "'");
    }
    if (parameter.passKind != ParameterPassKind::Value ||
        parameter.ownership != sema::OwnershipKind::None)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "default values are allowed only on unqualified value parameters");
}

void ProjectMemberModel::collectCallableParameters(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const NamedTypeDeclarationPart& part,
    const ast::AstNode* memberNode,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::unordered_map<std::string, const ast::AstNode*> parameterNames;
    bool sawOptionalParameter = false;
    bool sawParameterPack = false;
    for (const ast::AstNode* parameterNode : memberNode->children())
    {
        if (parameterNode == nullptr ||
            parameterNode->kind() != ast::AstNodeKind::ParameterDeclaration)
            continue;
        MethodParameterSymbol parameter;
        initializeMethodParameter(parameter, parameterNode, part, nameLookup);
        applyFfiParameterMarkers(parameter, parameterNode);
        validateParameterPack(
            parameter, nameLookup, sawParameterPack, diagnostics);
        applyParameterPassKind(parameter, parameterNode);
        validateParameterLifetime(parameter, symbol, type, parameterNode, diagnostics);
        validateDefaultParameter(
            parameter, sawOptionalParameter, nameLookup, diagnostics);
        validateCallableParameterIdentity(
            parameter, symbol, parameterNames, diagnostics);
        symbol.parameters.push_back(std::move(parameter));
    }
}

void ProjectMemberModel::validateMemberFfiContracts(
    ClassMemberSymbol& symbol,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const bool hasFfiContract =
        symbol.ffiUnwindPolicy != ast::FfiUnwindSyntaxKind::Default ||
        std::ranges::any_of(
            symbol.parameters,
            [](const MethodParameterSymbol& parameter)
            {
                return !parameter.ffiLengthParameterName.empty() ||
                    parameter.ffiCallback;
            });
    if (hasFfiContract && !symbol.isExternC)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "FFI length, callback, and unwind contracts are valid only on extern C boundaries");
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < symbol.parameters.size(); ++parameterOrdinal)
        validateFfiParameterContract(
            symbol, parameterOrdinal, nameLookup, diagnostics);
}

void ProjectMemberModel::inferReferenceReturnLifetime(
    ClassMemberSymbol& symbol,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (symbol.hasImplicitThis)
    {
        symbol.returnLifetimeName = "this";
        return;
    }
    std::unordered_set<std::string> inputs;
    for (const MethodParameterSymbol& parameter : symbol.parameters)
    {
        if (parameter.passKind == ParameterPassKind::Ref ||
            parameter.passKind == ParameterPassKind::In)
            inputs.insert(parameter.lifetimeName);
    }
    if (inputs.size() == 1U) symbol.returnLifetimeName = *inputs.begin();
    else if (inputs.empty()) symbol.returnLifetimeName = "static";
    else
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "ref-returning method '" + symbol.name +
                "' has multiple input lifetimes; write ref[a] and bind "
                "the intended ref/in parameters to lifetime a");
}

void ProjectMemberModel::markAccessorOwnership(ClassMemberSymbol& symbol)
{
    // Bloque 1 item 9, setter half. The setter's implicit `value` parameter is
    // built from the property's own TypeSyntax, so it already carries the `owned`
    // qualifier and already behaves as an owned parameter inside the accessor body
    // -- `backing = move(value)` compiles unchanged. What was missing is the
    // CALL SITE: `h.Slot = move(a)` was rejected because the assignment target was
    // neither a RAII local nor an array element nor an owned field.
    //
    // Marking the setter lets ProjectMethodBodyModel::analyzeAssignment recognize it
    // as a fourth legitimate move(...) destination. The `value` parameter is always
    // appended last (after any index parameters, for an indexer), which is what makes
    // `parameters.back()` the right one to read.
    if ((symbol.isPropertyAccessor || symbol.isIndexerAccessor) && symbol.isSetter &&
        !symbol.parameters.empty() &&
        symbol.parameters.back().ownership == sema::OwnershipKind::Owned)
        symbol.ownsAccessorStorage = true;
}

void ProjectMemberModel::finalizeCallableMember(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    collectCallableParameters(
        symbol, type, type.parts[symbol.partOrdinal], memberNode, nameLookup, diagnostics);
    validateMemberFfiContracts(symbol, nameLookup, diagnostics);
    validateReferenceReturnLifetime(symbol, type, memberNode, diagnostics);
    buildCallableSignature(symbol, diagnostics);
    markAccessorOwnership(symbol);
}

void ProjectMemberModel::validateInlineArraySignature(
    const ClassMemberSymbol& symbol,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // C2.1 "fixed arrays": a fixed array is INLINE STORAGE, not a value that can be
    // copied across a call. C says the same thing by decaying an array parameter to a
    // pointer and by refusing to return one at all, and NativeSharp says it out loud
    // instead of silently passing 16 bytes by value: the honest spelling for both is a
    // raw pointer to the element type. A FIELD, a LOCAL and an element read/write are
    // exactly where a fixed array does belong, and all three work.
    if (symbol.kind != ClassMemberKind::Method) return;
    if (fixedArrayElementTypeKey(symbol.declaredTypeKey).has_value() ||
        flexibleArrayElementTypeKey(symbol.declaredTypeKey).has_value())
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "an inline array cannot be returned by value; return a pointer to its element type");
    for (const MethodParameterSymbol& parameter : symbol.parameters)
    {
        if (!fixedArrayElementTypeKey(parameter.typeKey).has_value() &&
            !flexibleArrayElementTypeKey(parameter.typeKey).has_value())
            continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "an inline array cannot be passed by value; declare the parameter as a pointer to its element type");
    }
}

void ProjectMemberModel::validateUserConversion(
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!symbol.isUserConversion || symbol.parameters.size() != 1) return;
    const std::string containingTypeKey = "type:" + type.identity.key();
    const std::string& sourceTypeKey = symbol.parameters.front().typeKey;
    const std::string& targetTypeKey = symbol.declaredTypeKey;
    if (sourceTypeKey == targetTypeKey)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "user-defined conversion source and target types must differ");
    if (sourceTypeKey != containingTypeKey && targetTypeKey != containingTypeKey)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "user-defined conversion must convert to or from the containing type");
}
}
