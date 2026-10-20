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
void ProjectGenericModel::collectBodyTypeSpecializationDemand(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::string& namespaceName,
    const GenericLookupScope& scope)
{
    if (node == nullptr || node->kind() != ast::AstNodeKind::CallExpression ||
        node->auxiliaryText() != "list-pattern-rest" || node->first() == nullptr ||
        node->first()->kind() != ast::AstNodeKind::MemberExpression ||
        node->first()->first() == nullptr)
    {
        return;
    }

    ast::TypeSyntax* receiverType = inferExpressionType(unit, node->first()->first(), scope);
    if (receiverType == nullptr || receiverType->modifiers().empty()) return;

    // `in/ref/out T[N]` is represented as T[N]* for the calling convention. The synthetic
    // MatchValueExpression still denotes the array value, so look through exactly that recorded
    // reference wrapper before removing the one outer collection modifier used to derive T.
    // The pass kind remains intact in the lexical scope; definitive Sema uses `in` to select
    // ReadOnlySpan<T> and mutable passes to select Span<T>.
    std::size_t structuralModifierCount = receiverType->modifiers().size();
    if (node->first()->first()->kind() == ast::AstNodeKind::MatchValueExpression &&
        !scope.matchValuePassKind.empty() &&
        receiverType->modifiers()[structuralModifierCount - 1U].kind ==
            ast::TypeModifierSyntaxKind::Pointer)
    {
        --structuralModifierCount;
    }
    if (structuralModifierCount == 0U) return;
    const ast::TypeModifierSyntaxKind outer =
        receiverType->modifiers()[structuralModifierCount - 1U].kind;
    if (outer != ast::TypeModifierSyntaxKind::DynamicArray &&
        outer != ast::TypeModifierSyntaxKind::Array)
    {
        return;
    }

    ast::TypeSyntax* elementType =
        cloneType(unit.arena(), receiverType, {}, structuralModifierCount - 1U);
    if (elementType == nullptr) return;

    // Readonly-ness is a semantic property of the receiver (in-parameter, readonly field, const
    // local, and so on) and is deliberately not guessed by this pre-Sema pass. Materialize the
    // finite pair of canonical views. The specialization cache collapses repeated body requests,
    // while Sema selects the one that the actual receiver requires.
    for (const std::string_view templateName : {"ReadOnlySpan", "Span"})
    {
        ast::TypeSyntax* demand = unit.arena().make<ast::TypeSyntax>(
            node->range(), lexer::TokenKind::Identifier, std::string(templateName));
        demand->addTypeArgument(cloneType(unit.arena(), elementType, {}));
        std::string stableKey = std::to_string(unit.projectOrdinal()) + "|" + namespaceName + "|" +
            std::string(templateName) + "|" + typeSpelling(elementType) + "|" +
            std::to_string(node->range().begin().offset());
        const bool duplicated = !bodyTypeDemandKeys_.insert(stableKey).second;
        if (telemetry_.enabled) telemetry_.noteDemand(duplicated);
        // NSC305-G09: deduplicate demands by stableKey (skip the duplicate).
        if (duplicated) continue;
        bodyTypeSpecializationDemands_.push_back(BodyTypeSpecializationDemand{
            &unit, demand, scope, namespaceName, std::move(stableKey)});
    }
}

// NSC305-G09. Processes one wave of body-type demands (see the header).
bool ProjectGenericModel::processBodyTypeDemandWave(
    std::vector<BodyTypeSpecializationDemand>& wave,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::ranges::sort(wave, {}, &BodyTypeSpecializationDemand::stableKey);
#if 0 // INSTRUMENTATION CODE
    std::size_t demandIndex = 0;
#endif // INSTRUMENTATION CODE
    for (BodyTypeSpecializationDemand& demand : wave)
    {
#if 0 // INSTRUMENTATION CODE
        ++demandIndex;
#endif // INSTRUMENTATION CODE
        if (demand.unit == nullptr || demand.syntax == nullptr) continue;
#if 0 // INSTRUMENTATION CODE
        const GenericWorkProgress progress(
            telemetry_, "generics.body-type-demand",
            demand.stableKey + " type=" + demand.syntax->baseName(),
            demandIndex, wave.size());
#endif // INSTRUMENTATION CODE
        // The body-type demand is already the collected form (ReadOnlySpan<T>/Span<T>); materialize
        // it directly (do not re-collect) so the rest-capture Sema sees a materialized Span.
        TypeResolution resolution = resolveTypeTemplate(
            *demand.unit, demand.namespaceName, demand.syntax->baseName(),
            demand.syntax->typeArguments().size());
        if (resolution.kind != TypeResolutionKind::Unique)
        {
            resolution = resolveTypeTemplate(
                *demand.unit, demand.namespaceName, demand.syntax->baseName(),
                std::numeric_limits<std::size_t>::max());
        }
        if (resolution.kind != TypeResolutionKind::Unique || resolution.state == nullptr)
            continue;
        if (!materializeTypeSyntax(
                *demand.unit, demand.syntax, *resolution.state, demand.namespaceName, diagnostics))
        {
            return false;
        }
    }
    return true;
}

bool ProjectGenericModel::materializeBodyTypeSpecializationDemands(
    diagnostics::DiagnosticEngine& diagnostics)
{
#if 0 // INSTRUMENTATION CODE
    std::size_t waveIndex = 0;
#endif // INSTRUMENTATION CODE
    while (!bodyTypeSpecializationDemands_.empty())
    {
        std::vector<BodyTypeSpecializationDemand> wave;
        wave.swap(bodyTypeSpecializationDemands_);
        bodyTypeDemandKeys_.clear();
#if 0 // INSTRUMENTATION CODE
        ++waveIndex;
        const GenericWorkProgress progress(
            telemetry_, "generics.body-type-wave",
            "wave=" + std::to_string(waveIndex) +
                " demands=" + std::to_string(wave.size()),
            wave.size(), wave.size());
#endif // INSTRUMENTATION CODE
        if (!processBodyTypeDemandWave(wave, diagnostics)) return false;
    }
    return !diagnostics.hasErrors();
}

// NSC305-G08. Materializes a resolved closed type and rewrites the spelling (see the header).
bool ProjectGenericModel::materializeTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    TemplateState& state,
    const std::string& namespaceName,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string* outSpecializationKey)
{
    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    // NSC-LIMIT-007: el SITIO. Un `Caja<int>` invalido tiene que sonar donde alguien lo escribio,
    // no sobre la declaracion de `Caja<T>`, que es correcta.
    currentInstantiationRange_ = syntax->range();
    std::string specializationKey;
    const std::string specialized = ensureTypeSpecialization(
        state, syntax->typeArguments(), diagnostics, &specializationKey);
    if (specialized.empty()) return false;
    if (outSpecializationKey != nullptr) *outSpecializationKey = specializationKey;

    rewriteSpecializedSyntax(syntax, state.record, specializationKey, specialized);
    return true;
}

// NSC305-G08. Records a type specialization demand (the discovery half of the two-pass).
void ProjectGenericModel::collectTypeSpecializationDemand(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* target,
    TemplateState& state,
    const std::string& namespaceName)
{
    typeSpecializationDemands_.push_back(GenericTypeDemand{
        &unit,
        &state,
        {},
        target,
        {},
        {},
        {},
        InstantiationContext{
            unit.projectOrdinal(),
            namespaceName,
            target->range(),
            currentOverloadGroup_,
            activePackSubstitutions_,
            activeAssociatedProjections_},
        currentLogicalDepth_ + 1U,
        std::to_string(unit.projectOrdinal()) + "|" + namespaceName + "|" +
            std::to_string(target->range().begin().offset())});
}

void ProjectGenericModel::applyTypeDemandContext(const GenericTypeDemand& demand)
{
    currentInstantiationProjectOrdinal_ = demand.context.projectOrdinal;
    currentInstantiationNamespace_ = demand.context.namespaceName;
    currentInstantiationRange_ = demand.context.range;
    currentOverloadGroup_ = demand.context.overloadGroup;
    activePackSubstitutions_ = demand.context.packSubstitutions;
    activeAssociatedProjections_ = demand.context.associatedProjections;
    currentLogicalDepth_ = demand.logicalDepth;
}

std::vector<ProjectGenericModel::ReservedDemand>
ProjectGenericModel::reserveTypeDemandWave(
    std::vector<GenericTypeDemand>& wave,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G09. Pre-reserve every ordinal in the sorted order BEFORE any execute, so the
    // ordinals do not depend on worker completion order once the execute becomes parallel.
    std::vector<ReservedDemand> reserved;
    reserved.reserve(wave.size());
#if 0 // INSTRUMENTATION CODE
    std::size_t reserveIndex = 0;
#endif // INSTRUMENTATION CODE
    for (GenericTypeDemand& demand : wave)
    {
#if 0 // INSTRUMENTATION CODE
        ++reserveIndex;
#endif // INSTRUMENTATION CODE
        if (demand.unit == nullptr || demand.state == nullptr || demand.target == nullptr)
            continue;
#if 0 // INSTRUMENTATION CODE
        const GenericWorkProgress reserveProgress(
            telemetry_, "generics.type-demand-reserve",
            demand.state->record.canonicalKey + " site=" + demand.stableKey,
            reserveIndex, wave.size());
#endif // INSTRUMENTATION CODE
        applyTypeDemandContext(demand);
        ReservedDemand entry;
        entry.demand = &demand;
        entry.callerContext.save(*this);
        std::vector<std::string> identities;
        entry.ordinal = reserveTypeDemand(
            *demand.state, demand.target->typeArguments(), entry.key,
            entry.specializedName, identities, diagnostics);
        reserved.push_back(std::move(entry));
    }
    return reserved;
}

std::string ProjectGenericModel::applyReservedTypeDemand(
    ReservedDemand& reservedDemand,
    SpecializationProduct* preClonedProduct,
    diagnostics::DiagnosticEngine& diagnostics)
{
    GenericTypeDemand& demand = *reservedDemand.demand;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress applyProgress(
        telemetry_, "generics.type-demand-apply",
        reservedDemand.key + " site=" + demand.stableKey);
#endif // INSTRUMENTATION CODE
    applyTypeDemandContext(demand);
    if (reservedDemand.ordinal == std::numeric_limits<std::size_t>::max())
    {
        if (const auto existing = specializationPlanner_.request(reservedDemand.key))
        {
            rewriteSpecializedSyntax(
                demand.target, demand.state->record, reservedDemand.key,
                specializations_[*existing].specializedName);
            return specializations_[*existing].specializedName;
        }
        return {};
    }
    return executeTypeDemand(
        *demand.unit, demand.target, *demand.state, demand.context.namespaceName,
        reservedDemand.ordinal, reservedDemand.key, reservedDemand.specializedName,
        demand.target->typeArguments(), reservedDemand.callerContext, diagnostics,
        preClonedProduct);
}

void ProjectGenericModel::cloneReservedTypeDemands(
    std::vector<ReservedDemand>& reserved,
    std::vector<SpecializationProduct>& products)
{
    // NSC305-G09 corrective. Clone into one private product per stable index with at most
    // `parallelismWorkers_` tasks. The old loop launched one task per demand, captured its
    // range variable by reference, and wrote every clone into the shared unit AstArena.
    // After all workers finish, apply/commit strictly in stable order on the coordinator.
    std::atomic<std::size_t> nextIndex{0};
    const std::size_t workerCount = std::min(parallelismWorkers_, reserved.size());
    std::vector<std::future<void>> futures;
    futures.reserve(workerCount);
    for (std::size_t worker = 0; worker < workerCount; ++worker)
    {
        futures.push_back(std::async(std::launch::async,
            [&]()
            {
                while (true)
                {
                    const std::size_t index = nextIndex.fetch_add(1U);
                    if (index >= reserved.size()) return;
                    ReservedDemand& reservedDemand = reserved[index];
                    if (reservedDemand.ordinal == std::numeric_limits<std::size_t>::max())
                        continue;
                    GenericTypeDemand& demand = *reservedDemand.demand;
#if 0 // INSTRUMENTATION CODE
                    const GenericWorkProgress cloneProgress(
                        telemetry_, "generics.type-demand-clone",
                        reservedDemand.key + " site=" + demand.stableKey,
                        index + 1U, reserved.size());
#endif // INSTRUMENTATION CODE
                    products[index] = cloneTypeSpecializationProduct(
                        *demand.state, demand.target->typeArguments(),
                        reservedDemand.specializedName, reservedDemand.callerContext);
                    products[index].ordinal = reservedDemand.ordinal;
                }
            }));
    }
    for (std::future<void>& future : futures) future.get();
}

bool ProjectGenericModel::applyClonedTypeDemands(
    std::vector<ReservedDemand>& reserved,
    std::vector<SpecializationProduct>& products,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (std::size_t index = 0; index < reserved.size(); ++index)
    {
        SpecializationProduct* product =
            reserved[index].ordinal == std::numeric_limits<std::size_t>::max()
            ? nullptr : &products[index];
        if (applyReservedTypeDemand(reserved[index], product, diagnostics).empty())
        {
            currentLogicalDepth_ = 0;
            return false;
        }
    }
    return true;
}

bool ProjectGenericModel::applyReservedTypeDemands(
    std::vector<ReservedDemand>& reserved,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (ReservedDemand& reservedDemand : reserved)
    {
        if (applyReservedTypeDemand(reservedDemand, nullptr, diagnostics).empty())
        {
            currentLogicalDepth_ = 0;
            return false;
        }
    }
    return true;
}

bool ProjectGenericModel::executeTypeDemandWave(
    std::vector<GenericTypeDemand>& wave,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G09. Deterministic wave: sort by the stable key (project | namespace | offset) so
    // the materialization order does not depend on discovery order.
    std::ranges::sort(wave, {}, &GenericTypeDemand::stableKey);
    std::vector<ReservedDemand> reserved = reserveTypeDemandWave(wave, diagnostics);
    bool ok = true;
    if (parallelismWorkers_ > 1U && reserved.size() > 1U)
    {
        std::vector<SpecializationProduct> products(reserved.size());
        cloneReservedTypeDemands(reserved, products);
        ok = applyClonedTypeDemands(reserved, products, diagnostics);
    }
    else
        ok = applyReservedTypeDemands(reserved, diagnostics);
    currentLogicalDepth_ = 0;
    return ok;
}

// NSC305-G08. Executes the collected type demands in waves (the apply half of the two-pass).
bool ProjectGenericModel::executeTypeDemandWaves(
    diagnostics::DiagnosticEngine& diagnostics)
{
#if 0 // INSTRUMENTATION CODE
    std::size_t waveIndex = 0;
#endif // INSTRUMENTATION CODE
    while (!typeSpecializationDemands_.empty())
    {
        std::vector<GenericTypeDemand> wave;
        wave.swap(typeSpecializationDemands_);
#if 0 // INSTRUMENTATION CODE
        ++waveIndex;
        const GenericWorkProgress waveProgress(
            telemetry_, "generics.type-demand-wave",
            "wave=" + std::to_string(waveIndex) +
                " demands=" + std::to_string(wave.size()),
            wave.size(), wave.size());
#endif // INSTRUMENTATION CODE
        if (!executeTypeDemandWave(wave, diagnostics)) return false;
    }
    return !diagnostics.hasErrors();
}

std::optional<bool> ProjectGenericModel::processNestedTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (ast::TypeSyntax* argument : syntax->typeArguments())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics)) return false;
    }
    // Structural surfaces still recurse into their arguments above, but never name a reusable
    // nominal template. Keep this classification shared with metadata dependency discovery.
    if (!syntax->isFunction())
        return isStructuralGenericSurface(syntax) ? std::optional<bool>(true) : std::nullopt;
    if (!processTypeSyntax(unit, syntax->functionReturnType(), namespaceName, scope, diagnostics))
        return false;
    if (syntax->functionErrorType() != nullptr &&
        !processTypeSyntax(unit, syntax->functionErrorType(), namespaceName, scope, diagnostics))
        return false;
    return true;
}

void ProjectGenericModel::recordDeclaredTypeIdentity(
    ast::TypeSyntax* syntax,
    std::size_t projectOrdinal,
    const std::string& namespaceName,
    const GenericLookupScope& scope)
{
    const DeclaredTypeIdentity* declaration =
        resolveDeclaredTypeIdentity(syntax, projectOrdinal, namespaceName, scope);
    if (declaration == nullptr) return;
    std::string canonical = "type:" + declaration->identityKey;
    appendTypeModifiers(canonical, syntax);
    resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
    resolvedTypeOriginBySyntax_[syntax] =
        DeclaredTypeOrigin{declaration->projectOrdinal, declaration->namespaceName};
}

bool ProjectGenericModel::validateGenericTypeArgumentCount(
    const TemplateState& state,
    const ast::TypeSyntax* syntax,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (genericArityAccepts(state.record, syntax->typeArguments().size())) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidArgumentCount,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        "generic type '" + state.record.name + "' expects " +
            (trailingPackIndex(state.record).has_value()
                ? "at least " + std::to_string(*trailingPackIndex(state.record))
                : std::to_string(state.record.parameters.size())) +
            " type argument(s) but received " +
            std::to_string(syntax->typeArguments().size()));
    return false;
}

bool ProjectGenericModel::hasOpenGenericParameter(
    const ast::TypeSyntax* syntax,
    const GenericLookupScope& scope) const
{
    for (const ast::TypeSyntax* argument : syntax->typeArguments())
    {
        if (argument != nullptr && argument->typeArguments().empty() &&
            scope.typeParameters.read().contains(argument->baseName()))
            return true;
    }
    return false;
}

bool ProjectGenericModel::processTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string* /*outSpecializationKey*/)
{
    if (syntax == nullptr) return true;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.process-type-syntax", genericTypeDetail(*syntax));
#endif // INSTRUMENTATION CODE
    std::string constType;
    std::string constValue;
    if (decodeConstGenericArgument(syntax, constType, constValue)) return true;
    if (syntax->isPackExpansion())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "generic pack expansion is valid only for a declared type pack inside a generic argument list");
        return false;
    }
    const std::optional<bool> nestedResult =
        processNestedTypeSyntax(unit, syntax, namespaceName, scope, diagnostics);
    if (nestedResult.has_value()) return *nestedResult;
    TypeResolution resolution = resolveTypeTemplate(
        unit, namespaceName, syntax->baseName(), syntax->typeArguments().size());
    if (resolution.kind != TypeResolutionKind::Unique)
    {
        resolution = resolveTypeTemplate(
            unit, namespaceName, syntax->baseName(), std::numeric_limits<std::size_t>::max());
    }
    TemplateState* state = resolution.kind == TypeResolutionKind::Unique ? resolution.state : nullptr;
    if (state == nullptr)
    {
        if (!syntax->typeArguments().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                syntax->range(),
                "type '" + syntax->baseName() +
                    "' does not name a unique visible generic class/struct template");
            return false;
        }
        recordDeclaredTypeIdentity(syntax, unit.projectOrdinal(), namespaceName, scope);
        return true;
    }

    if (!validateGenericTypeArgumentCount(*state, syntax, diagnostics)) return false;
    if (!normalizeGenericArguments(
            state->record, syntax->typeArguments(), syntax->range(), diagnostics))
        return false;

    // NS304-T08-F06: an open generic reference whose argument is a scope parameter does not
    // materialize a closed clone; record its template identity and keep its source spelling so a
    // later substitution can still replace T.
    if (hasOpenGenericParameter(syntax, scope))
    {
        std::string canonical = "generic:" + state->record.canonicalKey;
        appendTypeModifiers(canonical, syntax);
        resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
        return true;
    }

    // NSC305-G08. The two-pass: record the demand (the wave scheduler executes + rewrites later).
    collectTypeSpecializationDemand(unit, syntax, *state, namespaceName);
    return true;
}
}
