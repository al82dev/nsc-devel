#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"

#include <ostream>
#include <string>

namespace nativesharp::frontend
{
namespace
{
std::string callableDisplayName(
    const ClassMemberSymbol& method,
    const ProjectNameLookup& nameLookup)
{
    if (method.declaringTypeOrdinal >= nameLookup.namedTypes().size())
    {
        return "<invalid-type>." + method.signatureKey;
    }
    return nameLookup.namedTypes()[method.declaringTypeOrdinal].identity.key() + "." +
        method.signatureKey;
}
}

void ProjectMethodBodyModelDumper::dump(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel& memberModel,
    const ProjectNameLookup& nameLookup,
    std::ostream& output) const
{
    output << "project-method-body-model\n";
    output << "callables:\n";
    for (std::size_t ordinal = 0; ordinal < memberModel.members().size(); ++ordinal)
    {
        const ClassMemberSymbol& member = memberModel.members()[ordinal];
        if (member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor &&
            member.kind != ClassMemberKind::Destructor)
        {
            continue;
        }
        output << "  [" << ordinal << "] " << callableDisplayName(member, nameLookup)
               << " -> " << member.declaredTypeKey << '\n';
    }

    output << "scopes=" << model.scopes().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.scopes().size(); ++ordinal)
    {
        const MethodScope& scope = model.scopes()[ordinal];
        output << "  [" << ordinal << "] kind=" << methodScopeKindName(scope.kind)
               << " callable=" << scope.methodMemberOrdinal
               << " depth=" << scope.depth << " parent=";
        if (scope.parentScopeOrdinal.has_value())
        {
            output << *scope.parentScopeOrdinal;
        }
        else
        {
            output << "none";
        }
        output << " locals=" << scope.localOrdinals.size() << '\n';
    }

    output << "locals=" << model.locals().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.locals().size(); ++ordinal)
    {
        const MethodLocalSymbol& local = model.locals()[ordinal];
        output << "  [" << ordinal << "] callable=" << local.methodMemberOrdinal
               << " scope=" << local.scopeOrdinal
               << " name=" << local.name
               << " type=" << local.typeKey
               << " ref=" << (local.isReference ? "true" : "false")
               << " const=" << (local.isConst ? "true" : "false")
               << " raii=" << (local.isRaii ? "true" : "false")
               << " pinned=" << (local.isPinned ? "true" : "false")
               << " ownership=" << sema::ownershipKindName(local.ownership)
               << " object-ownership=" << sema::ownershipKindName(local.objectLifetimeOwnership)
               << " storage-ownership=" << sema::ownershipKindName(local.storageOwnership);
        if (local.isReference)
        {
            output << " escape-origin=";
            switch (local.referenceEscapeOrigin.kind)
            {
                case MethodEscapeOriginKind::ExternalStorage: output << "external"; break;
                case MethodEscapeOriginKind::CurrentFrameLocal: output << "frame-local"; break;
                case MethodEscapeOriginKind::CurrentFrameParameter: output << "frame-parameter"; break;
                case MethodEscapeOriginKind::VariableLengthArray: output << "vla"; break;
            }
            if (local.referenceEscapeOrigin.ordinal.has_value())
                output << ':' << *local.referenceEscapeOrigin.ordinal;
            if (local.referenceEscapeOrigin.maximumValidScopeOrdinal.has_value())
                output << " max-scope=" << *local.referenceEscapeOrigin.maximumValidScopeOrdinal;
            if (local.referenceLifetimeScopeOrdinal.has_value())
                output << " alias-scope=" << *local.referenceLifetimeScopeOrdinal;
        }
        if (local.localBorrowBindingOrdinal.has_value())
        {
            output << " local-borrow=" << *local.localBorrowBindingOrdinal;
        }
        output << '\n';
    }

    output << "name-bindings=" << model.nameBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.nameBindings().size(); ++ordinal)
    {
        const MethodNameBinding& binding = model.nameBindings()[ordinal];
        output << "  [" << ordinal << "] callable=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodNameBindingKindName(binding.kind)
               << " name=" << binding.name
               << " type=" << binding.typeKey
               << " candidates=" << binding.candidateMemberOrdinals.size() << '\n';
    }

    output << "shadowing=" << model.shadowingEvents().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.shadowingEvents().size(); ++ordinal)
    {
        const MethodShadowingEvent& event = model.shadowingEvents()[ordinal];
        output << "  [" << ordinal << "] callable=" << event.methodMemberOrdinal
               << " scope=" << event.scopeOrdinal
               << " kind=" << methodShadowingKindName(event.kind)
               << " name=" << event.name << '\n';
    }

    output << "exception-bindings=" << model.exceptionBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.exceptionBindings().size(); ++ordinal)
    {
        const MethodExceptionBinding& binding = model.exceptionBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodExceptionOperationKindName(binding.kind)
               << " type=" << (binding.exceptionTypeKey.empty() ? "<current>" : binding.exceptionTypeKey)
               << " catch-all=" << (binding.catchAll ? "true" : "false") << '\n';
    }

    output << "using-bindings=" << model.usingBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.usingBindings().size(); ++ordinal)
    {
        const MethodUsingBinding& binding = model.usingBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " local=" << binding.resourceLocalOrdinal
               << " type=" << binding.resourceTypeKey
               << " idisposable-type=" << binding.disposableTypeOrdinal
               << " dispose-contract=" << binding.disposeContractMemberOrdinal
               << " dispose-implementation=";
        if (binding.disposeImplementationMemberOrdinal.has_value())
        {
            output << *binding.disposeImplementationMemberOrdinal;
        }
        else
        {
            output << "interface-slot";
        }
        output << " dispatch=" << (binding.interfaceDispatch ? "interface" : "resolved-member") << '\n';
    }

    output << "move-bindings=" << model.moveBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.moveBindings().size(); ++ordinal)
    {
        const MethodMoveBinding& binding = model.moveBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal;
        if (binding.sourceLocalOrdinal.has_value())
        {
            output << " source-local=" << *binding.sourceLocalOrdinal;
        }
        if (binding.sourceParameterOrdinal.has_value())
        {
            output << " source-parameter=" << *binding.sourceParameterOrdinal;
        }
        if (binding.partialSourceOwnerLocalOrdinal.has_value())
        {
            output << " partial-owner-local=" << *binding.partialSourceOwnerLocalOrdinal;
        }
        if (binding.partialSourceOwnerParameterOrdinal.has_value())
        {
            output << " partial-owner-parameter=" << *binding.partialSourceOwnerParameterOrdinal;
        }
        output << " type=" << binding.typeKey << '\n';
    }

    output << "ownership-transfers=" << model.ownershipTransferBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.ownershipTransferBindings().size(); ++ordinal)
    {
        const MethodOwnershipTransferBinding& binding = model.ownershipTransferBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodOwnershipTransferKindName(binding.kind)
               << " type=" << binding.typeKey;
        if (binding.sourceLocalOrdinal.has_value())
        {
            output << " source-local=" << *binding.sourceLocalOrdinal;
        }
        if (binding.sourceParameterOrdinal.has_value())
        {
            output << " source-parameter=" << *binding.sourceParameterOrdinal;
        }
        if (binding.targetMemberOrdinal.has_value())
        {
            output << " target-member=" << *binding.targetMemberOrdinal;
        }
        if (binding.targetParameterOrdinal.has_value())
        {
            output << " target-parameter=" << *binding.targetParameterOrdinal;
        }
        output << '\n';
    }

    output << "borrow-bindings=" << model.borrowBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.borrowBindings().size(); ++ordinal)
    {
        const MethodBorrowBinding& binding = model.borrowBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodBorrowKindName(binding.kind)
               << " type=" << binding.typeKey;
        if (binding.sourceLocalOrdinal.has_value())
        {
            output << " source-local=" << *binding.sourceLocalOrdinal;
        }
        if (binding.sourceParameterOrdinal.has_value())
        {
            output << " source-parameter=" << *binding.sourceParameterOrdinal;
        }
        if (binding.targetMemberOrdinal.has_value())
        {
            output << " target-member=" << *binding.targetMemberOrdinal;
        }
        if (binding.targetParameterOrdinal.has_value())
        {
            output << " target-parameter=" << *binding.targetParameterOrdinal;
        }
        output << '\n';
    }

    output << "local-borrow-bindings=" << model.localBorrowBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.localBorrowBindings().size(); ++ordinal)
    {
        const MethodLocalBorrowBinding& binding = model.localBorrowBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " local=" << binding.localOrdinal
               << " creation-scope=" << binding.creationScopeOrdinal
               << " max-scope=" << binding.maximumValidScopeOrdinal
               << " source-kind=" << methodLocalBorrowSourceKindName(binding.sourceKind)
               << " state=" << methodBorrowStateName(binding.state)
               << " type=" << binding.typeKey;
        if (binding.sourceLocalOrdinal.has_value())
        {
            output << " source-local=" << *binding.sourceLocalOrdinal;
        }
        if (binding.sourceParameterOrdinal.has_value())
        {
            output << " source-parameter=" << *binding.sourceParameterOrdinal;
        }
        if (binding.sourceOwnerLocalOrdinal.has_value())
        {
            output << " owner-local=" << *binding.sourceOwnerLocalOrdinal;
        }
        if (binding.sourceOwnerParameterOrdinal.has_value())
        {
            output << " owner-parameter=" << *binding.sourceOwnerParameterOrdinal;
        }
        output << '\n';
    }

    output << "cleanup-regions=" << model.cleanupRegions().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.cleanupRegions().size(); ++ordinal)
    {
        const MethodCleanupRegion& region = model.cleanupRegions()[ordinal];
        output << "  [" << ordinal << "] caller=" << region.methodMemberOrdinal
               << " owner-scope=" << region.ownerScopeOrdinal
               << " kind=" << methodCleanupRegionKindName(region.kind)
               << " depth=" << region.depth
               << " parent=";
        if (region.parentCleanupRegionOrdinal.has_value())
        {
            output << *region.parentCleanupRegionOrdinal;
        }
        else
        {
            output << "none";
        }
        if (region.resourceLocalOrdinal.has_value())
        {
            output << " resource-local=" << *region.resourceLocalOrdinal;
        }
        if (region.resourceParameterOrdinal.has_value())
        {
            output << " resource-parameter=" << *region.resourceParameterOrdinal;
        }
        if (region.disposeContractMemberOrdinal.has_value())
        {
            output << " dispose-contract=" << *region.disposeContractMemberOrdinal;
        }
        if (region.disposeImplementationMemberOrdinal.has_value())
        {
            output << " dispose-implementation=" << *region.disposeImplementationMemberOrdinal;
        }
        output << '\n';
    }

    output << "structured-exits=" << model.structuredExits().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.structuredExits().size(); ++ordinal)
    {
        const MethodStructuredExitBinding& exit = model.structuredExits()[ordinal];
        output << "  [" << ordinal << "] caller=" << exit.methodMemberOrdinal
               << " source-scope=" << exit.sourceScopeOrdinal
               << " kind=" << methodStructuredExitKindName(exit.kind)
               << " target-scope=";
        if (exit.targetScopeOrdinal.has_value())
        {
            output << *exit.targetScopeOrdinal;
        }
        else
        {
            output << "method-exit";
        }
        output << " cleanup-path=";
        if (exit.cleanupRegionOrdinals.empty())
        {
            output << "none";
        }
        else
        {
            for (std::size_t index = 0; index < exit.cleanupRegionOrdinals.size(); ++index)
            {
                if (index != 0)
                {
                    output << ',';
                }
                output << '#' << exit.cleanupRegionOrdinals[index];
            }
        }
        output << '\n';
    }

    output << "cleanup-continuations=" << model.cleanupContinuations().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.cleanupContinuations().size(); ++ordinal)
    {
        const MethodCleanupContinuation& continuation = model.cleanupContinuations()[ordinal];
        output << "  [" << ordinal << "] caller=" << continuation.methodMemberOrdinal
               << " source-scope=" << continuation.sourceScopeOrdinal
               << " reason=" << methodCleanupContinuationKindName(continuation.kind)
               << " cleanup=#" << continuation.cleanupRegionOrdinal
               << " next=";
        if (continuation.nextCleanupRegionOrdinal.has_value())
        {
            output << '#' << *continuation.nextCleanupRegionOrdinal;
        }
        else
        {
            output << "none";
        }
        output << " target-scope=";
        if (continuation.targetScopeOrdinal.has_value())
        {
            output << *continuation.targetScopeOrdinal;
        }
        else if (continuation.kind == MethodCleanupContinuationKind::Return)
        {
            output << "method-exit";
        }
        else
        {
            output << "normal";
        }
        output << '\n';
    }

    output << "calls=" << model.calls().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.calls().size(); ++ordinal)
    {
        const MethodCallBinding& call = model.calls()[ordinal];
        const char* receiverKind = call.staticCall
            ? "static"
            : call.baseReceiver
            ? "base"
            : call.explicitThis
                ? "this"
                : call.implicitThis
                    ? "implicit-this"
                    : "explicit-object";
        output << "  [" << ordinal << "] caller=" << call.callerMethodMemberOrdinal
               << " scope=" << call.scopeOrdinal
               << " name=" << call.methodName
               << " receiver=" << receiverKind
               << " receiver-type=" << (call.receiverTypeKey.empty() ? "<none>" : call.receiverTypeKey)
               << " resolution=" << methodCallResolutionKindName(call.resolution)
               << " candidates=" << call.candidateMemberOrdinals.size()
               << " viable=" << call.viableCandidateMemberOrdinals.size()
               << " selected=";
        if (call.selectedMemberOrdinal.has_value())
        {
            output << *call.selectedMemberOrdinal;
        }
        else
        {
            output << "none";
        }
        output << " result=" << (call.resultTypeKey.empty() ? "<none>" : call.resultTypeKey) << '\n';
        for (std::size_t conversionOrdinal = 0;
             conversionOrdinal < call.selectedConversions.size();
             ++conversionOrdinal)
        {
            const ProjectImplicitConversion& conversion = call.selectedConversions[conversionOrdinal];
            output << "      arg[" << conversionOrdinal << "] "
                   << conversion.sourceTypeKey << " -> " << conversion.targetTypeKey
                   << " conversion=" << projectConversionKindName(conversion.kind)
                   << " rank=" << conversion.rank << '\n';
        }
    }

    output << "delegate-bindings=" << model.delegateBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.delegateBindings().size(); ++ordinal)
    {
        const MethodDelegateBinding& binding = model.delegateBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodDelegateOperationKindName(binding.kind)
               << " delegate=" << binding.delegateTypeKey
               << " target=";
        if (binding.selectedMemberOrdinal.has_value()) output << '#' << *binding.selectedMemberOrdinal;
        else output << "<runtime-value>";
        output << " target-kind=" << (binding.staticTarget ? "static" : "instance")
               << " result=" << binding.resultTypeKey << '\n';
    }

    output << "lambda-bindings=" << model.lambdaBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.lambdaBindings().size(); ++ordinal)
    {
        const MethodLambdaBinding& binding = model.lambdaBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.ownerMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " delegate=" << binding.delegateTypeKey
               << " generated=" << binding.generatedName
               << " body=" << (binding.blockBody ? "block" : "expression")
               << " captures=" << binding.captures.size()
               << " return=" << binding.returnTypeKey << '\n';
        for (std::size_t captureOrdinal = 0; captureOrdinal < binding.captures.size(); ++captureOrdinal)
        {
            const MethodLambdaCapture& capture = binding.captures[captureOrdinal];
            output << "      capture[" << captureOrdinal << "] kind="
                   << methodLambdaCaptureKindName(capture.kind)
                   << " name=" << capture.name
                   << " type=" << capture.typeKey << '\n';
        }
    }

    output << "lambda-value-bindings=" << model.lambdaValueBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.lambdaValueBindings().size(); ++ordinal)
    {
        const MethodLambdaValueBinding& binding = model.lambdaValueBindings()[ordinal];
        output << "  [" << ordinal << "] lambda=#" << binding.lambdaOrdinal
               << " source=" << (binding.lambdaParameter ? "parameter" : "capture")
               << " value=#" << binding.valueOrdinal
               << " type=" << binding.typeKey << '\n';
    }

    output << "event-bindings=" << model.eventBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.eventBindings().size(); ++ordinal)
    {
        const MethodEventBinding& binding = model.eventBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodEventOperationKindName(binding.kind)
               << " event=#" << binding.eventMemberOrdinal
               << " backing=#" << binding.backingMemberOrdinal
               << " delegate=" << binding.delegateTypeKey
               << " storage=" << (binding.staticEvent ? "static" : "instance")
               << " result=" << binding.resultTypeKey << '\n';
    }

    output << "runtime-type-bindings=" << model.runtimeTypeBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.runtimeTypeBindings().size(); ++ordinal)
    {
        const MethodRuntimeTypeBinding& binding = model.runtimeTypeBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodRuntimeTypeOperationKindName(binding.kind)
               << " source=" << binding.sourceTypeKey
               << " target=" << binding.targetTypeKey
               << " target-type=#" << binding.targetTypeOrdinal
               << " target-kind=" << (binding.targetIsInterface ? "interface" : "class") << '\n';
    }

    output << "core-bindings=" << model.coreBindings().size() << "\n";
    for (std::size_t ordinal = 0; ordinal < model.coreBindings().size(); ++ordinal)
    {
        const MethodCoreBinding& binding = model.coreBindings()[ordinal];
        output << "  [" << ordinal << "] caller=" << binding.methodMemberOrdinal
               << " scope=" << binding.scopeOrdinal
               << " kind=" << methodCoreOperationKindName(binding.kind)
               << " result=" << binding.resultTypeKey
               << " element=" << (binding.elementTypeKey.empty() ? "<none>" : binding.elementTypeKey)
               << " assignable=" << (binding.assignable ? "true" : "false") << '\n';
    }

    output << "expression-types=" << model.expressionTypes().size() << "\n";
}
}
