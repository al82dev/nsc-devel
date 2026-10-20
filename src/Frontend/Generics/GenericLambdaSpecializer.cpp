#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::CachePayloadReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
bool ProjectGenericModel::collectExplicitGenericLambdaArguments(
    const ast::AstNode* call,
    const std::vector<ast::GenericParameterSyntax>& parameters,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred,
    std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const bool trailingPack = !parameters.empty() &&
        (parameters.back().kind == ast::GenericParameterSyntaxKind::TypePack ||
         parameters.back().kind == ast::GenericParameterSyntaxKind::ConstantPack);
    const std::size_t fixedCount = parameters.size() - (trailingPack ? 1U : 0U);
    if ((!trailingPack && call->typeSyntaxes().size() != fixedCount) ||
        (trailingPack && call->typeSyntaxes().size() < fixedCount))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "generic lambda expects " + std::to_string(fixedCount) +
                (trailingPack ? " or more" : std::string{}) +
                " generic arguments but received " +
                std::to_string(call->typeSyntaxes().size()));
        return false;
    }
    for (std::size_t index = 0; index < fixedCount; ++index)
        inferred.emplace(parameters[index].name, call->typeSyntaxes()[index]);
    if (trailingPack)
    {
        std::vector<const ast::TypeSyntax*>& values =
            packSubstitutions[parameters.back().name];
        values.insert(
            values.end(), call->typeSyntaxes().begin() +
                static_cast<std::ptrdiff_t>(fixedCount), call->typeSyntaxes().end());
    }
    return true;
}

bool ProjectGenericModel::inferGenericLambdaArgumentsFromValues(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* call,
    const ast::AstNode* source,
    const GenericTemplateRecord& inferenceRecord,
    const std::unordered_set<std::string>& names,
    const GenericLookupScope& scope,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<const ast::AstNode*> valueParameters;
    for (const ast::AstNode* child : source->children())
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
            valueParameters.push_back(child);
    if (valueParameters.size() != call->children().size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            call->range(), "generic lambda argument count does not match its parameter list");
        return false;
    }
    for (std::size_t index = 0; index < valueParameters.size(); ++index)
    {
        const ast::TypeSyntax* actual = inferExpressionType(unit, call->children()[index], scope);
        if (actual == nullptr || valueParameters[index]->typeSyntax() == nullptr ||
            !inferTypeArguments(
                unit, valueParameters[index]->typeSyntax(), actual, inferenceRecord, names,
                inferred))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->children()[index]->range(),
                "cannot infer generic-lambda arguments from value argument " +
                    std::to_string(index + 1U));
            return false;
        }
    }
    return true;
}

bool ProjectGenericModel::buildGenericLambdaSubstitutions(
    const ast::AstNode* call,
    const std::vector<ast::GenericParameterSyntax>& parameters,
    const std::unordered_map<std::string, ast::TypeSyntax*>& inferred,
    const std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
    std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const ast::GenericParameterSyntax& parameter : parameters)
    {
        if (parameter.kind == ast::GenericParameterSyntaxKind::TypePack ||
            parameter.kind == ast::GenericParameterSyntaxKind::ConstantPack)
        {
            if (!packSubstitutions.contains(parameter.name))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error,
                    call->range(),
                    "generic-lambda pack '" + parameter.name +
                        "' requires explicit generic arguments");
                return false;
            }
            continue;
        }
        const auto found = inferred.find(parameter.name);
        if (found == inferred.end())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "generic-lambda parameter '" + parameter.name + "' could not be inferred");
            return false;
        }
        substitutions.emplace(parameter.name, found->second);
    }
    return true;
}

void ProjectGenericModel::markGenericLambdaRecursiveCalls(
    ast::AstNode* node,
    const std::string& localName)
{
    const auto markRecursiveCalls = [&](auto&& self, ast::AstNode* current) -> void
    {
        if (current == nullptr) return;
        if (current->kind() == ast::AstNodeKind::CallExpression && current->first() != nullptr &&
            current->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
            current->first()->text() == localName)
        {
            current->first()->setAuxiliaryText("generic-lambda-self");
        }
        self(self, current->first());
        self(self, current->second());
        self(self, current->third());
        for (ast::AstNode* child : current->children()) self(self, child);
    };
    markRecursiveCalls(markRecursiveCalls, node);
}

void ProjectGenericModel::inferGenericLambdaReturnType(
    ProjectSyntaxUnit& unit,
    ast::AstNode* specialized,
    const GenericLookupScope& scope)
{
    if (specialized->flag() || specialized->first() == nullptr) return;
    GenericLookupScope bodyScope = scope;
    for (const ast::AstNode* parameter : specialized->children())
    {
        if (parameter != nullptr &&
            parameter->kind() == ast::AstNodeKind::ParameterDeclaration &&
            parameter->typeSyntax() != nullptr)
        {
            bodyScope.addValueType(parameter->text(), parameter->typeSyntax());
        }
    }
    if (ast::TypeSyntax* inferredReturn =
            inferExpressionType(unit, specialized->first(), bodyScope))
    {
        specialized->setTypeSyntax(inferredReturn);
        return;
    }
    // A captured name can be intentionally absent from the early generic value-type
    // table (the definitive body model owns capture lookup). Use the first concrete
    // value parameter as a bounded return candidate; semantic analysis still verifies
    // the complete body conversion against it before HIR is built.
    const auto firstParameter = std::ranges::find_if(
        specialized->children(), [](const ast::AstNode* item)
        {
            return item != nullptr &&
                item->kind() == ast::AstNodeKind::ParameterDeclaration &&
                item->typeSyntax() != nullptr;
        });
    if (firstParameter == specialized->children().end()) return;
    bool conflict = false;
    specialized->setTypeSyntax(cloneAliasType(
        unit.arena(), (*firstParameter)->typeSyntax(), {}, conflict));
}

std::size_t ProjectGenericModel::registerGenericLambdaTemplate(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* source)
{
    const auto priorTemplate = genericLambdaTemplateOrdinals_.find(source);
    if (priorTemplate != genericLambdaTemplateOrdinals_.end())
        return priorTemplate->second;
    GenericTemplateRecord record;
    record.kind = GenericTemplateKind::Method;
    record.syntax = source;
    record.projectOrdinal = unit.projectOrdinal();
    record.moduleName = unit.projectOrdinal() < projectModuleNames_.size()
        ? projectModuleNames_[unit.projectOrdinal()] : std::string{};
    record.name = "lambda@" + std::to_string(source->range().begin().offset());
    record.parameters = source->genericParameters();
    record.parameterSyntaxes = source->genericParameterSyntaxes();
    record.constraints = source->genericConstraints();
    record.canonicalKey = record.moduleName + "::<local>::" + record.name;
    const std::size_t templateOrdinal = templateRecords_.size();
    templateRecords_.push_back(std::move(record));
    genericLambdaTemplateOrdinals_.emplace(source, templateOrdinal);
    return templateOrdinal;
}

GenericSpecializationRecord ProjectGenericModel::createGenericLambdaSpecialization(
    const ast::AstNode* call,
    const ast::AstNode* source,
    ast::AstNode* specialized,
    std::size_t templateOrdinal,
    const std::vector<ast::GenericParameterSyntax>& parameters,
    const std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions) const
{
    GenericSpecializationRecord specialization;
    specialization.kind = GenericTemplateKind::Method;
    specialization.templateSyntax = source;
    specialization.specializedSyntax = specialized;
    specialization.templateOrdinal = templateOrdinal;
    specialization.instantiationSyntax = call;
    specialization.instantiationRange = call->range();
    specialization.cacheOrdinal = specializations_.size();
    specialization.templateKey = templateRecords_[templateOrdinal].canonicalKey;
    specialization.specializedName = "lambda-instance-" +
        std::to_string(specialization.cacheOrdinal);
    appendGenericLambdaSpecializationArguments(
        parameters, packSubstitutions, substitutions, specialization);
    return specialization;
}

void ProjectGenericModel::appendGenericLambdaSpecializationArguments(
    const std::vector<ast::GenericParameterSyntax>& parameters,
    const std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    GenericSpecializationRecord& specialization) const
{
    for (const ast::GenericParameterSyntax& parameter : parameters)
    {
        if (const auto pack = packSubstitutions.find(parameter.name);
            pack != packSubstitutions.end())
        {
            for (const ast::TypeSyntax* argument : pack->second)
            {
                const std::string identity = typeSpelling(argument);
                specialization.typeArguments.push_back(identity);
                specialization.specializationKey += "|" + identity;
            }
        }
        else
        {
            const ast::TypeSyntax* argument = substitutions.at(parameter.name);
            const std::string identity = typeSpelling(argument);
            specialization.typeArguments.push_back(identity);
            specialization.specializationKey += "|" + identity;
        }
    }
}

std::optional<std::string> ProjectGenericModel::synthesizeGenericLambdaDelegate(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* call,
    const GenericLookupScope::GenericLambda& local,
    const ast::AstNode* source,
    ast::AstNode* lambda,
    std::string_view suffix,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ast::AstNode* declarationContainer = local.declarationContainer != nullptr
        ? local.declarationContainer : unit.syntaxTree();
    if (declarationContainer == nullptr || lambda == nullptr || lambda->typeSyntax() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "capturing generic lambda cannot synthesize its concrete delegate carrier (" +
                std::string(
                    declarationContainer == nullptr ? "missing container" : "missing return type") +
                ")");
        return std::nullopt;
    }
    const std::string delegateName = "__nsc_generic_closure_" +
        std::to_string(unit.sourceOrdinal()) + "_" +
        std::to_string(source->range().begin().offset()) + std::string(suffix);
    ast::AstNode* delegate = unit.arena().make<ast::AstNode>(
        ast::AstNodeKind::DelegateDeclaration, source->range());
    delegate->setText(delegateName);
    bool ownershipConflict = false;
    delegate->setTypeSyntax(cloneAliasType(
        unit.arena(), lambda->typeSyntax(), {}, ownershipConflict));
    for (const ast::AstNode* parameter : lambda->children())
    {
        if (parameter != nullptr &&
            parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
            delegate->addChild(cloneNode(unit.arena(), parameter, {}, {}, {}));
    }
    declarationContainer->addChild(delegate);
    return delegateName;
}

bool ProjectGenericModel::rewriteGenericLambdaCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const GenericLookupScope::GenericLambda& local,
    const ast::AstNode* source,
    ast::AstNode* specialized,
    const std::string& instanceKey,
    std::size_t instanceOrdinal,
    bool captures,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!captures)
    {
        call->setFirst(specialized);
    }
    else if (const auto carrier = genericLambdaCarrierKeys_.find(source);
             carrier != genericLambdaCarrierKeys_.end() && carrier->second != instanceKey)
    {
        const std::optional<std::string> delegateName = synthesizeGenericLambdaDelegate(
            unit, call, local, source, specialized,
            "_" + std::to_string(instanceOrdinal), diagnostics);
        if (!delegateName.has_value()) return false;
        call->setTypeSyntax(unit.arena().make<ast::TypeSyntax>(
            source->range(), lexer::TokenKind::Identifier, *delegateName));
        call->setSecond(specialized);
        call->setAuxiliaryText("generic-lambda-specialized-call");
    }
    call->clearTypeSyntaxes();
    call->setFlag(false);
    return true;
}

bool ProjectGenericModel::materializeGenericLambdaCarrier(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* call,
    const GenericLookupScope::GenericLambda& local,
    const ast::AstNode* source,
    ast::AstNode* specialized,
    const std::string& instanceKey,
    bool captures,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Keep the lexical binding a concrete zero-size lambda value as well. Calls are rewritten to
    // their own concrete callable, so this carrier is never used for erased dispatch; it merely
    // gives `var` an ABI-valid native type and preserves ordinary declaration/capture analysis.
    if (!materializedGenericLambdas_.insert(source).second) return true;
    ast::AstNode* carrier = cloneNode(unit.arena(), specialized, {}, {}, {});
    carrier->setText("generic-lambda-carrier");
    local.declaration->setFirst(carrier);
    genericLambdaCarrierKeys_[source] = instanceKey;
    if (!captures) return true;
    const std::optional<std::string> delegateName = synthesizeGenericLambdaDelegate(
        unit, call, local, source, carrier, "", diagnostics);
    if (!delegateName.has_value()) return false;
    local.declaration->setTypeSyntax(unit.arena().make<ast::TypeSyntax>(
        source->range(), lexer::TokenKind::Identifier, *delegateName));
    return true;
}

bool ProjectGenericModel::specializeGenericLambdaCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const GenericLookupScope::GenericLambda& local,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (call == nullptr || local.templateSyntax == nullptr || local.declaration == nullptr)
        return false;
    const ast::AstNode* source = local.templateSyntax;
    const auto& parameters = source->genericParameterSyntaxes();
    if (parameters.empty()) return false;
    GenericTemplateRecord inferenceRecord;
    inferenceRecord.parameters = source->genericParameters();
    inferenceRecord.parameterSyntaxes = parameters;
    inferenceRecord.constraints = source->genericConstraints();
    const std::unordered_set<std::string> names(
        inferenceRecord.parameters.begin(), inferenceRecord.parameters.end());
    std::unordered_map<std::string, ast::TypeSyntax*> inferred;
    std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>> packSubstitutions;
    const bool argumentsReady = !call->typeSyntaxes().empty() || call->flag()
        ? collectExplicitGenericLambdaArguments(
            call, parameters, inferred, packSubstitutions, diagnostics)
        : inferGenericLambdaArgumentsFromValues(
            unit, call, source, inferenceRecord, names, scope, inferred, diagnostics);
    if (!argumentsReady) return false;
    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    if (!buildGenericLambdaSubstitutions(
            call, parameters, inferred, packSubstitutions, substitutions, diagnostics))
        return false;
    const auto savedPackSubstitutions = activePackSubstitutions_;
    activePackSubstitutions_ = packSubstitutions;
    ast::AstNode* specialized = cloneNode(unit.arena(), source, substitutions, {}, {});
    activePackSubstitutions_ = savedPackSubstitutions;
    specialized->clearGenericParameters();
    const std::string localName = call->first() != nullptr ? call->first()->text() : std::string{};
    specialized->setText("generic-lambda-instance:" + localName);
    markGenericLambdaRecursiveCalls(specialized->first(), localName);
    inferGenericLambdaReturnType(unit, specialized, scope);
    const std::size_t templateOrdinal = registerGenericLambdaTemplate(unit, source);
    GenericSpecializationRecord specialization = createGenericLambdaSpecialization(
        call, source, specialized, templateOrdinal, parameters, packSubstitutions, substitutions);
    const std::string instanceKey = specialization.specializationKey;
    const std::size_t instanceOrdinal = specialization.cacheOrdinal;
    specializations_.push_back(std::move(specialization));
    specializations_.back().state = SpecializationState::Ready;
    const bool captures = source->auxiliaryText().starts_with("captures:") &&
        source->auxiliaryText().size() > std::string_view("captures:").size();
    if (!rewriteGenericLambdaCall(
            unit, call, local, source, specialized, instanceKey, instanceOrdinal,
            captures, diagnostics))
        return false;
    return materializeGenericLambdaCarrier(
        unit, call, local, source, specialized, instanceKey, captures, diagnostics);
}
}
