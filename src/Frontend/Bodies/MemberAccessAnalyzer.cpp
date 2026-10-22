#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using lexer::TokenKind;
using method_body_detail::pointerContract;

namespace
{
// Private markers exchanged by member binding and call binding; they never escape the body model.
constexpr const char* kStructRootFacadeTypeKey = "<struct-root-facade>";
constexpr const char* kEnumRootFacadeTypeKey = "<enum-root-facade>";
constexpr const char* kIteratorFacadeTypeKey = "<iterator-facade>";

std::string typeCandidateList(
    std::span<const std::size_t> candidates,
    const ProjectNameLookup& lookup)
{
    std::string result;
    for (const std::size_t ordinal : candidates)
    {
        if (ordinal >= lookup.namedTypes().size()) continue;
        if (!result.empty()) result += ", ";
        result += lookup.namedTypes()[ordinal].identity.key();
    }
    return result;
}
}

bool ProjectMethodBodyModel::analyzeQualifiedNamespaceMember(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    if (namespaceFunctionModel_ == nullptr || expression == nullptr) return false;
    const bool receiverShadowed = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        lookupScopeName(scopeOrdinal, expression->first()->text()).has_value();
    if (receiverShadowed) return false;
    if (const NamespaceFunctionReferenceBinding* functionReference =
            namespaceFunctionModel_->referenceBinding(expression);
        functionReference != nullptr && functionReference->qualified)
    {
        result = analyzeNamespaceFunctionGroup(
            expression, *functionReference, methodMemberOrdinal, scopeOrdinal,
            diagnostics, allowMethodGroup);
        return true;
    }
    if (const NamespaceStateReferenceBinding* stateReference =
            namespaceFunctionModel_->stateReferenceBinding(expression);
        stateReference != nullptr && stateReference->qualified)
    {
        result = analyzeNamespaceState(
            expression, *stateReference, methodMemberOrdinal, scopeOrdinal, diagnostics);
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::memberValueShadowsType(
    const ast::AstNode* receiverSyntax,
    std::size_t methodMemberOrdinal) const
{
    // A value in lexical/member scope wins over an equally named type. Besides matching the
    // usual expression lookup rule, this is essential for APIs with a property named after its
    // type (`Transform Transform` followed by `Transform.Position`). The old type-first probe
    // interpreted that receiver as the class and then complained that Position was not static.
    if (receiverSyntax->kind() != ast::AstNodeKind::IdentifierExpression) return false;
    const std::string& receiverName = receiverSyntax->text();
    const std::vector<std::size_t> candidates = memberCandidates(
        methodMemberOrdinal, receiverName);
    const bool directValue = std::any_of(
        candidates.begin(), candidates.end(), [&](std::size_t ordinal)
        {
            if (ordinal >= memberModel_->members().size()) return false;
            const ClassMemberKind kind = memberModel_->members()[ordinal].kind;
            return kind != ClassMemberKind::Constructor &&
                kind != ClassMemberKind::Destructor;
        });
    return directValue ||
        !memberCandidates(methodMemberOrdinal, "get_" + receiverName).empty() ||
        !memberCandidates(methodMemberOrdinal, "set_" + receiverName).empty();
}

bool ProjectMethodBodyModel::resolveWrittenMemberType(
    std::optional<std::size_t>& typeOrdinal,
    std::string& writtenTypeName,
    const ast::AstNode* receiverSyntax,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
        receiverSyntax->typeSyntax() != nullptr)
    {
        // ID 131. `Caja<int>.Make(...)`. El parser deja el TypeSyntax del tipo CERRADO colgado del
        // identificador cuando la forma escrita llevaba argumentos de tipo. Cuando esta, se
        // resuelve por CLAVE DE TIPO y no por nombre suelto: `resolveTypeIdentifier` busca por
        // `identity.name()`, que para un generico es el nombre ABIERTO, y devolveria la
        // declaracion sin cerrar -- o sea `Caja<T>` donde se escribio `Caja<int>`, y a partir de
        // ahi todo lo que se dedujera del miembro seria de otro tipo.
        //
        // `canonicalTypeKey` es el MISMO servicio que usan la declaracion de un local y el `new`,
        // asi que la clave que sale aqui es la de la monomorfizacion que esas dos ya crearon. Si
        // no existe --se escribio un tipo cerrado que nadie mas usa-- no se resuelve nada y el
        // camino de abajo da el diagnostico de siempre, que es lo correcto: inventar la
        // instanciacion desde un acceso a miembro seria monomorfizar en el sitio equivocado.
        const ast::TypeSyntax* closedType = receiverSyntax->typeSyntax();
        typeOrdinal = namedTypeOrdinalForTypeKey(
            memberModel_->canonicalTypeKey(closedType, *nameLookup_));
        writtenTypeName = receiverSyntax->text();
        return true;
    }
    const ExpressionTypeNameBinding* binding =
        nameLookup_->expressionTypeBinding(receiverSyntax);
    if (binding != nullptr)
    {
        writtenTypeName = binding->writtenName;
        if (binding->candidateTypeOrdinals.size() > 1)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::AmbiguousTypeName,
                diagnostics::DiagnosticSeverity::Error,
                receiverSyntax->range(),
                "ambiguous type name '" + writtenTypeName + "'; candidates: " +
                    typeCandidateList(binding->candidateTypeOrdinals, *nameLookup_));
            return false;
        }
        if (!binding->candidateTypeOrdinals.empty())
            typeOrdinal = binding->candidateTypeOrdinals.front();
    }
    else if (receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        typeOrdinal = resolveTypeIdentifier(methodMemberOrdinal, receiverSyntax->text());
        writtenTypeName = receiverSyntax->text();
    }
    return true;
}

void ProjectMethodBodyModel::configureTypeMemberReceiver(
    ExpressionResult& receiver,
    const ast::AstNode* expression,
    std::size_t typeOrdinal,
    std::string_view writtenTypeName,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    receiver.typeKey = inheritanceModel_->typeKeyForOrdinal(typeOrdinal);
    receiver.valid = true;
    receiver.receiverSyntax = expression->first();
    receiver.receiverTypeKey = receiver.typeKey;
    receiver.receiverTypeOrdinal = typeOrdinal;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::TypeReference, expression->first(), methodMemberOrdinal,
        scopeOrdinal, std::string(writtenTypeName), receiver.typeKey, std::nullopt,
        std::nullopt, {}, expression->first(), receiver.typeKey, typeOrdinal, false});
}

bool ProjectMethodBodyModel::analyzeBaseMemberReceiver(
    ExpressionResult& receiver,
    const ast::AstNode* expression,
    std::size_t accessingTypeOrdinal,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!contextHasImplicitThis(methodMemberOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "base is not available in a static member or field initializer");
        return false;
    }
    const std::optional<std::size_t> baseType =
        inheritanceModel_->baseTypeOrdinal(accessingTypeOrdinal);
    if (!baseType.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "base cannot be used because the containing class has no base class");
        return false;
    }
    receiver.typeKey = inheritanceModel_->typeKeyForOrdinal(*baseType);
    receiver.valid = true;
    receiver.receiverSyntax = expression->first();
    receiver.receiverTypeKey = receiver.typeKey;
    receiver.receiverTypeOrdinal = baseType;
    receiver.baseReceiver = true;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::BaseReference, expression->first(), methodMemberOrdinal,
        scopeOrdinal, "base", receiver.typeKey, std::nullopt, std::nullopt, {},
        expression->first(), receiver.typeKey, baseType, true});
    return true;
}

bool ProjectMethodBodyModel::resolveMemberReceiver(
    ExpressionResult& receiver,
    bool& typeReceiver,
    const ast::AstNode* expression,
    std::size_t accessingTypeOrdinal,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const bool valueReceiver =
        expression->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        (lookupScopeName(scopeOrdinal, expression->first()->text()).has_value() ||
         memberValueShadowsType(expression->first(), methodMemberOrdinal));
    if (valueReceiver)
    {
        ++partialProjectionReceiverDepth_;
        receiver = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        --partialProjectionReceiverDepth_;
        if (!receiver.valid || receiver.methodGroup) return false;
        receiver.receiverSyntax = expression->first();
        receiver.receiverTypeKey = receiver.typeKey;
        receiver.receiverTypeOrdinal = namedTypeOrdinalForTypeKey(receiver.typeKey);
        return true;
    }
    std::optional<std::size_t> typeOrdinal;
    std::string writtenTypeName;
    if (!resolveWrittenMemberType(
            typeOrdinal, writtenTypeName, expression->first(),
            methodMemberOrdinal, diagnostics)) return false;
    if (typeOrdinal.has_value())
    {
        typeReceiver = true;
        configureTypeMemberReceiver(
            receiver, expression, *typeOrdinal, writtenTypeName,
            methodMemberOrdinal, scopeOrdinal);
        return true;
    }
    if (expression->first()->kind() == ast::AstNodeKind::BaseExpression)
        return analyzeBaseMemberReceiver(
            receiver, expression, accessingTypeOrdinal,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    ++partialProjectionReceiverDepth_;
    receiver = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    --partialProjectionReceiverDepth_;
    if (!receiver.valid || receiver.methodGroup) return false;
    receiver.receiverSyntax = expression->first();
    receiver.receiverTypeKey = receiver.typeKey;
    receiver.receiverTypeOrdinal = namedTypeOrdinalForTypeKey(receiver.typeKey);
    return true;
}

bool ProjectMethodBodyModel::analyzeBuiltinLengthMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    bool typeReceiver,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    if (!typeReceiver && expression->text() == "Length")
    {
        const std::optional<std::uint64_t> fixedCount =
            ProjectMemberModel::fixedArrayCount(receiver.typeKey);
        if (fixedCount.has_value())
        {
            result.typeKey = "builtin:int";
            result.valid = true;
            result.assignable = false;
            result.integerConstant = ProjectIntegerConstant{*fixedCount, false};
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::FixedArrayLength, expression, methodMemberOrdinal,
                scopeOrdinal, result.typeKey, receiver.typeKey, false,
                std::nullopt, std::nullopt});
            return true;
        }
    }
    if (!typeReceiver && expression->text() == "Length" &&
        (ProjectTypeConversion::isNativeArray(receiver.typeKey) ||
         ProjectTypeConversion::isNativeString(receiver.typeKey) ||
         receiver.typeKey == "string-literal"))
    {
        result.typeKey = "builtin:int";
        result.valid = true;
        result.assignable = false;
        const bool arrayLength = ProjectTypeConversion::isNativeArray(receiver.typeKey);
        coreBindings_.push_back(MethodCoreBinding{
            arrayLength ? MethodCoreOperationKind::ArrayLength
                        : MethodCoreOperationKind::StringLength,
            expression, methodMemberOrdinal, scopeOrdinal, result.typeKey,
            arrayLength
                ? ProjectTypeConversion::nativeArrayElementType(receiver.typeKey)
                      .value_or(std::string{})
                : "builtin:char",
            false, std::nullopt, std::nullopt});
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::analyzeBuiltinMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    bool typeReceiver,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    bool allowMethodGroup)
{
    if (analyzeBuiltinLengthMember(
            result, receiver, typeReceiver, expression,
            methodMemberOrdinal, scopeOrdinal)) return true;
    // NS304-T06-F08 / NS304-T07-F02: `it.Current` reads the iterator's item (field 2).
    if (!typeReceiver && expression->text() == "Current")
    {
        auto itemInner = language::iteratorInnerTypeKey(receiver.typeKey);
        if (!itemInner.has_value()) itemInner = language::asyncIteratorInnerTypeKey(receiver.typeKey);
        if (itemInner.has_value())
        {
            result.typeKey = std::string(*itemInner);
            result.valid = true;
            result.assignable = false;
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::IteratorCurrent, expression, methodMemberOrdinal,
                scopeOrdinal, result.typeKey, receiver.typeKey, false,
                std::nullopt, std::nullopt});
            return true;
        }
    }
    const bool asyncIterator = language::asyncIteratorInnerTypeKey(receiver.typeKey).has_value();
    const bool syncIterator = language::iteratorInnerTypeKey(receiver.typeKey).has_value();
    // NS304-T07-F02: `it.MoveNextAsync()` on an async-iterator returns the future of the next demand.
    const bool moveNextAsync = expression->text() == "MoveNextAsync" && asyncIterator;
    // NS304-T06-F08 / NS304-T07-F02: `MoveNext`/`GetEnumerator` are sync-iterator-only; `Drop` is on
    // both (the async-iterator consumes via MoveNextAsync, not a sync MoveNext).
    const bool iteratorMethod = moveNextAsync ||
        ((expression->text() == "MoveNext" || expression->text() == "GetEnumerator") &&
         syncIterator) ||
        (expression->text() == "Drop" && (syncIterator || asyncIterator)) ||
        (expression->text() == "Cancel" && asyncIterator);
    if (typeReceiver || !iteratorMethod) return false;
    result.methodGroup = true;
    result.methodCandidates.clear();
    result.valid = allowMethodGroup;
    result.receiverSyntax = expression->first();
    result.receiverTypeKey = receiver.typeKey;
    result.receiverTypeOrdinal = receiver.receiverTypeOrdinal;
    result.typeKey = kIteratorFacadeTypeKey;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::ExplicitObjectMethodGroup, expression, methodMemberOrdinal,
        scopeOrdinal, expression->text(), kIteratorFacadeTypeKey, std::nullopt,
        std::nullopt, {}, expression->first(), receiver.typeKey,
        receiver.receiverTypeOrdinal, false});
    return true;
}

bool ProjectMethodBodyModel::analyzeNativeStringFacadeMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    bool typeReceiver,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    // Stage 51: native string and primitive instance syntax is a language-level facade over
    // NativeSharp.Core.String static methods.  The receiver is forwarded as parameter zero;
    // this keeps string itself a native reference type without inventing a CLR-style object.
    if (typeReceiver || nameLookup_ == nullptr || memberModel_ == nullptr) return false;
    const bool nativeStringReceiver = ProjectTypeConversion::isNativeString(receiver.typeKey) ||
        receiver.typeKey == "string-literal";
    const bool primitiveToStringReceiver = expression->text() == "ToString" &&
        (nativeStringReceiver || receiver.typeKey == "builtin:bool" ||
         ProjectTypeConversion::isNumeric(receiver.typeKey));
    if (!nativeStringReceiver && !primitiveToStringReceiver) return false;
    if (!coreStringTypeOrdinal_.has_value()) return false;
    std::vector<std::size_t> methods;
    for (const std::size_t candidateOrdinal :
         memberModel_->findMembers(*coreStringTypeOrdinal_, expression->text()))
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        if (candidate.kind != ClassMemberKind::Method || !candidate.isStatic ||
            candidate.parameters.empty()) continue;
        const ProjectImplicitConversion receiverConversion = classifyImplicit(
            receiver.typeKey, candidate.parameters.front().typeKey, receiver.integerConstant);
        if (receiverConversion.exists()) methods.push_back(candidateOrdinal);
    }
    if (methods.empty()) return false;
    result.methodGroup = true;
    result.methodCandidates = methods;
    result.valid = allowMethodGroup;
    result.receiverSyntax = expression->first();
    result.receiverTypeKey = receiver.typeKey;
    result.receiverTypeOrdinal.reset();
    result.ownership = receiver.ownership;
    result.ownershipSourceLocalOrdinal = receiver.ownershipSourceLocalOrdinal;
    result.ownershipSourceParameterOrdinal = receiver.ownershipSourceParameterOrdinal;
    result.borrowedLocalOrdinal = receiver.borrowedLocalOrdinal;
    result.integerConstant = receiver.integerConstant;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::ExplicitObjectMethodGroup, expression, methodMemberOrdinal,
        scopeOrdinal, expression->text(), "<native-string-method-group>", std::nullopt,
        std::nullopt, methods, expression->first(), receiver.typeKey, std::nullopt, false});
    if (!allowMethodGroup)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "NativeSharp.Core.String instance facade member '" + expression->text() +
                "' can only be used as a call target");
    }
    return true;
}

void ProjectMethodBodyModel::collectExtensionMemberCandidates(
    std::vector<std::size_t>& methods,
    const ExpressionResult& receiver,
    bool typeReceiver,
    const NamespaceFunctionReferenceBinding& extension)
{
    for (const std::size_t functionOrdinal : extension.candidateFunctionOrdinals)
    {
        if (functionOrdinal >= namespaceCallableAdapters().size()) continue;
        const ClassMemberSymbol& candidate = namespaceCallableAdapters()[functionOrdinal];
        if (candidate.parameters.empty())
        {
            // NS304-T08-F03: a static extension member (no receiver) is reachable via T.Miembro().
            if (typeReceiver) methods.push_back(namespaceCallableBase_ + functionOrdinal);
            continue;
        }
        if (typeReceiver) continue;
        const MethodParameterSymbol& receiverParameter = candidate.parameters.front();
        const std::string& receiverTarget = receiverParameter.passKind == ParameterPassKind::Value
            ? receiverParameter.typeKey : receiverParameter.pointeeTypeKey;
        const ProjectImplicitConversion receiverConversion = classifyImplicit(
            receiver.typeKey, receiverTarget, receiver.integerConstant);
        if (!receiverConversion.exists()) continue;
        const ParameterPassKind pass = receiverParameter.passKind;
        if (pass != ParameterPassKind::Value &&
            receiverConversion.kind != ProjectConversionKind::Identity) continue;
        if (pass != ParameterPassKind::Value && !receiver.addressable) continue;
        if (pass == ParameterPassKind::Ref && receiver.readonlyReceiver) continue;
        methods.push_back(namespaceCallableBase_ + functionOrdinal);
    }
}

bool ProjectMethodBodyModel::bindExtensionMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    bool typeReceiver,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    if (namespaceFunctionModel_ == nullptr) return false;
    const NamespaceFunctionReferenceBinding* extension =
        namespaceFunctionModel_->extensionBinding(expression);
    if (extension == nullptr) return false;
    std::vector<std::size_t> methods;
    collectExtensionMemberCandidates(methods, receiver, typeReceiver, *extension);
    if (methods.empty()) return false;
    // NS304-T08-F03: a static extension member is a plain static method group (no receiver).
    if (typeReceiver)
    {
        result.valid = allowMethodGroup;
        result.methodGroup = true;
        result.methodCandidates = methods;
        result.typeKey = "<method-group>";
        recordNameBinding(MethodNameBinding{
            MethodNameBindingKind::StaticMethodGroup, expression, methodMemberOrdinal,
            scopeOrdinal, expression->text(), "<method-group>", std::nullopt,
            std::nullopt, methods, nullptr, std::string{},
            std::optional<std::size_t>{}, false});
        if (!allowMethodGroup)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "static extension member '" + expression->text() +
                    "' can only be used as a call target");
        return true;
    }
    result = receiver;
    result.typeKey = "<extension-method-group>";
    result.valid = allowMethodGroup;
    result.methodGroup = true;
    result.extensionMethodGroup = true;
    result.methodCandidates = methods;
    result.receiverSyntax = expression->first();
    result.receiverTypeKey = receiver.typeKey;
    result.receiverTypeOrdinal = receiver.receiverTypeOrdinal;
    result.assignable = false;
    // NS304-T08-F02: auto-invoca el getter ANTES de registrar el method group, para que el HIR
    // vea la llamada (no un method-group que solo vale como call target).
    if (!allowMethodGroup && methods.size() == 1U)
    {
        const std::size_t functionOrdinal = methods.front() - namespaceCallableBase_;
        if (functionOrdinal < namespaceCallableAdapters().size() &&
            namespaceCallableAdapters()[functionOrdinal].parameters.size() == 1U)
        {
            result = analyzeNamespaceFunctionCall(
                expression, *extension, methodMemberOrdinal, scopeOrdinal, diagnostics, &result);
            return true;
        }
    }
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::ExplicitObjectMethodGroup, expression, methodMemberOrdinal,
        scopeOrdinal, expression->text(), "<extension-method-group>", std::nullopt,
        std::nullopt, methods, expression->first(), receiver.typeKey,
        receiver.receiverTypeOrdinal, false});
    if (!allowMethodGroup)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "extension method group '" + expression->text() +
                "' can only be used as a call target");
    return true;
}

bool ProjectMethodBodyModel::validateMemberReceiverType(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    bool typeReceiver,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    const std::optional<std::size_t> receiverType = receiver.receiverTypeOrdinal;
    const bool receiverIsStruct = receiverType.has_value() && nameLookup_ != nullptr &&
        *receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*receiverType].kind == NamedTypeDeclarationKind::Struct;
    // Stage 51.x: an enum type reference is only ever a *static* receiver (`Enum.Member`) --
    // there is no such thing as an enum instance flowing through this expression evaluator, so
    // this is safe to allow alongside class/struct/interface without widening instance-member
    // access to enums.
    const bool receiverIsEnum = typeReceiver && receiverType.has_value() &&
        nameLookup_ != nullptr && *receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*receiverType].kind == NamedTypeDeclarationKind::Enum;
    // ID 129. Un VALOR de enum, no la referencia al tipo. `receiverIsEnum` de arriba es
    // `Color.Rojo` -- el tipo como receptor estatico; esto es `c.ToString()` con `c` un valor.
    const bool receiverIsEnumValue = !typeReceiver && receiverType.has_value() &&
        nameLookup_ != nullptr && *receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*receiverType].kind == NamedTypeDeclarationKind::Enum;
    if (receiverType.has_value() &&
        (inheritanceModel_->isClass(*receiverType) || receiverIsStruct || receiverIsEnum ||
         receiverIsEnumValue ||
         (interfaceModel_ != nullptr && interfaceModel_->isInterface(*receiverType)))) return true;
    if (bindExtensionMember(
            result, receiver, typeReceiver, expression,
            methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup)) return false;
    if (!typeReceiver && namespaceFunctionModel_ != nullptr &&
        namespaceFunctionModel_->extensionBinding(expression) != nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownMember,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "receiver/type '" + receiver.typeKey +
                "' has no compatible member named '" + expression->text() + "'");
        return false;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidMemberAccess,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "member access requires a class/interface reference, struct value, or enum type "
        "receiver; receiver has type '" + receiver.typeKey + "'");
    return false;
}

void ProjectMethodBodyModel::collectAccessibleMemberCandidates(
    std::vector<std::size_t>& fields,
    std::vector<std::size_t>& events,
    std::vector<std::size_t>& methods,
    bool& inaccessibleFound,
    const ExpressionResult& receiver,
    bool typeReceiver,
    std::size_t receiverType,
    std::size_t accessingTypeOrdinal,
    std::string_view memberName) const
{
    for (const std::size_t candidate : memberCandidatesForType(receiverType, memberName))
    {
        if (candidate >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[candidate];
        if (member.kind == ClassMemberKind::Constructor ||
            member.kind == ClassMemberKind::Destructor) continue;
        if ((typeReceiver && !member.isStatic) || (!typeReceiver && member.isStatic)) continue;
        const std::size_t protectedReceiverType = receiver.baseReceiver
            ? accessingTypeOrdinal : receiverType;
        const bool interfaceReceiver = interfaceModel_ != nullptr &&
            interfaceModel_->isInterface(receiverType);
        if ((!interfaceReceiver && !ProjectMemberAccessibility::isAccessible(
                member, accessingTypeOrdinal, protectedReceiverType,
                *nameLookup_, *inheritanceModel_)) ||
            (interfaceReceiver && member.accessibility != ClassMemberAccessibility::Public))
        {
            inaccessibleFound = true;
            continue;
        }
        if (member.kind == ClassMemberKind::Field) fields.push_back(candidate);
        else if (member.kind == ClassMemberKind::Event) events.push_back(candidate);
        else if (member.kind == ClassMemberKind::Method) methods.push_back(candidate);
    }
}

bool ProjectMethodBodyModel::rejectIncompleteMemberReceiver(
    const ast::AstNode* expression,
    const std::vector<std::size_t>& fields,
    bool typeReceiver,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (typeReceiver || !fields.empty()) return false;
    const std::optional<std::string> receiverPlace = partialMovePlaceKey(
        expression->first(), scopeOrdinal);
    if (!receiverPlace.has_value() ||
        (!placeHasMovedDescendant(*receiverPlace) &&
         !placeHasMaybeMovedDescendant(*receiverPlace))) return false;
    diagnostics.report(
        diagnostics::DiagnosticId::NS3020InvalidValueCategory,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "member invocation/access requires complete value '" + *receiverPlace +
            "', but one of its fields was moved");
    return true;
}

bool ProjectMethodBodyModel::analyzeEventMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    const ast::AstNode* expression,
    const std::vector<std::size_t>& events,
    bool typeReceiver,
    std::size_t receiverType,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowEvent)
{
    if (events.empty()) return false;
    const std::size_t eventOrdinal = events.front();
    const ClassMemberSymbol& event = memberModel_->members()[eventOrdinal];
    result.typeKey = event.declaredTypeKey;
    result.valid = allowEvent;
    result.assignable = false;
    result.eventMemberOrdinal = eventOrdinal;
    result.readonlyReceiver = receiver.readonlyReceiver;
    result.receiverSyntax = typeReceiver ? nullptr : expression->first();
    result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
    result.receiverTypeOrdinal = typeReceiver
        ? std::nullopt : std::optional<std::size_t>(receiverType);
    result.baseReceiver = receiver.baseReceiver;
    const MethodNameBindingKind eventBindingKind = typeReceiver
        ? MethodNameBindingKind::StaticEvent
        : expression->first()->kind() == ast::AstNodeKind::ThisExpression
            ? MethodNameBindingKind::ExplicitThisEvent
            : MethodNameBindingKind::ExplicitObjectEvent;
    recordNameBinding(MethodNameBinding{
        eventBindingKind, expression, methodMemberOrdinal, scopeOrdinal,
        expression->text(), result.typeKey, std::nullopt, std::nullopt, events,
        typeReceiver ? nullptr : expression->first(),
        typeReceiver ? "" : receiver.typeKey,
        typeReceiver ? std::nullopt : std::optional<std::size_t>(receiverType),
        receiver.baseReceiver});
    if (!allowEvent)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "event '" + expression->text() + "' may only be used with +=, -=, or invocation "
                "from within its declaring type in Stage 42");
    }
    return true;
}

bool ProjectMethodBodyModel::validateFieldMemberMoveState(
    ExpressionResult& result,
    const ast::AstNode* expression,
    bool isProjectionReceiver,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const std::optional<std::string> place = partialMovePlaceKey(expression, scopeOrdinal);
    const bool assignmentWrite = expression == definiteAssignmentWriteTarget_ ||
        expression == activeUnionWriteTarget_;
    if (!place.has_value() || assignmentWrite || partialMoveSourceDepth_ != 0) return true;
    const bool unavailable = placeOrAncestorIsPartiallyMoved(*place) ||
        placeOrAncestorIsMaybePartiallyMoved(*place) ||
        (!isProjectionReceiver &&
         (placeHasMovedDescendant(*place) || placeHasMaybeMovedDescendant(*place)));
    if (!unavailable) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::NS3020InvalidValueCategory,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "use of moved or possibly moved field path '" + *place + "'");
    result.valid = false;
    return false;
}

bool ProjectMethodBodyModel::analyzeFieldMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    const ast::AstNode* expression,
    const std::vector<std::size_t>& fields,
    bool typeReceiver,
    bool isProjectionReceiver,
    std::size_t receiverType,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (fields.empty()) return false;
    const ClassMemberSymbol& field = memberModel_->members()[fields.front()];
    result.typeKey = field.declaredTypeKey;
    result.valid = true;
    result.assignmentMemberOrdinal = fields.front();
    if (!validateFieldMemberMoveState(
            result, expression, isProjectionReceiver, scopeOrdinal, diagnostics)) return true;
    if (!typeReceiver && aggregateRequiresActiveMember(receiver.typeKey))
    {
        std::optional<std::size_t> storageKey;
        if (expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> storage = lookupScopeName(
                scopeOrdinal, expression->first()->text());
            if (storage.has_value() &&
                (storage->kind == ScopeNameKind::Local ||
                 storage->kind == ScopeNameKind::Parameter))
                storageKey = storage->semanticOrdinal * 2U +
                    (storage->kind == ScopeNameKind::Parameter ? 1U : 0U);
        }
        const bool writing = expression == activeUnionWriteTarget_;
        const bool activeMemberProven = storageKey.has_value() &&
            activeUnionMembers_.contains(*storageKey) &&
            activeUnionMembers_.at(*storageKey) == fields.front();
        static_cast<void>(writing);
        static_cast<void>(activeMemberProven);
    }
    const bool readonlyOnThis = field.isReadonly && !typeReceiver &&
        expression->first()->kind() == ast::AstNodeKind::ThisExpression &&
        readonlyFieldAssignable(field, methodMemberOrdinal);
    result.assignable = !receiver.readonlyReceiver && !field.isConst &&
        (!field.isReadonly || readonlyOnThis);
    result.addressable = !field.isBitField;
    result.readonlyReceiver = receiver.readonlyReceiver || field.isReadonly;
    result.pointerPointeeReadonly = field.declaredPointerPointeeReadonly;
    result.pointerNullable = field.declaredPointerNullable;
    result.pointerHasProvenance = field.declaredPointerHasProvenance;
    result.pointerKnownAlignment = field.declaredPointerKnownAlignment;
    result.pointerAddressSpace = field.declaredPointerAddressSpace;
    result.receiverSyntax = typeReceiver ? nullptr : expression->first();
    result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
    result.receiverTypeOrdinal = typeReceiver
        ? std::nullopt : std::optional<std::size_t>(receiverType);
    result.baseReceiver = receiver.baseReceiver;
    const MethodNameBindingKind fieldBindingKind = typeReceiver
        ? MethodNameBindingKind::StaticField
        : receiver.baseReceiver ? MethodNameBindingKind::ExplicitBaseField
        : expression->first()->kind() == ast::AstNodeKind::ThisExpression
            ? MethodNameBindingKind::ExplicitThisField
            : MethodNameBindingKind::ExplicitObjectField;
    recordNameBinding(MethodNameBinding{
        fieldBindingKind, expression, methodMemberOrdinal, scopeOrdinal,
        expression->text(), result.typeKey, std::nullopt, std::nullopt, fields,
        typeReceiver ? nullptr : expression->first(),
        typeReceiver ? "" : receiver.typeKey,
        typeReceiver ? std::nullopt : std::optional<std::size_t>(receiverType),
        receiver.baseReceiver});
    return true;
}

bool ProjectMethodBodyModel::analyzePropertyMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    const ast::AstNode* expression,
    const ClassMemberSymbol& caller,
    bool typeReceiver,
    std::size_t receiverType,
    std::size_t accessingTypeOrdinal,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowConstructorPropertyWrite)
{
    const std::size_t protectedReceiverType = receiver.baseReceiver
        ? accessingTypeOrdinal : receiverType;
    const std::vector<std::size_t> propertyGetters = stage38AccessorCandidates(
        receiverType, "get_" + expression->text(), true, true, typeReceiver,
        accessingTypeOrdinal, protectedReceiverType);
    const std::vector<std::size_t> propertySetters = stage38AccessorCandidates(
        receiverType, "set_" + expression->text(), true, false, typeReceiver,
        accessingTypeOrdinal, protectedReceiverType);
    if (propertyGetters.empty()) return false;
    const ClassMemberSymbol& getter = memberModel_->members()[propertyGetters.front()];
    const std::optional<std::size_t> backing =
        allowConstructorPropertyWrite && propertySetters.empty() && !typeReceiver &&
            !getter.isStatic && caller.kind == ClassMemberKind::Constructor &&
            caller.declaringTypeOrdinal == receiverType &&
            expression->first()->kind() == ast::AstNodeKind::ThisExpression
        ? automaticPropertyBackingField(receiverType, expression->text(), false)
        : std::nullopt;
    if (backing.has_value())
    {
        const ClassMemberSymbol& field = memberModel_->members()[*backing];
        result.typeKey = field.declaredTypeKey;
        result.valid = true;
        result.assignable = true;
        result.addressable = true;
        result.pointerPointeeReadonly = field.declaredPointerPointeeReadonly;
        result.pointerNullable = field.declaredPointerNullable;
        result.pointerHasProvenance = field.declaredPointerHasProvenance;
        result.pointerKnownAlignment = field.declaredPointerKnownAlignment;
        result.pointerAddressSpace = field.declaredPointerAddressSpace;
        result.assignmentMemberOrdinal = *backing;
        result.receiverSyntax = expression->first();
        result.receiverTypeKey = receiver.typeKey;
        result.receiverTypeOrdinal = receiverType;
        recordNameBinding(MethodNameBinding{
            MethodNameBindingKind::ExplicitThisField, expression, methodMemberOrdinal,
            scopeOrdinal, expression->text(), result.typeKey, std::nullopt,
            std::nullopt, {*backing}, expression->first(), receiver.typeKey,
            receiverType, false});
        return true;
    }
    const bool interfaceCall = !typeReceiver && interfaceModel_ != nullptr &&
        interfaceModel_->isInterface(receiverType);
    result = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, "get_" + expression->text(),
        propertyGetters, {}, typeReceiver ? nullptr : expression->first(),
        typeReceiver ? "" : receiver.typeKey,
        typeReceiver ? std::nullopt : std::optional<std::size_t>(receiverType),
        receiver.baseReceiver, interfaceCall, diagnostics, true, receiver.readonlyReceiver);
    result.assignable = !receiver.readonlyReceiver && !propertySetters.empty();
    // NS304-T08-F02: a ref-returning getter is assignable even without a setter.
    const bool refGetter = std::any_of(
        propertyGetters.begin(), propertyGetters.end(), [&](std::size_t ordinal)
        {
            return ordinal < memberModel_->members().size() &&
                memberModel_->members()[ordinal].isReferenceReturn;
        });
    if (refGetter) result.assignable = !receiver.readonlyReceiver;
    result.receiverSyntax = typeReceiver ? nullptr : expression->first();
    result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
    result.receiverTypeOrdinal = typeReceiver
        ? std::nullopt : std::optional<std::size_t>(receiverType);
    result.baseReceiver = receiver.baseReceiver;
    return true;
}

bool ProjectMethodBodyModel::analyzeListPatternLengthMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    const ast::AstNode* expression,
    const std::vector<std::size_t>& methods,
    bool typeReceiver,
    std::size_t receiverType,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T03-F03: the list-pattern lowering marks its length access with
    // "list-pattern-length". Built-ins (array/string/inline) and user fields resolve
    // through the core-op/field paths above; a zero-argument `Length()` METHOD on a
    // borrowed view (Span<T>/ReadOnlySpan<T>/StringView) or a user protocol type is
    // auto-invoked here so the marked `.Length` member resolves to an int value.
    if (typeReceiver || expression->text() != "Length" ||
        expression->auxiliaryText() != "list-pattern-length" || methods.empty()) return false;
    std::vector<std::size_t> lengthMethods;
    for (const std::size_t ordinal : methods)
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.parameters.empty() && member.declaredTypeKey == "builtin:int")
            lengthMethods.push_back(ordinal);
    }
    if (lengthMethods.empty()) return false;
    const std::vector<ExpressionResult> noArguments;
    const bool interfaceCall = interfaceModel_ != nullptr &&
        interfaceModel_->isInterface(receiverType);
    ExpressionResult lengthResult = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, "Length",
        std::move(lengthMethods), noArguments, expression->first(), receiver.typeKey,
        receiverType, receiver.baseReceiver, interfaceCall,
        diagnostics, true, receiver.readonlyReceiver);
    if (!lengthResult.valid || lengthResult.methodGroup ||
        lengthResult.typeKey != "builtin:int") return false;
    result = std::move(lengthResult);
    return true;
}

bool ProjectMethodBodyModel::analyzeMethodGroupMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    const ast::AstNode* expression,
    const std::vector<std::size_t>& methods,
    bool typeReceiver,
    std::size_t receiverType,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    if (methods.empty()) return false;
    result.methodGroup = true;
    result.methodCandidates = methods;
    result.valid = allowMethodGroup;
    result.receiverSyntax = typeReceiver ? nullptr : expression->first();
    result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
    result.receiverTypeOrdinal = typeReceiver
        ? std::nullopt : std::optional<std::size_t>(receiverType);
    result.baseReceiver = receiver.baseReceiver;
    result.readonlyReceiver = receiver.readonlyReceiver;
    result.ownership = receiver.ownership;
    result.ownershipFreshOwner = receiver.ownershipFreshOwner;
    result.ownershipSourceLocalOrdinal = receiver.ownershipSourceLocalOrdinal;
    result.ownershipSourceParameterOrdinal = receiver.ownershipSourceParameterOrdinal;
    result.borrowedLocalOrdinal = receiver.borrowedLocalOrdinal;
    const MethodNameBindingKind methodBindingKind = typeReceiver
        ? MethodNameBindingKind::StaticMethodGroup
        : receiver.baseReceiver ? MethodNameBindingKind::ExplicitBaseMethodGroup
        : expression->first()->kind() == ast::AstNodeKind::ThisExpression
            ? MethodNameBindingKind::ExplicitThisMethodGroup
            : MethodNameBindingKind::ExplicitObjectMethodGroup;
    recordNameBinding(MethodNameBinding{
        methodBindingKind, expression, methodMemberOrdinal, scopeOrdinal,
        expression->text(), "<method-group>", std::nullopt, std::nullopt, methods,
        typeReceiver ? nullptr : expression->first(),
        typeReceiver ? "" : receiver.typeKey,
        typeReceiver ? std::nullopt : std::optional<std::size_t>(receiverType),
        receiver.baseReceiver});
    if (!allowMethodGroup)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "instance method group '" + expression->text() +
                "' can only be used as a call target");
    }
    return true;
}

bool ProjectMethodBodyModel::analyzeSynthesizedRootMember(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    const ast::AstNode* expression,
    bool typeReceiver,
    bool receiverIsStruct,
    bool receiverIsEnumValue,
    bool inaccessibleFound,
    std::size_t receiverType,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    // C8 "type root", struct half. A struct is a VALUE: it has no vtable pointer and no room for
    // one (that is exactly what C6 proved and what the C host header certifies), so it cannot
    // inherit the root's three members the way a class does. It gets them synthesized instead,
    // resolved on the STATIC type -- which is what a struct is -- and lowered inline. No boxing,
    // no allocation, no layout change.
    //
    // This runs only after ordinary member lookup has already failed, so a struct that declares
    // its own ToString/Equals/GetHashCode simply wins; these fill a gap, they never shadow.
    // ID 129, la mitad de los ENUMS. Misma forma exacta que la del struct de abajo y por el mismo
    // motivo: se sintetiza SOLO despues de que la busqueda ordinaria haya fallado, asi que un enum
    // con su propio ToString gana y esto solo rellena el hueco.
    //
    // Solo `ToString`. `Equals` y `GetHashCode` sobre un enum son la comparacion y el hash del
    // entero que hay debajo, y eso ya funciona sin sintetizar nada.
    const bool enumRoot = receiverIsEnumValue && !inaccessibleFound &&
        expression->text() == "ToString";
    const bool structRoot = !typeReceiver && receiverIsStruct && !inaccessibleFound &&
        (expression->text() == "ToString" || expression->text() == "Equals" ||
         expression->text() == "GetHashCode" || expression->text() == "Clone") &&
        aggregateAllowsSynthesizedMember(receiver.typeKey, expression->text());
    if (!enumRoot && !structRoot) return false;
    result.methodGroup = true;
    result.methodCandidates.clear();
    result.valid = allowMethodGroup;
    result.receiverSyntax = expression->first();
    result.receiverTypeKey = receiver.typeKey;
    result.receiverTypeOrdinal = receiverType;
    result.typeKey = enumRoot ? kEnumRootFacadeTypeKey : kStructRootFacadeTypeKey;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::ExplicitObjectMethodGroup, expression, methodMemberOrdinal,
        scopeOrdinal, expression->text(), result.typeKey, std::nullopt, std::nullopt, {},
        expression->first(), receiver.typeKey, receiverType, false});
    if (!allowMethodGroup)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            enumRoot
                ? "enum ToString can only be used as a call target"
                : "struct root member '" + expression->text() +
                    "' can only be used as a call target");
    }
    return true;
}

void ProjectMethodBodyModel::finishMemberLookup(
    ExpressionResult& result,
    const ExpressionResult& receiver,
    bool typeReceiver,
    bool inaccessibleFound,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    // NativeSharp 3.0 T05-F06. Extension lookup is deliberately the last member lookup step:
    // real fields/properties/methods and compiler-provided root members always win. The
    // namespace model has already limited the set to the lexical namespace or explicit using
    // imports, so the receiver's namespace never participates accidentally (no ADL).
    if (!inaccessibleFound && bindExtensionMember(
            result, receiver, typeReceiver, expression,
            methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup)) return;
    diagnostics.report(
        inaccessibleFound ? diagnostics::DiagnosticId::InaccessibleMember
                          : diagnostics::DiagnosticId::UnknownMember,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        inaccessibleFound
            ? "member '" + expression->text() + "' is not accessible through receiver/type '" +
                receiver.typeKey + "'"
            : "receiver/type '" + receiver.typeKey + "' has no compatible member named '" +
                expression->text() + "'");
}

void ProjectMethodBodyModel::classifyMemberReceiverKinds(
    bool& receiverIsStruct,
    bool& receiverIsEnumValue,
    const ExpressionResult& receiver,
    bool typeReceiver,
    std::size_t receiverType) const
{
    (void)receiver;
    receiverIsStruct = receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[receiverType].kind == NamedTypeDeclarationKind::Struct;
    receiverIsEnumValue = !typeReceiver &&
        receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[receiverType].kind == NamedTypeDeclarationKind::Enum;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeMember(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    const bool isProjectionReceiver = partialProjectionReceiverDepth_ != 0;
    if (analyzeQualifiedNamespaceMember(
            result, expression, methodMemberOrdinal, scopeOrdinal,
            diagnostics, allowMethodGroup)) return result;
    if (expression == nullptr || expression->first() == nullptr || memberModel_ == nullptr ||
        nameLookup_ == nullptr || inheritanceModel_ == nullptr) return result;
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (caller == nullptr) return result;
    const std::size_t accessingTypeOrdinal = caller->declaringTypeOrdinal;
    ExpressionResult receiver;
    bool typeReceiver = false;
    if (!resolveMemberReceiver(
            receiver, typeReceiver, expression, accessingTypeOrdinal,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
    if (analyzeBuiltinMember(
            result, receiver, typeReceiver, expression,
            methodMemberOrdinal, scopeOrdinal, allowMethodGroup)) return result;
    if (analyzeNativeStringFacadeMember(
            result, receiver, typeReceiver, expression,
            methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup)) return result;
    if (!validateMemberReceiverType(
            result, receiver, typeReceiver, expression,
            methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup)) return result;
    const std::size_t receiverType = *receiver.receiverTypeOrdinal;
    bool receiverIsStruct = false, receiverIsEnumValue = false;
    classifyMemberReceiverKinds(receiverIsStruct, receiverIsEnumValue, receiver, typeReceiver, receiverType);
    std::vector<std::size_t> fields;
    std::vector<std::size_t> events;
    std::vector<std::size_t> methods;
    bool inaccessibleFound = false;
    collectAccessibleMemberCandidates(
        fields, events, methods, inaccessibleFound, receiver, typeReceiver,
        receiverType, accessingTypeOrdinal, expression->text());
    if (rejectIncompleteMemberReceiver(
            expression, fields, typeReceiver, scopeOrdinal, diagnostics)) return result;
    if (analyzeEventMember(
            result, receiver, expression, events, typeReceiver, receiverType,
            methodMemberOrdinal, scopeOrdinal, diagnostics, allowEvent)) return result;
    if (analyzeFieldMember(
            result, receiver, expression, fields, typeReceiver, isProjectionReceiver,
            receiverType, methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
    if (analyzePropertyMember(
            result, receiver, expression, *caller, typeReceiver, receiverType,
            accessingTypeOrdinal, methodMemberOrdinal, scopeOrdinal,
            diagnostics, allowConstructorPropertyWrite)) return result;
    if (analyzeListPatternLengthMember(
            result, receiver, expression, methods, typeReceiver, receiverType,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
    if (analyzeMethodGroupMember(
            result, receiver, expression, methods, typeReceiver, receiverType,
            methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup)) return result;
    if (analyzeSynthesizedRootMember(
            result, receiver, expression, typeReceiver, receiverIsStruct,
            receiverIsEnumValue, inaccessibleFound, receiverType,
            methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup)) return result;
    finishMemberLookup(
        result, receiver, typeReceiver, inaccessibleFound, expression,
        methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup);
    return result;
}
std::vector<std::size_t> ProjectMethodBodyModel::stage38AccessorCandidates(
    std::size_t receiverTypeOrdinal,
    std::string_view accessorName,
    bool propertyAccessor,
    bool getter,
    bool typeReceiver,
    std::size_t accessingTypeOrdinal,
    std::size_t protectedReceiverTypeOrdinal) const
{
    std::vector<std::size_t> result;
    if (memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr) return result;
    const bool interfaceReceiver = interfaceModel_ != nullptr && interfaceModel_->isInterface(receiverTypeOrdinal);
    for (const std::size_t ordinal : memberCandidatesForType(receiverTypeOrdinal, accessorName))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind != ClassMemberKind::Method) continue;
        if (propertyAccessor ? !member.isPropertyAccessor : !member.isIndexerAccessor) continue;
        if (getter != member.isGetter) continue;
        if (typeReceiver != member.isStatic) continue;
        if ((!interfaceReceiver && !ProjectMemberAccessibility::isAccessible(
                member,
                accessingTypeOrdinal,
                protectedReceiverTypeOrdinal,
                *nameLookup_,
                *inheritanceModel_)) ||
            (interfaceReceiver && member.accessibility != ClassMemberAccessibility::Public))
        {
            continue;
        }
        result.push_back(ordinal);
    }
    return result;
}

std::vector<std::size_t> ProjectMethodBodyModel::stage38OperatorCandidates(
    std::string_view operatorTraitKey,
    std::string_view methodName,
    std::string_view leftTypeKey,
    std::optional<std::string_view> rightTypeKey) const
{
    std::vector<std::size_t> result;
    std::unordered_set<std::size_t> seen;
    if (inheritanceModel_ == nullptr || memberModel_ == nullptr) return result;
    const auto appendForType = [&](std::string_view typeKey)
    {
        // C6 "struct members": a user-defined operator is a STATIC method, so nothing about
        // resolving or calling one depends on whether its type is a class or a struct. This used
        // to filter on `isClass`, and that single condition was the entire reason `a + b` on a
        // struct reported "binary operator requires compatible numeric operands" -- the candidate
        // was never even looked for. Measured after removing it: a Vec2 operator+ resolves, and
        // the 8-byte struct travels directly in an i64, which is the correct Win64 ABI for that
        // size. That is what makes vector/matrix math -- the reason structs exist in a game
        // framework -- writable the way it is written everywhere else.
        const std::optional<std::size_t> typeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
        if (!typeOrdinal.has_value()) return;
        if (interfaceModel_ != nullptr)
        {
            for (const TraitOperatorWitness& witness :
                 interfaceModel_->operatorWitnessesOfType(*typeOrdinal))
            {
                if (witness.langItem != operatorTraitKey ||
                    witness.implementationMemberOrdinal >= memberModel_->members().size() ||
                    seen.contains(witness.implementationMemberOrdinal))
                    continue;
                const ClassMemberSymbol& member =
                    memberModel_->members()[witness.implementationMemberOrdinal];
                if (!member.isUserOperator || !member.isStatic || member.name != methodName ||
                    member.operatorTraitKey != operatorTraitKey)
                    continue;
                seen.insert(witness.implementationMemberOrdinal);
                result.push_back(witness.implementationMemberOrdinal);
            }
        }
        for (const std::size_t ordinal : memberModel_->findMembers(*typeOrdinal, methodName))
        {
            if (ordinal >= memberModel_->members().size() || seen.contains(ordinal)) continue;
            const ClassMemberSymbol& member = memberModel_->members()[ordinal];
            if (!member.isUserOperator || !member.isStatic ||
                member.operatorTraitKey != operatorTraitKey) continue;
            seen.insert(ordinal);
            result.push_back(ordinal);
        }
        // NS304-T08-F04: extension operators (namespace functions) participate in the same lookup.
        if (namespaceFunctionModel_ != nullptr)
        {
            for (std::size_t functionOrdinal = 0;
                 functionOrdinal < namespaceFunctionModel_->functions().size(); ++functionOrdinal)
            {
                const NamespaceFunctionSymbol& function =
                    namespaceFunctionModel_->functions()[functionOrdinal];
                if (!function.isExtension || function.name != methodName) continue;
                const std::size_t adapterOrdinal = namespaceCallableBase_ + functionOrdinal;
                if (seen.contains(adapterOrdinal)) continue;
                seen.insert(adapterOrdinal);
                result.push_back(adapterOrdinal);
            }
        }
    };
    appendForType(leftTypeKey);
    if (rightTypeKey.has_value()) appendForType(*rightTypeKey);
    return result;
}
}
