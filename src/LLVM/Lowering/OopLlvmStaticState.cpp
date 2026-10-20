#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

void OopLlvmImplementation::applyOdrDefinition(LLVMValueRef value)
{
        if (value == nullptr) return;
        // weak_odr, rather than linkonce_odr, is required because a public ODR entity may be listed
        // in a DLL export table even after all of its local calls were inlined at O2. The optimizer
        // may discard an unreferenced linkonce definition; weak_odr keeps the external definition
        // while its symbol-specific COMDAT selects one copy at final link.
        applyOdrLinkage(module_, value);
    }

void OopLlvmImplementation::applyGenericOdrDefinition(LLVMValueRef value, const std::string& specializationKey)
{
        if (specializationKey.empty()) return;
        applyOdrDefinition(value);
    }

bool OopLlvmImplementation::declareStaticFields(std::string& error, bool defineStorage)
{
        const std::span<const frontend::ProjectOopMirStaticField> fields = mirModel_.staticFields();
        for (std::size_t fieldOrdinal = 0; fieldOrdinal < fields.size(); ++fieldOrdinal)
        {
            if (fieldOrdinal >= staticFieldDeclarations_.size() ||
                staticFieldDeclarations_[fieldOrdinal] == 0U) continue;
            const frontend::ProjectOopMirStaticField& field = fields[fieldOrdinal];
            LLVMTypeRef type = llvmType(field.typeKey, error);
            if (type == nullptr || type == LLVMVoidTypeInContext(context_))
            {
                if (error.empty()) error = "Stage 37 static field has invalid LLVM type";
                return false;
            }
            staticFieldInfo_[field.memberOrdinal] = &field;
            if (!field.isConst)
            {
                LLVMTypeRef accessorType = LLVMFunctionType(
                    LLVMPointerTypeInContext(context_, 0), nullptr, 0, 0);
                LLVMValueRef accessor = LLVMAddFunction(module_, field.accessorName.c_str(), accessorType);
                if (accessor == nullptr)
                    return fail(error, "Stage 37 failed to declare static field accessor " + field.accessorName);
                if (defineStorage)
                    applyGenericOdrDefinition(accessor, field.genericSpecializationKey);
                applyUwtableAttribute(accessor);
                staticFieldAccessorFunctions_[field.memberOrdinal] =
                    FunctionInfo{accessor, accessorType, nullptr};
                if (field.isImported) continue;
            }

            LLVMValueRef global = LLVMAddGlobal(module_, type, field.storageSymbol.c_str());
            if (global == nullptr)
                return fail(error, "Stage 37 failed to declare static field global " + field.storageSymbol);
            staticFieldGlobals_[field.memberOrdinal] = global;
            if (!defineStorage)
            {
                LLVMSetLinkage(global, LLVMExternalLinkage);
                continue;
            }
            if (!field.genericSpecializationKey.empty())
                applyGenericOdrDefinition(global, field.genericSpecializationKey);

            if (field.isConst && !field.hasConstantInitializer)
            {
                return fail(error, "Stage 37 const field lost its compile-time initializer");
            }
            LLVMValueRef initializer = staticFieldInitializer(field, type, error);
            if (initializer == nullptr) return false;
            if (field.isConst) LLVMSetGlobalConstant(global, 1);
            LLVMSetInitializer(global, initializer);
            // Constants are materialized from the same certified value in producers and
            // consumers. Mark both sides ODR: a DLL consumer keeps a local selectable copy,
            // while a static producer and consumer contribute equivalent COMDAT definitions
            // instead of one strong plus one selectable definition that COFF still rejects.
            if (field.isConst) applyOdrDefinition(global);
            if (!emitDebugGlobal(field, global, error)) return false;
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::staticFieldInitializer(
        const frontend::ProjectOopMirStaticField& field,
        LLVMTypeRef type,
        std::string& error)
{
        if (!field.hasConstantInitializer) return LLVMConstNull(type);
        const frontend::builtins::IntegerTypeInfo* fieldEnum =
            findEnumUnderlyingInfo(nameLookup_, field.typeKey);
        if (frontend::ProjectTypeConversion::isIntegral(field.typeKey) || field.isEnumConstant)
        {
            return LLVMConstInt(
                type,
                field.constantIntegerValue,
                (fieldEnum != nullptr ? fieldEnum->isSigned : isSignedInteger(field.typeKey)) ? 1 : 0);
        }
        if (field.typeKey == "builtin:bool")
            return LLVMConstInt(type, field.constantBooleanValue ? 1U : 0U, 0);
        if (field.typeKey == "builtin:float" || field.typeKey == "builtin:double")
            return LLVMConstReal(type, field.constantFloatingValue);
        if (field.typeKey == "builtin:decimal")
        {
            // 128-bit decimal const: coefficient (low 96) | scale (bits 96..101).
            std::uint64_t coefficient = 0;
            int scale = 0;
            if (!decimalCoefficientAndScale(field.constantFloatingValue, coefficient, scale))
            {
                fail(error, "decimal const coefficient exceeds 64 bits in Stage 37");
                return nullptr;
            }
            std::uint64_t words[2];
            words[0] = coefficient;
            words[1] = static_cast<std::uint64_t>(scale) << 32;
            return LLVMConstIntOfArbitraryPrecision(type, 2, words);
        }
        if (field.typeKey == "builtin:string")
            return internStringLiteral(field.constantStringValue, error);
        if (frontend::ProjectTypeConversion::isPointer(field.typeKey) ||
            frontend::ProjectTypeConversion::isNativeArray(field.typeKey))
            return LLVMConstNull(type);
        fail(error, "Stage 37 static storage type is not supported by native constant initialization");
        return nullptr;
    }

bool OopLlvmImplementation::defineStaticFieldAccessors(std::string& error)
{
        const std::span<const frontend::ProjectOopMirStaticField> fields = mirModel_.staticFields();
        for (std::size_t fieldOrdinal = 0; fieldOrdinal < fields.size(); ++fieldOrdinal)
        {
            if (fieldOrdinal >= staticFieldDeclarations_.size() ||
                staticFieldDeclarations_[fieldOrdinal] == 0U) continue;
            const frontend::ProjectOopMirStaticField& field = fields[fieldOrdinal];
            if (field.isConst || field.isImported) continue;
            const auto accessorFound = staticFieldAccessorFunctions_.find(field.memberOrdinal);
            const auto globalFound = staticFieldGlobals_.find(field.memberOrdinal);
            if (accessorFound == staticFieldAccessorFunctions_.end() ||
                globalFound == staticFieldGlobals_.end())
            {
                return fail(error, "Stage 37 lost mutable static field storage or accessor");
            }

            currentFunction_ = accessorFound->second.function;
            currentMirFunction_ = nullptr;
            thisValue_ = nullptr;
            closureEnvironment_ = nullptr;
            constructionThisOverride_ = nullptr;
            parameterStorage_.clear();
            localStorage_.clear();
            parameterValueDropFlags_.clear();
            localValueDropFlags_.clear();
            activeCatchExceptions_.clear();
            loopStack_.clear();
            exceptionDispatchBlocks_.clear();
            cleanupRegions_.clear();
            cleanupChainCache_.clear();
            activeCleanupRegions_.clear();
            exceptionHandlerStack_.clear();
            funcletStack_.clear();
            returnStorage_ = nullptr;
            sretStorage_ = nullptr;
            blockCounter_ = 0;
            currentDebugSubprogram_ = nullptr;
            currentDebugScope_ = nullptr;
            debugDeclaredLocals_.clear();
            currentDebugFileKey_ = 0;

            LLVMBasicBlockRef entry = appendBlock("static.accessor.entry");
            LLVMPositionBuilderAtEnd(builder_, entry);
            if (!ensureTypeInitialized(field.declaringTypeOrdinal, error)) return false;
            LLVMBuildRet(builder_, globalFound->second);
        }
        currentFunction_ = nullptr;
        currentMirFunction_ = nullptr;
        return true;
    }

bool OopLlvmImplementation::declareTypeInitializers(std::string& error, bool defineStorage)
{
        std::unordered_map<std::size_t, bool> needsInitializer;
        if (!collectTypeInitializerRequirements(needsInitializer, defineStorage, error))
            return false;

        LLVMTypeRef voidType = LLVMVoidTypeInContext(context_);
        for (const auto& [typeOrdinal, ignored] : needsInitializer)
        {
            (void)ignored;
            if (typeOrdinal >= nameLookup_.namedTypes().size())
            {
                return fail(error, "Stage 37 type initializer has an invalid declaring type ordinal");
            }
            const std::string localSuffix = std::to_string(typeOrdinal);
            const std::string physicalSuffix = compilation::symbolFragmentForIdentityKey(
                nameLookup_.namedTypes()[typeOrdinal].identity.key());
            if (defineStorage)
            {
                LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
                LLVMValueRef state = LLVMAddGlobal(
                    module_, i8, ("__nsc_cctor_state_" + localSuffix).c_str());
                if (state == nullptr)
                {
                    return fail(error, "Stage 37 failed to declare type-initialization state");
                }
                LLVMSetInitializer(state, LLVMConstInt(i8, 0, 0));
                LLVMSetLinkage(state, LLVMPrivateLinkage);
                staticInitStateGlobals_[typeOrdinal] = state;

                // The owner/state storage has one coordinator definition. Code units call the
                // externally declared cctor and never address this private guard storage.
                LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
                LLVMValueRef owner = LLVMAddGlobal(
                    module_, i32, ("__nsc_cctor_owner_" + localSuffix).c_str());
                if (owner == nullptr)
                {
                    return fail(error, "Stage 54.3 failed to declare type-initialization owner");
                }
                LLVMSetInitializer(owner, LLVMConstInt(i32, 0, 0));
                LLVMSetLinkage(owner, LLVMPrivateLinkage);
                staticInitOwnerGlobals_[typeOrdinal] = owner;
            }

            LLVMTypeRef functionType = LLVMFunctionType(voidType, nullptr, 0, 0);
            const std::string functionName = "__nsc_cctor_" + physicalSuffix;
            LLVMValueRef function = LLVMAddFunction(module_, functionName.c_str(), functionType);
            if (function == nullptr)
            {
                return fail(error, "Stage 37 failed to declare type initializer");
            }
            // A monolithic module can keep its initializer private. Partitioned code units call
            // the coordinator definition across COFF objects, so that definition must be
            // external and its name must be derived from nominal type identity (above), never a
            // project-local ordinal that can collide when independent libraries are linked.
            if (!partitioned_) LLVMSetLinkage(function, LLVMPrivateLinkage);
            applyUwtableAttribute(function);
            LLVMMetadataRef subprogram = nullptr;
            if (defineStorage && typeOrdinal < nameLookup_.namedTypes().size())
            {
                const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[typeOrdinal];
                const std::vector<std::string> noParameters;
                const std::string displayName =
                    type.identity.fullyQualifiedName() + ".cctor [generated]";
                subprogram = createSynthesizedDebugSubprogram(
                    type.range, displayName, functionName, "builtin:void",
                    noParameters, true, false);
            }
            if (debugBuilder_ != nullptr && defineStorage && subprogram == nullptr)
            {
                return fail(error, "failed to create debug subprogram for type initializer " + functionName);
            }
            if (subprogram != nullptr) LLVMSetSubprogram(function, subprogram);
            staticInitFunctions_[typeOrdinal] = FunctionInfo{function, functionType, subprogram};
        }
        return true;
    }

bool OopLlvmImplementation::collectTypeInitializerRequirements(
        std::unordered_map<std::size_t, bool>& needsInitializer,
        bool defineStorage,
        std::string& error)
{
        for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
        {
            if (initializer.isStatic && !initializer.isConst)
            {
                needsInitializer[initializer.declaringTypeOrdinal] = true;
            }
        }
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            if (!function.constructor || !function.isStatic)
            {
                continue;
            }
            needsInitializer[function.declaringTypeOrdinal] = true;
            // Only the coordinator partition defines the initializer and therefore needs the
            // constructor body. Ordinary code partitions deliberately prune unrelated function
            // declarations; they only require the externally visible coordinator declaration.
            if (!defineStorage)
            {
                continue;
            }
            const auto body = functions_.find(function.mangledName);
            if (body == functions_.end())
            {
                return fail(error, "static constructor body was not declared before its guard");
            }
            if (!staticConstructorBodies_.emplace(function.declaringTypeOrdinal, body->second).second)
            {
                return fail(error, "type initialization guard has more than one static constructor body");
            }
        }
        return true;
    }

void OopLlvmImplementation::prepareTypeInitializerFunction(
        std::size_t typeOrdinal,
        const FunctionInfo& info,
        SourceRange& debugRange)
{
        currentFunction_ = info.function;
        currentMirFunction_ = nullptr;
        thisValue_ = nullptr;
        closureEnvironment_ = nullptr;
        constructionThisOverride_ = nullptr;
        parameterStorage_.clear();
        localStorage_.clear();
        parameterValueDropFlags_.clear();
        localValueDropFlags_.clear();
        activeCatchExceptions_.clear();
        loopStack_.clear();
        exceptionDispatchBlocks_.clear();
        cleanupRegions_.clear();
        cleanupChainCache_.clear();
        activeCleanupRegions_.clear();
        exceptionHandlerStack_.clear();
        funcletStack_.clear();
        returnStorage_ = nullptr;
        sretStorage_ = nullptr;
        blockCounter_ = 0;
        currentDebugSubprogram_ = info.subprogram;
        currentDebugScope_ = currentDebugSubprogram_;
        debugDeclaredLocals_.clear();
        currentDebugFileKey_ = 0;
        if (typeOrdinal < nameLookup_.namedTypes().size())
        {
            debugRange = nameLookup_.namedTypes()[typeOrdinal].range;
            if (debugRange.isValid()) currentDebugFileKey_ = debugRange.begin().fileId().value();
        }
    }

LLVMValueRef OopLlvmImplementation::emitTypeInitializerGuard(
        LLVMValueRef stateGlobal,
        LLVMValueRef ownerGlobal,
        LLVMValueRef zero,
        LLVMValueRef one,
        LLVMValueRef two,
        LLVMValueRef three,
        LLVMBasicBlockRef fastFailureCheck,
        LLVMBasicBlockRef casAttempt,
        LLVMBasicBlockRef casInitializedCheck,
        LLVMBasicBlockRef recursionCheck,
        LLVMBasicBlockRef spinWait,
        LLVMBasicBlockRef spinTerminal,
        LLVMBasicBlockRef initialize,
        LLVMBasicBlockRef done,
        LLVMBasicBlockRef failed)
{
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        // Fast path: an atomic load lets an already-initialized class (the overwhelming
        // majority of calls -- this function runs on EVERY static-member access, not just the
        // first) skip straight to `done` without ever touching the CAS below.
        LLVMValueRef fastState = LLVMBuildLoad2(builder_, i8, stateGlobal, "cctor.state.fast");
        LLVMSetOrdering(fastState, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMValueRef fastDone = LLVMBuildICmp(builder_, LLVMIntEQ, fastState, two, "cctor.fast_done");
        LLVMBuildCondBr(builder_, fastDone, done, fastFailureCheck);
        LLVMPositionBuilderAtEnd(builder_, fastFailureCheck);
        LLVMValueRef fastFailed = LLVMBuildICmp(
            builder_, LLVMIntEQ, fastState, three, "cctor.fast_failed");
        LLVMBuildCondBr(builder_, fastFailed, failed, casAttempt);
        LLVMPositionBuilderAtEnd(builder_, casAttempt);
        // Computed here (in the block that dominates BOTH `initialize` -- the CAS-won path --
        // and `recursionCheck` -- the CAS-lost path) so the same SSA value `currentTid` is
        // valid to use in either successor: `initialize` needs it to publish ownership below,
        // `recursionCheck` needs it to test for self-recursion.
        LLVMValueRef currentTid = buildNoUnwindCall(
            getCurrentThreadIdFunctionType_, getCurrentThreadIdFunction_, nullptr, 0, "cctor.current_tid");
        // Exactly one thread's CAS observes `state == 0` and successfully writes 1 ("in
        // progress") -- that thread, and only that thread, proceeds to `initialize`. Every
        // other thread's CAS fails (because it either already sees 1 or 2) and falls through
        // to `recursionCheck` instead, regardless of which value it actually observed.
        LLVMValueRef cas = LLVMBuildAtomicCmpXchg(
            builder_, stateGlobal, zero, one,
            LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, 0);
        LLVMValueRef observedState = LLVMBuildExtractValue(builder_, cas, 0, "cctor.cas.observed");
        LLVMValueRef won = LLVMBuildExtractValue(builder_, cas, 1, "cctor.cas.won");
        LLVMBuildCondBr(builder_, won, initialize, casInitializedCheck);
        LLVMPositionBuilderAtEnd(builder_, casInitializedCheck);
        LLVMValueRef observedInitialized = LLVMBuildICmp(
            builder_, LLVMIntEQ, observedState, two, "cctor.cas_initialized");
        LLVMBasicBlockRef casFailureCheck = appendBlock("cctor.cas_failure_check");
        LLVMBuildCondBr(builder_, observedInitialized, done, casFailureCheck);
        LLVMPositionBuilderAtEnd(builder_, casFailureCheck);
        LLVMValueRef observedFailed = LLVMBuildICmp(
            builder_, LLVMIntEQ, observedState, three, "cctor.cas_failed");
        LLVMBuildCondBr(builder_, observedFailed, failed, recursionCheck);
        LLVMPositionBuilderAtEnd(builder_, recursionCheck);
        LLVMValueRef ownerTid = LLVMBuildLoad2(builder_, i32, ownerGlobal, "cctor.owner_tid");
        LLVMSetOrdering(ownerTid, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMValueRef isSelf = LLVMBuildICmp(builder_, LLVMIntEQ, ownerTid, currentTid, "cctor.is_self");
        LLVMBuildCondBr(builder_, isSelf, done, spinWait);
        LLVMPositionBuilderAtEnd(builder_, spinWait);
        // A losing thread (confirmed, via recursionCheck above, to be a genuinely DIFFERENT
        // thread from whichever one is initializing) spins on an atomic load until the winner
        // publishes terminal state 2 or 3. This window is only ever hit by the rare threads
        // that race a class's FIRST static-field access at the same instant -- once
        // state reaches a terminal value it never changes again, so every later call takes
        // the fast path above instead. A busy spin (rather than a real OS wait) keeps this guard self-
        // contained: no risk of the guard itself needing to wait on a primitive (like Mutex)
        // that might not have finished constructing yet.
        LLVMValueRef spinState = LLVMBuildLoad2(builder_, i8, stateGlobal, "cctor.state.spin");
        LLVMSetOrdering(spinState, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMValueRef spinPending = LLVMBuildICmp(builder_, LLVMIntEQ, spinState, one, "cctor.spin_pending");
        LLVMBuildCondBr(builder_, spinPending, spinWait, spinTerminal);
        LLVMPositionBuilderAtEnd(builder_, spinTerminal);
        LLVMValueRef spinDone = LLVMBuildICmp(builder_, LLVMIntEQ, spinState, two, "cctor.spin_done");
        LLVMBuildCondBr(builder_, spinDone, done, failed);
        return currentTid;
    }

bool OopLlvmImplementation::emitTypeInitializerBody(
        std::size_t typeOrdinal,
        LLVMValueRef stateGlobal,
        LLVMValueRef ownerGlobal,
        LLVMValueRef currentTid,
        LLVMValueRef initializedState,
        LLVMBasicBlockRef initialize,
        LLVMBasicBlockRef done,
        LLVMBasicBlockRef failureUnwind,
        std::string& error)
{
        LLVMPositionBuilderAtEnd(builder_, initialize);
        // Publish this thread's id BEFORE running any field initializer below -- a
        // self-referencing initializer's recursive call (see the header comment above) must
        // see ITS OWN thread id here in order for recursionCheck to recognize it as
        // self-recursion rather than spin-waiting on itself forever.
        LLVMValueRef ownerStore = LLVMBuildStore(builder_, currentTid, ownerGlobal);
        LLVMSetOrdering(ownerStore, LLVMAtomicOrderingSequentiallyConsistent);
        staticInitializationFailureUnwindBlock_ = failureUnwind;
        for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
        {
            if (!initializer.isStatic || initializer.isConst ||
                initializer.declaringTypeOrdinal != typeOrdinal) continue;
            const auto globalFound = staticFieldGlobals_.find(initializer.memberOrdinal);
            if (globalFound == staticFieldGlobals_.end())
            {
                staticInitializationFailureUnwindBlock_ = nullptr;
                return fail(error, "Stage 37 runtime static initializer lost its field global");
            }
            LLVMValueRef value = lowerExpression(initializer.value.get(), error);
            if (value == nullptr)
            {
                staticInitializationFailureUnwindBlock_ = nullptr;
                return false;
            }
            LLVMBuildStore(builder_, value, globalFound->second);
        }
        const auto explicitBody = staticConstructorBodies_.find(typeOrdinal);
        if (explicitBody != staticConstructorBodies_.end() &&
            buildPotentiallyThrowingCall(
                explicitBody->second.functionType, explicitBody->second.function,
                nullptr, 0, "", error) == nullptr && !error.empty())
        {
            staticInitializationFailureUnwindBlock_ = nullptr;
            return false;
        }
        staticInitializationFailureUnwindBlock_ = nullptr;
        LLVMValueRef publish = LLVMBuildStore(builder_, initializedState, stateGlobal);
        LLVMSetOrdering(publish, LLVMAtomicOrderingSequentiallyConsistent);
        if (!terminated()) LLVMBuildBr(builder_, done);
        LLVMPositionBuilderAtEnd(builder_, done);
        LLVMBuildRetVoid(builder_);
        return true;
    }

bool OopLlvmImplementation::emitTypeInitializerFailure(
        LLVMValueRef stateGlobal,
        LLVMValueRef failedState,
        LLVMBasicBlockRef failed,
        LLVMBasicBlockRef failureUnwind,
        std::string& error)
{
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMPositionBuilderAtEnd(builder_, failed);
        if (mirModel_.panicProfile() == language::PanicProfile::Abort)
        {
            LLVMValueRef reason = LLVMConstInt(
                i32, static_cast<std::uint64_t>(LlvmTrapReason::Panic), 0);
            LLVMValueRef arguments[] = {reason};
            buildNoUnwindCall(trapFunctionType_, trapFunction_, arguments, 1, "");
        }
        else
        {
            LLVMValueRef arguments[] = {
                LLVMConstInt(i32, kNativeSharpPanicExceptionCode, 0),
                LLVMConstInt(i32, 1, 0), LLVMConstInt(i32, 0, 0),
                LLVMConstNull(LLVMPointerTypeInContext(context_, 0))};
            buildNoUnwindCall(
                raiseExceptionFunctionType_, raiseExceptionFunction_, arguments, 4, "");
            LLVMValueRef reason = LLVMConstInt(
                i32, static_cast<std::uint64_t>(LlvmTrapReason::Panic), 0);
            LLVMValueRef trapArguments[] = {reason};
            buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        }
        LLVMBuildUnreachable(builder_);
        LLVMPositionBuilderAtEnd(builder_, failureUnwind);
        LLVMValueRef cleanupPad = LLVMBuildCleanupPad(
            builder_, nullptr, nullptr, 0, "cctor.failure.pad");
        if (cleanupPad == nullptr)
            return fail(error, "static initialization failed to create its terminal-state cleanuppad");
        LLVMValueRef failurePublish = LLVMBuildStore(builder_, failedState, stateGlobal);
        LLVMSetOrdering(failurePublish, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMBuildCleanupRet(builder_, cleanupPad, nullptr);
        return true;
    }

bool OopLlvmImplementation::defineTypeInitializer(
        std::size_t typeOrdinal,
        const FunctionInfo& info,
        std::string& error)
{
            SourceRange debugRange;
            prepareTypeInitializerFunction(typeOrdinal, info, debugRange);

            // Stage 54.3 "Mutex" -- FIXED a genuine data race in this guard, found while
            // stress-testing Mutex under real thread contention (see 59_stage54_3_mutex.md for
            // the full diagnosis). The guard USED TO BE a plain non-atomic
            // load-compare-branch-store ("state == 0 ? run initializer : skip"): two threads that
            // both reach a class's very first static-field access at roughly the same time could
            // BOTH observe state == 0 and BOTH proceed into the initializer block concurrently --
            // a classic check-then-act race, unrelated to x86-64's memory-ordering guarantees
            // (those explain visibility/tearing, not mutual exclusion of a multi-instruction
            // critical section). This is now a proper double-checked-locking guard using a real
            // atomic compare-and-swap for the 0->1 transition: EXACTLY ONE thread wins the race
            // to `state`, runs the field initializers, and publishes state=2; every other thread
            // either sees state==2 immediately (fast path) or loses the CAS and spin-waits on an
            // atomic load until the winner publishes state=2 -- so no caller of this function ever
            // proceeds while another thread is still mid-initialization, and the initializer body
            // itself (which can call into arbitrary NativeSharp code, including Win32-backed
            // constructors like Mutex's) never runs twice concurrently.
            // A SECOND race this guard must handle, found only after the atomic-CAS fix above was
            // written and tested: a SAME-thread recursive re-entry. `ensureTypeInitialized`
            // unconditionally emits a call to this very function on EVERY static-field access --
            // including one made WHILE evaluating this type's OWN field initializers below, e.g. a
            // self-referencing initializer like `static int B = Foo.A + 1;` (A's read calls
            // __nsc_cctor_N again, recursively, from the SAME thread that is already running it).
            // Under a naive CAS-only guard, that recursive call's CAS would fail (state is
            // already 1), and it would spin-wait for state==2 -- forever, since state can only
            // reach 2 by THIS SAME call frame finishing, which it never will while stuck spinning
            // on itself (confirmed experimentally: this exact pattern hung under a 15s timeout
            // before this second fix). The owner-thread-id global distinguishes the two cases: a
            // losing CAS whose owner is MY OWN thread id is self-recursion (proceed to `done`
            // immediately, matching this guard's pre-Stage-54.3 behavior for that case: rely on
            // field-initializer declaration order, exactly as the loop below already does); a
            // losing CAS whose owner is a DIFFERENT thread id is a genuine cross-thread race
            // (spin-wait as before).
            LLVMBasicBlockRef entry = appendBlock("cctor.entry");
            LLVMBasicBlockRef fastFailureCheck = appendBlock("cctor.fast_failure_check");
            LLVMBasicBlockRef casAttempt = appendBlock("cctor.cas_attempt");
            LLVMBasicBlockRef casInitializedCheck = appendBlock("cctor.cas_initialized_check");
            LLVMBasicBlockRef recursionCheck = appendBlock("cctor.recursion_check");
            LLVMBasicBlockRef spinWait = appendBlock("cctor.spin_wait");
            LLVMBasicBlockRef spinTerminal = appendBlock("cctor.spin_terminal");
            LLVMBasicBlockRef initialize = appendBlock("cctor.initialize");
            LLVMBasicBlockRef done = appendBlock("cctor.done");
            LLVMBasicBlockRef failed = appendBlock("cctor.failed");
            LLVMBasicBlockRef failureUnwind = appendBlock("cctor.failure_unwind");
            LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
            LLVMPositionBuilderAtEnd(builder_, entry);
            ScopedDebugLocation generatedLocation(
                builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
            LLVMValueRef stateGlobal = staticInitStateGlobals_[typeOrdinal];
            LLVMValueRef ownerGlobal = staticInitOwnerGlobals_[typeOrdinal];
            LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
            const auto stateValue = [](language::StaticInitializationState state) noexcept
            {
                return static_cast<std::uint64_t>(state);
            };
            LLVMValueRef zero = LLVMConstInt(
                i8, stateValue(language::StaticInitializationState::Uninitialized), 0);
            LLVMValueRef one = LLVMConstInt(
                i8, stateValue(language::StaticInitializationState::Initializing), 0);
            LLVMValueRef two = LLVMConstInt(
                i8, stateValue(language::StaticInitializationState::Initialized), 0);
            LLVMValueRef three = LLVMConstInt(
                i8, stateValue(language::StaticInitializationState::Failed), 0);
            LLVMValueRef currentTid = emitTypeInitializerGuard(
                stateGlobal, ownerGlobal, zero, one, two, three,
                fastFailureCheck, casAttempt, casInitializedCheck, recursionCheck,
                spinWait, spinTerminal, initialize, done, failed);

            if (!emitTypeInitializerBody(
                    typeOrdinal, stateGlobal, ownerGlobal, currentTid, two,
                    initialize, done, failureUnwind, error)) return false;
            if (!emitTypeInitializerFailure(
                    stateGlobal, three, failed, failureUnwind, error)) return false;
        return true;
    }

bool OopLlvmImplementation::defineTypeInitializers(std::string& error)
{
        for (const auto& [typeOrdinal, info] : staticInitFunctions_)
        {
            if (!defineTypeInitializer(typeOrdinal, info, error)) return false;
        }
        currentDebugSubprogram_ = nullptr;
        currentDebugScope_ = nullptr;
        currentDebugFileKey_ = 0;
        return true;
    }

}
