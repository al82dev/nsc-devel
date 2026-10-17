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
#include "nativesharp/Language/SumTypeSemantics.h"
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
constexpr std::uint8_t kLocalMoved = 1U << 0U;
constexpr std::uint8_t kLocalMaybeMoved = 1U << 1U;
constexpr std::uint8_t kParameterMoved = 1U << 0U;
constexpr std::uint8_t kParameterMaybeMoved = 1U << 1U;

std::optional<std::size_t> snapshotLocalIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t globalLocalOrdinal) noexcept
{
    if (globalLocalOrdinal < snapshot.localBaseOrdinal ||
        globalLocalOrdinal >= snapshot.localCount)
    {
        return std::nullopt;
    }
    const std::size_t index = globalLocalOrdinal - snapshot.localBaseOrdinal;
    return index < snapshot.localFlags.size() ? std::optional<std::size_t>(index) : std::nullopt;
}

bool snapshotLocalHasAnyFlag(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t localOrdinal,
    std::uint8_t flags) noexcept
{
    const std::optional<std::size_t> index = snapshotLocalIndex(snapshot, localOrdinal);
    return index.has_value() && (snapshot.localFlags[*index] & flags) != 0U;
}

bool snapshotParameterHasAnyFlag(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t parameterOrdinal,
    std::uint8_t flags) noexcept
{
    return parameterOrdinal < snapshot.parameterFlags.size() &&
        (snapshot.parameterFlags[parameterOrdinal] & flags) != 0U;
}

bool isErrorType(std::string_view key) noexcept
{
    return key.empty() || key == "<error>";
}

sema::OwnershipKind ownershipKindFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    if (syntax == nullptr)
    {
        return sema::OwnershipKind::None;
    }

    switch (syntax->ownershipQualifier())
    {
        case ast::OwnershipQualifierSyntaxKind::None: return sema::OwnershipKind::None;
        case ast::OwnershipQualifierSyntaxKind::Owned: return sema::OwnershipKind::Owned;
        case ast::OwnershipQualifierSyntaxKind::Borrowed: return sema::OwnershipKind::Borrowed;
    }
    return sema::OwnershipKind::None;
}

language::PointerContract pointerContractFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    language::PointerContract contract;
    if (syntax == nullptr) return contract;
    contract.mutablePointee = !syntax->isPointeeConst();
    for (auto modifier = syntax->modifiers().rbegin(); modifier != syntax->modifiers().rend(); ++modifier)
    {
        if (modifier->kind != ast::TypeModifierSyntaxKind::Pointer) continue;
        contract.nullable = modifier->pointerNullability !=
            ast::PointerNullabilitySyntaxKind::NonNull;
        contract.hasProvenance = modifier->pointerHasProvenance;
        contract.knownAlignment = modifier->pointerKnownAlignment;
        contract.addressSpace = modifier->pointerAddressSpace;
        break;
    }
    return contract;
}
}

void ProjectMethodBodyModel::analyzeBlock(
    const ast::AstNode* block,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    bool createNestedScope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (block == nullptr)
    {
        return;
    }

    std::size_t activeScope = scopeOrdinal;
    if (createNestedScope)
    {
        activeScope = createScope(MethodScopeKind::Block, methodMemberOrdinal, scopeOrdinal);
    }

    const bool deferCaptureBoundary =
        block->auxiliaryText() == "defer-capture-boundary";
    if (deferCaptureBoundary)
    {
        DeferCaptureContext context;
        context.boundaryScopeOrdinal = activeScope;
        std::string_view names(block->text());
        std::size_t begin = 0;
        while (begin <= names.size())
        {
            const std::size_t end = names.find(',', begin);
            const std::string_view name = names.substr(
                begin, end == std::string_view::npos ? names.size() - begin : end - begin);
            if (name == "this") context.capturesThis = true;
            else if (!name.empty()) context.allowedNames.emplace(name);
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }
        deferCaptureStack_.push_back(std::move(context));
    }

    const std::size_t cleanupDepthAtEntry = activeCleanupRegions_.size();
    for (const ast::AstNode* statement : block->children())
    {
        // A switch section keeps its labels in the same child list as its statements, and the
        // labels were already analyzed and folded by analyzeSwitch before this runs. Skipping
        // them here is what lets a section reuse this routine wholesale -- nested scope, cleanup
        // continuations and the unreachable-code probe included -- instead of growing a second,
        // slightly-different copy of it.
        if (statement != nullptr && statement->kind() == ast::AstNodeKind::SwitchLabel)
        {
            continue;
        }
        if (ownershipFlowReachable_)
        {
            analyzeStatement(statement, methodMemberOrdinal, activeScope, diagnostics);
            continue;
        }

        // Unreachable source still needs semantic/type binding because later HIR/MIR
        // construction walks the full AST. Analyze it in a disposable reachable probe,
        // then restore the terminated ownership state so dead code never contributes to
        // a subsequent control-flow join.
        const OwnershipFlowSnapshot terminatedState = captureOwnershipFlow();
        OwnershipFlowSnapshot probeState = terminatedState;
        probeState.reachable = true;
        restoreOwnershipFlow(probeState);
        analyzeStatement(statement, methodMemberOrdinal, activeScope, diagnostics);
        restoreOwnershipFlow(terminatedState);
    }
    // NS304-T07-F06: a scoped child borrow of a local that dies here (block exit) is a dangling
    // Future. Reject it unless a Join already discharged the borrow.
    if (!scopedChildBorrows_.empty())
    {
        const std::span<const MethodLocalSymbol> frameLocals = locals();
        for (std::size_t ordinal = 0; ordinal < frameLocals.size(); ++ordinal)
        {
            if (frameLocals[ordinal].scopeOrdinal == activeScope &&
                scopedChildBorrows_.contains(ordinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    block->range(),
                    "scoped child borrow outlives its source; join or cancel the TaskScope "
                    "before leaving the source lifetime");
                break;
            }
        }
    }
    for (std::size_t index = activeCleanupRegions_.size(); index > cleanupDepthAtEntry; --index)
    {
        recordNormalCleanupContinuation(
            activeCleanupRegions_[index - 1], block, methodMemberOrdinal, activeScope);
    }
    activeCleanupRegions_.resize(cleanupDepthAtEntry);
    if (deferCaptureBoundary) deferCaptureStack_.pop_back();
    constexpr std::string_view matchCoveragePrefix = "match-coverage:v1:";
    if (block->auxiliaryText().starts_with(matchCoveragePrefix) && !block->text().empty())
    {
        std::vector<language::MatchArmCoverage> arms;
        std::string_view encoded(block->auxiliaryText());
        encoded.remove_prefix(matchCoveragePrefix.size());
        std::size_t begin = 0;
        while (begin <= encoded.size())
        {
            const std::size_t end = encoded.find(';', begin);
            const std::string_view armText = encoded.substr(
                begin, end == std::string_view::npos ? encoded.size() - begin : end - begin);
            language::MatchArmCoverage arm;
            std::string_view casesText = armText;
            if (!casesText.empty() && casesText.front() == '~')
            {
                arm.hasGuard = true;
                casesText.remove_prefix(1);
            }
            arm.wildcard = casesText == "*";
            if (!arm.wildcard && casesText != "?" && !casesText.empty() && casesText.front() == '#')
            {
                std::size_t lengthBegin = 0;
                while (lengthBegin <= casesText.size())
                {
                    const std::size_t lengthEnd = casesText.find('|', lengthBegin);
                    std::string_view item = casesText.substr(
                        lengthBegin,
                        lengthEnd == std::string_view::npos
                            ? casesText.size() - lengthBegin : lengthEnd - lengthBegin);
                    if (!item.empty() && item.front() == '#') item.remove_prefix(1);
                    const bool hasRest = !item.empty() && item.back() == '+';
                    if (hasRest) item.remove_suffix(1);
                    arm.listLengths.push_back({
                        static_cast<std::size_t>(std::stoull(std::string(item))), hasRest});
                    if (lengthEnd == std::string_view::npos) break;
                    lengthBegin = lengthEnd + 1;
                }
            }
            else if (!arm.wildcard && casesText != "?")
            {
                std::size_t caseBegin = 0;
                while (caseBegin <= casesText.size())
                {
                    const std::size_t caseEnd = casesText.find('|', caseBegin);
                    const std::string_view name = casesText.substr(
                        caseBegin,
                        caseEnd == std::string_view::npos
                            ? casesText.size() - caseBegin : caseEnd - caseBegin);
                    if (!name.empty()) arm.cases.emplace_back(name);
                    if (caseEnd == std::string_view::npos) break;
                    caseBegin = caseEnd + 1;
                }
            }
            arms.push_back(std::move(arm));
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }

        std::vector<std::string> domain;
        if (const std::optional<ScopeName> source =
                lookupScopeNameInScope(activeScope, block->text());
            source.has_value() && source->kind == ScopeNameKind::Local &&
            source->semanticOrdinal < locals_.size() && nameLookup_ != nullptr)
        {
            const std::optional<std::size_t> typeOrdinal =
                namedTypeOrdinalForTypeKey(locals_[source->semanticOrdinal].typeKey);
            if (typeOrdinal.has_value() && *typeOrdinal < nameLookup_->namedTypes().size())
            {
                const ast::AstNode* declaration = nameLookup_->namedTypes()[*typeOrdinal].syntax;
                constexpr std::string_view casesMarker = "cases=";
                if (declaration != nullptr && declaration->auxiliaryText().starts_with("variant,"))
                {
                    const std::string_view metadata(declaration->auxiliaryText());
                    const std::size_t casesBegin = metadata.find(casesMarker);
                    if (casesBegin != std::string_view::npos)
                    {
                        std::string_view cases = metadata.substr(casesBegin + casesMarker.size());
                        std::size_t caseBegin = 0;
                        while (caseBegin <= cases.size())
                        {
                            const std::size_t caseEnd = cases.find('|', caseBegin);
                            std::string_view item = cases.substr(
                                caseBegin,
                                caseEnd == std::string_view::npos
                                    ? cases.size() - caseBegin : caseEnd - caseBegin);
                            if (const std::size_t payload = item.find(':');
                                payload != std::string_view::npos) item = item.substr(0, payload);
                            if (!item.empty()) domain.emplace_back(item);
                            if (caseEnd == std::string_view::npos) break;
                            caseBegin = caseEnd + 1;
                        }
                    }
                }
                // Closed generic variants and NSMETA-rehydrated declarations may expose their
                // concrete API through generated members even when the aggregate syntax attached
                // to the specialized type no longer carries the original compact marker. The
                // factory marker is part of the same compiler-owned contract used by metadata.
                if (domain.empty() && memberModel_ != nullptr)
                {
                    constexpr std::string_view factoryPrefix = "variant-case-factory:";
                    for (const std::size_t memberOrdinal : memberModel_->membersOfType(*typeOrdinal))
                    {
                        if (memberOrdinal >= memberModel_->members().size()) continue;
                        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                        if (member.syntax == nullptr ||
                            !member.syntax->auxiliaryText().starts_with(factoryPrefix)) continue;
                        std::string_view marker(member.syntax->auxiliaryText());
                        marker.remove_prefix(factoryPrefix.size());
                        if (const std::size_t tag = marker.rfind(':');
                            tag != std::string_view::npos) marker = marker.substr(0, tag);
                        if (!marker.empty() &&
                            std::ranges::find(domain, marker) == domain.end())
                            domain.emplace_back(marker);
                    }
                }
            }
        }

        if (domain.empty())
        {
            const bool hasListLength = std::ranges::any_of(
                arms, [](const language::MatchArmCoverage& arm)
                {
                    return !arm.listLengths.empty();
                });
            if (hasListLength)
            {
                const language::ExhaustivenessResult result = language::analyzeListMatch(arms);
                for (const std::size_t arm : result.unreachableArms)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        block->range(),
                        "match arm #" + std::to_string(arm + 1) +
                            " is unreachable for the list-length domain");
                }
                if (!result.exhaustive)
                {
                    std::string message = "non-exhaustive match; missing list length";
                    if (result.missingCases.empty())
                    {
                        message += ": a list pattern needs a rest or wildcard arm to cover "
                            "unbounded lengths";
                    }
                    else
                    {
                        message += result.missingCases.size() == 1 ? ": " : "s: ";
                        for (std::size_t index = 0; index < result.missingCases.size(); ++index)
                        {
                            if (index != 0) message += ", ";
                            message += result.missingCases[index];
                        }
                    }
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        block->range(),
                        message);
                }
            }
            else if (!std::ranges::any_of(arms, [](const language::MatchArmCoverage& arm)
                {
                    return arm.wildcard && !arm.hasGuard;
                }))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    block->range(),
                    "match over an open or unresolved domain requires a final wildcard or "
                    "binding arm");
            }
        }
        else
        {
            const language::ExhaustivenessResult result =
                language::analyzeClosedMatch(domain, arms);
            for (const std::size_t arm : result.unreachableArms)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    block->range(),
                    "match arm #" + std::to_string(arm + 1) +
                        " is unreachable for the closed variant domain");
            }
            if (!result.exhaustive)
            {
                std::string message = "non-exhaustive match; missing variant case";
                message += result.missingCases.size() == 1 ? ": " : "s: ";
                for (std::size_t index = 0; index < result.missingCases.size(); ++index)
                {
                    if (index != 0) message += ", ";
                    message += result.missingCases[index];
                }
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    block->range(),
                    std::move(message));
            }
        }
    }
}

void ProjectMethodBodyModel::analyzeStatement(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr)
    {
        return;
    }

    switch (statement->kind())
    {
        case ast::AstNodeKind::BlockStatement:
            analyzeBlock(
                statement,
                methodMemberOrdinal,
                scopeOrdinal,
                statement->auxiliaryText() != "declaration-list",
                diagnostics);
            break;
        case ast::AstNodeKind::VariableDeclarationStatement:
            analyzeVariable(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::DeconstructionDeclarationStatement:
        {
            const ExpressionResult source = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!source.valid) break;
            if (statement->typeSyntax() != nullptr)
            {
                if (memberModel_ == nullptr || nameLookup_ == nullptr) break;
                const std::string positionalTypeKey =
                    memberModel_->canonicalTypeKey(statement->typeSyntax(), *nameLookup_);
                if (positionalTypeKey != source.typeKey)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->range(),
                        "positional pattern type '" + positionalTypeKey +
                            "' does not match governing value type '" + source.typeKey + "'");
                    break;
                }
            }
            const std::optional<MethodDeconstructionBinding> deconstruction =
                bindDeconstructionSource(
                    statement, statement->first(), source, methodMemberOrdinal, diagnostics);
            if (!deconstruction.has_value()) break;
            const std::optional<std::vector<std::string>> elements =
                ProjectTypeConversion::tupleElementTypeKeys(deconstruction->tupleTypeKey);
            if (!elements.has_value()) break;
            if (elements->size() != statement->children().size())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidArgumentCount,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "deconstruction declaration has " +
                        std::to_string(statement->children().size()) +
                        " binding(s), but the tuple has " + std::to_string(elements->size()));
                break;
            }
            if (!elementTypeHasTrivialLifetime(deconstruction->tupleTypeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "deconstruction declaration of owning or move-only elements requires "
                    "per-element move bindings and drop flags");
                break;
            }
            deconstructionBindingBySyntax_[statement] = deconstructionBindings_.size();
            deconstructionBindings_.push_back(*deconstruction);

            std::unordered_set<std::string> declaredHere;
            std::function<void(const ast::AstNode*, const std::string&, const std::string&)>
                bindPattern;
            bindPattern = [&](const ast::AstNode* binding, const std::string& typeKey,
                              const std::string& path)
            {
                if (binding == nullptr) return;
                if (binding->kind() == ast::AstNodeKind::TupleExpression)
                {
                    const std::optional<std::vector<std::string>> nested =
                        ProjectTypeConversion::tupleElementTypeKeys(typeKey);
                    if (!nested.has_value())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            binding->range(),
                            "nested deconstruction pattern at " + path +
                                " requires a tuple element");
                        return;
                    }
                    if (nested->size() != binding->children().size())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidArgumentCount,
                            diagnostics::DiagnosticSeverity::Error,
                            binding->range(),
                            "nested deconstruction pattern at " + path + " has " +
                                std::to_string(binding->children().size()) +
                                " binding(s), but the tuple element has " +
                                std::to_string(nested->size()));
                        return;
                    }
                    for (std::size_t index = 0; index < nested->size(); ++index)
                        bindPattern(
                            binding->children()[index], (*nested)[index],
                            path + "." + std::to_string(index));
                    return;
                }
                if (binding->kind() != ast::AstNodeKind::IdentifierExpression)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        binding->range(),
                        "deconstruction declaration patterns contain only names, '_', or tuples");
                    return;
                }
                if (binding->text() == "_") return;
                if (!declaredHere.insert(binding->text()).second)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        binding->range(),
                        "duplicate deconstruction binding '" + binding->text() + "'");
                    return;
                }
                const std::optional<ScopeName> sameScope =
                    lookupScopeNameInScope(scopeOrdinal, binding->text());
                if (sameScope.has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        binding->range(),
                        "duplicate local/parameter name '" + binding->text() +
                            "' in the same method scope");
                    reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
                    return;
                }
                MethodLocalSymbol local;
                local.methodMemberOrdinal = methodMemberOrdinal;
                local.scopeOrdinal = scopeOrdinal;
                local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
                if (const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                    method != nullptr) local.fileId = method->fileId;
                local.range = binding->range();
                local.syntax = binding;
                local.name = binding->text();
                local.typeKey = typeKey;
                local.lambdaOrdinal = currentLambdaOrdinal_;
                if (const std::optional<ScopeName> outer =
                        lookupScopeName(scopeOrdinal, binding->text());
                    outer.has_value())
                {
                    MethodShadowingKind kind = outer->kind == ScopeNameKind::Parameter
                        ? MethodShadowingKind::LocalShadowsParameter
                        : MethodShadowingKind::LocalShadowsLocal;
                    shadowingEvents_.push_back(MethodShadowingEvent{
                        kind, methodMemberOrdinal, scopeOrdinal, binding->text(), binding->range()});
                }
                else if (!memberCandidates(methodMemberOrdinal, binding->text()).empty())
                {
                    shadowingEvents_.push_back(MethodShadowingEvent{
                        MethodShadowingKind::LocalShadowsMember,
                        methodMemberOrdinal,
                        scopeOrdinal,
                        binding->text(),
                        binding->range()});
                }
                const std::size_t localOrdinal = locals_.size();
                locals_.push_back(std::move(local));
                scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
                scopeNames_[scopeOrdinal].push_back(ScopeName{
                    ScopeNameKind::Local,
                    binding->text(),
                    localOrdinal,
                    binding->range()});
                assignedLocals_.insert(localOrdinal);
            };
            for (std::size_t index = 0; index < statement->children().size(); ++index)
            {
                bindPattern(
                    statement->children()[index], (*elements)[index],
                    std::to_string(index));
            }
            break;
        }
        case ast::AstNodeKind::IfStatement:
        {
            const ExpressionResult condition = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (condition.valid &&
                condition.typeKey != "builtin:bool" &&
                condition.typeKey != "builtin:never")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCondition,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "if condition must have type bool");
            }

            // CC1-200. Este case ya hacia lo correcto a mano; ahora lo hace a traves de las dos
            // funciones compartidas. El cambio no altera su comportamiento -- es literalmente el
            // mismo capture / restore(entry) / analizar / capture / restore(join) -- y ese es el
            // punto: `?:`, `&&` y `||` estaban rotos porque tenian que ACORDARSE de repetir este
            // patron, y no se acordaron. Reescribirlo aqui es lo que demuestra que la pieza
            // compartida es equivalente a la version que ya era correcta.
            const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
            OwnershipFlowSnapshot thenEntry = branchEntry;
            OwnershipFlowSnapshot elseEntry = branchEntry;
            if (const auto flow = conditionFlowBySyntax_.find(statement->first());
                flow != conditionFlowBySyntax_.end())
            {
                thenEntry = flow->second.whenTrue;
                elseEntry = flow->second.whenFalse;
            }
            const OwnershipFlowSnapshot thenState = analyzeBranchState(
                thenEntry,
                [&]
                {
                    analyzeBlock(
                        statement->second(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
                });
            const OwnershipFlowSnapshot elseState = analyzeBranchState(
                elseEntry,
                [&]
                {
                    if (statement->third() == nullptr) return;
                    if (statement->third()->kind() == ast::AstNodeKind::IfStatement)
                    {
                        analyzeStatement(
                            statement->third(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                    }
                    else
                    {
                        analyzeBlock(
                            statement->third(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
                    }
                });
            commitBranchJoin(thenState, elseState);
            break;
        }
        // CC1-210/CC1-211/CC1-212. Los dos bucles COMPARTEN este case, pero ya NO comparten su
        // semantica de flujo: las dos diferencias reales estan marcadas abajo con `isDoWhile`.
        //
        // Por que un solo case y no dos: lo unico que distingue a un do-while de un while son (a)
        // el orden entre cuerpo y condicion y (b) que NO existe el camino de cero iteraciones. Todo
        // lo demas -- el LoopContext, la union de los estados de `continue`, y sobre todo la
        // comprobacion de RAII a traves del back-edge, que son sesenta lineas -- es identico.
        // Duplicarlo daria dos copias de la parte dificil que habria que mantener en paso para
        // siempre, que es exactamente el problema que este milestone existe para reducir.
        //
        // El comentario anterior afirmaba que analizar un do-while como un while era "meramente
        // conservador". Para definite assignment eso era falso en un sentido: incluir el camino de
        // cero iteraciones en una INTERSECCION hacia que una variable asignada solo en el cuerpo NO
        // contara como definitivamente asignada despues del bucle -- rechazaba codigo valido. No era
        // inseguro, pero era incorrecto, y la correccion es esta.
        case ast::AstNodeKind::DoWhileStatement:
        case ast::AstNodeKind::WhileStatement:
        {
            const bool isDoWhile = statement->kind() == ast::AstNodeKind::DoWhileStatement;
            const bool conditionAlwaysTrue =
                statement->first() != nullptr &&
                statement->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
                statement->first()->tokenKind() == lexer::TokenKind::KwTrue;

            // La condicion se analiza EXACTAMENTE UNA VEZ, aqui para un while y despues del cuerpo
            // para un do-while. Una vez y no cero: aunque el punto en que se analiza sea
            // inalcanzable, el HIR va a recorrer esa sintaxis igual y necesita su binding de tipo.
            const auto analyzeLoopCondition = [&]()
            {
                const ExpressionResult condition = analyzeExpression(
                    statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (condition.valid &&
                    condition.typeKey != "builtin:bool" &&
                    condition.typeKey != "builtin:never")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidCondition,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        isDoWhile
                            ? "do-while condition must have type bool"
                            : "while condition must have type bool");
                }
            };

            if (!isDoWhile)
            {
                analyzeLoopCondition();
            }

            const OwnershipFlowSnapshot loopEntry = captureOwnershipFlow();
            const std::size_t localCountAtBodyEntry = locals_.size();
            const ClassMemberSymbol* loopMethod = methodSymbol(methodMemberOrdinal);
            loopStack_.push_back(LoopContext{
                scopeOrdinal,
                activeCleanupRegions_.size(),
                activeCleanupRegions_.size(),
                {},
                {},
                false,
                nullptr,
                {},
                false,
                {}});
            analyzeBlock(statement->second(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
            const OwnershipFlowSnapshot fallthroughState = captureOwnershipFlow();
            LoopContext loopContext = std::move(loopStack_.back());
            loopStack_.pop_back();

            std::vector<OwnershipFlowSnapshot> backEdgeStates = std::move(loopContext.continueStates);
            if (fallthroughState.reachable)
            {
                backEdgeStates.push_back(fallthroughState);
            }
            OwnershipFlowSnapshot noBackEdge;
            noBackEdge.reachable = false;
            const OwnershipFlowSnapshot backEdgeState = joinOwnershipFlows(noBackEdge, backEdgeStates);

            // CC1-212. En un do-while la condicion se evalua DESPUES del cuerpo, sobre el estado que
            // llega por el back-edge. Eso es lo que hace que la condicion pueda leer una variable
            // asignada dentro del cuerpo, que es legal y antes se rechazaba.
            OwnershipFlowSnapshot afterCondition;
            afterCondition.reachable = false;
            if (isDoWhile)
            {
                if (backEdgeState.reachable)
                {
                    restoreOwnershipFlow(backEdgeState);
                    analyzeLoopCondition();
                    afterCondition = captureOwnershipFlow();
                }
                else
                {
                    // Cuerpo que nunca cae ni continua (todas sus salidas retornan o lanzan). La
                    // condicion es codigo inalcanzable, pero el HIR va a recorrerla igual, asi que
                    // se analiza en una sonda alcanzable desechable -- el mismo patron que usa
                    // analyzeBlock para el codigo muerto -- y el estado terminado se restaura.
                    const OwnershipFlowSnapshot terminated = captureOwnershipFlow();
                    OwnershipFlowSnapshot probe = backEdgeState;
                    probe.reachable = true;
                    restoreOwnershipFlow(probe);
                    analyzeLoopCondition();
                    restoreOwnershipFlow(terminated);
                }
            }

            if (backEdgeState.reachable)
            {
                for (std::size_t localOrdinal = 0; localOrdinal < localCountAtBodyEntry; ++localOrdinal)
                {
                    if (localOrdinal >= locals_.size() ||
                        (!locals_[localOrdinal].isRaii && !locals_[localOrdinal].isMoveOnly))
                    {
                        continue;
                    }
                    const bool aliveAtEntry = !snapshotLocalHasAnyFlag(
                        loopEntry,
                        localOrdinal,
                        kLocalMoved | kLocalMaybeMoved);
                    const bool movedAtBackEdge = snapshotLocalHasAnyFlag(
                        backEdgeState,
                        localOrdinal,
                        kLocalMoved | kLocalMaybeMoved);
                    if (aliveAtEntry && movedAtBackEdge)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            statement->range(),
                            "RAII owner '" + locals_[localOrdinal].name +
                                "' may remain moved across a while-loop back-edge; reinitialize it before the next iteration");
                    }
                }
                if (loopMethod != nullptr)
                {
                    for (std::size_t parameterOrdinal = 0;
                         parameterOrdinal < loopMethod->parameters.size(); ++parameterOrdinal)
                    {
                        if (loopMethod->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned)
                        {
                            continue;
                        }
                        const bool aliveAtEntry = !snapshotParameterHasAnyFlag(
                            loopEntry,
                            parameterOrdinal,
                            kParameterMoved | kParameterMaybeMoved);
                        const bool movedAtBackEdge = snapshotParameterHasAnyFlag(
                            backEdgeState,
                            parameterOrdinal,
                            kParameterMoved | kParameterMaybeMoved);
                        if (aliveAtEntry && movedAtBackEdge)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                statement->range(),
                                "owned parameter '" + loopMethod->parameters[parameterOrdinal].name +
                                    "' may remain moved across a while-loop back-edge; transfer it only on an exiting path");
                        }
                    }
                }
                std::unordered_set<std::string> entryPartial = loopEntry.partiallyMovedPlaces;
                entryPartial.insert(
                    loopEntry.maybePartiallyMovedPlaces.begin(),
                    loopEntry.maybePartiallyMovedPlaces.end());
                std::unordered_set<std::string> backPartial = backEdgeState.partiallyMovedPlaces;
                backPartial.insert(
                    backEdgeState.maybePartiallyMovedPlaces.begin(),
                    backEdgeState.maybePartiallyMovedPlaces.end());
                for (const std::string& place : backPartial)
                {
                    if (entryPartial.contains(place)) continue;
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->range(),
                        "field path '" + place +
                            "' may remain moved across a while-loop back-edge; reinitialize it before the next iteration");
                }
            }

            // CC1-211. AQUI ESTA LA SEGUNDA DIFERENCIA, y es la que importa para definite
            // assignment: un while puede no ejecutar el cuerpo NI UNA VEZ, asi que su estado
            // posterior parte de `loopEntry`; un do-while SIEMPRE lo ejecuta, asi que ese camino no
            // existe y no debe entrar en el join. Como el join de un conjunto MUST es una
            // interseccion, incluir `loopEntry` en un do-while borraba toda asignacion hecha en el
            // cuerpo.
            OwnershipFlowSnapshot postLoop;
            if (isDoWhile)
            {
                postLoop.reachable = false;
                postLoop = joinOwnershipFlows(postLoop, loopContext.breakStates);
                if (afterCondition.reachable)
                {
                    postLoop = joinOwnershipFlow(postLoop, afterCondition);
                }
            }
            else
            {
                // T07-F04. `while (true)` has no zero-iteration/fallthrough edge. Its only
                // reachable successor is an explicit `break`; if none exists, the statement is
                // proven divergent. Ordinary while loops retain the conservative entry edge.
                postLoop.reachable = false;
                if (!conditionAlwaysTrue) postLoop = loopEntry;
                postLoop = joinOwnershipFlows(postLoop, loopContext.breakStates);
                if (!conditionAlwaysTrue && backEdgeState.reachable)
                {
                    postLoop = joinOwnershipFlow(postLoop, backEdgeState);
                }
            }
            if (isDoWhile && conditionAlwaysTrue)
            {
                OwnershipFlowSnapshot onlyBreaks;
                onlyBreaks.reachable = false;
                postLoop = joinOwnershipFlows(onlyBreaks, loopContext.breakStates);
            }
            restoreOwnershipFlow(postLoop);
            break;
        }
        case ast::AstNodeKind::ForStatement:
            analyzeFor(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::ForeachStatement:
            analyzeForeach(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::SwitchStatement:
            analyzeSwitch(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::ReturnStatement:
        {
            // NS304-T06-F02: a generator (Iterator<T> return) ends with `yield break`, not an
            // ordinary value return; reject `return v;` but allow the bare `return;`.
            const ClassMemberSymbol* returnMethod = methodSymbol(methodMemberOrdinal);
            if (statement->first() != nullptr && returnMethod != nullptr &&
                language::iteratorInnerTypeKey(returnMethod->declaredTypeKey).has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "a generator cannot return a value; use yield return or yield break (NS304-T06-F02)");
            }
            analyzeReturn(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            recordRequiredConstructorNormalExit(methodMemberOrdinal);
            ownershipFlowReachable_ = false;
            break;
        }
        case ast::AstNodeKind::TryStatement:
            analyzeTry(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::UsingStatement:
            analyzeUsing(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::ThrowStatement:
            analyzeThrow(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            ownershipFlowReachable_ = false;
            break;
        case ast::AstNodeKind::StaticAssertStatement:
        {
            const ExpressionResult condition = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (condition.valid &&
                condition.typeKey != "builtin:bool" &&
                condition.typeKey != "builtin:never")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCondition,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "static_assert condition must have type bool");
            }
            break;
        }
        case ast::AstNodeKind::InlineAssemblyStatement:
            for (ast::AstNode* child : statement->children())
            {
                if (child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand &&
                    child->first() != nullptr)
                {
                    const ExpressionResult input = analyzeExpression(
                        child->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                    if (child->auxiliaryText() == "i" && input.valid &&
                        (!ProjectTypeConversion::isIntegral(input.typeKey) ||
                         !input.integerConstant.has_value()))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm immediate input requires a compile-time integral constant");
                    }
                    else if (child->auxiliaryText() == "m" && input.valid && !input.addressable)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm memory input requires an addressable storage location");
                    }
                }
                else if (child->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand &&
                    child->first() != nullptr)
                {
                    const ast::AstNode* const previousWriteTarget = definiteAssignmentWriteTarget_;
                    markAssignmentTarget(child->first(), scopeOrdinal);
                    const ExpressionResult target = analyzeExpression(
                        child->first(),
                        methodMemberOrdinal,
                        scopeOrdinal,
                        diagnostics,
                        false,
                        true,
                        false,
                        true);
                    definiteAssignmentWriteTarget_ = previousWriteTarget;
                    if (target.valid && !target.assignable)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidAssignmentTarget,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm output must be a modifiable lvalue");
                    }
                    else if ((child->auxiliaryText() == "=m" ||
                              child->auxiliaryText() == "+m") &&
                        target.valid && !target.addressable)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm memory output requires an addressable storage location");
                    }
                }
            }
            // Item 298: the project pipeline now has an explicit HIR/MIR representation for the
            // complete validated contract. The standalone frontend keeps its pre-lowering
            // diagnostic until it acquires the same physical backend path.
            break;
        case ast::AstNodeKind::ExpressionStatement:
        {
            const ExpressionResult expression = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (expression.valid && expression.typeKey == "builtin:never")
                ownershipFlowReachable_ = false;
            break;
        }
        case ast::AstNodeKind::BreakStatement:
            if (analyzeLoopExit(
                    statement,
                    MethodStructuredExitKind::Break,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    diagnostics))
            {
                ownershipFlowReachable_ = false;
            }
            break;
        case ast::AstNodeKind::ContinueStatement:
            if (analyzeLoopExit(
                    statement,
                    MethodStructuredExitKind::Continue,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    diagnostics))
            {
                ownershipFlowReachable_ = false;
            }
            break;
        case ast::AstNodeKind::FunctionDeclaration:
            // NS304-T04-F06: el parser ya produce la funcion local, pero el registro de su nombre
            // en el scope y el analisis de su cuerpo (como callable) estan pendientes.
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "local functions are parsed but their callable registration is not yet "
                "implemented (NS304-T04-F06)");
            break;
        case ast::AstNodeKind::YieldReturnStatement:
        case ast::AstNodeKind::YieldBreakStatement:
        {
            // NS304-T06-F02: distinguish a yield inside a generator (declared `Iterator<T>` return)
            // from a yield in an ordinary callable (context error). The productive lowering to a
            // frame/state machine is still T06-F03.
            const ClassMemberSymbol* yieldMethod = methodSymbol(methodMemberOrdinal);
            auto itemInner = yieldMethod != nullptr
                ? language::iteratorInnerTypeKey(yieldMethod->declaredTypeKey)
                : std::optional<std::string_view>{};
            // NS304-T07-F02: an async generator (`AsyncIterator<T,E>`) also yields; the item is T.
            if (!itemInner.has_value() && yieldMethod != nullptr)
                itemInner = language::asyncIteratorInnerTypeKey(yieldMethod->declaredTypeKey);
            if (!itemInner.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "yield requires a generator whose return type is Iterator<T> (NS304-T06-F02)");
            }
            else
            {
                // NS304-T06-F02: unify every productive yield to the declared item type.
                if (statement->kind() == ast::AstNodeKind::YieldReturnStatement &&
                    statement->first() != nullptr)
                {
                    // NS304-T07-F04: a move-only item is transferred once via `move(x)`; the yield
                    // operand is a consuming context, so allow the canonical move.
                    const ExpressionResult yielded = analyzeExpression(
                        statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                        false, false, true);
                    if (yielded.valid)
                    {
                        // NS304-T06-F05: a `ref`/`ref mut` item (`ref:<T>`/`refmut:<T>`) yields a
                        // reference to a local; the value type is the inner `<T>`.
                        const std::string itemType(*itemInner);
                        const bool itemByRef = itemType.starts_with("ref:");
                        const bool itemByRefMut = itemType.starts_with("refmut:");
                        const std::string itemValueType = itemByRef
                            ? itemType.substr(4)
                            : itemByRefMut ? itemType.substr(7) : itemType;
                        if (yielded.typeKey != itemValueType)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                statement->first()->range(),
                                "generator yields '" + yielded.typeKey +
                                    "' but its item type is '" + std::string(*itemInner) + "' (NS304-T06-F02)");
                        }
                    }
                }
                // NS304-T06-F03: the yield is valid; the productive state-machine lowering is the
                // HIR split (splitGeneratorFunction), no longer a semantic rejection.
            }
            break;
        }
        case ast::AstNodeKind::SuspendStatement:
        {
            // NS304-T05-F09: a bare `yield;` is an explicit suspension point of the async poll.
            const ClassMemberSymbol* suspendMethod = methodSymbol(methodMemberOrdinal);
            if (suspendMethod == nullptr ||
                !suspendMethod->effects.contains(sema::Effect::Async))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "yield requires an async function (NS304-T05-F09)");
            }
            else
            {
                // Record the suspension point so F05 (borrow/pin across suspension) sees it too.
                awaitExpressions_.push_back(statement);
            }
            break;
        }
        default:
            break;
    }
}

std::optional<std::size_t> ProjectMethodBodyModel::exceptionRootTypeOrdinal() const noexcept
{
    if (nameLookup_ == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if (type.kind == NamedTypeDeclarationKind::Class &&
            type.identity.fullyQualifiedName() == "NativeSharp.Core.Exception")
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::isExceptionTypeOrdinal(std::size_t typeOrdinal) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return false;
    }
    const std::optional<std::size_t> root = exceptionRootTypeOrdinal();
    return root.has_value() && inheritanceModel_->isClass(typeOrdinal) &&
        inheritanceModel_->isDerivedFrom(typeOrdinal, *root, true);
}

std::size_t ProjectMethodBodyModel::addCatchLocal(
    const ast::AstNode* catchClause,
    std::string typeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (catchClause == nullptr || scopeOrdinal >= scopeNames_.size())
    {
        return static_cast<std::size_t>(-1);
    }
    const std::optional<ScopeName> sameScope = lookupScopeNameInScope(scopeOrdinal, catchClause->text());
    if (sameScope.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            catchClause->range(),
            "duplicate catch/local name '" + catchClause->text() + "' in the same scope");
        reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
        return static_cast<std::size_t>(-1);
    }

    MethodLocalSymbol local;
    local.methodMemberOrdinal = methodMemberOrdinal;
    local.scopeOrdinal = scopeOrdinal;
    local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
    if (const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal); method != nullptr)
    {
        local.fileId = method->fileId;
    }
    local.range = catchClause->range();
    local.syntax = catchClause;
    local.name = catchClause->text();
    local.typeKey = std::move(typeKey);
    local.lambdaOrdinal = currentLambdaOrdinal_;

    const std::size_t localOrdinal = locals_.size();
    locals_.push_back(std::move(local));
    scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
    scopeNames_[scopeOrdinal].push_back(ScopeName{
        ScopeNameKind::Local, catchClause->text(), localOrdinal, catchClause->range()});
    // C7: the exception object is bound by the handler itself, never by user code.
    assignedLocals_.insert(localOrdinal);
    return localOrdinal;
}

std::optional<std::size_t> ProjectMethodBodyModel::disposableTypeOrdinal() const noexcept
{
    if (nameLookup_ == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if (type.kind == NamedTypeDeclarationKind::Interface &&
            type.identity.fullyQualifiedName() == "NativeSharp.Core.IDisposable")
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectMethodBodyModel::disposableContractMemberOrdinal(
    std::size_t disposableTypeOrdinalValue) const noexcept
{
    if (interfaceModel_ == nullptr || memberModel_ == nullptr)
    {
        return std::nullopt;
    }
    const InterfaceTypeInfo* info = interfaceModel_->interfaceInfo(disposableTypeOrdinalValue);
    if (info == nullptr)
    {
        return std::nullopt;
    }

    std::optional<std::size_t> result;
    for (const std::size_t memberOrdinal : info->effectiveMethodMemberOrdinals)
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Method || member.name != "Dispose" ||
            !member.parameters.empty() || member.declaredTypeKey != "builtin:void" || member.isStatic)
        {
            continue;
        }
        if (result.has_value())
        {
            return std::nullopt;
        }
        result = memberOrdinal;
    }
    return result;
}

std::size_t ProjectMethodBodyModel::addUsingLocal(
    const ast::AstNode* statement,
    std::string typeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr || scopeOrdinal >= scopeNames_.size())
    {
        return static_cast<std::size_t>(-1);
    }
    const std::optional<ScopeName> sameScope = lookupScopeNameInScope(scopeOrdinal, statement->text());
    if (sameScope.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "duplicate using/local name '" + statement->text() + "' in the same scope");
        reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
        return static_cast<std::size_t>(-1);
    }

    MethodLocalSymbol local;
    local.methodMemberOrdinal = methodMemberOrdinal;
    local.scopeOrdinal = scopeOrdinal;
    local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
    if (const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal); method != nullptr)
    {
        local.fileId = method->fileId;
    }
    local.range = statement->range();
    local.syntax = statement;
    local.name = statement->text();
    local.typeKey = std::move(typeKey);
    local.lambdaOrdinal = currentLambdaOrdinal_;

    const std::optional<ScopeName> outer = lookupScopeName(scopeOrdinal, statement->text());
    if (outer.has_value())
    {
        MethodShadowingKind kind = MethodShadowingKind::LocalShadowsLocal;
        if (outer->kind == ScopeNameKind::Parameter)
        {
            kind = MethodShadowingKind::LocalShadowsParameter;
        }
        shadowingEvents_.push_back(MethodShadowingEvent{
            kind,
            methodMemberOrdinal,
            scopeOrdinal,
            statement->text(),
            statement->range()});
    }
    else if (!memberCandidates(methodMemberOrdinal, statement->text()).empty())
    {
        shadowingEvents_.push_back(MethodShadowingEvent{
            MethodShadowingKind::LocalShadowsMember,
            methodMemberOrdinal,
            scopeOrdinal,
            statement->text(),
            statement->range()});
    }

    const std::size_t localOrdinal = locals_.size();
    locals_.push_back(std::move(local));
    scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
    scopeNames_[scopeOrdinal].push_back(ScopeName{
        ScopeNameKind::Local, statement->text(), localOrdinal, statement->range()});
    // C7: the foreach element is bound by the loop itself, never by user code.
    assignedLocals_.insert(localOrdinal);
    return localOrdinal;
}

void ProjectMethodBodyModel::analyzeUsing(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        inheritanceModel_ == nullptr || interfaceModel_ == nullptr)
    {
        return;
    }

    const std::size_t usingScope = createScope(
        MethodScopeKind::Using,
        methodMemberOrdinal,
        scopeOrdinal);
    const std::string resourceTypeKey = memberModel_->canonicalTypeKey(statement->typeSyntax(), *nameLookup_);

    ExpressionResult initializer;
    if (statement->first() != nullptr)
    {
        initializer = analyzeExpression(
            statement->first(), methodMemberOrdinal, usingScope, diagnostics);
        if (initializer.valid)
        {
            const ProjectImplicitConversion conversion = classifyImplicit(
                initializer.typeKey,
                resourceTypeKey,
                initializer.integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "cannot initialize using resource '" + statement->text() + "' of type '" +
                        resourceTypeKey + "' from '" + initializer.typeKey + "'");
            }
        }
    }

    const std::size_t localOrdinal = addUsingLocal(
        statement, resourceTypeKey, methodMemberOrdinal, usingScope, diagnostics);

    const std::optional<std::size_t> disposableOrdinal = disposableTypeOrdinal();
    if (!disposableOrdinal.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "NativeSharp.Core.IDisposable must be declared as an interface before using semantics can be used");
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    const std::optional<std::size_t> disposeContract = disposableContractMemberOrdinal(*disposableOrdinal);
    if (!disposeContract.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "NativeSharp.Core.IDisposable must declare exactly one instance 'void Dispose()' contract");
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    const std::optional<std::size_t> resourceTypeOrdinal =
        inheritanceModel_->typeOrdinalForTypeKey(resourceTypeKey);
    bool disposable = false;
    bool interfaceDispatch = false;
    std::optional<std::size_t> implementation;
    if (resourceTypeOrdinal.has_value() && *resourceTypeOrdinal < nameLookup_->namedTypes().size())
    {
        const NamedTypeDeclarationKind kind = nameLookup_->namedTypes()[*resourceTypeOrdinal].kind;
        if (kind == NamedTypeDeclarationKind::Class)
        {
            disposable = interfaceModel_->classImplementsInterface(*resourceTypeOrdinal, *disposableOrdinal);
            if (disposable)
            {
                const ClassInterfaceTablePlan* plan = interfaceModel_->tablePlan(
                    *resourceTypeOrdinal, *disposableOrdinal);
                if (plan != nullptr)
                {
                    for (std::size_t index = 0; index < plan->interfaceMethodMemberOrdinals.size(); ++index)
                    {
                        if (plan->interfaceMethodMemberOrdinals[index] == *disposeContract &&
                            index < plan->implementationMemberOrdinals.size())
                        {
                            implementation = plan->implementationMemberOrdinals[index];
                            break;
                        }
                    }
                }
                interfaceDispatch = !implementation.has_value();
            }
        }
        else if (kind == NamedTypeDeclarationKind::Interface)
        {
            disposable = interfaceModel_->interfaceDerivesFrom(
                *resourceTypeOrdinal, *disposableOrdinal, true);
            interfaceDispatch = disposable;
        }
    }

    if (!disposable)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "using resource type '" + resourceTypeKey +
                "' must implement NativeSharp.Core.IDisposable and provide instance void Dispose()");
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    if (localOrdinal == static_cast<std::size_t>(-1) || !resourceTypeOrdinal.has_value())
    {
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    MethodUsingBinding binding;
    binding.syntax = statement;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = usingScope;
    binding.resourceLocalOrdinal = localOrdinal;
    binding.resourceTypeKey = resourceTypeKey;
    binding.resourceTypeOrdinal = *resourceTypeOrdinal;
    binding.disposableTypeOrdinal = *disposableOrdinal;
    binding.disposeContractMemberOrdinal = *disposeContract;
    binding.disposeImplementationMemberOrdinal = implementation;
    binding.interfaceDispatch = interfaceDispatch;
    usingBindings_.push_back(binding);

    const std::size_t cleanupRegionOrdinal = createCleanupRegion(
        MethodCleanupRegionKind::UsingDispose,
        statement,
        nullptr,
        methodMemberOrdinal,
        usingScope);
    MethodCleanupRegion& region = cleanupRegions_[cleanupRegionOrdinal];
    region.resourceLocalOrdinal = localOrdinal;
    region.disposeContractMemberOrdinal = *disposeContract;
    region.disposeImplementationMemberOrdinal = implementation;
    recordNormalCleanupContinuation(
        cleanupRegionOrdinal, statement, methodMemberOrdinal, usingScope);

    activeCleanupRegions_.push_back(cleanupRegionOrdinal);
    if (statement->second() != nullptr)
    {
        analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
    }
    activeCleanupRegions_.pop_back();
}

void ProjectMethodBodyModel::analyzeTry(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr)
    {
        return;
    }

    if (statement->children().empty() && statement->second() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "try statement requires at least one catch clause or a finally block");
    }

    std::optional<std::size_t> cleanupRegionOrdinal;
    if (statement->second() != nullptr)
    {
        cleanupRegionOrdinal = createCleanupRegion(
            MethodCleanupRegionKind::Finally,
            statement,
            statement->second(),
            methodMemberOrdinal,
            scopeOrdinal);
        recordNormalCleanupContinuation(
            *cleanupRegionOrdinal, statement, methodMemberOrdinal, scopeOrdinal);
        activeCleanupRegions_.push_back(*cleanupRegionOrdinal);
    }

    const OwnershipFlowSnapshot tryEntryState = captureOwnershipFlow();
    const std::size_t throwStateBegin = throwFlowStates_.size();
    analyzeBlock(statement->first(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
    const OwnershipFlowSnapshot tryExitState = captureOwnershipFlow();

    // A catch can observe ownership state from any explicit throw point and, conservatively,
    // from both try entry and normal try exit to account for other potentially-throwing calls.
    OwnershipFlowSnapshot catchEntryState = joinOwnershipFlow(tryEntryState, tryExitState);
    for (std::size_t index = throwStateBegin; index < throwFlowStates_.size(); ++index)
    {
        catchEntryState = joinOwnershipFlow(catchEntryState, throwFlowStates_[index]);
    }
    OwnershipFlowSnapshot joinedExitState = tryExitState;

    const std::optional<std::size_t> exceptionRoot = exceptionRootTypeOrdinal();
    bool sawCatchAll = false;
    std::vector<std::size_t> previousTypedCatchOrdinals;
    for (const ast::AstNode* catchClause : statement->children())
    {
        if (catchClause == nullptr || catchClause->kind() != ast::AstNodeKind::CatchClause)
        {
            continue;
        }
        const std::size_t catchScope = createScope(MethodScopeKind::Catch, methodMemberOrdinal, scopeOrdinal);
        MethodExceptionBinding binding;
        binding.kind = MethodExceptionOperationKind::Catch;
        binding.syntax = catchClause;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = catchScope;
        binding.catchAll = catchClause->flag();
        binding.hasFilter = catchClause->second() != nullptr;
        binding.filterAlwaysTrue = !binding.hasFilter ||
            (catchClause->second()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
             catchClause->second()->tokenKind() == lexer::TokenKind::KwTrue);

        if (sawCatchAll)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                catchClause->range(),
                "catch clause is unreachable because a previous catch-all handles every exception");
        }

        if (catchClause->flag())
        {
            if (binding.filterAlwaysTrue) sawCatchAll = true;
            binding.exceptionTypeKey = "<any-exception>";
        }
        else
        {
            const std::string typeKey = memberModel_->canonicalTypeKey(catchClause->typeSyntax(), *nameLookup_);
            binding.exceptionTypeKey = typeKey;
            const std::optional<std::size_t> typeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
            binding.exceptionTypeOrdinal = typeOrdinal;

            if (!exceptionRoot.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    catchClause->range(),
                    "NativeSharp.Core.Exception must be declared before typed catch/throw semantics can be used");
            }
            else if (!typeOrdinal.has_value() || !isExceptionTypeOrdinal(*typeOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    catchClause->range(),
                    "catch type '" + typeKey + "' must be NativeSharp.Core.Exception or derive from it");
            }
            else
            {
                for (const std::size_t previous : previousTypedCatchOrdinals)
                {
                    if (inheritanceModel_->isDerivedFrom(*typeOrdinal, previous, true))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            catchClause->range(),
                            "catch type '" + typeKey + "' is unreachable because an earlier catch handles the same or a base exception type");
                        break;
                    }
                }
                if (binding.filterAlwaysTrue)
                    previousTypedCatchOrdinals.push_back(*typeOrdinal);
            }
            if (!catchClause->text().empty())
            {
                addCatchLocal(catchClause, typeKey, methodMemberOrdinal, catchScope, diagnostics);
            }
        }

        restoreOwnershipFlow(catchEntryState);
        if (binding.hasFilter)
        {
            const ExpressionResult filter = analyzeExpression(
                catchClause->second(), methodMemberOrdinal, catchScope, diagnostics);
            if (filter.valid &&
                filter.typeKey != "builtin:bool" &&
                filter.typeKey != "builtin:never")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCondition,
                    diagnostics::DiagnosticSeverity::Error,
                    catchClause->second()->range(),
                    "catch filter expression must have type bool");
            }
            if (catchClause->second()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
                catchClause->second()->tokenKind() == lexer::TokenKind::KwFalse)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    catchClause->second()->range(),
                    "catch filter is unreachable because its condition is always false");
            }
        }

        exceptionBindings_.push_back(std::move(binding));
        ++currentCatchDepth_;
        currentCatchErrorTypes_.push_back(catchClause->flag()
            ? (exceptionRoot.has_value()
                ? inheritanceModel_->typeKeyForOrdinal(*exceptionRoot)
                : std::string{"<any-exception>"})
            : memberModel_->canonicalTypeKey(catchClause->typeSyntax(), *nameLookup_));
        analyzeBlock(catchClause->first(), methodMemberOrdinal, catchScope, false, diagnostics);
        currentCatchErrorTypes_.pop_back();
        --currentCatchDepth_;
        joinedExitState = joinOwnershipFlow(joinedExitState, captureOwnershipFlow());
    }

    restoreOwnershipFlow(joinedExitState);
    if (cleanupRegionOrdinal.has_value())
    {
        activeCleanupRegions_.pop_back();
    }

    if (statement->second() != nullptr)
    {
        const OwnershipFlowSnapshot postTryState = captureOwnershipFlow();
        OwnershipFlowSnapshot finallyInput = postTryState;
        if (!finallyInput.reachable)
        {
            finallyInput = catchEntryState;
            finallyInput.reachable = true;
        }
        restoreOwnershipFlow(finallyInput);
        finallyStack_.push_back(FinallyContext{
            *cleanupRegionOrdinal,
            loopStack_.size()});
        analyzeBlock(statement->second(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
        finallyStack_.pop_back();
        const OwnershipFlowSnapshot finalState = captureOwnershipFlow();
        if (postTryState.reachable)
        {
            restoreOwnershipFlow(finalState);
        }
        else
        {
            restoreOwnershipFlow(postTryState);
        }
    }
}

void ProjectMethodBodyModel::analyzeThrow(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr)
    {
        return;
    }

    MethodExceptionBinding binding;
    binding.syntax = statement;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;

    if (statement->first() == nullptr)
    {
        binding.kind = MethodExceptionOperationKind::Rethrow;
        if (currentCatchDepth_ == 0)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "throw; is only valid within a catch block");
        }
        else if (!currentCatchErrorTypes_.empty())
        {
            binding.exceptionTypeKey = currentCatchErrorTypes_.back();
        }
        exceptionBindings_.push_back(std::move(binding));
        throwFlowStates_.push_back(captureOwnershipFlow());
        return;
    }

    binding.kind = MethodExceptionOperationKind::Throw;
    const ExpressionResult value = analyzeExpression(
        statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (value.valid)
    {
        binding.exceptionTypeKey = value.typeKey;
        if (inheritanceModel_ != nullptr)
        {
            const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(value.typeKey);
            binding.exceptionTypeOrdinal = ordinal;
            if (!exceptionRootTypeOrdinal().has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "NativeSharp.Core.Exception must be declared before throw semantics can be used");
            }
            else if (!ordinal.has_value() || !isExceptionTypeOrdinal(*ordinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "throw operand type '" + value.typeKey + "' must be NativeSharp.Core.Exception or derive from it");
            }
        }
    }
    exceptionBindings_.push_back(std::move(binding));
    throwFlowStates_.push_back(captureOwnershipFlow());
}

std::string ProjectMethodBodyModel::delegateErrorTypeKey(
    std::string_view delegateTypeKey) const
{
    if (const std::optional<language::AnonymousLambdaType> lambdaType =
            language::parseAnonymousLambdaTypeIdentity(delegateTypeKey))
        return lambdaType->signature.errorType;
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(delegateTypeKey))
        return functionType->errorType;
    const std::optional<std::size_t> ordinal =
        delegateTypeOrdinalForTypeKey(delegateTypeKey);
    if (!ordinal.has_value() || nameLookup_ == nullptr || memberModel_ == nullptr ||
        *ordinal >= nameLookup_->namedTypes().size())
        return {};
    const ast::AstNode* syntax = nameLookup_->namedTypes()[*ordinal].syntax;
    return syntax == nullptr || syntax->throwsTypeSyntax() == nullptr
        ? std::string{}
        : memberModel_->canonicalTypeKey(syntax->throwsTypeSyntax(), *nameLookup_);
}

void ProjectMethodBodyModel::analyzeVariable(
    const ast::AstNode* variable,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (variable == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        scopeOrdinal >= scopeNames_.size())
    {
        return;
    }

    const bool isPinnedReferenceLocal = variable->auxiliaryText() == "local:pinned-ref";
    const bool isReadonlyForeachReference =
        variable->auxiliaryText() == "local:foreach-ref";
    const bool isMutableForeachReference =
        variable->auxiliaryText() == "local:foreach-ref-mut";
    const bool isReadonlyPatternReference =
        variable->auxiliaryText() == "local:pattern-ref";
    const bool isMutablePatternReference =
        variable->auxiliaryText() == "local:pattern-ref-mut";
    const bool isReferenceLocal =
        variable->auxiliaryText() == "local:ref" || isPinnedReferenceLocal ||
        isReadonlyForeachReference || isMutableForeachReference ||
        isReadonlyPatternReference || isMutablePatternReference;
    const bool isReadonlyReference =
        isReadonlyForeachReference || isReadonlyPatternReference;
    const bool isPinnedLocal =
        variable->auxiliaryText() == "local:pinned" || isPinnedReferenceLocal;
    const bool isDeferValueCapture =
        variable->auxiliaryText() == "local:defer-value-capture";
    const bool isForeachEnumerator =
        variable->auxiliaryText() == "local:foreach-enumerator";
    const bool isStaticLocal = variable->hasModifier(TokenKind::KwStatic);

    // `var`: the parser bypasses parseType() entirely for this keyword and hands us a marker
    // TypeSyntax (baseTokenKind() == TokenKind::KwVar) instead -- see Parser::parseVariableDeclaration.
    // Infer the real typeKey from the initializer's own resolved type, analyzed here (before any
    // of the diagnostics below, which all need a real typeKey) with no delegate-target-typing --
    // matching real C#'s restriction that `var` cannot bind a bare lambda or method group, since
    // there is no declared delegate type to bind it against. The resulting ExpressionResult is
    // reused later in this function instead of being re-analyzed.
    const bool isVarInferred =
        variable->typeSyntax() != nullptr && variable->typeSyntax()->baseTokenKind() == TokenKind::KwVar;
    std::optional<ExpressionResult> precomputedVarInitializer;
    std::string varInferredTypeKey;
    if (isVarInferred)
    {
        if (variable->first() == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                variable->range(),
                "'var' requires an initializer");
            return;
        }
        // Pattern lowering is the one compiler-owned producer of `ref var`: it cannot spell the
        // element type before semantic analysis, but it still passes through the ordinary exact-
        // type, mutable-address and escape checks below. Source declarations continue to require
        // `ref <Type> name = ref storage` in the parser, so this inference does not widen syntax.
        //
        // C7 "aggregate initializer": a braced initializer is filled AGAINST a declared type; it
        // has no type of its own to infer from, exactly like a bare lambda or a method group.
        if (variable->first()->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "'var' cannot infer a type from an aggregate initializer; name the struct or "
                "fixed-array type");
            return;
        }
        ExpressionResult varInit = analyzeExpression(
            variable->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            /*allowMethodGroup=*/true, /*allowEvent=*/false, /*allowMove=*/true);
        if (!varInit.valid)
        {
            return;
        }
        // NS304-T04-F07: tipo natural de method group. Un overload UNICO infiere su `fn*`. El
        // typeKey de un method group esta vacio, asi que se comprueba ANTES del chequeo de tipo.
        if (varInit.methodGroup)
        {
            if (varInit.methodCandidates.size() != 1U)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "a method group with multiple overloads cannot infer a natural callable type; "
                    "use an explicit fn* target");
                return;
            }
            const ClassMemberSymbol* candidate = methodSymbol(varInit.methodCandidates[0]);
            if (candidate == nullptr || candidate->kind != ClassMemberKind::Method) return;
            language::FunctionType signature;
            if (candidate->hasImplicitThis) signature.parameters.emplace_back("abi:this");
            for (const MethodParameterSymbol& parameter : candidate->parameters)
                signature.parameters.push_back(parameter.typeKey);
            signature.result = candidate->declaredTypeKey;
            signature.carrier = language::FunctionCarrier::Pointer;
            signature.callingConvention = candidate->isExternC
                ? language::CallingConvention::C : language::CallingConvention::NativeSharp;
            signature.errorType = candidate->declaredErrorTypeKey;
            signature.lifetime = candidate->returnLifetimeName;
            signature.variadic = candidate->isVariadic;
            signature.isUnsafe = candidate->isUnsafe;
            signature.isNoHeap = candidate->isNoHeap;
            varInferredTypeKey = language::functionTypeIdentity(signature);
            precomputedVarInitializer = std::move(varInit);
        }
        else
        {
            if (varInit.typeKey.empty() || varInit.typeKey == "<error>")
            {
                return;
            }
            if (varInit.typeKey == "builtin:void")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "'var' cannot infer a type from a void-returning expression");
                return;
            }
            varInferredTypeKey = varInit.typeKey;
            precomputedVarInitializer = std::move(varInit);
        }
    }

    bool isVariableLengthArray = false;
    std::string variableLengthArrayElementTypeKey;
    const ast::AstNode* variableLengthArrayCountSyntax = nullptr;
    std::vector<const ast::AstNode*> variableLengthArrayExtentSyntaxes;
    std::vector<std::string> variableLengthArrayExtentTypeKeys;

    // NativeSharp 2.0 item 269 -- a non-constant array extent is meaningful only on a local
    // declaration: `T[length] values;` reserves raw, contiguous storage in the current stack
    // frame.  Constant extents retain the fixed inline-array semantics implemented by item 267.
    //
    // Internally the local is pointer-shaped because the existing raw-indexing and stackalloc
    // pipeline already define precisely the ABI we need.  The explicit VLA marker kept on the
    // local prevents this representation choice from leaking into the source grammar or from
    // enabling variable extents on fields and callable signatures.
    if (!isVarInferred && variable->typeSyntax() != nullptr)
    {
        const auto& modifiers = variable->typeSyntax()->modifiers();
        if (std::ranges::any_of(modifiers, [](const ast::TypeModifierSyntax& modifier) {
                return modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray;
            }))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                variable->typeSyntax()->range(),
                "a flexible array is incomplete trailing struct storage and cannot be a local variable");
            return;
        }
        std::size_t fixedArrayModifierCount = 0;
        bool hasRuntimeExtent = false;
        for (const ast::TypeModifierSyntax& modifier : modifiers)
        {
            if (modifier.kind != ast::TypeModifierSyntaxKind::Array) continue;
            ++fixedArrayModifierCount;
            if (!ProjectMemberModel::evaluateConstantArrayCount(modifier.arrayCount).has_value())
                hasRuntimeExtent = true;
        }

        if (hasRuntimeExtent)
        {
            const bool supportedShape = fixedArrayModifierCount != 0 &&
                modifiers.size() >= fixedArrayModifierCount &&
                std::all_of(
                    modifiers.end() - static_cast<std::ptrdiff_t>(fixedArrayModifierCount),
                    modifiers.end(),
                    [](const ast::TypeModifierSyntax& modifier)
                    {
                        return modifier.kind == ast::TypeModifierSyntaxKind::Array;
                    });
            if (!supportedShape)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidArrayCount,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->typeSyntax()->range(),
                    "a local variable-length array requires its runtime dimensions to be the "
                    "outermost type modifiers");
            }
            else
            {
                const std::string declaredKey =
                    memberModel_->canonicalTypeKey(variable->typeSyntax(), *nameLookup_);
                std::size_t elementEnd = declaredKey.size();
                bool completeSuffix = true;
                for (std::size_t dimension = 0; dimension < fixedArrayModifierCount; ++dimension)
                {
                    const std::size_t extentStart = declaredKey.rfind('[', elementEnd - 1);
                    if (extentStart == std::string::npos)
                    {
                        completeSuffix = false;
                        break;
                    }
                    elementEnd = extentStart;
                }
                if (!completeSuffix)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        variable->typeSyntax()->range(),
                        "cannot determine the element type of the variable-length array");
                }
                else
                {
                    variableLengthArrayElementTypeKey = declaredKey.substr(0, elementEnd);
                    // Parser stores fixed-array modifiers inside-out so canonical layout is
                    // nested correctly. Reverse them back here: runtime evaluation and row-major
                    // indexing must follow the dimensions as the programmer wrote them.
                    for (auto modifier = modifiers.rbegin(); modifier != modifiers.rend(); ++modifier)
                    {
                        if (modifier->kind != ast::TypeModifierSyntaxKind::Array) break;
                        variableLengthArrayExtentSyntaxes.push_back(modifier->arrayCount);
                    }
                    variableLengthArrayCountSyntax = variableLengthArrayExtentSyntaxes.front();
                    isVariableLengthArray = true;

                    for (const ast::AstNode* extentSyntax : variableLengthArrayExtentSyntaxes)
                    {
                        const ExpressionResult count = analyzeExpression(
                            extentSyntax, methodMemberOrdinal, scopeOrdinal, diagnostics);
                        variableLengthArrayExtentTypeKeys.push_back(count.typeKey);
                        if (!count.valid || !ProjectTypeConversion::isIntegral(count.typeKey) ||
                            count.typeKey == "builtin:bool")
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidArrayCount,
                                diagnostics::DiagnosticSeverity::Error,
                                extentSyntax != nullptr ? extentSyntax->range() : variable->range(),
                                "every variable-length array dimension must have an integral type");
                        }
                    }
                    if (variableLengthArrayElementTypeKey == "builtin:void")
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidStackalloc,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->typeSyntax()->range(),
                            "a variable-length array requires an element type with a size; void has none");
                    }
                    else if (!isErrorType(variableLengthArrayElementTypeKey) &&
                        !elementTypeHasTrivialLifetime(variableLengthArrayElementTypeKey))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidStackalloc,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->typeSyntax()->range(),
                            "a variable-length array requires an element type with no destructor "
                            "and no owned fields; '" + variableLengthArrayElementTypeKey +
                                "' owns a lifetime that raw stack storage cannot discharge");
                    }
                    if (variable->first() != nullptr)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->first()->range(),
                            "a variable-length array declaration cannot have an initializer; "
                            "initialize its elements explicitly");
                    }
                }
            }
        }
    }

    if (isStaticLocal && isReferenceLocal)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "static local cannot be a reference local because its referent may not outlive the module");
    }
    if (isStaticLocal && isVariableLengthArray)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "static local cannot be a variable-length array because module storage requires a fixed extent");
    }

    const std::string typeKey = isVarInferred
        ? varInferredTypeKey
        : (isVariableLengthArray
            ? variableLengthArrayElementTypeKey + '*'
            : memberModel_->canonicalTypeKey(variable->typeSyntax(), *nameLookup_));
    const sema::OwnershipKind declaredOwnership = ownershipKindFromSyntax(variable->typeSyntax());
    const bool isBorrowed = declaredOwnership == sema::OwnershipKind::Borrowed;
    const bool isMoveOnly = memberModel_->typeIsMoveOnly(typeKey);
    // Bloque 1 item 1/6/7 fix -- `owned T x = ...` declares an OWNING local, exactly like
    // `raii T x = ...`. Before this, the `owned` qualifier on a local was computed here (into
    // `declaredOwnership`) and then silently DISCARDED: only the Borrowed case was ever consulted,
    // so an `owned` local parsed, type-checked, compiled and ran while being treated as an
    // ordinary non-owning binding. Found by real execution, not by reading IR
    // (tests/unit/Stage50_11OwnershipExecutionTests.cpp): the object it owned was never destroyed
    // -- a silent leak of a declared language feature -- it could not be used as a `move(...)`
    // source (so the project's own Stage50_1 syntax sample
    // `owned Resource local = new Resource(); return move(local);` parsed but was rejected by
    // semantic analysis), and copying it into a second owner was accepted, which is a
    // double-ownership hole the equivalent `raii` code has always rejected.
    //
    // Routing it through the same `isRaii` flag is what makes all of that correct at once: that one
    // flag is what opens the cleanup region (destruction at scope exit), what admits a local as a
    // move source, what enables the copy-owner rejection, and what drives moved-from tracking.
    //
    // Scope of the `owned` spelling, deliberately narrower than `raii`'s: it makes the local an
    // owner only for the types that actually own a separate allocation -- a class/interface
    // reference or a native string -- and stays the no-op it always was for everything else. That
    // is not a shortcut, it is the SAME rule `owned` PARAMETERS already follow (see
    // requiresOwnedDestruction and emitOwnedParameterDestroy: "a value type owns no separate heap
    // allocation for `owned` to release"). Making the spellings disagree is what would be
    // surprising.
    //
    // It also keeps working code working. `owned T[] scratch = new T[1]` is an established idiom in
    // this project's own library (LinkedList<T>.AddFirst's real shape, covered by
    // Stage52GenericCollectionsCompilerFixesTests fix 10) used purely as a relocation buffer. An
    // array does not currently destroy its elements at all, so treating that local as an owner
    // would either reject the idiom outright or promise a destruction traversal the compiler does
    // not yet perform -- both worse than leaving it exactly as it was.
    const bool isOwnedQualified = declaredOwnership == sema::OwnershipKind::Owned;
    const bool managedDelegate = isAnyDelegateTypeKey(typeKey) && !isExternCDelegateTypeKey(typeKey);
    const bool ownedQualifierOwnsStorage =
        isOwnedQualified && (isReferenceTypeKey(typeKey) || typeKey == "builtin:string" || managedDelegate);
    // A foreach enumerator has an automatic lifetime whose representation follows the inferred
    // GetEnumerator() result. Reference-shaped results retain the established RAII cleanup path;
    // value-shaped results are ordinary stack locals and therefore allocate nothing.
    // NS304-T06-F07: an iterator enumerator is RAII too (it carries a Drop function pointer).
    const bool foreachEnumeratorNeedsRaii = isForeachEnumerator &&
        (isReferenceTypeKey(typeKey) || typeKey == "builtin:string" || managedDelegate ||
         typeKey.starts_with("iterator:") || typeKey.starts_with("async-iterator:"));
    const bool deferCaptureNeedsRaii = isDeferValueCapture &&
        (isReferenceTypeKey(typeKey) || typeKey == "builtin:string" || managedDelegate);
    const bool isRaii = variable->hasModifier(TokenKind::KwRaii) || ownedQualifierOwnsStorage ||
        foreachEnumeratorNeedsRaii || deferCaptureNeedsRaii;
    // NS304-T13-F03: `scope` forces frame storage (never heap).
    const bool isScope = variable->auxiliaryText() == "local:scope";
    const bool isVolatile = variable->hasModifier(TokenKind::KwVolatile);
    // Wording helper: every owner diagnostic below has to name the spelling the user actually
    // wrote, otherwise an `owned` local reports errors about "raii local" and sends them looking at
    // the wrong keyword.
    const char* const ownerSpelling = ownedQualifierOwnsStorage
        ? "owned local"
        : (isForeachEnumerator ? "foreach enumerator"
            : (isDeferValueCapture ? "defer value capture" : "raii local"));
    std::optional<MethodLocalBorrowBinding> pendingLocalBorrow;
    MethodEscapeOrigin pendingReferenceEscapeOrigin;
    std::string pendingReferencePlaceKey;
    std::optional<std::size_t> pendingReferenceReborrowSourceLocalOrdinal;
    const bool raiiOwnsNativeArrayBuffer =
        variable->hasModifier(TokenKind::KwRaii) && ProjectTypeConversion::isNativeArray(typeKey);
    // T41: `raii borrowed T[]` owns the allocation that stores the slots, while every T slot is
    // an alias. The two keywords are not contradictory for an array: `raii` qualifies the
    // buffer and `borrowed` qualifies its elements. For non-arrays the old contradiction remains.
    const bool raiiBorrowedElementArray =
        raiiOwnsNativeArrayBuffer && isBorrowed;
    if (isVolatile &&
        (!ProjectTypeConversion::isNumeric(typeKey) &&
         !ProjectTypeConversion::isPointer(typeKey) && !isEnumTypeKey(typeKey)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "volatile local must be a native scalar, enum, or raw pointer; volatile does not define aggregate-copy or managed-reference semantics");
    }
    if (isVolatile && (variable->flag() || isRaii || isBorrowed || isReferenceLocal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "volatile local cannot also be const, raii, owned, borrowed, or a reference local");
    }
    if (isReferenceLocal &&
        ((!isReadonlyReference && variable->flag()) ||
         variable->hasModifier(TokenKind::KwRaii) ||
         declaredOwnership != sema::OwnershipKind::None))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "a mutable reference local cannot be const, raii, owned, or borrowed; it aliases storage without ownership");
    }
    if (isReferenceLocal && variable->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            isReadonlyReference
                ? "readonly reference local requires an initializer naming addressable storage"
                : "reference local requires an initializer naming mutable addressable storage");
    }
    if (isRaii && !isReferenceTypeKey(typeKey) && typeKey != "builtin:string" && !managedDelegate &&
        !raiiOwnsNativeArrayBuffer &&
        !typeKey.starts_with("iterator:") && !typeKey.starts_with("async-iterator:"))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            std::string(ownerSpelling) +
                " requires a NativeSharp class/interface reference, managed delegate, native string, or native array buffer");
    }
    if (isForeachEnumerator && typeKeyIsStruct(typeKey) &&
        !elementTypeHasTrivialLifetime(typeKey) &&
        !typeKey.starts_with("iterator:") && !typeKey.starts_with("async-iterator:"))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "foreach struct enumerator '" + typeKey +
                "' must have a trivial lifetime; automatic destruction of value locals is not "
                "yet supported");
    }
    if (isRaii && variable->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            std::string(ownerSpelling) + " requires an initializer: new T(...), null, or move(other)");
    }
    if (isBorrowed && variable->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "borrowed local requires an initializer naming a live owner or borrow");
    }
    sema::OwnershipKind pendingObjectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind pendingStorageOwnership = sema::OwnershipKind::None;
    std::optional<language::CallableTrait> pendingCallableTrait;
    bool pendingOnePastEndPointer = false;
    const bool declaredPointerPointeeReadonly = variable->typeSyntax() != nullptr &&
        variable->typeSyntax()->isPointeeConst();
    const language::PointerContract declaredPointerContract =
        pointerContractFromSyntax(variable->typeSyntax());
    bool pendingPointerPointeeReadonly = declaredPointerPointeeReadonly;
    bool pendingPointerNullable = declaredPointerContract.nullable;
    bool pendingPointerHasProvenance = declaredPointerContract.hasProvenance;
    std::size_t pendingPointerKnownAlignment = declaredPointerContract.knownAlignment;
    unsigned pendingPointerAddressSpace = declaredPointerContract.addressSpace;
    if (variable->first() != nullptr)
    {
        ExpressionResult initializer;
        if (isVarInferred)
        {
            // Already analyzed above (before typeKey existed, so this couldn't happen in the
            // usual typeKey-first order) -- reuse it rather than analyzing the initializer twice.
            initializer = std::move(*precomputedVarInitializer);
            // NS304-T04-F07: el method group natural ya infirio su fn*; convertirlo en un valor
            // delegado real (cell/function pointer) antes de registrar el local.
            if (initializer.methodGroup && isDelegateTypeKey(typeKey))
            {
                initializer = bindMethodGroupToDelegate(
                    variable->first(), initializer, typeKey, methodMemberOrdinal, scopeOrdinal,
                    diagnostics);
            }
        }
        else if (variable->first()->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            // C7: a braced initializer has no type of its own -- it is filled AGAINST the
            // declared type, exactly like a lambda is bound against a delegate type just below.
            initializer = bindAggregateInitializer(
                variable->first(), typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics, true);
        }
        else if (variable->first()->kind() == ast::AstNodeKind::LambdaExpression && isDelegateTypeKey(typeKey))
        {
            initializer = bindLambdaToDelegate(
                variable->first(), typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        else
        {
            // NS304-T09-F07: expone el tipo declarado al `new()` target-typed del inicializador.
            const std::string previousTargetTypeKey = activeTargetTypeKey_;
            activeTargetTypeKey_ = typeKey;
            initializer = analyzeExpression(
                variable->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                isDelegateTypeKey(typeKey), false, isRaii || isMoveOnly);
            activeTargetTypeKey_ = previousTargetTypeKey;
            if (initializer.methodGroup && isDelegateTypeKey(typeKey))
            {
                initializer = bindMethodGroupToDelegate(
                    variable->first(), initializer, typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
            }
        }
        pendingCallableTrait = initializer.callableTrait;
        // Pattern matching materializes one compiler-owned, non-owning snapshot so the governing
        // expression is evaluated once.  It is deliberately not an owner and receives no cleanup;
        // rejecting that snapshot as a user copy makes every lvalue variant unmatchable.  This
        // exemption is restricted to the unspellable temporary prefix emitted by Parser.
        const bool isCompilerGeneratedMatchSnapshot =
            variable->text().starts_with("$__nsc_match_") ||
            variable->text().starts_with("$__nsc_conditional_pattern_");
        const bool isCompilerGeneratedForeachViewSnapshot =
            variable->text().starts_with("$__foreach_") &&
            variable->text().ends_with("_source") && typeKeyIsViewLike(typeKey);
        if (isMoveOnly && initializer.valid && initializer.addressable &&
            !isCompilerGeneratedMatchSnapshot && !isCompilerGeneratedForeachViewSnapshot &&
            variable->first()->kind() != ast::AstNodeKind::MoveExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "move-only value of type '" + typeKey +
                    "' cannot be copied into local '" + variable->text() +
                    "'; use move(source)");
        }
        if (!isRaii && !isBorrowed && variable->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> sourceName = lookupScopeName(scopeOrdinal, variable->first()->text());
            if (sourceName.has_value() && sourceName->kind == ScopeNameKind::Local &&
                sourceName->semanticOrdinal < locals_.size() && locals_[sourceName->semanticOrdinal].isRaii)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "RAII owner '" + locals_[sourceName->semanticOrdinal].name +
                        "' cannot be copied into a non-owning local; Stage 49 requires ownership transfer with move(...)");
            }
        }
        // Stage 52: an "owned"/"borrowed"-returning call whose declared return type is a
        // generic T closed over a plain value type (int, bool, a struct, ...) carries an
        // ownership qualifier that is structurally meaningless for that T -- there is nothing
        // to transfer or to keep alive. Requiring a raii/borrowed destination local in that
        // case would make it impossible to write `int x = list.Pop();` for a generic
        // collection. Only enforce the raii-destination / borrowed-local requirement when the
        // local's own declared type actually needs ownership tracking.
        const bool localTypeOwnable = isReferenceTypeKey(typeKey) || typeKey == "builtin:string" || managedDelegate ||
            raiiOwnsNativeArrayBuffer;
        if (!isRaii && initializer.valid && initializer.ownership == sema::OwnershipKind::Owned &&
            initializer.ownershipTransfer && !isBorrowed && localTypeOwnable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "owned return value cannot initialize a non-owning local; use a raii owner destination");
        }
        const bool ownedReturningInitializer = initializer.valid &&
            initializer.ownership == sema::OwnershipKind::Owned && initializer.ownershipTransfer;
        if (isRaii && variable->first()->kind() != ast::AstNodeKind::ObjectCreationExpression &&
            variable->first()->kind() != ast::AstNodeKind::PlacementConstructionExpression &&
            variable->first()->kind() != ast::AstNodeKind::NewExpression &&
            variable->first()->kind() != ast::AstNodeKind::MoveExpression &&
            variable->first()->kind() != ast::AstNodeKind::NullLiteralExpression &&
            !ownedReturningInitializer && !initializer.ownershipFreshOwner &&
            !(isForeachEnumerator &&
              (typeKey.starts_with("iterator:") || typeKey.starts_with("async-iterator:"))))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "RAII ownership cannot be copied or borrowed; initialize with new, null, move(other), or an owned-returning call");
        }
        if (isReferenceLocal && initializer.valid)
        {
            // NS304-T06-F05/T07-F04: a `ref:`/`refmut:` item IS an addressable reference into the
            // frame (the Current core-op yields the reference, not a copy).
            const bool itemByReference = initializer.typeKey.starts_with("ref:") ||
                initializer.typeKey.starts_with("refmut:");
            std::string initializerReferentType = initializer.typeKey;
            if (initializerReferentType.starts_with("ref:"))
                initializerReferentType = initializerReferentType.substr(4);
            else if (initializerReferentType.starts_with("refmut:"))
                initializerReferentType = initializerReferentType.substr(7);
            if ((!initializer.addressable && !itemByReference) ||
                (!isReadonlyReference && !initializer.assignable && !itemByReference))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    isReadonlyReference
                        ? "readonly reference initializer must be an addressable storage location"
                        : "reference-local initializer must be a mutable addressable storage location");
            }
            if (initializerReferentType != typeKey)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "reference-local initializer type '" + initializer.typeKey +
                        "' must exactly match declared type '" + typeKey + "'");
            }
            pendingReferenceEscapeOrigin =
                referenceEscapeOriginOfExpression(variable->first(), scopeOrdinal);
            if (!pendingReferenceEscapeOrigin.maximumValidScopeOrdinal.has_value())
            {
                std::size_t rootScope = scopeOrdinal;
                while (scopes_[rootScope].parentScopeOrdinal.has_value())
                    rootScope = *scopes_[rootScope].parentScopeOrdinal;
                pendingReferenceEscapeOrigin.maximumValidScopeOrdinal = rootScope;
            }
            if (!scopeContains(*pendingReferenceEscapeOrigin.maximumValidScopeOrdinal, scopeOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->range(),
                    "reference local lifetime exceeds the lifetime region of its source storage");
            }
            if ((initializer.addressable || itemByReference) &&
                (isReadonlyReference || initializer.assignable || itemByReference) &&
                initializerReferentType == typeKey)
            {
                pendingReferenceReborrowSourceLocalOrdinal = referenceRootLocalOrdinal(
                    variable->first(), scopeOrdinal);
                if (!isReadonlyReference)
                {
                    bool placeIsPrecise = true;
                    const std::optional<std::string> place = storagePlaceKey(
                        variable->first(), scopeOrdinal, &placeIsPrecise);
                    pendingReferencePlaceKey = place.value_or("U");
                    // NS304-T07-F04: a second mutable borrow of the same iterator item is rejected
                    // while the first one is still live (ref mut aliasing).
                    if (itemByReference && place.has_value() &&
                        conflictingMutableReference(*place, scopeOrdinal).has_value())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->first()->range(),
                            "cannot create another reference to the iterator item while a mutable borrow is still live");
                    }
                    if (isPinnedReferenceLocal && (!place.has_value() || !placeIsPrecise))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->first()->range(),
                            "pin ref requires a statically stable storage place; dynamic or "
                            "unknown projections may relocate their elements");
                    }
                }
            }
        }
        else if (initializer.valid)
        {
            const ProjectImplicitConversion conversion = classifyImplicit(
                initializer.typeKey,
                typeKey,
                initializer.integerConstant);
            if (!conversion.exists())
            {
                if (!reportTupleConversionFailure(
                        initializer.typeKey, typeKey, variable->first()->range(), diagnostics))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidConversion,
                        diagnostics::DiagnosticSeverity::Error,
                        variable->first()->range(),
                        "cannot initialize local '" + variable->text() + "' of type '" + typeKey +
                            "' from '" + initializer.typeKey + "'");
                }
            }
            if (!isVarInferred && ProjectTypeConversion::isPointer(typeKey) &&
                initializer.pointerPointeeReadonly && !declaredPointerPointeeReadonly)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "initialization of mutable-pointee pointer '" + variable->text() +
                        "' would discard pointee constness; declare 'T const*' or use const_cast explicitly");
            }
            if (!isVarInferred && ProjectTypeConversion::isPointer(typeKey))
            {
                const language::PointerContract source{
                    initializer.pointerNullable,
                    !initializer.pointerPointeeReadonly,
                    initializer.pointerHasProvenance,
                    initializer.pointerKnownAlignment,
                    initializer.pointerAddressSpace};
                if (!language::pointerContractImplicitlyAssignable(
                        source, declaredPointerContract))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidConversion,
                        diagnostics::DiagnosticSeverity::Error,
                        variable->first()->range(),
                        "pointer initializer does not satisfy the declared nullability, mutability, provenance, alignment, or address-space contract");
                }
            }
        }

        if (!isBorrowed && !isRaii && initializer.valid &&
            initializer.ownership == sema::OwnershipKind::Borrowed && localTypeOwnable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "borrowed value cannot be stored in unqualified local '" + variable->text() +
                    "'; declare the local borrowed so its lifetime remains checked");
        }

        if (isBorrowed && !raiiBorrowedElementArray)
        {
            if (isRaii)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->range(),
                    "a borrowed local cannot also be declared raii");
            }
            else if (variable->first()->kind() != ast::AstNodeKind::IdentifierExpression)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "borrowed local requires a direct live owner or borrowed identifier source");
            }
            else if (initializer.valid)
            {
                const std::optional<ScopeName> source = lookupScopeName(
                    scopeOrdinal, variable->first()->text());
                if (!source.has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        variable->first()->range(),
                        "borrowed local requires a checked owner or borrowed source");
                }
                else
                {
                    MethodLocalBorrowBinding borrow;
                    borrow.syntax = variable;
                    borrow.methodMemberOrdinal = methodMemberOrdinal;
                    borrow.creationScopeOrdinal = scopeOrdinal;
                    borrow.typeKey = typeKey;

                    if (source->kind == ScopeNameKind::Local && source->semanticOrdinal < locals_.size())
                    {
                        const MethodLocalSymbol& sourceLocal = locals_[source->semanticOrdinal];
                        borrow.sourceLocalOrdinal = source->semanticOrdinal;
                        if (sourceLocal.isRaii)
                        {
                            borrow.sourceKind = MethodLocalBorrowSourceKind::OwnerLocal;
                            borrow.sourceOwnerLocalOrdinal = source->semanticOrdinal;
                            borrow.maximumValidScopeOrdinal = sourceLocal.scopeOrdinal;
                        }
                        else if (sourceLocal.ownership == sema::OwnershipKind::Borrowed &&
                            sourceLocal.localBorrowBindingOrdinal.has_value() &&
                            *sourceLocal.localBorrowBindingOrdinal < localBorrowBindings_.size())
                        {
                            const MethodLocalBorrowBinding& sourceBorrow =
                                localBorrowBindings_[*sourceLocal.localBorrowBindingOrdinal];
                            borrow.sourceKind = MethodLocalBorrowSourceKind::BorrowedLocal;
                            borrow.sourceOwnerLocalOrdinal = sourceBorrow.sourceOwnerLocalOrdinal;
                            borrow.sourceOwnerParameterOrdinal = sourceBorrow.sourceOwnerParameterOrdinal;
                            borrow.possibleSourceOwnerLocalOrdinals =
                                sourceBorrow.possibleSourceOwnerLocalOrdinals;
                            borrow.possibleSourceOwnerParameterOrdinals =
                                sourceBorrow.possibleSourceOwnerParameterOrdinals;
                            borrow.maximumValidScopeOrdinal = sourceBorrow.maximumValidScopeOrdinal;
                        }
                        else
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                variable->first()->range(),
                                "borrowed local source '" + sourceLocal.name +
                                    "' is not a RAII owner or borrowed local");
                        }
                    }
                    else if (source->kind == ScopeNameKind::Parameter)
                    {
                        const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                        if (method != nullptr && source->semanticOrdinal < method->parameters.size())
                        {
                            const MethodParameterSymbol& parameter = method->parameters[source->semanticOrdinal];
                            borrow.sourceParameterOrdinal = source->semanticOrdinal;
                            std::size_t rootScope = scopeOrdinal;
                            while (scopes_[rootScope].parentScopeOrdinal.has_value())
                            {
                                rootScope = *scopes_[rootScope].parentScopeOrdinal;
                            }
                            borrow.maximumValidScopeOrdinal = rootScope;
                            if (parameter.ownership == sema::OwnershipKind::Owned)
                            {
                                borrow.sourceKind = MethodLocalBorrowSourceKind::OwnedParameter;
                                borrow.sourceOwnerParameterOrdinal = source->semanticOrdinal;
                            }
                            else if (parameter.ownership == sema::OwnershipKind::Borrowed)
                            {
                                borrow.sourceKind = MethodLocalBorrowSourceKind::BorrowedParameter;
                                borrow.sourceOwnerParameterOrdinal = source->semanticOrdinal;
                            }
                            else
                            {
                                diagnostics.report(
                                    diagnostics::DiagnosticId::InvalidOperand,
                                    diagnostics::DiagnosticSeverity::Error,
                                    variable->first()->range(),
                                    "borrowed local source parameter '" + parameter.name +
                                        "' must be declared owned or borrowed");
                            }
                        }
                    }

                    if (borrow.sourceOwnerLocalOrdinal.has_value())
                    {
                        borrow.possibleSourceOwnerLocalOrdinals.push_back(
                            *borrow.sourceOwnerLocalOrdinal);
                    }
                    if (borrow.sourceOwnerParameterOrdinal.has_value())
                    {
                        borrow.possibleSourceOwnerParameterOrdinals.push_back(
                            *borrow.sourceOwnerParameterOrdinal);
                    }
                    const bool hasRootOwner = borrow.sourceOwnerLocalOrdinal.has_value() ||
                        borrow.sourceOwnerParameterOrdinal.has_value();
                    if (hasRootOwner &&
                        scopeContains(borrow.maximumValidScopeOrdinal, borrow.creationScopeOrdinal))
                    {
                        pendingLocalBorrow = std::move(borrow);
                    }
                    else if (hasRootOwner)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->range(),
                            "borrowed local lifetime exceeds the lifetime region of its source owner");
                    }
                }
            }
        }
        pendingObjectLifetimeOwnership = initializer.objectLifetimeOwnership;
        pendingStorageOwnership = initializer.storageOwnership;
        pendingOnePastEndPointer = initializer.onePastEndPointer;
        pendingPointerPointeeReadonly = declaredPointerPointeeReadonly ||
            initializer.pointerPointeeReadonly;
        if (isVarInferred)
        {
            pendingPointerNullable = initializer.pointerNullable;
            pendingPointerHasProvenance = initializer.pointerHasProvenance;
            pendingPointerKnownAlignment = initializer.pointerKnownAlignment;
            pendingPointerAddressSpace = initializer.pointerAddressSpace;
        }
    }

    const std::optional<ScopeName> sameScope = lookupScopeNameInScope(scopeOrdinal, variable->text());
    if (sameScope.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "duplicate local/parameter name '" + variable->text() + "' in the same method scope");
        reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
        return;
    }

    MethodLocalSymbol local;
    local.methodMemberOrdinal = methodMemberOrdinal;
    local.scopeOrdinal = scopeOrdinal;
    local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method != nullptr)
    {
        local.fileId = method->fileId;
    }
    local.range = variable->range();
    local.syntax = variable;
    local.name = variable->text();
    local.typeKey = typeKey;
    local.isStatic = isStaticLocal;
    local.isReference = isReferenceLocal;
    local.referenceMutable = isReferenceLocal && !isReadonlyReference;
    local.referenceEscapeOrigin = pendingReferenceEscapeOrigin;
    if (isReferenceLocal)
    {
        local.referenceLifetimeScopeOrdinal = scopeOrdinal;
        local.referencePlaceKey = std::move(pendingReferencePlaceKey);
        local.referenceReborrowSourceLocalOrdinal =
            pendingReferenceReborrowSourceLocalOrdinal;
    }
    local.isVariableLengthArray = isVariableLengthArray;
    local.variableLengthArrayElementTypeKey = variableLengthArrayElementTypeKey;
    local.variableLengthArrayCountSyntax = variableLengthArrayCountSyntax;
    local.variableLengthArrayExtentSyntaxes = variableLengthArrayExtentSyntaxes;
    local.variableLengthArrayExtentTypeKeys = variableLengthArrayExtentTypeKeys;
    if (!isVariableLengthArray && variable->first() != nullptr && typeKey.ends_with("*"))
    {
        local.variableLengthArrayRootLocalOrdinal =
            variableArrayRootLocalOfExpression(variable->first(), scopeOrdinal);
    }
    local.isConst = variable->flag();
    local.isVolatile = isVolatile;
    local.onePastEndPointer = pendingOnePastEndPointer;
    local.pointerPointeeReadonly = pendingPointerPointeeReadonly;
    local.pointerNullable = pendingPointerNullable;
    local.pointerHasProvenance = pendingPointerHasProvenance;
    local.pointerKnownAlignment = pendingPointerKnownAlignment;
    local.pointerAddressSpace = pendingPointerAddressSpace;
    // El valor de un `const` entero, plegado aqui. Ver el comentario del campo: sirve para
    // contestar "¿el divisor es cero?" y para nada mas.
    if (local.isConst && variable->first() != nullptr &&
        ProjectIntegerEvaluator::isIntegerType(typeKey))
    {
        const std::optional<ProjectIntegerValue> folded =
            foldIntegerConstant(variable->first(), std::nullopt, methodMemberOrdinal);
        if (folded.has_value())
        {
            local.constantIntegerValue =
                static_cast<std::int64_t>(ProjectIntegerEvaluator::normalize(folded->bits, typeKey));
        }
    }
    local.isRaii = isRaii;
    local.isScope = isScope;
    local.isMoveOnly = isMoveOnly;
    local.callableTrait = pendingCallableTrait;
    local.requiresValueDrop = memberModel_->typeRequiresValueDrop(typeKey);
    local.isPinned = isPinnedLocal;
    local.ownership = isBorrowed ? sema::OwnershipKind::Borrowed : sema::OwnershipKind::None;
    local.objectLifetimeOwnership = pendingObjectLifetimeOwnership;
    local.storageOwnership = pendingStorageOwnership;
    // NS304-T13-F03: the frame object owns object destruction (exactly once at scope exit) while its
    // storage is the frame itself, so there is no heap deallocator to call.
    if (isScope)
    {
        local.objectLifetimeOwnership = sema::OwnershipKind::Owned;
        local.storageOwnership = sema::OwnershipKind::None;
    }
    if (pendingLocalBorrow.has_value())
    {
        local.localBorrowBindingOrdinal = localBorrowBindings_.size();
    }
    local.lambdaOrdinal = currentLambdaOrdinal_;
    // SHR-013: de que local presta esta vista, si presta de alguno. Se graba AQUI, al declarar,
    // y no se recalcula al usarla: la expresion que la creo es la unica que lo sabe, y para
    // cuando alguien la devuelve ya no esta a la vista.
    if (typeKeyIsViewLike(typeKey))
    {
        const std::optional<std::string> root =
            viewRootLocalOfExpression(variable->first(), scopeOrdinal);
        if (root.has_value()) local.viewRootLocalName = *root;
        else if (expressionUsesExpandedParameterPack(variable->first(), methodMemberOrdinal))
        {
            // The backing array is an alloca in this method. Naming the view local as its own
            // root reuses the ordinary SHR-013 return check and gives the user a useful name.
            local.viewRootLocalName = local.name;
        }
    }

    const std::optional<ScopeName> outer = lookupScopeName(scopeOrdinal, variable->text());
    if (outer.has_value())
    {
        MethodShadowingKind kind = MethodShadowingKind::LocalShadowsLocal;
        if (outer->kind == ScopeNameKind::Parameter)
        {
            kind = MethodShadowingKind::LocalShadowsParameter;
        }
        shadowingEvents_.push_back(MethodShadowingEvent{
            kind,
            methodMemberOrdinal,
            scopeOrdinal,
            variable->text(),
            variable->range()});
    }
    else if (!memberCandidates(methodMemberOrdinal, variable->text()).empty())
    {
        shadowingEvents_.push_back(MethodShadowingEvent{
            MethodShadowingKind::LocalShadowsMember,
            methodMemberOrdinal,
            scopeOrdinal,
            variable->text(),
            variable->range()});
    }

    const std::size_t localOrdinal = locals_.size();
    locals_.push_back(std::move(local));
    if (isVariableLengthArray)
        locals_[localOrdinal].variableLengthArrayRootLocalOrdinal = localOrdinal;
    scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
    scopeNames_[scopeOrdinal].push_back(ScopeName{
        ScopeNameKind::Local,
        variable->text(),
        localOrdinal,
        variable->range()});

    // Item 270: multidimensional extents are values, not aliases for their source expressions.
    // Snapshot them in hidden nuint locals in source order so side effects happen once and later
    // assignments to a variable used as an extent cannot silently reshape the array's strides.
    if (isVariableLengthArray && variableLengthArrayExtentSyntaxes.size() > 1)
    {
        for (std::size_t dimension = 0;
             dimension < variableLengthArrayExtentSyntaxes.size(); ++dimension)
        {
            MethodLocalSymbol extentLocal;
            extentLocal.methodMemberOrdinal = methodMemberOrdinal;
            extentLocal.scopeOrdinal = scopeOrdinal;
            extentLocal.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
            extentLocal.fileId = locals_[localOrdinal].fileId;
            extentLocal.range = variableLengthArrayExtentSyntaxes[dimension] != nullptr
                ? variableLengthArrayExtentSyntaxes[dimension]->range() : variable->range();
            extentLocal.syntax = variableLengthArrayExtentSyntaxes[dimension];
            extentLocal.name = "__vla_extent_" + std::to_string(localOrdinal) + "_" +
                std::to_string(dimension);
            extentLocal.typeKey = dimension < variableLengthArrayExtentTypeKeys.size()
                ? variableLengthArrayExtentTypeKeys[dimension] : "builtin:nuint";
            extentLocal.lambdaOrdinal = currentLambdaOrdinal_;
            const std::size_t extentOrdinal = locals_.size();
            locals_.push_back(std::move(extentLocal));
            scopes_[scopeOrdinal].localOrdinals.push_back(extentOrdinal);
            assignedLocals_.insert(extentOrdinal);
            locals_[localOrdinal].variableLengthArrayExtentLocalOrdinals.push_back(extentOrdinal);
        }
    }
    // C7 "definite assignment": a declaration with an initializer assigns the local, full stop.
    // Without one, only the types the analysis does not track (aggregates, which the backend
    // zeroes) start out assigned; everything else must be written before it is read. A local
    // re-entering scope on a later loop iteration is re-declared through this same path, so its
    // assigned state is correctly reset rather than inherited from the previous iteration.
    if (variable->first() != nullptr || isVariableLengthArray ||
        !definiteAssignmentTracksLocal(localOrdinal))
    {
        assignedLocals_.insert(localOrdinal);
    }
    else
    {
        assignedLocals_.erase(localOrdinal);
    }
    if (variable->first() != nullptr &&
        variable->first()->kind() == ast::AstNodeKind::AggregateInitializerExpression &&
        aggregateRequiresActiveMember(typeKey))
    {
        const std::optional<std::size_t> typeOrdinal = namedTypeOrdinalForTypeKey(typeKey);
        if (typeOrdinal.has_value())
        {
            for (const std::size_t memberOrdinal : memberModel_->membersOfType(*typeOrdinal))
            {
                const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                if (member.kind != ClassMemberKind::Field || member.isStatic || member.isConst ||
                    member.isUnnamedBitField) continue;
                activeUnionMembers_[localOrdinal * 2U] = memberOrdinal;
                break;
            }
        }
    }
    else if (variable->first() != nullptr &&
        variable->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        aggregateRequiresActiveMember(typeKey))
    {
        const std::optional<ScopeName> source =
            lookupScopeName(scopeOrdinal, variable->first()->text());
        if (source.has_value() &&
            (source->kind == ScopeNameKind::Local || source->kind == ScopeNameKind::Parameter))
        {
            const std::size_t sourceKey = source->semanticOrdinal * 2U +
                (source->kind == ScopeNameKind::Parameter ? 1U : 0U);
            const auto active = activeUnionMembers_.find(sourceKey);
            if (active != activeUnionMembers_.end())
                activeUnionMembers_[localOrdinal * 2U] = active->second;
        }
    }
    if (pendingLocalBorrow.has_value())
    {
        pendingLocalBorrow->localOrdinal = localOrdinal;
        localBorrowBindings_.push_back(std::move(*pendingLocalBorrow));
    }
    if (!isStaticLocal && (isRaii || isScope || locals_[localOrdinal].requiresValueDrop))
    {
        const std::size_t cleanupOrdinal = createCleanupRegion(
            (isRaii || isScope)
                ? MethodCleanupRegionKind::RaiiDestroy
                : MethodCleanupRegionKind::ValueLocalDrop,
            variable,
            variable,
            methodMemberOrdinal,
            scopeOrdinal);
        cleanupRegions_[cleanupOrdinal].resourceLocalOrdinal = localOrdinal;
        activeCleanupRegions_.push_back(cleanupOrdinal);
        movedRaiiLocals_.erase(localOrdinal);
        maybeMovedRaiiLocals_.erase(localOrdinal);
    }
    else if (isMoveOnly)
    {
        movedRaiiLocals_.erase(localOrdinal);
        maybeMovedRaiiLocals_.erase(localOrdinal);
    }
}

void ProjectMethodBodyModel::analyzeFor(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t forScope = createScope(
        MethodScopeKind::For,
        methodMemberOrdinal,
        scopeOrdinal);
    const std::size_t breakCleanupDepth = activeCleanupRegions_.size();

    if (statement->first() != nullptr)
    {
        if (statement->first()->kind() == ast::AstNodeKind::VariableDeclarationStatement)
        {
            analyzeVariable(statement->first(), methodMemberOrdinal, forScope, diagnostics);
        }
        else if (statement->first()->kind() == ast::AstNodeKind::BlockStatement)
        {
            if (statement->first()->auxiliaryText() == "declaration-list")
            {
                analyzeBlock(
                    statement->first(), methodMemberOrdinal, forScope, false, diagnostics);
            }
            else
            {
                for (const ast::AstNode* expressionStatement : statement->first()->children())
                {
                    if (expressionStatement != nullptr && expressionStatement->first() != nullptr)
                        analyzeExpression(
                            expressionStatement->first(), methodMemberOrdinal, forScope, diagnostics);
                }
            }
        }
        else
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, forScope, diagnostics);
        }
    }

    const bool conditionAlwaysTrue = statement->second() == nullptr ||
        (statement->second()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
         statement->second()->tokenKind() == lexer::TokenKind::KwTrue);
    if (statement->second() != nullptr)
    {
        const ExpressionResult condition = analyzeExpression(
            statement->second(), methodMemberOrdinal, forScope, diagnostics);
        if (condition.valid &&
            condition.typeKey != "builtin:bool" &&
            condition.typeKey != "builtin:never")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCondition,
                diagnostics::DiagnosticSeverity::Error,
                statement->second()->range(),
                "for condition must have type bool");
        }
    }

    const OwnershipFlowSnapshot loopEntry = captureOwnershipFlow();
    const std::size_t localCountAtBodyEntry = locals_.size();
    const ClassMemberSymbol* loopMethod = methodSymbol(methodMemberOrdinal);
    const std::size_t continueCleanupDepth = activeCleanupRegions_.size();
    loopStack_.push_back(LoopContext{
        forScope,
        breakCleanupDepth,
        continueCleanupDepth,
        {},
        {},
        false,
        nullptr,
        {},
        false,
        {}});

    if (!statement->children().empty())
    {
        analyzeBlock(
            statement->children().front(),
            methodMemberOrdinal,
            forScope,
            true,
            diagnostics);
    }
    const OwnershipFlowSnapshot bodyFallthroughState = captureOwnershipFlow();
    LoopContext loopContext = std::move(loopStack_.back());
    loopStack_.pop_back();

    std::vector<OwnershipFlowSnapshot> updateEntryStates = std::move(loopContext.continueStates);
    if (bodyFallthroughState.reachable)
    {
        updateEntryStates.push_back(bodyFallthroughState);
    }
    OwnershipFlowSnapshot noUpdateEntry;
    noUpdateEntry.reachable = false;
    OwnershipFlowSnapshot backEdgeState = joinOwnershipFlows(noUpdateEntry, updateEntryStates);

    if (backEdgeState.reachable && statement->third() != nullptr)
    {
        restoreOwnershipFlow(backEdgeState);
        if (statement->third()->kind() == ast::AstNodeKind::BlockStatement)
        {
            for (const ast::AstNode* expressionStatement : statement->third()->children())
            {
                if (expressionStatement != nullptr && expressionStatement->first() != nullptr)
                    analyzeExpression(
                        expressionStatement->first(), methodMemberOrdinal, forScope, diagnostics);
            }
        }
        else
        {
            analyzeExpression(statement->third(), methodMemberOrdinal, forScope, diagnostics);
        }
        backEdgeState = captureOwnershipFlow();
    }

    if (backEdgeState.reachable)
    {
        for (std::size_t localOrdinal = 0; localOrdinal < localCountAtBodyEntry; ++localOrdinal)
        {
            if (localOrdinal >= locals_.size() ||
                (!locals_[localOrdinal].isRaii && !locals_[localOrdinal].isMoveOnly))
            {
                continue;
            }
            const bool aliveAtEntry = !snapshotLocalHasAnyFlag(
                loopEntry,
                localOrdinal,
                kLocalMoved | kLocalMaybeMoved);
            const bool movedAtBackEdge = snapshotLocalHasAnyFlag(
                backEdgeState,
                localOrdinal,
                kLocalMoved | kLocalMaybeMoved);
            if (aliveAtEntry && movedAtBackEdge)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "RAII owner '" + locals_[localOrdinal].name +
                        "' may remain moved across a for-loop back-edge; reinitialize it before the next iteration");
            }
        }
        if (loopMethod != nullptr)
        {
            for (std::size_t parameterOrdinal = 0;
                 parameterOrdinal < loopMethod->parameters.size(); ++parameterOrdinal)
            {
                if (loopMethod->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned)
                {
                    continue;
                }
                const bool aliveAtEntry = !snapshotParameterHasAnyFlag(
                    loopEntry,
                    parameterOrdinal,
                    kParameterMoved | kParameterMaybeMoved);
                const bool movedAtBackEdge = snapshotParameterHasAnyFlag(
                    backEdgeState,
                    parameterOrdinal,
                    kParameterMoved | kParameterMaybeMoved);
                if (aliveAtEntry && movedAtBackEdge)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->range(),
                        "owned parameter '" + loopMethod->parameters[parameterOrdinal].name +
                            "' may remain moved across a for-loop back-edge; transfer it only on an exiting path");
                }
            }
        }
        std::unordered_set<std::string> entryPartial = loopEntry.partiallyMovedPlaces;
        entryPartial.insert(
            loopEntry.maybePartiallyMovedPlaces.begin(),
            loopEntry.maybePartiallyMovedPlaces.end());
        std::unordered_set<std::string> backPartial = backEdgeState.partiallyMovedPlaces;
        backPartial.insert(
            backEdgeState.maybePartiallyMovedPlaces.begin(),
            backEdgeState.maybePartiallyMovedPlaces.end());
        for (const std::string& place : backPartial)
        {
            if (entryPartial.contains(place)) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "field path '" + place +
                    "' may remain moved across a for-loop back-edge; reinitialize it before the next iteration");
        }
    }

    // T07-F04. A conditionless `for` and `for (; true; ...)` can leave only through `break`.
    // Do not retain the fictitious zero-iteration edge used by ordinary conditional loops.
    OwnershipFlowSnapshot postLoop;
    postLoop.reachable = false;
    if (!conditionAlwaysTrue) postLoop = loopEntry;
    postLoop = joinOwnershipFlows(postLoop, loopContext.breakStates);
    if (!conditionAlwaysTrue && backEdgeState.reachable)
    {
        postLoop = joinOwnershipFlow(postLoop, backEdgeState);
    }
    restoreOwnershipFlow(postLoop);

    for (std::size_t index = activeCleanupRegions_.size(); index > breakCleanupDepth; --index)
    {
        recordNormalCleanupContinuation(
            activeCleanupRegions_[index - 1], statement, methodMemberOrdinal, forScope);
    }
    activeCleanupRegions_.resize(breakCleanupDepth);
}

// ---------------------------------------------------------------------------------------------
// analyzeForeach -- semantic resolution of the array-vs-enumerator-pattern desugaring.
//
// Parser::parseForeachStatement() cannot know the collection's type (parsing is a purely
// syntactic pass), so it builds BOTH possible desugarings unconditionally and stores them on the
// ForeachStatement node:
//   first()  -- `var $__foreach_<id>_source = <collection>;` -- evaluates the collection expression
//                exactly once, regardless of which candidate is chosen below. Whether this ends
//                up `raii` is decided in this function (P0.2, see below), not by the parser.
//   second() -- the array-mode candidate: an index/`.Length` while-loop.
//   third()  -- the enumerator-mode candidate: a `GetEnumerator()`/`MoveNext()`/`Current`
//                while-loop, with the enumerator held in a `raii var` local so it is destroyed
//                automatically on every exit path (break/return/exception) -- NativeSharp's
//                RAII machinery standing in for C#'s implicit `Dispose()` call in foreach's
//                generated `finally` block. `Current` is read as a plain property (P1.1), not a
//                method call.
//   children()[0] -- `collectionProbe`, a throwaway second parse of the collection expression,
//                used below (P0.2) to decide first()'s raii-ness before it is analyzed for real.
//
// This function is the ONE place that decides which candidate is "real": it analyzes first() to
// resolve the synthesized foreach source's typeKey (reusing analyzeVariable exactly as any other `var` local
// would), then dispatches on that typeKey (array types are `<element>[]`, see
// ProjectGenericModel::canonicalTypeKey) to analyze ONLY the matching candidate via analyzeBlock.
// The other candidate's AST subtree is left completely unanalyzed -- no locals are registered for
// it, no diagnostics are produced from it, and it never reaches HIR/MIR/LLVM. It is simply dead
// arena memory, reclaimed with everything else when the compilation unit's AstArena is destroyed.
//
// No new control-flow analysis is written here on purpose: both candidates are built entirely out
// of ordinary VariableDeclarationStatement/WhileStatement nodes, so delegating to analyzeBlock
// means the WhileStatement case in analyzeStatement (loopStack_ push/pop, RAII-across-back-edge
// checking, ownership-flow join, break/continue support) runs completely unmodified. foreach gets
// full loop semantics for free, with zero duplicated logic -- exactly like the HIR/MIR/LLVM
// layers below it, which likewise need no foreach-specific lowering code at all.
void ProjectMethodBodyModel::analyzeForeach(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t foreachScope = createScope(
        MethodScopeKind::Foreach,
        methodMemberOrdinal,
        scopeOrdinal);

    // Captured BEFORE the synthesized foreach source is declared below, exactly like analyzeFor captures its
    // own breakCleanupDepth before analyzing its init statement -- see the teardown block at the
    // end of this function for why this is needed (P0.2 fix, see comment there for the full
    // story: the foreach source can now become `raii`, and its cleanup region must be popped by
    // THIS function, the same way analyzeFor pops its own init-scope's cleanup regions, since
    // nothing else ever will).
    const std::size_t foreachCleanupDepth = activeCleanupRegions_.size();

    if (statement->first() == nullptr)
    {
        return;
    }

    // P0.2 of the closure review (STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md): decide
    // whether $__foreach_source needs to be `raii` BEFORE analyzing it for real below.
    // Parser::parseForeachStatement always builds sourceDecl as a plain, non-raii `var` local,
    // because at parse time the collection expression's ownership shape is unknown. But if the
    // collection expression is itself owned-returning (e.g. `foreach (var x in
    // factory.CreateList())` where CreateList() returns `owned List<T>`), analyzing it into a
    // non-raii local would hit the ordinary "owned return value cannot initialize a non-owning
    // local" diagnostic (see analyzeVariable, ~line 1738) -- exactly the P0.2 bug this probe
    // exists to avoid.
    //
    // The probe: Parser attached a throwaway, structurally-identical copy of the collection
    // expression (`collectionProbe`, parsed a second time from the same tokens) as this
    // ForeachStatement node's own extra child. Analyzing THAT copy -- rather than analyzing
    // statement->first()'s own initializer expression twice -- avoids double-registering the
    // real node's bindings in nameBindings_/locals_/etc (analyzeExpression has real side
    // effects beyond its return value). The probe's own diagnostics are deliberately discarded
    // into a scratch DiagnosticEngine: this is not real analysis, only its ExpressionResult
    // (ownership / ownershipTransfer / typeKey) is consulted, and the SAME expression is about
    // to be analyzed again for real via analyzeVariable below, whose diagnostics (if any) are
    // the ones that actually reach the user.
    if (!statement->children().empty())
    {
        ast::AstNode* collectionProbe = statement->children()[0];
        diagnostics::DiagnosticEngine scratchDiagnostics;
        const ExpressionResult probe = analyzeExpression(
            collectionProbe, methodMemberOrdinal, foreachScope, scratchDiagnostics);

        // Arrays can never be raii (analyzeVariable rejects `raii` on anything but a class/
        // interface reference type or native string), so an owned-returning expression of
        // array type must stay a plain `var` -- it simply forgoes the extra owned-destructor
        // NativeSharp would otherwise emit for a raii owner, same as any other owned array
        // temporary elsewhere in this compiler (arrays are not RAII-tracked). Only
        // reference-like/class-shaped or string owned returns get the `raii` treatment here,
        // mirroring the exact `localTypeOwnable` condition analyzeVariable itself uses.
        const bool probeTypeOwnable = probe.valid &&
            (isReferenceTypeKey(probe.typeKey) || probe.typeKey == "builtin:string");
        if (probe.valid && probe.ownership == sema::OwnershipKind::Owned &&
            probe.ownershipTransfer && probeTypeOwnable)
        {
            statement->first()->addModifier(TokenKind::KwRaii);
        }
        else if (probe.valid &&
            (probe.ownership == sema::OwnershipKind::Owned ||
             probe.ownership == sema::OwnershipKind::Borrowed) &&
            collectionProbe->kind() == ast::AstNodeKind::IdentifierExpression &&
            statement->first()->typeSyntax() != nullptr)
        {
            // A named owner is observed by foreach; it is never copied into the synthesized
            // synthesized foreach source. Mark that hidden local as borrowed so its lifetime remains
            // rooted in the original RAII owner for both array and enumerator modes.
            statement->first()->typeSyntax()->setOwnershipQualifier(
                ast::OwnershipQualifierSyntaxKind::Borrowed);
        }
    }

    // Resolve the synthesized foreach source's type by analyzing its declaration exactly like any other `var`
    // local. On success this appends exactly one MethodLocalSymbol to locals_ (see
    // analyzeVariable); on failure (e.g. the collection expression itself doesn't type-check) it
    // reports its own diagnostic and appends nothing, and there is no reliable typeKey left to
    // dispatch on -- bail out rather than guess which candidate to analyze.
    const std::size_t localCountBeforeSource = locals_.size();
    analyzeVariable(statement->first(), methodMemberOrdinal, foreachScope, diagnostics);
    if (locals_.size() != localCountBeforeSource + 1)
    {
        return;
    }

    const std::string sourceTypeKey = locals_.back().typeKey;
    const bool isArray = sourceTypeKey.size() > 2 &&
        sourceTypeKey.compare(sourceTypeKey.size() - 2, 2, "[]") == 0;
    const bool isWritableSpan =
        sourceTypeKey.starts_with("type:NativeSharp.Core.Span<") ||
        sourceTypeKey.starts_with("type:NativeSharp.Core.Span__g") ||
        sourceTypeKey.find("::NativeSharp.Core.Span__g") != std::string::npos;
    const bool isReadonlySpan =
        sourceTypeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan<") ||
        sourceTypeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan__g") ||
        sourceTypeKey.find("::NativeSharp.Core.ReadOnlySpan__g") != std::string::npos;
    const bool isSpan = isWritableSpan || isReadonlySpan;
    if (isReadonlySpan && statement->auxiliaryText() == "foreach:ref-mut")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "foreach ref mut cannot iterate ReadOnlySpan<T>; use foreach ref or value iteration");
    }

    ast::AstNode* chosenCandidate = nullptr;
    if (isArray) chosenCandidate = const_cast<ast::AstNode*>(statement->second());
    else if (isSpan && statement->children().size() > 1U)
        chosenCandidate = const_cast<ast::AstNode*>(statement->children()[1]);
    else chosenCandidate = const_cast<ast::AstNode*>(statement->third());
    if (chosenCandidate != nullptr)
    {
        const bool lendingIteration =
            statement->auxiliaryText() == "foreach:ref" ||
            statement->auxiliaryText() == "foreach:ref-mut";
        const std::optional<std::string> borrowedSourcePlace = lendingIteration &&
                statement->first()->first() != nullptr
            ? storagePlaceKey(statement->first()->first(), foreachScope)
            : std::nullopt;
        if (borrowedSourcePlace.has_value())
        {
            activeIterationBorrows_.push_back(ActiveIterationBorrow{
                *borrowedSourcePlace,
                statement->auxiliaryText() == "foreach:ref-mut"});
        }
        // Everything past this point is ordinary VariableDeclarationStatement/WhileStatement
        // analysis -- including, for the enumerator-mode candidate, the
        // `GetEnumerator()`/`MoveNext()`/`Current` member lookups. Those go through the SAME
        // accessible-member resolution as any other method call/property access, so a collection
        // missing one of these members (or having them with the wrong signature/shape) is
        // reported by the ordinary "no accessible member" / argument-mismatch diagnostics --
        // foreach needs no bespoke "does this type support foreach" pre-check.
        analyzeBlock(chosenCandidate, methodMemberOrdinal, foreachScope, true, diagnostics);
        if (borrowedSourcePlace.has_value()) activeIterationBorrows_.pop_back();
    }

    // Teardown for the synthesized foreach source's OWN cleanup region, mirroring analyzeFor's end-of-forScope
    // teardown (see the last lines of analyzeFor above). This is genuinely new: before the P0.2
    // fix, the foreach source was never `raii`, so it never created a cleanup region and this
    // function never needed to pop one -- the gap was latent, not exercised by any existing test.
    // Now that the foreach source CAN be raii (an owned-returning collection source), its cleanup
    // region -- pushed by analyzeVariable above as if it were any other raii local -- must be
    // popped from activeCleanupRegions_ here, once, after the whole foreach statement (source
    // declaration AND whichever candidate was chosen) has finished analysis. Without this, the
    // region stays permanently "active" in the semantic model's bookkeeping, and the NEXT
    // structured exit analyzed anywhere later in the method (a `return`, an enclosing loop's
    // `break`/`continue`) would compute a cleanup path that includes an ordinal LLVM lowering
    // never actually enters/exits for real (foreach's own scope was never wired to do so) --
    // surfacing as "Stage 45.10 structured exit references an inactive cleanup region" in
    // ProjectOopLlvmLowerer, discovered while writing the regression test for this exact fix.
    for (std::size_t index = activeCleanupRegions_.size(); index > foreachCleanupDepth; --index)
    {
        recordNormalCleanupContinuation(
            activeCleanupRegions_[index - 1], statement, methodMemberOrdinal, foreachScope);
    }
    activeCleanupRegions_.resize(foreachCleanupDepth);
}

void ProjectMethodBodyModel::analyzeReturn(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr)
    {
        return;
    }

    // NS304-T05-F03: an async method returns the future surface at the ABI level, but `return v`
    // yields the awaited inner type; the semantic return check unwraps the future.
    std::string declaredReturnTypeKey = method->declaredTypeKey;
    if (method->effects.contains(sema::Effect::Async))
    {
        const auto inner = language::futureInnerTypeKey(declaredReturnTypeKey);
        if (inner.has_value()) declaredReturnTypeKey = std::string(*inner);
    }

    if (currentLambdaOrdinal_.has_value())
    {
        if (statement->isReference())
        {
            if (statement->first() != nullptr)
                analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "lambda ref returns are not part of the item 313 callable contract");
            return;
        }
        const std::string expected = currentLambdaReturnTypeKey_;
        if (expected == "builtin:void")
        {
            if (statement->first() != nullptr)
            {
                analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn, diagnostics::DiagnosticSeverity::Error,
                    statement->range(), "void lambda cannot return a value");
            }
            return;
        }
        if (statement->first() == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn, diagnostics::DiagnosticSeverity::Error,
                statement->range(), "non-void lambda must return a value");
            return;
        }
        if (statement->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> returnedName = lookupScopeName(scopeOrdinal, statement->first()->text());
            if (returnedName.has_value() && returnedName->kind == ScopeNameKind::Local &&
                returnedName->semanticOrdinal < locals_.size() && locals_[returnedName->semanticOrdinal].isRaii)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "returning RAII owner '" + locals_[returnedName->semanticOrdinal].name +
                        "' requires return move(owner) to transfer ownership");
            }
        }
        ExpressionResult value;
        if (statement->first()->kind() == ast::AstNodeKind::LambdaExpression && isDelegateTypeKey(expected))
            value = bindLambdaToDelegate(statement->first(), expected, methodMemberOrdinal, scopeOrdinal, diagnostics);
        else
            value = analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                isDelegateTypeKey(expected), false, true);
        if (value.methodGroup && isDelegateTypeKey(expected))
            value = bindMethodGroupToDelegate(statement->first(), value, expected, methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (value.valid && !classifyImplicit(value.typeKey, expected, value.integerConstant).exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion, diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(), "cannot convert lambda return expression from '" + value.typeKey +
                    "' to '" + expected + "'");
        }
        return;
    }

    if (!finallyStack_.empty())
    {
        if (statement->first() != nullptr)
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "return cannot leave a finally block");
        return;
    }

    recordStructuredExit(
        MethodStructuredExitKind::Return,
        statement,
        methodMemberOrdinal,
        scopeOrdinal,
        std::nullopt,
        0);

    if (method->kind == ClassMemberKind::Constructor || method->kind == ClassMemberKind::Destructor)
    {
        if (statement->first() != nullptr)
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                method->kind == ClassMemberKind::Constructor
                    ? "constructor cannot return a value"
                    : "destructor cannot return a value");
        }
        return;
    }

    if (declaredReturnTypeKey == "builtin:void")
    {
        if (statement->first() != nullptr)
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "void instance method cannot return a value");
        }
        return;
    }

    if (statement->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "non-void instance method must return a value");
        return;
    }

    if (statement->isReference() && !method->isReferenceReturn)
    {
        analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "return ref is valid only inside a ref-returning method");
        return;
    }

    if (method->isReferenceReturn)
    {
        const ExpressionResult value = analyzeExpression(
            statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!statement->isReference())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "ref-returning method must return an alias with 'return ref expression;'");
            return;
        }
        if (!value.valid) return;
        if (value.typeKey != method->declaredTypeKey)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return requires exact type identity; expression has '" + value.typeKey +
                    "' but method returns ref '" + method->declaredTypeKey + "'");
            return;
        }
        if (!value.addressable || !value.assignable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return requires a mutable addressable storage location");
            return;
        }

        const MethodEscapeOrigin escape =
            referenceEscapeOriginOfExpression(statement->first(), scopeOrdinal);
        if (escape.kind == MethodEscapeOriginKind::CurrentFrameLocal ||
            escape.kind == MethodEscapeOriginKind::VariableLengthArray)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return cannot expose storage owned by the current stack frame");
            return;
        }
        if (escape.kind == MethodEscapeOriginKind::CurrentFrameParameter)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return cannot expose a by-value or readonly parameter");
            return;
        }
        const auto lifetimeOutlives = [&](std::string_view longer, std::string_view shorter)
        {
            if (longer == shorter || longer == "static") return true;
            std::unordered_set<std::string> visited;
            std::vector<std::string> pending{std::string(longer)};
            while (!pending.empty())
            {
                std::string current = std::move(pending.back());
                pending.pop_back();
                if (!visited.insert(current).second) continue;
                for (const auto& [declaredLonger, declaredShorter] :
                     method->lifetimeOutlivesConstraints)
                {
                    if (declaredLonger != current) continue;
                    if (declaredShorter == shorter) return true;
                    pending.push_back(declaredShorter);
                }
            }
            return false;
        };
        const std::string_view targetLifetime = method->returnLifetimeName;
        bool lifetimeMatches = true;
        if (targetLifetime == "this")
            lifetimeMatches = escape.kind == MethodEscapeOriginKind::CurrentReceiver;
        else if (targetLifetime == "static")
            lifetimeMatches = escape.lifetimeName.empty() || escape.lifetimeName == "static";
        else if (!targetLifetime.empty() && !escape.lifetimeName.empty())
            lifetimeMatches = lifetimeOutlives(escape.lifetimeName, targetLifetime);
        if (!lifetimeMatches)
        {
            const std::string sourceLifetime = escape.lifetimeName.empty()
                ? std::string("unknown") : escape.lifetimeName;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return requires lifetime '" + std::string(targetLifetime) +
                    "' but the returned storage has lifetime '" + sourceLifetime +
                    "'; add or correct an outlives constraint");
            return;
        }
        if (escape.ordinal.has_value())
        {
            noteParameterEscape(
                methodMemberOrdinal, *escape.ordinal, language::EscapeClass::Return);
        }
        return;
    }

    if (const std::optional<std::size_t> vlaRoot =
            method->declaredTypeKey.ends_with("*")
                ? variableArrayRootLocalOfExpression(statement->first(), scopeOrdinal)
                : std::nullopt;
        vlaRoot.has_value())
    {
        const std::string name = *vlaRoot < locals_.size()
            ? locals_[*vlaRoot].name : std::string("<variable-length-array>");
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->first()->range(),
            "variable-length array storage rooted at '" + name +
                "' cannot be returned; its stack frame ends with this call");
        return;
    }

    bool directOwnerReturnWithoutMove = false;
    if (method->declaredOwnership != sema::OwnershipKind::Borrowed &&
        statement->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> returnedName = lookupScopeName(scopeOrdinal, statement->first()->text());
        if (returnedName.has_value())
        {
            if (returnedName->kind == ScopeNameKind::Local &&
                returnedName->semanticOrdinal < locals_.size() &&
                (locals_[returnedName->semanticOrdinal].isRaii ||
                 locals_[returnedName->semanticOrdinal].isMoveOnly))
            {
                directOwnerReturnWithoutMove = true;
                const MethodLocalSymbol& local = locals_[returnedName->semanticOrdinal];
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    local.isRaii
                        ? "returning RAII owner '" + local.name +
                            "' requires return move(owner) to transfer ownership"
                        : "returning move-only value '" + local.name +
                            "' requires return move(value)");
            }
            else if (returnedName->kind == ScopeNameKind::Parameter &&
                returnedName->semanticOrdinal < method->parameters.size() &&
                ((method->parameters[returnedName->semanticOrdinal].ownership == sema::OwnershipKind::Owned &&
                  // Stage 52: an "owned T" parameter closed over a Copy value type needs no
                  // ownership ceremony; a move-only value is handled by the next alternative.
                  (isReferenceTypeKey(method->parameters[returnedName->semanticOrdinal].typeKey) ||
                   method->parameters[returnedName->semanticOrdinal].typeKey == "builtin:string")) ||
                 (method->parameters[returnedName->semanticOrdinal].passKind == ParameterPassKind::Value &&
                  memberModel_->typeIsMoveOnly(
                      method->parameters[returnedName->semanticOrdinal].typeKey))))
            {
                directOwnerReturnWithoutMove = true;
                const MethodParameterSymbol& parameter =
                    method->parameters[returnedName->semanticOrdinal];
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    parameter.ownership == sema::OwnershipKind::Owned
                        ? "returning owned parameter '" + parameter.name +
                            "' requires return move(owner) to transfer ownership"
                        : "returning move-only parameter '" + parameter.name +
                            "' requires return move(value)");
            }
        }
    }
    ExpressionResult value;
    if (statement->first()->kind() == ast::AstNodeKind::LambdaExpression && isDelegateTypeKey(method->declaredTypeKey))
        value = bindLambdaToDelegate(
            statement->first(), method->declaredTypeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
    else
        value = analyzeExpression(
            statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            isDelegateTypeKey(method->declaredTypeKey), false, true);
    if (value.methodGroup && isDelegateTypeKey(method->declaredTypeKey))
    {
        value = bindMethodGroupToDelegate(
            statement->first(), value, method->declaredTypeKey,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    if (!value.valid)
    {
        return;
    }

    // NS304-T11-F07: a value struct carrying a ref field whose recorded storage is owned by the
    // current stack frame cannot be returned — the struct copy would keep the dangling pointer.
    if (memberModel_ != nullptr && inheritanceModel_ != nullptr)
    {
        const std::optional<std::size_t> returnedType =
            inheritanceModel_->typeOrdinalForTypeKey(value.typeKey);
        for (const auto& [fieldOrdinal, origin] : refFieldOrigins_)
        {
            if (fieldOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& field = memberModel_->members()[fieldOrdinal];
            if (!field.isReferenceReturn) continue;
            if (!returnedType.has_value() || field.declaringTypeOrdinal != *returnedType) continue;
            if (origin.kind == MethodEscapeOriginKind::CurrentFrameLocal ||
                origin.kind == MethodEscapeOriginKind::VariableLengthArray)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "cannot return a value whose ref field '" + field.name +
                        "' aliases storage owned by the current stack frame");
                return;
            }
        }
    }

    if (value.boundMethodBorrowsReceiver &&
        (value.ownershipSourceLocalOrdinal.has_value() ||
         value.ownershipSourceParameterOrdinal.has_value()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->first()->range(),
            "bound method borrows its receiver and cannot escape through return; use "
            "move(receiver.Method) to transfer receiver ownership explicitly");
        return;
    }

    const ProjectImplicitConversion conversion = classifyImplicit(
        value.typeKey,
        declaredReturnTypeKey,
        value.integerConstant);
    if (!conversion.exists())
    {
        if (!reportTupleConversionFailure(
                value.typeKey, declaredReturnTypeKey, statement->first()->range(), diagnostics))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "cannot convert return expression from '" + value.typeKey + "' to '" +
                    declaredReturnTypeKey + "'");
        }
        return;
    }

    // NativeSharp 3.0 T08-F06. Unlike `inline`/`noinline`, must_tail is a checked
    // source contract. It is accepted only for a direct return-call whose selected callable has
    // the same physical source signature and while no lexical cleanup remains pending.
    if (statement->flag())
    {
        if (statement->first()->kind() != ast::AstNodeKind::CallExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "must_tail return requires a direct call expression");
            return;
        }

        const ClassMemberSymbol* callee = nullptr;
        if (const NamespaceFunctionCallBinding* call =
                namespaceFunctionCall(statement->first());
            call != nullptr && call->resolution == MethodCallResolutionKind::Resolved &&
            call->selectedFunctionOrdinal.has_value())
        {
            callee = methodSymbol(namespaceCallableBase_ + *call->selectedFunctionOrdinal);
        }
        if (callee == nullptr)
        {
            const MethodCallBinding* call = callBinding(statement->first(), methodMemberOrdinal);
            if (call != nullptr &&
                call->resolution == MethodCallResolutionKind::Resolved &&
                call->selectedMemberOrdinal.has_value())
            {
                callee = methodSymbol(*call->selectedMemberOrdinal);
            }
        }
        if (callee == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "must_tail return requires a statically resolved function or method call");
            return;
        }

        const auto functionType = [](const ClassMemberSymbol& callable)
        {
            language::FunctionType type;
            if (callable.hasImplicitThis) type.parameters.emplace_back("abi:this");
            for (const MethodParameterSymbol& parameter : callable.parameters)
                type.parameters.push_back(parameter.typeKey);
            type.result = callable.declaredTypeKey;
            type.callingConvention = callable.isExternC
                ? language::CallingConvention::C
                : language::CallingConvention::NativeSharp;
            type.errorType = callable.declaredErrorTypeKey;
            type.lifetime = callable.returnLifetimeName;
            type.variadic = callable.isVariadic;
            type.isUnsafe = callable.isUnsafe;
            type.isNoHeap = callable.isNoHeap;
            return type;
        };

        std::string mustTailError;
        if (conversion.kind != ProjectConversionKind::Identity)
            mustTailError = "must_tail return cannot apply a result conversion after the call";
        else
            language::validateMustTail(
                functionType(*method), functionType(*callee),
                !activeCleanupRegions_.empty(), mustTailError);
        if (!mustTailError.empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                std::move(mustTailError));
            return;
        }
    }
    if (ProjectTypeConversion::isPointer(method->declaredTypeKey) &&
        !language::pointerContractImplicitlyAssignable(
            pointerContract(
                value.pointerNullable,
                value.pointerPointeeReadonly,
                value.pointerHasProvenance,
                value.pointerKnownAlignment,
                value.pointerAddressSpace),
            pointerContract(
                method->declaredPointerNullable,
                method->declaredPointerPointeeReadonly,
                method->declaredPointerHasProvenance,
                method->declaredPointerKnownAlignment,
                method->declaredPointerAddressSpace)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            statement->first()->range(),
            "return value does not satisfy the declared pointer nullability, mutability, provenance, alignment, or address-space contract");
        return;
    }

    if (method->declaredTypeKey.ends_with("*") ||
        method->declaredOwnership == sema::OwnershipKind::Borrowed ||
        typeKeyIsViewLike(method->declaredTypeKey))
    {
        const MethodEscapeOrigin escape =
            referenceEscapeOriginOfExpression(statement->first(), scopeOrdinal);
        const std::optional<std::size_t> parameterOrdinal = escape.ordinal.has_value()
            ? escape.ordinal : value.ownershipSourceParameterOrdinal;
        if (parameterOrdinal.has_value())
            noteParameterEscape(methodMemberOrdinal, *parameterOrdinal,
                language::EscapeClass::Return);
    }

    // Stage 52: a generic method's declared "owned T"/"borrowed T" return type may monomorphize
    // to a plain value type (int, bool, a struct, ...) for a given closed T. Ownership/borrow
    // provenance tracking is meaningless there -- a value has no lifetime to escape and nothing
    // to transfer -- so the strict transfer/borrow-provenance checks below only apply when the
    // method's ACTUAL closed return type needs ownership tracking. The bookkeeping past each
    // check (registering a forwarding borrow/transfer binding) still runs unconditionally; it
    // is inert for a value-typed return since there is no RAII cleanup to suppress.
    const bool returnTypeOwnable = typeKeyIsOwnable(method->declaredTypeKey);

    // SHR-013 "vistas y tiempo de vida" -- EL CRITERIO DEL BLOQUE 3, y esta es su linea.
    //
    // Devolver una vista enraizada en un local del propio metodo es un uso-despues-de-liberar
    // con la cara de un valor normal: el struct viaja por valor, el puntero que lleva dentro
    // apunta a memoria que acaba de morir, y nada en el tipo de retorno lo delata.
    //
    // Por que NO lo atrapaba la comprobacion de prestamo que ya existia (Stage 50.7): esa mira
    // el CALIFICATIVO `borrowed` sobre un tipo por referencia, y `returnTypeOwnable` de arriba
    // la apaga para cualquier struct por valor. Una vista es un struct por valor. El hueco era
    // exactamente ese, y no se ve leyendo la regla vieja: se ve preguntandole por un tipo nuevo
    // que hasta el Bloque 3 no existia.
    if (typeKeyIsViewLike(method->declaredTypeKey))
    {
        const std::optional<std::string> root =
            viewRootLocalOfExpression(statement->first(), scopeOrdinal);
        if (root.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "view of type '" + method->declaredTypeKey + "' borrows from local '" + *root +
                    "' and cannot be returned: the local dies when the method returns. Take the "
                    "view from a parameter, a field, or return a copy that owns its storage");
            return;
        }
        if (expressionUsesExpandedParameterPack(statement->first(), methodMemberOrdinal))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "view returned from an expanded parameter pack refers to temporary caller "
                "stack storage and cannot escape the current method");
            return;
        }
    }
    // NS304-T13-F03: a `scope` object is frame storage and can never escape via return. Only the
    // returned value itself (a direct identifier naming the scope object) escapes; a field read or
    // a method call on it produces a copy and is legal.
    if (statement->first() != nullptr &&
        statement->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, statement->first()->text());
        if (name.has_value() && name->kind == ScopeNameKind::Local &&
            name->semanticOrdinal < locals_.size() && locals_[name->semanticOrdinal].isScope)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "scope object '" + locals_[name->semanticOrdinal].name +
                    "' lives in the current frame and cannot be returned");
            return;
        }
    }
    if (method->declaredOwnership == sema::OwnershipKind::Borrowed)
    {
        const bool nullBorrow = statement->first()->kind() == ast::AstNodeKind::NullLiteralExpression;
        if (!nullBorrow && value.ownership != sema::OwnershipKind::Borrowed && returnTypeOwnable)
        {
            std::string message = "borrowed return requires a borrowed value whose lifetime outlives the call";
            if (value.ownershipSourceLocalOrdinal.has_value() &&
                *value.ownershipSourceLocalOrdinal < locals_.size())
            {
                message = "borrowed return value outlives local owner '" +
                    locals_[*value.ownershipSourceLocalOrdinal].name + "'";
            }
            else if (value.ownershipSourceParameterOrdinal.has_value() &&
                *value.ownershipSourceParameterOrdinal < method->parameters.size())
            {
                message = "borrowed return value refers to owned parameter '" +
                    method->parameters[*value.ownershipSourceParameterOrdinal].name +
                    "' whose lifetime ends when the method returns";
            }
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                std::move(message));
            return;
        }

        if (!nullBorrow)
        {
            if (!value.borrowedLocalOrdinal.has_value() &&
                value.ownershipSourceLocalOrdinal.has_value())
            {
                const std::size_t ownerOrdinal = *value.ownershipSourceLocalOrdinal;
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "borrowed return value outlives local owner '" +
                        (ownerOrdinal < locals_.size() ? locals_[ownerOrdinal].name
                                                      : std::string("<local-owner>")) + "'");
                return;
            }
            MethodBorrowBinding borrow;
            borrow.syntax = statement->first();
            borrow.methodMemberOrdinal = methodMemberOrdinal;
            borrow.scopeOrdinal = scopeOrdinal;
            borrow.targetMemberOrdinal = methodMemberOrdinal;
            borrow.typeKey = method->declaredTypeKey;

            if (value.borrowedLocalOrdinal.has_value() &&
                *value.borrowedLocalOrdinal < locals_.size())
            {
                const MethodLocalSymbol& local = locals_[*value.borrowedLocalOrdinal];
                if (!local.localBorrowBindingOrdinal.has_value() ||
                    *local.localBorrowBindingOrdinal >= localBorrowBindings_.size())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        "borrowed return value has no valid lifetime provenance");
                    return;
                }

                const MethodLocalBorrowBinding& localBorrow =
                    localBorrowBindings_[*local.localBorrowBindingOrdinal];
                const std::optional<std::size_t> localOwner =
                    localBorrow.sourceOwnerLocalOrdinal.has_value()
                        ? localBorrow.sourceOwnerLocalOrdinal
                        : (!localBorrow.possibleSourceOwnerLocalOrdinals.empty()
                            ? std::optional<std::size_t>(
                                  localBorrow.possibleSourceOwnerLocalOrdinals.front())
                            : std::nullopt);
                if (localOwner.has_value())
                {
                    const std::size_t ownerOrdinal = *localOwner;
                    const std::string ownerName = ownerOrdinal < locals_.size()
                        ? locals_[ownerOrdinal].name : std::string("<local-owner>");
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        "borrowed return value '" + local.name +
                            "' outlives its owner '" + ownerName + "'");
                    return;
                }
                std::optional<std::size_t> parameterOwner =
                    localBorrow.sourceOwnerParameterOrdinal;
                for (const std::size_t candidate :
                     localBorrow.possibleSourceOwnerParameterOrdinals)
                {
                    if (candidate >= method->parameters.size() ||
                        method->parameters[candidate].ownership != sema::OwnershipKind::Borrowed)
                    {
                        parameterOwner = candidate;
                        break;
                    }
                    if (!parameterOwner.has_value()) parameterOwner = candidate;
                }
                if (parameterOwner.has_value())
                {
                    const std::size_t parameterOrdinal = *parameterOwner;
                    if (parameterOrdinal >= method->parameters.size() ||
                        method->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Borrowed)
                    {
                        const std::string ownerName = parameterOrdinal < method->parameters.size()
                            ? method->parameters[parameterOrdinal].name : std::string("<owned-parameter>");
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidReturn,
                            diagnostics::DiagnosticSeverity::Error,
                            statement->first()->range(),
                            "borrowed return value '" + local.name +
                                "' refers to owned parameter '" + ownerName +
                                "' whose lifetime ends when the method returns");
                        return;
                    }
                    borrow.sourceParameterOrdinal = parameterOrdinal;
                }
                borrow.sourceLocalOrdinal = *value.borrowedLocalOrdinal;
                borrow.kind = MethodBorrowKind::BorrowedLocalToBorrowedReturn;
            }
            else if (value.ownershipSourceParameterOrdinal.has_value())
            {
                const std::size_t parameterOrdinal = *value.ownershipSourceParameterOrdinal;
                if (parameterOrdinal >= method->parameters.size() ||
                    method->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Borrowed)
                {
                    const std::string ownerName = parameterOrdinal < method->parameters.size()
                        ? method->parameters[parameterOrdinal].name : std::string("<owned-parameter>");
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        "borrowed return value refers to owned parameter '" + ownerName +
                            "' whose lifetime ends when the method returns");
                    return;
                }
                borrow.sourceParameterOrdinal = parameterOrdinal;
                borrow.kind = MethodBorrowKind::BorrowedParameterToBorrowedReturn;
            }
            else
            {
                // A borrowed-returning call is itself a lifetime contract supplied by the callee.
                // Forwarding it preserves that contract without manufacturing a new owner.
                borrow.kind = MethodBorrowKind::BorrowedReturnForward;
            }
            borrowBindings_.push_back(std::move(borrow));
        }
        return;
    }

    if (method->declaredOwnership == sema::OwnershipKind::None &&
        value.ownership == sema::OwnershipKind::Borrowed)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->first()->range(),
            "borrowed value cannot escape through an unqualified return type; declare the return type borrowed");
        return;
    }

    if (method->declaredOwnership == sema::OwnershipKind::Owned)
    {
        const bool freshOwnedObject =
            statement->first()->kind() == ast::AstNodeKind::ObjectCreationExpression;
        const bool nullOwnedValue =
            statement->first()->kind() == ast::AstNodeKind::NullLiteralExpression;
        const bool validOwnedTransfer =
            (value.ownership == sema::OwnershipKind::Owned && value.ownershipTransfer) ||
            value.ownershipFreshOwner || freshOwnedObject || nullOwnedValue;
        if (!validOwnedTransfer && returnTypeOwnable)
        {
            if (!directOwnerReturnWithoutMove)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "owned return requires an ownership-transfer expression such as move(owner), new T(...), or an owned-returning call");
            }
            return;
        }

        MethodOwnershipTransferBinding transfer;
        transfer.syntax = statement->first();
        transfer.methodMemberOrdinal = methodMemberOrdinal;
        transfer.scopeOrdinal = scopeOrdinal;
        transfer.sourceLocalOrdinal = value.ownershipSourceLocalOrdinal;
        transfer.sourceParameterOrdinal = value.ownershipSourceParameterOrdinal;
        transfer.targetMemberOrdinal = methodMemberOrdinal;
        transfer.typeKey = method->declaredTypeKey;
        if (transfer.sourceLocalOrdinal.has_value())
        {
            transfer.kind = MethodOwnershipTransferKind::MoveLocalToOwnedReturn;
        }
        else if (transfer.sourceParameterOrdinal.has_value())
        {
            transfer.kind = MethodOwnershipTransferKind::MoveParameterToOwnedReturn;
        }
        else
        {
            transfer.kind = MethodOwnershipTransferKind::OwnedReturnForward;
        }
        ownershipTransferBindings_.push_back(std::move(transfer));
    }
}

const MethodNameBinding* ProjectMethodBodyModel::recordedNameBinding(
    const ast::AstNode* syntax,
    std::size_t callableOrdinal) const noexcept
{
    return nameBinding(syntax, callableOrdinal);
}

// The constant folder for `case` labels.
//
// WHY IT EXISTS SEPARATELY. The body model already computes `ExpressionResult::integerConstant`,
// but only for the shapes that need it for overload resolution -- literals, unary minus on a
// literal, casts. A `case` label needs more than that and needs it for a different reason: two
// labels that name the same value must be REJECTED, and "the same value" cannot be decided by
// comparing syntax. `case WM_DESTROY:` and `case 2:` are the same label.
//
// WHAT IT DELIBERATELY DOES NOT DO. It is not a general constant evaluator: no floats, no strings,
// no method calls, no `sizeof`. Anything it cannot answer for comes back as std::nullopt and the
// caller reports "a case label must be a compile-time integer constant", which is a better outcome
// than a folder that quietly gets a hard case wrong.
std::optional<std::size_t> ProjectMethodBodyModel::constantMemberOfType(
    std::size_t typeOrdinal, std::string_view name) const noexcept
{
    if (memberModel_ == nullptr) return std::nullopt;
    for (const std::size_t ordinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind != ClassMemberKind::Field || member.name != name) continue;
        if (member.isConst || member.isEnumConstant) return ordinal;
    }
    return std::nullopt;
}

// CC1-105. El plegado de constantes enteras ya NO se implementa aqui.
//
// Aqui vivia un segundo evaluador de constantes, escrito para las etiquetas de `switch`, que
// operaba todo sobre `std::int64_t` sin consultar el tipo semantico del valor. Produjo cuatro
// defectos P1 de una sola vez -- casts que no normalizaban (C-001), division/modulo/shift sin
// signo evaluados con signo (C-002), minimo-con-signo entre -1 sin proteger (C-003, comportamiento
// indefinido DENTRO del compilador disparado por texto de entrada) y ancho de shift fijo a 64
// (C-004) -- y no por descuido: un evaluador que no conoce el tipo del valor que evalua no puede
// dar la respuesta correcta a ninguna de esas cuatro preguntas.
//
// Ahora esto es un adaptador: `ProjectIntegerEvaluator` hace el trabajo, y este modelo solo le
// presta su resolucion de nombres. Devuelve el par (bits, tipo) para que el llamador pueda
// normalizar al tipo gobernante antes de comparar.
std::optional<ProjectIntegerValue> ProjectMethodBodyModel::foldIntegerConstant(
    const ast::AstNode* expression,
    std::optional<std::size_t> declaringTypeOrdinal,
    std::size_t callableOrdinal) const
{
    const ConstantMemberResolver resolver(*this, callableOrdinal);
    return ProjectIntegerEvaluator(&resolver).evaluate(expression, declaringTypeOrdinal);
}

std::optional<std::size_t> ProjectMethodBodyModel::ConstantMemberResolver::resolveConstantMember(
    const ast::AstNode* syntax,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    // Dos formas llegan aqui y las dos importan: `Color.Red`, y `Messages.Destroy` (un campo
    // `const int`, que es lo que es todo identificador de mensaje de Win32). El nombre ya lo
    // resolvio analyzeExpression, asi que esto lee el binding que dejo en vez de hacer lookup una
    // segunda vez y arriesgar una respuesta distinta.
    if (syntax == nullptr) return std::nullopt;
    const MethodNameBinding* binding = model_.recordedNameBinding(syntax, callableOrdinal_);
    if (binding != nullptr && binding->candidateMemberOrdinals.size() == 1)
    {
        return binding->candidateMemberOrdinals.front();
    }

    // Dentro del inicializador de un campo `const` puede no haber binding que leer: build()
    // analiza los CUERPOS DE METODO ANTES que los inicializadores de campo, asi que cuando una
    // etiqueta en un cuerpo pliega `Messages.Second`, el inicializador `First + 1` todavia no paso
    // por analyzeExpression y su `First` no tiene binding. Resolver un nombre sin calificar contra
    // el propio tipo declarante del campo es exactamente la regla bajo la que ese inicializador
    // esta escrito, y mantiene vivo el idioma de Win32 (`WM_USER + 1`) sin reordenar build().
    if (declaringTypeOrdinal.has_value() &&
        syntax->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        return model_.constantMemberOfType(*declaringTypeOrdinal, syntax->text());
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::ConstantMemberResolver::describeConstantMember(
    std::size_t memberOrdinal,
    bool& isEnumConstant,
    std::int64_t& enumConstantValue,
    std::string& enumUnderlyingTypeKey,
    const ast::AstNode*& initializer,
    std::string& declaredTypeKey,
    std::optional<std::size_t>& declaringTypeOrdinal) const
{
    if (model_.memberModel_ == nullptr ||
        memberOrdinal >= model_.memberModel_->members().size())
    {
        return false;
    }
    const ClassMemberSymbol& member = model_.memberModel_->members()[memberOrdinal];
    if (member.kind != ClassMemberKind::Field) return false;

    isEnumConstant = member.isEnumConstant;
    enumConstantValue = member.enumConstantValue;
    enumUnderlyingTypeKey.clear();
    if (member.isEnumConstant && model_.nameLookup_ != nullptr &&
        member.declaringTypeOrdinal < model_.nameLookup_->namedTypes().size())
    {
        // Nombre completo: el parametro de salida se llama igual que la funcion y la tapa.
        enumUnderlyingTypeKey = std::string(
            ::nativesharp::frontend::enumUnderlyingTypeKey(
                model_.nameLookup_->namedTypes()[member.declaringTypeOrdinal].syntax));
    }
    declaredTypeKey = member.declaredTypeKey;
    declaringTypeOrdinal = member.declaringTypeOrdinal;
    initializer = member.syntax != nullptr ? member.syntax->first() : nullptr;

    if (isEnumConstant) return true;
    return member.isConst && initializer != nullptr;
}

std::string ProjectMethodBodyModel::ConstantMemberResolver::resolveTypeKey(
    const ast::TypeSyntax* typeSyntax) const
{
    if (typeSyntax == nullptr || model_.memberModel_ == nullptr || model_.nameLookup_ == nullptr)
    {
        return {};
    }
    return model_.memberModel_->canonicalTypeKey(typeSyntax, *model_.nameLookup_);
}

// `switch (governing) { case a: ... default: ... }`
//
// THE FOUR THINGS THIS DECIDES, and why each is here rather than downstream:
//
//   1. The governing type must be INTEGRAL or an ENUM. Not `string`: a string switch is a hash
//      table and a chain of comparisons, not a jump table, and pretending otherwise in the syntax
//      would hide that cost. Not `float`: equality on floats is a trap.
//   2. Every `case` label is folded to an integer HERE. Downstream passes get numbers, so neither
//      the HIR, the MIR nor the backend needs to know what a constant expression is.
//   3. DUPLICATE LABELS ARE REJECTED. Two labels with the same value are unreachable code that
//      looks like working code, and LLVM's `switch` has undefined behaviour with a repeated case
//      value, so this is a correctness check and not a style one.
//   4. At most one `default`, in any position.
//
// A switch pushes a LoopContext with isSwitch set, so `break` leaves the switch while `continue`
// walks past it to the enclosing loop. See analyzeLoopExit.
void ProjectMethodBodyModel::analyzeSwitch(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr) return;

    const ExpressionResult governing = analyzeExpression(
        statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);

    const bool governingIsEnum = governing.valid && isEnumTypeKey(governing.typeKey);
    const bool governingIsIntegral =
        governing.valid && ProjectTypeConversion::isIntegral(governing.typeKey);
    if (governing.valid && !governingIsEnum && !governingIsIntegral)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            statement->first() != nullptr ? statement->first()->range() : statement->range(),
            "switch governing expression must have an integral or enum type; it has type '" +
                governing.typeKey + "'");
    }

    MethodSwitchBinding binding;
    binding.syntax = statement;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.governingTypeKey = governing.typeKey;

    // Every label value already claimed, across the whole switch -- sections do not get their own
    // namespace, so this is one flat list.
    std::vector<std::int64_t> seenValues;
    const ast::AstNode* defaultLabel = nullptr;

    for (const ast::AstNode* section : statement->children())
    {
        if (section == nullptr || section->kind() != ast::AstNodeKind::SwitchSection) continue;

        MethodSwitchSectionBinding sectionBinding;
        sectionBinding.syntax = section;

        const std::size_t labelCount = static_cast<std::size_t>(section->integerValue());
        for (std::size_t index = 0; index < labelCount && index < section->children().size(); ++index)
        {
            const ast::AstNode* label = section->children()[index];
            if (label == nullptr || label->kind() != ast::AstNodeKind::SwitchLabel) continue;

            if (label->flag())
            {
                if (defaultLabel != nullptr)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        label->range(),
                        "a switch may declare only one default label");
                    continue;
                }
                defaultLabel = label;
                sectionBinding.hasDefault = true;
                continue;
            }

            const ExpressionResult value = analyzeExpression(
                label->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!value.valid) continue;

            if (governing.valid)
            {
                const ProjectImplicitConversion conversion =
                    classifyImplicit(value.typeKey, governing.typeKey, value.integerConstant);
                if (!conversion.exists())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidConversion,
                        diagnostics::DiagnosticSeverity::Error,
                        label->range(),
                        "case label of type '" + value.typeKey +
                            "' cannot be converted to the switch governing type '" +
                            governing.typeKey + "'");
                    continue;
                }
            }

            const std::optional<ProjectIntegerValue> folded =
                foldIntegerConstant(label->first(), std::nullopt, methodMemberOrdinal);
            if (!folded.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::ConstRequiresConstant,
                    diagnostics::DiagnosticSeverity::Error,
                    label->range(),
                    "a case label must be a compile-time integer constant");
                continue;
            }

            // CC1-106. LA NORMALIZACION VA ANTES DE COMPARAR, y esa es toda la correccion de C-001.
            //
            // Una etiqueta se pliega en el tipo de SU PROPIA expresion, que puede ser mas ancho que
            // el tipo gobernante. El backend, en cambio, materializa la constante con el ancho del
            // gobernante. Comparar sin normalizar deja que el frontend vea dos numeros distintos
            // donde LLVM vera dos casos identicos -- y un `switch` de LLVM con un valor de caso
            // repetido es comportamiento indefinido.
            //
            // El caso concreto: gobernante `char`, `case 65:` y `case 321:`. Antes de CC1-100 la
            // conversion constante aceptaba 321 hacia `char`, el frontend los veia distintos, y el
            // backend emitia `i8 65` para los dos.
            const std::int64_t canonical = static_cast<std::int64_t>(
                governing.valid && ProjectIntegerEvaluator::isIntegerType(governing.typeKey)
                    ? ProjectIntegerEvaluator::normalize(folded->bits, governing.typeKey)
                    : folded->bits);

            bool duplicate = false;
            for (const std::int64_t seen : seenValues)
            {
                if (seen != canonical) continue;
                duplicate = true;
                // No es una regla de estilo: el `switch` de LLVM tiene comportamiento indefinido con
                // un valor de caso repetido, y la segunda etiqueta es codigo muerto que se lee como
                // codigo vivo.
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    label->range(),
                    "duplicate case label with value " + std::to_string(canonical));
                break;
            }
            if (duplicate) continue;

            // CC1-107. Aguas abajo viaja el valor YA CANONICO, para que HIR, MIR y el backend
            // reciban exactamente el patron de bits que LLVM materializara. Si el binding llevara el
            // valor sin normalizar, la comprobacion de duplicados de arriba estaria protegiendo un
            // numero distinto del que acaba en la instruccion.
            seenValues.push_back(canonical);
            sectionBinding.labelValues.push_back(canonical);
        }

        binding.sections.push_back(std::move(sectionBinding));
    }

    // Every section starts from the state at the switch, never from the previous section's exit:
    // there is no fall-through, so the previous section's effects cannot reach this one.
    const OwnershipFlowSnapshot switchEntry = captureOwnershipFlow();
    loopStack_.push_back(LoopContext{
        scopeOrdinal,
        activeCleanupRegions_.size(),
        activeCleanupRegions_.size(),
        {},
        {},
        true,
        nullptr,
        {},
        false,
        {}});

    std::vector<OwnershipFlowSnapshot> sectionExitStates;
    for (const ast::AstNode* section : statement->children())
    {
        if (section == nullptr || section->kind() != ast::AstNodeKind::SwitchSection) continue;
        restoreOwnershipFlow(switchEntry);
        analyzeBlock(section, methodMemberOrdinal, scopeOrdinal, true, diagnostics);
        const OwnershipFlowSnapshot exitState = captureOwnershipFlow();
        if (exitState.reachable)
        {
            sectionExitStates.push_back(exitState);
        }
    }

    LoopContext switchContext = std::move(loopStack_.back());
    loopStack_.pop_back();

    // Control reaches the statement after the switch three ways: off the end of a section, through
    // a `break`, or -- only when there is no `default` -- by matching nothing at all.
    std::vector<OwnershipFlowSnapshot> exitStates = std::move(switchContext.breakStates);
    for (OwnershipFlowSnapshot& state : sectionExitStates)
    {
        exitStates.push_back(std::move(state));
    }
    OwnershipFlowSnapshot seed;
    if (defaultLabel != nullptr)
    {
        seed.reachable = false;
    }
    else
    {
        seed = switchEntry;
    }
    restoreOwnershipFlow(joinOwnershipFlows(seed, exitStates));

    switchBindings_.push_back(std::move(binding));
}

bool ProjectMethodBodyModel::analyzeLoopExit(
    const ast::AstNode* statement,
    MethodStructuredExitKind kind,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (loopStack_.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            statement != nullptr ? statement->range() : SourceRange(),
            kind == MethodStructuredExitKind::Break
                ? "break is only valid within a loop or a switch"
                : "continue is only valid within a loop");
        return false;
    }

    // `break` binds to the innermost frame, loop or switch alike -- that is what makes `break`
    // inside a case leave the switch. `continue` binds to the innermost LOOP, so it walks past
    // every switch frame on the way out: a `continue` inside a `switch` inside a `while` continues
    // the while. Getting this backwards is not a compile error anywhere downstream; it is a jump
    // to the wrong label.
    std::size_t targetLoopIndex = loopStack_.size() - 1;
    if (statement != nullptr && !statement->text().empty())
    {
        bool found = false;
        for (std::size_t index = loopStack_.size(); index > 0; --index)
        {
            const LoopContext& candidate = loopStack_[index - 1];
            if (!candidate.isSwitch && candidate.label == statement->text())
            {
                targetLoopIndex = index - 1;
                found = true;
                break;
            }
        }
        if (!found)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                std::string(kind == MethodStructuredExitKind::Break ? "break" : "continue") +
                    " targets unknown loop label '" + statement->text() + "'");
            return false;
        }
    }
    else if (kind == MethodStructuredExitKind::Continue)
    {
        while (loopStack_[targetLoopIndex].isSwitch)
        {
            if (targetLoopIndex == 0)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement != nullptr ? statement->range() : SourceRange(),
                    "continue is only valid within a loop");
                return false;
            }
            --targetLoopIndex;
        }
    }

    for (const FinallyContext& finallyContext : finallyStack_)
    {
        if (targetLoopIndex < finallyContext.loopDepthAtEntry)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                statement != nullptr ? statement->range() : SourceRange(),
                kind == MethodStructuredExitKind::Break
                    ? "break cannot leave a finally block"
                    : "continue cannot leave a finally block");
            return false;
        }
    }

    if (kind == MethodStructuredExitKind::Break)
    {
        if (loopStack_[targetLoopIndex].isExpression)
        {
            ExpressionResult value;
            if (statement != nullptr && statement->first() != nullptr)
            {
                value = analyzeExpression(
                    statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                    false, false, true);
            }
            else
            {
                value.typeKey = "builtin:void";
                value.valid = true;
            }
            LoopContext& targetLoop = loopStack_[targetLoopIndex];
            if (value.valid && value.ownership == sema::OwnershipKind::Borrowed)
            {
                std::optional<std::size_t> borrowedRoot = value.ownershipSourceLocalOrdinal;
                if (!borrowedRoot.has_value()) borrowedRoot = value.borrowedLocalOrdinal;
                if (borrowedRoot.has_value() && *borrowedRoot < locals_.size() &&
                    scopeContains(targetLoop.targetScopeOrdinal, locals_[*borrowedRoot].scopeOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->range(),
                        "loop break value borrows from local '" + locals_[*borrowedRoot].name +
                            "' whose lifetime ends when the loop expression exits");
                    value.valid = false;
                }
            }
            targetLoop.breakValues.push_back(LoopContext::BreakValue{statement, value});
            if (value.valid && value.typeKey != "builtin:never")
                targetLoop.breakStates.push_back(captureOwnershipFlow());
        }
        else
        {
            if (statement != nullptr && statement->first() != nullptr)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "break with a value may target only a loop expression");
                return false;
            }
            loopStack_[targetLoopIndex].breakStates.push_back(captureOwnershipFlow());
        }
    }
    else
    {
        loopStack_[targetLoopIndex].continueStates.push_back(captureOwnershipFlow());
    }
    const LoopContext& loop = loopStack_[targetLoopIndex];
    recordStructuredExit(
        kind,
        statement,
        methodMemberOrdinal,
        scopeOrdinal,
        loop.targetScopeOrdinal,
        kind == MethodStructuredExitKind::Break
            ? loop.breakCleanupDepth
            : loop.continueCleanupDepth);
    return true;
}

std::size_t ProjectMethodBodyModel::createCleanupRegion(
    MethodCleanupRegionKind kind,
    const ast::AstNode* syntax,
    const ast::AstNode* cleanupSyntax,
    std::size_t methodMemberOrdinal,
    std::size_t ownerScopeOrdinal)
{
    MethodCleanupRegion region;
    region.kind = kind;
    region.syntax = syntax;
    region.cleanupSyntax = cleanupSyntax;
    region.methodMemberOrdinal = methodMemberOrdinal;
    region.ownerScopeOrdinal = ownerScopeOrdinal;
    region.depth = activeCleanupRegions_.size();
    if (!activeCleanupRegions_.empty())
    {
        region.parentCleanupRegionOrdinal = activeCleanupRegions_.back();
    }

    const std::size_t ordinal = cleanupRegions_.size();
    cleanupRegions_.push_back(std::move(region));
    return ordinal;
}

std::vector<std::size_t> ProjectMethodBodyModel::cleanupPathFromDepth(std::size_t retainedDepth) const
{
    retainedDepth = std::min(retainedDepth, activeCleanupRegions_.size());
    std::vector<std::size_t> path;
    path.reserve(activeCleanupRegions_.size() - retainedDepth);
    for (std::size_t index = activeCleanupRegions_.size(); index > retainedDepth; --index)
    {
        path.push_back(activeCleanupRegions_[index - 1]);
    }
    return path;
}

void ProjectMethodBodyModel::recordStructuredExit(
    MethodStructuredExitKind kind,
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t sourceScopeOrdinal,
    std::optional<std::size_t> targetScopeOrdinal,
    std::size_t retainedCleanupDepth)
{
    MethodStructuredExitBinding binding;
    binding.kind = kind;
    binding.syntax = syntax;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.sourceScopeOrdinal = sourceScopeOrdinal;
    binding.targetScopeOrdinal = targetScopeOrdinal;
    binding.cleanupRegionOrdinals = cleanupPathFromDepth(retainedCleanupDepth);
    recordCleanupContinuations(binding);
    structuredExits_.push_back(std::move(binding));
}

void ProjectMethodBodyModel::recordCleanupContinuations(const MethodStructuredExitBinding& exit)
{
    MethodCleanupContinuationKind continuationKind = MethodCleanupContinuationKind::Return;
    switch (exit.kind)
    {
        case MethodStructuredExitKind::Return:
            continuationKind = MethodCleanupContinuationKind::Return;
            break;
        case MethodStructuredExitKind::Break:
            continuationKind = MethodCleanupContinuationKind::Break;
            break;
        case MethodStructuredExitKind::Continue:
            continuationKind = MethodCleanupContinuationKind::Continue;
            break;
    }

    for (std::size_t index = 0; index < exit.cleanupRegionOrdinals.size(); ++index)
    {
        MethodCleanupContinuation continuation;
        continuation.kind = continuationKind;
        continuation.syntax = exit.syntax;
        continuation.methodMemberOrdinal = exit.methodMemberOrdinal;
        continuation.sourceScopeOrdinal = exit.sourceScopeOrdinal;
        continuation.cleanupRegionOrdinal = exit.cleanupRegionOrdinals[index];
        if (index + 1 < exit.cleanupRegionOrdinals.size())
        {
            continuation.nextCleanupRegionOrdinal = exit.cleanupRegionOrdinals[index + 1];
        }
        continuation.targetScopeOrdinal = exit.targetScopeOrdinal;
        cleanupContinuations_.push_back(std::move(continuation));
    }
}

void ProjectMethodBodyModel::recordNormalCleanupContinuation(
    std::size_t cleanupRegionOrdinal,
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t sourceScopeOrdinal)
{
    MethodCleanupContinuation continuation;
    continuation.kind = MethodCleanupContinuationKind::Normal;
    continuation.syntax = syntax;
    continuation.methodMemberOrdinal = methodMemberOrdinal;
    continuation.sourceScopeOrdinal = sourceScopeOrdinal;
    continuation.cleanupRegionOrdinal = cleanupRegionOrdinal;
    cleanupContinuations_.push_back(std::move(continuation));
}
}

