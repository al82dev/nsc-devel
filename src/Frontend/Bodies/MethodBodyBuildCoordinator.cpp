#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Language/TraitSemantics.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;
using method_body_detail::resetMethodBodyTrace;
using method_body_detail::writeMethodBodyTrace;

void ProjectMethodBodyModel::initializeNamespaceCallableAdapters(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel)
{
    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    namespaceFunctionModel_ = &namespaceFunctionModel;
    expressionTypeBindingIndex_.clear();
    callBindingIndex_.clear();
    nameBindingIndex_.clear();
    constructionSites_.clear();
    constructionSiteIndex_.clear();
    nextConstructionSequenceByCallable_.clear();
    namespaceCallableBase_ = memberModel.members().size();
    namespaceCallableAdapters_.clear();
    namespaceCallableAdapters_.reserve(namespaceFunctionModel.functions().size());
    for (std::size_t functionOrdinal = 0;
         functionOrdinal < namespaceFunctionModel.functions().size(); ++functionOrdinal)
    {
        const NamespaceFunctionSymbol& function = namespaceFunctionModel.functions()[functionOrdinal];
        ClassMemberSymbol adapter;
        adapter.kind = ClassMemberKind::Method;
        adapter.accessibility = function.accessibility == NamespaceFunctionAccessibility::Public
            ? ClassMemberAccessibility::Public : ClassMemberAccessibility::Internal;
        adapter.declaringTypeOrdinal = nameLookup.namedTypes().size();
        adapter.sourceOrdinal = function.sourceOrdinal;
        adapter.declarationOrdinal = functionOrdinal;
        adapter.fileId = function.fileId;
        adapter.range = function.range;
        adapter.syntax = function.definitionSyntax;
        for (const NamespaceFunctionPart& part : function.parts)
        {
            if (!part.hasBody) continue;
            adapter.sourceOrdinal = part.sourceOrdinal;
            adapter.fileId = part.fileId;
            adapter.range = part.range;
            break;
        }
        adapter.name = function.name;
        adapter.declaredTypeSyntax = function.declaredTypeSyntax;
        adapter.declaredTypeKey = function.declaredTypeKey;
        adapter.declaredErrorTypeSyntax = function.declaredErrorTypeSyntax;
        adapter.declaredErrorTypeKey = function.declaredErrorTypeKey;
        adapter.effects = function.effects;
        adapter.declaredOwnership = function.declaredOwnership;
        adapter.parameters = function.parameters;
        adapter.signatureKey = function.signatureKey;
        adapter.hasImplicitThis = false;
        adapter.isStatic = true;
        adapter.isReferenceReturn = function.isReferenceReturn;
        adapter.returnLifetimeName = function.returnLifetimeName;
        adapter.lifetimeParameters = function.lifetimeParameters;
        adapter.lifetimeOutlivesConstraints = function.lifetimeOutlivesConstraints;
        adapter.parameterEscapeSummary = function.parameterEscapeSummary;
        adapter.isVariadic = function.isVariadic;
        adapter.inliningPolicy = function.inliningPolicy;
        // NS304-T08-F04: an extension operator adapter is a user operator with its trait key. The
        // operator is detected by its operator method name (op_Addition), not the operatorToken.
        const language::OperatorTraitContract* contract =
            language::operatorTraitContractForMethod(function.name, function.parameters.size());
        adapter.isUserOperator = contract != nullptr;
        if (adapter.isUserOperator) adapter.operatorTraitKey = std::string(contract->langItem);
        namespaceCallableAdapters_.push_back(std::move(adapter));
    }
    namespaceCallableAdaptersView_ = namespaceCallableAdapters_;
}

void ProjectMethodBodyModel::validateDeclaredErrorTypes(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // T07-F02 validates every declared error in the completed type graph. Name binding has
    // already canonicalized the syntax; inheritance now proves that the contract is a native
    // Exception reference rather than an arbitrary payload type.
    const std::optional<std::size_t> exceptionRoot = exceptionRootTypeOrdinal();
    const std::size_t callableCount =
        memberModel.members().size() + namespaceCallableAdapters().size();
    for (std::size_t ordinal = 0; ordinal < callableCount; ++ordinal)
    {
        const ClassMemberSymbol* callable = methodSymbol(ordinal);
        if (callable == nullptr || callable->declaredErrorTypeKey.empty()) continue;
        const std::optional<std::size_t> errorOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(callable->declaredErrorTypeKey);
        if (!exceptionRoot.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                callable->range,
                "NativeSharp.Core.Exception must be declared before throws contracts can be used");
        }
        else if (!errorOrdinal.has_value() || !isExceptionTypeOrdinal(*errorOrdinal))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                callable->range,
                "throws type '" + callable->declaredErrorTypeKey +
                    "' must be NativeSharp.Core.Exception or derive from it");
        }
    }
    for (const NamedTypeDeclaration& type : nameLookup.namedTypes())
    {
        if (type.kind != NamedTypeDeclarationKind::Delegate || type.syntax == nullptr ||
            type.syntax->throwsTypeSyntax() == nullptr) continue;
        const std::string errorTypeKey = memberModel.canonicalTypeKey(
            type.syntax->throwsTypeSyntax(), nameLookup);
        const std::optional<std::size_t> errorOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(errorTypeKey);
        if (exceptionRoot.has_value() && errorOrdinal.has_value() &&
            isExceptionTypeOrdinal(*errorOrdinal)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "delegate throws type '" + errorTypeKey +
                "' must be NativeSharp.Core.Exception or derive from it");
    }
}

void ProjectMethodBodyModel::initializeParameterEscapeSummaries(
    const ProjectMemberModel& memberModel)
{
    parameterEscapeSummaries_.clear();
    parameterEscapeSummaries_.resize(
        memberModel.members().size() + namespaceCallableAdapters().size());
    for (std::size_t callableOrdinal = 0;
         callableOrdinal < parameterEscapeSummaries_.size(); ++callableOrdinal)
    {
        const ClassMemberSymbol* callable = methodSymbol(callableOrdinal);
        if (callable == nullptr) continue;
        std::vector<language::EscapeClass>& summary = parameterEscapeSummaries_[callableOrdinal];
        if (callable->parameterEscapeSummary.size() == callable->parameters.size())
        {
            summary = callable->parameterEscapeSummary;
            continue;
        }
        const bool hasBody = callable->syntax != nullptr && callable->syntax->first() != nullptr;
        const bool conservative = callable->isPrecompiled || callable->isImportedC ||
            callable->isExternC || !hasBody || callable->genericTemplateOrdinal.has_value() ||
            !callable->genericTemplateKey.empty();
        summary.assign(
            callable->parameters.size(),
            conservative ? language::EscapeClass::Unknown : language::EscapeClass::NoEscape);
    }
}

bool ProjectMethodBodyModel::isDeferredGenericMethod(
    std::size_t ordinal,
    const ProjectMemberModel& memberModel,
    const ProjectNameLookup& nameLookup) const
{
    if (ordinal >= memberModel.members().size()) return false;
    const ClassMemberSymbol& candidate = memberModel.members()[ordinal];
    if (candidate.kind != ClassMemberKind::Method || candidate.isVirtual ||
        candidate.isOverride || candidate.isAbstract) return false;
    constexpr std::string_view prefix = "stage44-specialization:";
    if (candidate.syntax != nullptr && candidate.syntax->auxiliaryText().starts_with(prefix))
    {
        return true;
    }
    return candidate.declaringTypeOrdinal < nameLookup.namedTypes().size() &&
        nameLookup.namedTypes()[candidate.declaringTypeOrdinal].syntax != nullptr &&
        nameLookup.namedTypes()[candidate.declaringTypeOrdinal].syntax->auxiliaryText().starts_with(prefix);
}

bool ProjectMethodBodyModel::declaringTypeIsPubliclyReachable(
    std::size_t typeOrdinal,
    const ProjectNameLookup& nameLookup,
    std::optional<std::size_t> publicSurfaceProjectOrdinal) const
{
    if (!publicSurfaceProjectOrdinal.has_value()) return false;
    while (typeOrdinal < nameLookup.namedTypes().size())
    {
        const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
        if (type.projectOrdinal != *publicSurfaceProjectOrdinal ||
            type.accessibility != NamedTypeAccessibility::Public) return false;
        if (!type.containingTypeOrdinal.has_value()) return true;
        typeOrdinal = *type.containingTypeOrdinal;
    }
    return false;
}

void ProjectMethodBodyModel::seedDemandedGenericMethods(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInterfaceModel& interfaceModel,
    std::optional<std::size_t> publicSurfaceProjectOrdinal,
    std::unordered_set<std::size_t>& demandedGenericMethods) const
{
    // A library publishes the complete callable surface of every public closed generic type it
    // owns. Demand those bodies even when the producer never calls them itself: a consumer can.
    // Executables pass no public-surface ordinal and retain the ordinary reachability-only policy.
    // Private/internal helpers are discovered transitively from these roots by the call/delegate
    // closure below, exactly as they are for roots demanded by source code.
    if (publicSurfaceProjectOrdinal.has_value())
    {
        for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
        {
            const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
            const bool callableAccessibility =
                member.accessibility == ClassMemberAccessibility::Public ||
                member.accessibility == ClassMemberAccessibility::Protected;
            if (callableAccessibility &&
                isDeferredGenericMethod(memberOrdinal, memberModel, nameLookup) &&
                declaringTypeIsPubliclyReachable(
                    member.declaringTypeOrdinal, nameLookup, publicSurfaceProjectOrdinal))
            {
                demandedGenericMethods.insert(memberOrdinal);
            }
        }
    }
    // A concrete interface table is an executable reference just like a direct call or delegate
    // conversion. A method cloned from `Box<T>` is otherwise deferred, and an invocation through
    // `IValue` selects the interface contract member rather than the concrete `Box<int>.Get`
    // member. Waiting for the call/delegate closure below therefore leaves the implementation
    // without bindings: Stage 36 can see the semantic slot but HIR has no function to put in it.
    // Seed every concrete implementation selected by ProjectInterfaceModel before analyzing the
    // transitive call graph. Constraint-invalid specializations remain protected by the normal
    // specializationSatisfiesConstraints guard inside the loop.
    for (const ClassInterfaceTablePlan& plan : interfaceModel.tablePlans())
    {
        for (const std::optional<std::size_t> implementation : plan.implementationMemberOrdinals)
        {
            if (implementation.has_value() &&
                isDeferredGenericMethod(*implementation, memberModel, nameLookup))
            {
                demandedGenericMethods.insert(*implementation);
            }
        }
    }
}

std::vector<std::size_t> ProjectMethodBodyModel::createCallableWorklist(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const std::unordered_set<std::size_t>& demandedGenericMethods) const
{
    std::vector<std::size_t> callableWorklist;
    callableWorklist.reserve(memberModel.members().size());
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.isPrecompiled ||
            ((member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor &&
              member.kind != ClassMemberKind::Destructor) ||
             member.syntax == nullptr || member.syntax->first() == nullptr) ||
            (isDeferredGenericMethod(memberOrdinal, memberModel, nameLookup) &&
             !demandedGenericMethods.contains(memberOrdinal))) continue;
        callableWorklist.push_back(memberOrdinal);
    }
    for (std::size_t functionOrdinal = 0;
         functionOrdinal < namespaceCallableAdapters().size(); ++functionOrdinal)
    {
        const ClassMemberSymbol& callable = namespaceCallableAdapters()[functionOrdinal];
        if (callable.syntax != nullptr && callable.syntax->first() != nullptr)
        {
            callableWorklist.push_back(namespaceCallableBase_ + functionOrdinal);
        }
    }
    return callableWorklist;
}

std::vector<std::size_t> ProjectMethodBodyModel::collectEligibleCallables(
    const std::vector<std::size_t>& callableWorklist,
    std::size_t& workCursor,
    std::size_t waveOrdinal,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectGenericModel* genericModel,
    const ProjectGenericConstraintModel* genericConstraintModel)
{
    const std::size_t waveEnd = callableWorklist.size();
    std::vector<std::size_t> eligibleCallables;
    eligibleCallables.reserve(waveEnd - workCursor);
    for (; workCursor < waveEnd; ++workCursor)
    {
        const ProjectCallableAnalysisContext context(
            callableWorklist[workCursor], waveOrdinal,
            nameLookup, memberModel, namespaceFunctionModel,
            inheritanceModel, interfaceModel, genericModel, genericConstraintModel);
        if (!callableCanBeAnalyzed(context))
        {
            ++callableAnalysisStatistics_.callablesSkipped;
            continue;
        }
        eligibleCallables.push_back(context.callableOrdinal());
    }
    return eligibleCallables;
}

void ProjectMethodBodyModel::mergeCallableWave(
    std::vector<ProjectCallableAnalysisResult> waveResults,
    std::size_t waveOrdinal,
    std::size_t analyzedCount,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectMethodBodyBuildOptions& options,
    std::unordered_set<std::size_t>& demandedGenericMethods,
    std::vector<std::size_t>& callableWorklist,
    diagnostics::DiagnosticEngine& diagnostics)
{
    callableAnalysisStatistics_.callablesAnalyzed += waveResults.size();
    callableAnalysisStatistics_.shardsCreated += waveResults.size();
    const auto mergeStarted = options.phaseTimingSink
        ? std::chrono::steady_clock::now()
        : std::chrono::steady_clock::time_point{};
    const std::vector<std::size_t> discoveries = ProjectCallableWaveReducer::reduce(
        *this, std::move(waveResults), diagnostics);
    if (options.phaseTimingSink)
    {
        const std::string phase = "method-bodies-wave-" +
            std::to_string(waveOrdinal) + "-merge";
        options.phaseTimingSink(
            phase,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - mergeStarted));
    }
    std::vector<std::size_t> newlyDemanded;
    for (const std::size_t target : discoveries)
    {
        if (isDeferredGenericMethod(target, memberModel, nameLookup) &&
            demandedGenericMethods.insert(target).second) newlyDemanded.push_back(target);
    }
    callableWorklist.insert(
        callableWorklist.end(), newlyDemanded.begin(), newlyDemanded.end());
    callableAnalysisStatistics_.callablesQueued += newlyDemanded.size();
    writeMethodBodyTrace(
        "WAVE-END",
        "wave=" + std::to_string(waveOrdinal) +
            "|analyzed=" + std::to_string(analyzedCount) +
            "|discovered=" + std::to_string(newlyDemanded.size()));
}

bool ProjectMethodBodyModel::analyzeCallableWaves(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectGenericModel* genericModel,
    const ProjectGenericConstraintModel* genericConstraintModel,
    const ProjectMethodBodyBuildOptions& options,
    std::unordered_set<std::size_t>& demandedGenericMethods,
    std::vector<std::size_t>& callableWorklist,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::size_t workCursor = 0;
    std::size_t waveOrdinal = 0;
    ProjectCallableWaveScheduler waveScheduler(options.workerCount, options.workerPool);
    callableAnalysisStatistics_.requestedWorkers = waveScheduler.requestedWorkerCount();
    callableAnalysisStatistics_.effectiveWorkers = waveScheduler.effectiveWorkerCount();
    while (workCursor < callableWorklist.size())
    {
        std::vector<std::size_t> eligibleCallables = collectEligibleCallables(
            callableWorklist, workCursor, waveOrdinal, nameLookup, memberModel,
            namespaceFunctionModel, inheritanceModel, interfaceModel,
            genericModel, genericConstraintModel);
        callableAnalysisStatistics_.maximumWaveWidth = std::max(
            callableAnalysisStatistics_.maximumWaveWidth, eligibleCallables.size());
        if (waveScheduler.usesParallelWave(eligibleCallables.size()))
            ++callableAnalysisStatistics_.parallelWaves;
        else
            ++callableAnalysisStatistics_.synchronousWaves;
        std::vector<ProjectCallableAnalysisResult> waveResults;
        std::string waveError;
        writeMethodBodyTrace(
            "WAVE-BEGIN",
            "wave=" + std::to_string(waveOrdinal) +
                "|callables=" + std::to_string(eligibleCallables.size()));
        const bool waveComplete = waveScheduler.run(
            eligibleCallables,
            waveOrdinal,
            [&](std::size_t callableOrdinal, std::size_t scheduledWave) {
                return analyzeCallableIsolated(ProjectCallableAnalysisContext(
                    callableOrdinal, scheduledWave,
                    nameLookup, memberModel, namespaceFunctionModel,
                    inheritanceModel, interfaceModel, genericModel,
                    genericConstraintModel));
            },
            options.cancellationCheck,
            waveResults,
            waveError,
            options.phaseTimingSink);
        if (!waveComplete)
        {
            if (options.executionError != nullptr) *options.executionError = std::move(waveError);
            return false;
        }
        const std::size_t analyzedCount = eligibleCallables.size();
        mergeCallableWave(
            std::move(waveResults), waveOrdinal, analyzedCount, nameLookup, memberModel, options,
            demandedGenericMethods, callableWorklist, diagnostics);
        ++waveOrdinal;
    }
    return true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::bindFieldInitializerExpression(
    std::size_t memberOrdinal,
    const ClassMemberSymbol& field,
    std::size_t rootScopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (field.syntax->first()->kind() == ast::AstNodeKind::AggregateInitializerExpression)
    {
        // C7 "aggregate initializer": a field is filled against its declared type exactly the
        // way a local is. Same binder, so `V field = {1, 2};` and `V local = {1, 2};` cannot
        // disagree about arity, element types or partial fills.
        return bindAggregateInitializer(
            field.syntax->first(), field.declaredTypeKey,
            memberOrdinal, rootScopeOrdinal, diagnostics);
    }
    if (field.syntax->first()->kind() == ast::AstNodeKind::LambdaExpression &&
        isDelegateTypeKey(field.declaredTypeKey))
    {
        return bindLambdaToDelegate(
            field.syntax->first(), field.declaredTypeKey,
            memberOrdinal, rootScopeOrdinal, diagnostics);
    }
    ExpressionResult initializer = analyzeExpression(
        field.syntax->first(), memberOrdinal, rootScopeOrdinal,
        diagnostics, isDelegateTypeKey(field.declaredTypeKey));
    if (initializer.methodGroup && isDelegateTypeKey(field.declaredTypeKey))
    {
        initializer = bindMethodGroupToDelegate(
            field.syntax->first(), initializer, field.declaredTypeKey,
            memberOrdinal, rootScopeOrdinal, diagnostics);
    }
    return initializer;
}

bool ProjectMethodBodyModel::validateFieldInitializerConversion(
    const ClassMemberSymbol& field,
    const ExpressionResult& initializer,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (ProjectTypeConversion::isPointer(field.declaredTypeKey) &&
        !language::pointerContractImplicitlyAssignable(
            pointerContract(
                initializer.pointerNullable,
                initializer.pointerPointeeReadonly,
                initializer.pointerHasProvenance,
                initializer.pointerKnownAlignment,
                initializer.pointerAddressSpace),
            pointerContract(
                field.declaredPointerNullable,
                field.declaredPointerPointeeReadonly,
                field.declaredPointerHasProvenance,
                field.declaredPointerKnownAlignment,
                field.declaredPointerAddressSpace)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            field.syntax->first()->range(),
            "field initializer does not satisfy the destination pointer nullability, mutability, "
            "provenance, alignment, or address-space contract");
        return false;
    }
    const ProjectImplicitConversion conversion = classifyImplicit(
        initializer.typeKey, field.declaredTypeKey, initializer.integerConstant);
    if (!conversion.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            field.syntax->first()->range(),
            "cannot implicitly convert field initializer from '" + initializer.typeKey +
                "' to '" + field.declaredTypeKey + "'");
    }
    return true;
}

void ProjectMethodBodyModel::analyzeFieldInitializer(
    std::size_t memberOrdinal,
    const ClassMemberSymbol& field,
    diagnostics::DiagnosticEngine& diagnostics)
{
    currentCallableLocalBase_ = locals_.size();
    currentCallableParameterCount_ = 0;
    const std::size_t rootScopeOrdinal = createScope(
        MethodScopeKind::FieldInitializer, memberOrdinal, std::nullopt);
    const ExpressionResult initializer = bindFieldInitializerExpression(
        memberOrdinal, field, rootScopeOrdinal, diagnostics);
    if (!initializer.valid || initializer.methodGroup) return;
    if (!validateFieldInitializerConversion(field, initializer, diagnostics)) return;
    if (field.isConst &&
        !ProjectTypeConversion::isIntegral(field.declaredTypeKey) &&
        field.declaredTypeKey != "builtin:bool" &&
        field.declaredTypeKey != "builtin:float" &&
        field.declaredTypeKey != "builtin:double" &&
        field.declaredTypeKey != "builtin:string" &&
        field.declaredTypeKey != "builtin:decimal")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            field.syntax->range(),
            "const fields are limited to integral, bool, float, double, and string types");
    }
}

void ProjectMethodBodyModel::analyzeFieldInitializers(
    const ProjectMemberModel& memberModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Stage 37: field initializers are semantic executable contexts too.  They deliberately have
    // no implicit-this context: an initializer may use literals, locals do not exist, and static
    // members may be referenced, but partially-constructed instance state is not addressable.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind != ClassMemberKind::Field ||
            field.syntax == nullptr || field.syntax->first() == nullptr) continue;
        // Stage 51.x: an enum constant's value was already validated and computed by
        // ProjectMemberModel::collectEnumMembers (plain integer literal or auto-increment,
        // range-checked against NativeSharp int) -- this generic field-initializer typechecking
        // loop does not apply: its declared type is deliberately the enum's own type, not an
        // integral/bool/float/double builtin, so the checks below would reject it by construction.
        if (field.isEnumConstant) continue;
        analyzeFieldInitializer(memberOrdinal, field, diagnostics);
    }
}

void ProjectMethodBodyModel::validateConstantFieldInitializers(
    const ProjectMemberModel& memberModel,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NSC-LIMIT-019. Validate after every initializer has been analyzed. ProjectMemberModel orders
    // types by stable identity rather than source order, so a transitive evaluator needs all name
    // and expression bindings to exist before it follows qualified dependencies.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind != ClassMemberKind::Field || !field.isConst || field.isEnumConstant ||
            field.syntax == nullptr || field.syntax->first() == nullptr) continue;
        if (!isCompileTimeConstantExpression(field.syntax->first(), memberOrdinal))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                field.syntax->first()->range(),
                "const field initializer must be a compile-time constant expression");
        }
    }
}

void ProjectMethodBodyModel::validateRequiredInliningCycles(
    const ProjectMemberModel& memberModel,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // A required inlining contract cannot be honored by an all-required recursive cycle.  LLVM
    // deliberately refuses to expand recursive alwaysinline call sites, so diagnose the source
    // contract here instead of silently leaving a call behind.  Edges through an automatic or
    // preferred method are not rejected: after expanding the required callee, that ordinary call
    // remains a valid recursion boundary.
    std::vector<std::vector<std::size_t>> requiredCallGraph(memberModel.members().size());
    for (const MethodCallBinding& call : calls_)
    {
        if (call.resolution != MethodCallResolutionKind::Resolved ||
            !call.selectedMemberOrdinal.has_value() ||
            call.callerMethodMemberOrdinal >= memberModel.members().size() ||
            *call.selectedMemberOrdinal >= memberModel.members().size()) continue;
        const ClassMemberSymbol& caller = memberModel.members()[call.callerMethodMemberOrdinal];
        const ClassMemberSymbol& callee = memberModel.members()[*call.selectedMemberOrdinal];
        if (caller.inliningPolicy == InliningPolicy::Required &&
            callee.inliningPolicy == InliningPolicy::Required)
        {
            requiredCallGraph[call.callerMethodMemberOrdinal].push_back(*call.selectedMemberOrdinal);
        }
    }
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.inliningPolicy != InliningPolicy::Required || member.syntax == nullptr) continue;
        std::vector<std::size_t> pending(
            requiredCallGraph[memberOrdinal].begin(), requiredCallGraph[memberOrdinal].end());
        std::vector<bool> visited(memberModel.members().size(), false);
        bool recursive = false;
        while (!pending.empty() && !recursive)
        {
            const std::size_t candidate = pending.back();
            pending.pop_back();
            if (candidate == memberOrdinal)
            {
                recursive = true;
                break;
            }
            if (candidate >= visited.size() || visited[candidate]) continue;
            visited[candidate] = true;
            pending.insert(
                pending.end(), requiredCallGraph[candidate].begin(), requiredCallGraph[candidate].end());
        }
        if (recursive)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                member.syntax->range(),
                "forceinline method '" + member.name +
                    "' participates in a recursive forceinline call cycle and cannot be "
                    "expanded at every call site");
        }
    }
}

bool ProjectMethodBodyModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectGenericModel* genericModel,
    diagnostics::DiagnosticEngine& diagnostics,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::optional<std::size_t> publicSurfaceProjectOrdinal,
    const ProjectMethodBodyBuildOptions& options)
{
    genericConstraintModel_ = genericConstraintModel;
    genericModel_ = genericModel;
    if (built_) return !diagnostics.hasErrors();
    resetMethodBodyTrace();
    writeMethodBodyTrace(
        "BUILD-BEGIN",
        "members=" + std::to_string(memberModel.members().size()) +
            "|namespace-functions=" + std::to_string(namespaceFunctionModel.functions().size()) +
            "|workers=" + std::to_string(options.workerCount));
    ownershipFlowStatistics_ = {};
    escapePropagationStatistics_ = {};
    callableAnalysisStatistics_ = {};
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt() ||
        !interfaceModel.isBuilt())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            SourceRange(),
            "project method-body binding requires completed name, member and inheritance models");
        built_ = true;
        return false;
    }
    initializeNamespaceCallableAdapters(nameLookup, memberModel, namespaceFunctionModel);
    inheritanceModel_ = &inheritanceModel;
    interfaceModel_ = &interfaceModel;
    validateDeclaredErrorTypes(nameLookup, memberModel, diagnostics);
    initializeParameterEscapeSummaries(memberModel);

    // NSC-LIMIT-018. A closed generic type materializes all of its member SIGNATURES so normal
    // lookup and overload resolution can see the complete API. Its ordinary method BODIES remain
    // deferred for executables until a real call selects them. A library has external callers,
    // however, so its public/protected closed surface is seeded explicitly below. Constructors/
    // destructors are lifetime machinery and virtual methods can be referenced by a vtable
    // without a syntactic call, so those remain eager for every product.
    std::unordered_set<std::size_t> demandedGenericMethods;
    seedDemandedGenericMethods(
        nameLookup, memberModel, interfaceModel,
        publicSurfaceProjectOrdinal, demandedGenericMethods);
    std::vector<std::size_t> callableWorklist = createCallableWorklist(
        nameLookup, memberModel, demandedGenericMethods);
    callableAnalysisStatistics_.callablesQueued = callableWorklist.size();
    writeMethodBodyTrace(
        "WORKLIST-READY", "callables=" + std::to_string(callableWorklist.size()));
    if (!analyzeCallableWaves(
            nameLookup, memberModel, namespaceFunctionModel, inheritanceModel, interfaceModel,
            genericModel, genericConstraintModel, options,
            demandedGenericMethods, callableWorklist, diagnostics))
    {
        return false;
    }

    writeMethodBodyTrace("FIELD-INITIALIZERS-BEGIN");
    analyzeFieldInitializers(memberModel, diagnostics);
    writeMethodBodyTrace("FIELD-INITIALIZERS-END");
    validateConstantFieldInitializers(memberModel, diagnostics);
    writeMethodBodyTrace("POST-ANALYSIS-BEGIN");
    verifyOutParameterAssignments(diagnostics);
    verifyRequiredFieldAssignments(diagnostics);
    validateRequiredInliningCycles(memberModel, diagnostics);
    writeMethodBodyTrace("ESCAPE-SUMMARIES-BEGIN");
    finalizeEscapeSummaries(diagnostics);
    writeMethodBodyTrace("ESCAPE-SUMMARIES-END");
    built_ = true;
    writeMethodBodyTrace("BUILD-END");
    return !diagnostics.hasErrors();
}

}
