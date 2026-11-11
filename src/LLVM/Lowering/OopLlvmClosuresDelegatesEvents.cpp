#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/CppBoolAbi.h"
#include "../Internal/CppDllExportHelper.h"
#include "../Internal/CppSysVRecordAbi.h"
#include "../Internal/DropCase.h"
#include "../Internal/OopLlvmDirectCFunction.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/CppAbiPlan.h"
#include "nativesharp/Interop/CppVariadicAbiValidation.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::emitGuard(
    LLVMValueRef valid,
    LlvmTrapReason reason,
    std::string_view stem,
    std::string& error)
{
        if (valid == nullptr || trapFunction_ == nullptr || currentFunction_ == nullptr)
        {
            return fail(error, "Stage 41 cannot emit runtime guard");
        }
        LLVMBasicBlockRef okBlock = appendBlock(std::string(stem) + ".ok");
        LLVMBasicBlockRef trapBlock = appendBlock(std::string(stem) + ".trap");
        LLVMBuildCondBr(builder_, valid, okBlock, trapBlock);
        enterBlock(trapBlock);
        LLVMValueRef reasonValue = LLVMConstInt(
            LLVMInt32TypeInContext(context_), static_cast<std::uint64_t>(reason), 0);
        LLVMValueRef args[] = {reasonValue};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, args, 1, "");
        LLVMBuildUnreachable(builder_);
        enterBlock(okBlock);
        return true;
    }

LLVMValueRef OopLlvmImplementation::byteOffset(LLVMValueRef base, std::uint64_t offset, const char* name)
{
        LLVMValueRef index = LLVMConstInt(LLVMInt64TypeInContext(context_), offset, 0);
        LLVMValueRef indices[] = {index};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), base, indices, 1, name);
    }

std::pair<std::uint64_t, std::uint64_t> OopLlvmImplementation::captureValueLayout(
        std::string_view typeKey, std::string& error) const
{
        if (const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, typeKey); layout != nullptr)
            return {layout->size, layout->alignment};
        if (const frontend::builtins::IntegerTypeInfo* info =
                frontend::builtins::findIntegerType(typeKey); info != nullptr)
        {
            const std::uint64_t bytes = std::max<std::uint64_t>(1, info->width / 8U);
            return {bytes, bytes};
        }
        if (const frontend::builtins::IntegerTypeInfo* info =
                findEnumUnderlyingInfo(nameLookup_, typeKey);
            info != nullptr)
        {
            const std::uint64_t bytes = std::max<std::uint64_t>(1, info->width / 8U);
            return {bytes, bytes};
        }
        if (typeKey == "builtin:bool" || typeKey == "builtin:char") return {1, 1};
        if (typeKey == "builtin:float") return {4, 4};
        if (typeKey == "builtin:double") return {8, 8};
        if (typeKey == "builtin:string" || frontend::ProjectTypeConversion::isNativeArray(typeKey) ||
            isPointerLikeType(typeKey) || isClassReferenceType(nameLookup_, typeKey) ||
            isDelegateReferenceType(nameLookup_, typeKey))
            return {8, 8};
        if (const std::optional<std::string> element =
                frontend::ProjectMemberModel::fixedArrayElementTypeKey(typeKey); element.has_value())
        {
            const std::optional<std::uint64_t> count =
                frontend::ProjectMemberModel::fixedArrayCount(typeKey);
            const auto [elementSize, elementAlignment] = captureValueLayout(*element, error);
            if (!count.has_value() || elementSize == 0)
            {
                if (error.empty()) error = "E11 cannot size captured fixed array '" + std::string(typeKey) + "'";
                return {0, 0};
            }
            return {elementSize * *count, elementAlignment};
        }
        error = "E11 cannot determine capture-cell layout for '" + std::string(typeKey) + "'";
        return {0, 0};
    }

bool OopLlvmImplementation::ensureCaptureCellRuntime(std::string& error)
{
        if (captureCellRetainFunction_ != nullptr && captureCellReleaseFunction_ != nullptr) return true;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef parameters[] = {ptr};
        captureCellFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameters, 1, 0);
        captureCellRetainFunction_ = LLVMAddFunction(
            module_, "__nativesharp_capture_cell_retain", captureCellFunctionType_);
        captureCellReleaseFunction_ = LLVMAddFunction(
            module_, "__nativesharp_capture_cell_release", captureCellFunctionType_);
        if (captureCellRetainFunction_ == nullptr || captureCellReleaseFunction_ == nullptr)
            return fail(error, "E11 could not declare capture-cell lifetime helpers");
        // These helpers are implementation details emitted on demand in every code unit that
        // owns capturing-lambda code. Calls never cross module boundaries, so internal linkage is
        // the precise contract and prevents duplicate strong definitions at the final link.
        LLVMSetLinkage(captureCellRetainFunction_, LLVMInternalLinkage);
        LLVMSetLinkage(captureCellReleaseFunction_, LLVMInternalLinkage);

        LLVMBuilderRef helper = LLVMCreateBuilderInContext(context_);
        LLVMBasicBlockRef retainEntry = LLVMAppendBasicBlockInContext(
            context_, captureCellRetainFunction_, "entry");
        LLVMPositionBuilderAtEnd(helper, retainEntry);
        LLVMValueRef retainCell = LLVMGetParam(captureCellRetainFunction_, 0);
        LLVMBuildAtomicRMW(
            helper, LLVMAtomicRMWBinOpAdd, retainCell,
            LLVMConstInt(LLVMInt64TypeInContext(context_), 1, 0),
            LLVMAtomicOrderingSequentiallyConsistent, 0);
        LLVMBuildRetVoid(helper);

        LLVMBasicBlockRef releaseEntry = LLVMAppendBasicBlockInContext(
            context_, captureCellReleaseFunction_, "entry");
        LLVMBasicBlockRef releaseFree = LLVMAppendBasicBlockInContext(
            context_, captureCellReleaseFunction_, "free");
        LLVMBasicBlockRef releaseDone = LLVMAppendBasicBlockInContext(
            context_, captureCellReleaseFunction_, "done");
        LLVMPositionBuilderAtEnd(helper, releaseEntry);
        LLVMValueRef releaseCell = LLVMGetParam(captureCellReleaseFunction_, 0);
        LLVMValueRef oldCount = LLVMBuildAtomicRMW(
            helper, LLVMAtomicRMWBinOpAdd, releaseCell,
            LLVMConstInt(LLVMInt64TypeInContext(context_), ~std::uint64_t{0}, 0),
            LLVMAtomicOrderingSequentiallyConsistent, 0);
        LLVMValueRef last = LLVMBuildICmp(
            helper, LLVMIntEQ, oldCount,
            LLVMConstInt(LLVMInt64TypeInContext(context_), 1, 0),
            "capture.cell.last");
        LLVMBuildCondBr(helper, last, releaseFree, releaseDone);
        LLVMPositionBuilderAtEnd(helper, releaseFree);
        LLVMValueRef freeArguments[] = {releaseCell};
        LLVMBuildCall2(
            helper, deallocationFunctionType_, deallocationFunction_, freeArguments, 1, "");
        LLVMBuildBr(helper, releaseDone);
        LLVMPositionBuilderAtEnd(helper, releaseDone);
        LLVMBuildRetVoid(helper);
        LLVMDisposeBuilder(helper);
        return true;
    }

LLVMValueRef OopLlvmImplementation::allocateCaptureCell(
        std::string_view typeKey,
        LLVMValueRef& valueAddress,
        std::string_view stem,
        std::string& error)
{
        if (!ensureCaptureCellRuntime(error)) return nullptr;
        const auto [valueSize, valueAlignment] = captureValueLayout(typeKey, error);
        if (valueSize == 0 || valueAlignment == 0) return nullptr;
        const std::uint64_t valueOffset =
            ((8U + valueAlignment - 1U) / valueAlignment) * valueAlignment;
        LLVMValueRef bytes = LLVMConstInt(
            LLVMInt64TypeInContext(context_), valueOffset + valueSize, 0);
        LLVMValueRef arguments[] = {bytes};
        LLVMValueRef cell = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, arguments, 1,
            (std::string(stem) + ".cell").c_str());
        cell = checkedStage42Allocation(cell, std::string(stem) + ".cell.alloc", error);
        if (cell == nullptr) return nullptr;
        LLVMBuildStore(
            builder_, LLVMConstInt(LLVMInt64TypeInContext(context_), 1, 0), cell);
        valueAddress = byteOffset(
            cell, valueOffset, (std::string(stem) + ".value.addr").c_str());
        return cell;
    }

LLVMValueRef OopLlvmImplementation::captureCellValueAddress(
        LLVMValueRef cell,
        std::string_view typeKey,
        std::string_view stem,
        std::string& error)
{
        const auto [valueSize, valueAlignment] = captureValueLayout(typeKey, error);
        if (cell == nullptr || valueSize == 0 || valueAlignment == 0) return nullptr;
        const std::uint64_t valueOffset =
            ((8U + valueAlignment - 1U) / valueAlignment) * valueAlignment;
        return byteOffset(cell, valueOffset, (std::string(stem) + ".value.addr").c_str());
    }

void OopLlvmImplementation::retainCaptureCell(LLVMValueRef cell)
{
        LLVMValueRef arguments[] = {cell};
        buildNoUnwindCall(
            captureCellFunctionType_, captureCellRetainFunction_, arguments, 1, "");
    }

void OopLlvmImplementation::releaseCaptureCell(LLVMValueRef cell)
{
        LLVMValueRef arguments[] = {cell};
        buildNoUnwindCall(
            captureCellFunctionType_, captureCellReleaseFunction_, arguments, 1, "");
    }

void OopLlvmImplementation::releaseOwnedCaptureCells()
{
        for (LLVMValueRef cell : ownedCaptureCells_) releaseCaptureCell(cell);
    }

LLVMBasicBlockRef OopLlvmImplementation::captureCellExceptionalCleanup(std::string& error)
{
        if (captureCellExceptionalCleanupBlock_ != nullptr)
            return captureCellExceptionalCleanupBlock_;
        if (ownedCaptureCells_.empty()) return nullptr;

        LLVMBasicBlockRef sourceBlock = LLVMGetInsertBlock(builder_);
        if (sourceBlock == nullptr)
        {
            error = "E11 cannot emit capture-cell exceptional cleanup without an insertion block";
            return nullptr;
        }

        const std::vector<FuncletContext> savedFunclets = funcletStack_;
        captureCellExceptionalCleanupBlock_ = appendBlock("eh.capture.cells");
        enterBlock(captureCellExceptionalCleanupBlock_);
        LLVMValueRef cleanupPad = LLVMBuildCleanupPad(
            builder_, nullptr, nullptr, 0, "eh.capture.cells.pad");
        if (cleanupPad == nullptr)
        {
            captureCellExceptionalCleanupBlock_ = nullptr;
            enterBlock(sourceBlock);
            error = "E11 failed to create the function-level capture-cell cleanuppad";
            return nullptr;
        }

        // The release calls are emitted inside this funclet and therefore carry the required
        // operand bundle on Windows SEH. The cells themselves dominate the block because all are
        // allocated in the function entry before statement lowering starts.
        funcletStack_.clear();
        funcletStack_.push_back(FuncletContext{
            cleanupPad, FuncletKind::Cleanup, 0, nullptr, nullptr});
        releaseOwnedCaptureCells();
        LLVMBuildCleanupRet(builder_, cleanupPad, nullptr);
        funcletStack_ = savedFunclets;
        enterBlock(sourceBlock);
        return captureCellExceptionalCleanupBlock_;
    }

LLVMValueRef OopLlvmImplementation::closureContextDestroy(
        std::string_view calleeName,
        std::size_t captureCount,
        std::string& error)
{
        const auto existing = closureContextDestroyFunctions_.find(std::string(calleeName));
        if (existing != closureContextDestroyFunctions_.end()) return existing->second;
        if (!ensureCaptureCellRuntime(error)) return nullptr;

        const frontend::ProjectOopMirFunction* origin =
            mirModel_.functionForMangledName(calleeName);
        if (origin == nullptr || origin->captureTypeKeys.size() != captureCount ||
            origin->captureModes.size() != captureCount ||
            origin->captureOwnsValues.size() != captureCount)
            return failValue(error, "T08-F03 closure destroy lost capture ownership metadata");

        const std::string name = "__nsc_closure_context_destroy_" + std::string(calleeName);
        LLVMMetadataRef subprogram = nullptr;
        SourceRange debugRange;
        LLVMValueRef function = createClosureContextDestroyFunction(
            *origin, name, debugRange, subprogram, error);
        if (function == nullptr) return nullptr;
        if (!emitClosureContextDestroyFunctionBody(
                function, *origin, captureCount, debugRange, subprogram, error)) return nullptr;
        closureContextDestroyFunctions_[std::string(calleeName)] = function;
        return function;
    }

LLVMValueRef OopLlvmImplementation::createClosureContextDestroyFunction(
        const frontend::ProjectOopMirFunction& origin,
        std::string_view name,
        SourceRange& debugRange,
        LLVMMetadataRef& subprogram,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef parameters[] = {ptr};
        LLVMTypeRef functionType = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameters, 1, 0);
        LLVMValueRef function = LLVMAddFunction(module_, std::string(name).c_str(), functionType);
        if (function == nullptr)
            return failValue(error, "E11 could not create closure-context destroy helper");
        if (memberModel_ != nullptr && origin.memberOrdinal < memberModel_->members().size() &&
            origin.declaringTypeOrdinal < nameLookup_.namedTypes().size())
        {
            const frontend::ClassMemberSymbol& member = memberModel_->members()[origin.memberOrdinal];
            debugRange = member.range;
            if (origin.body != nullptr && origin.body->syntax != nullptr &&
                origin.body->syntax->range().isValid())
                debugRange = origin.body->syntax->range();
            std::string displayName =
                nameLookup_.namedTypes()[origin.declaringTypeOrdinal].identity.fullyQualifiedName() +
                "." + member.name;
            if (origin.isLambda)
                displayName += ".lambda#" + std::to_string(origin.lambdaOrdinal.value_or(0));
            displayName += " [generated: closure context destroy]";
            const std::vector<std::string> debugParameters = {"builtin:void*"};
            subprogram = createSynthesizedDebugSubprogram(
                debugRange, displayName, name, "builtin:void", debugParameters, true, false);
        }
        if (debugBuilder_ != nullptr && subprogram == nullptr)
        {
            return failValue(
                error, "failed to create debug subprogram for closure-context destroy helper " +
                    std::string(name));
        }
        if (subprogram != nullptr) LLVMSetSubprogram(function, subprogram);
        return function;
    }

bool OopLlvmImplementation::dropClosureContextCaptures(
        LLVMValueRef environment,
        const frontend::ProjectOopMirFunction& origin,
        std::size_t captureCount,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        for (std::size_t index = 0; index < captureCount; ++index)
        {
            LLVMValueRef slot = byteOffset(
                environment, static_cast<std::uint64_t>(index) * 8U, "closure.cell.slot");
            LLVMValueRef cell = LLVMBuildLoad2(builder_, ptr, slot, "closure.cell");
            if (origin.captureModes[index] == language::CaptureMode::Move &&
                origin.captureOwnsValues[index])
            {
                const std::string& typeKey = origin.captureTypeKeys[index];
                LLVMValueRef valueAddress = captureCellValueAddress(
                    cell, typeKey, "closure.owned.capture", error);
                if (valueAddress == nullptr) return false;
                if (requiresOwnedDestruction(typeKey))
                {
                    LLVMValueRef value = LLVMBuildLoad2(
                        builder_, ptr, valueAddress, "closure.owned.capture.load");
                    LLVMBuildStore(builder_, LLVMConstNull(ptr), valueAddress);
                    if (!emitOwnedPointerDestroy(
                            value, typeKey, "closure.owned.capture.destroy", error)) return false;
                }
                else if (!emitValueDropAddress(
                             valueAddress, typeKey, "closure.owned.capture.drop", error)) return false;
            }
            releaseCaptureCell(cell);
        }
        return true;
    }

bool OopLlvmImplementation::emitClosureContextDestroyFunctionBody(
        LLVMValueRef function,
        const frontend::ProjectOopMirFunction& origin,
        std::size_t captureCount,
        const SourceRange& debugRange,
        LLVMMetadataRef subprogram,
        std::string& error)
{
        LLVMBuilderRef savedBuilder = builder_;
        LLVMValueRef savedFunction = currentFunction_;
        const frontend::ProjectOopMirFunction* savedMirFunction = currentMirFunction_;
        const std::size_t savedBlockCounter = blockCounter_;
        const std::vector<std::size_t> savedActiveCleanupRegions = activeCleanupRegions_;
        const std::vector<ExceptionHandlerContext> savedExceptionHandlers = exceptionHandlerStack_;
        const std::vector<FuncletContext> savedFunclets = funcletStack_;
        const std::vector<LLVMValueRef> savedOwnedCaptureCells = ownedCaptureCells_;
        LLVMBasicBlockRef savedCaptureCellCleanup = captureCellExceptionalCleanupBlock_;
        LLVMBuilderRef helper = LLVMCreateBuilderInContext(context_);
        builder_ = helper;
        currentFunction_ = function;
        currentMirFunction_ = nullptr;
        blockCounter_ = 0;
        activeCleanupRegions_.clear();
        exceptionHandlerStack_.clear();
        funcletStack_.clear();
        ownedCaptureCells_.clear();
        captureCellExceptionalCleanupBlock_ = nullptr;
        const auto restoreLoweringState = [&]()
        {
            builder_ = savedBuilder;
            currentFunction_ = savedFunction;
            currentMirFunction_ = savedMirFunction;
            blockCounter_ = savedBlockCounter;
            activeCleanupRegions_ = savedActiveCleanupRegions;
            exceptionHandlerStack_ = savedExceptionHandlers;
            funcletStack_ = savedFunclets;
            ownedCaptureCells_ = savedOwnedCaptureCells;
            captureCellExceptionalCleanupBlock_ = savedCaptureCellCleanup;
        };
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context_, function, "entry");
        enterBlock(entry);
        {
            ScopedDebugLocation generatedLocation(
                builder_, debugSubprogramLocation(debugRange, subprogram));
            LLVMValueRef environment = LLVMGetParam(function, 0);
            LLVMSetValueName2(environment, "context", 7);
            if (!dropClosureContextCaptures(environment, origin, captureCount, error))
            {
                restoreLoweringState();
                LLVMDisposeBuilder(helper);
                return false;
            }
            LLVMValueRef freeArguments[] = {environment};
            buildNoUnwindCall(
                deallocationFunctionType_, deallocationFunction_, freeArguments, 1, "");
            LLVMBuildRetVoid(builder_);
        }
        restoreLoweringState();
        LLVMDisposeBuilder(helper);
        return true;
    }

LLVMValueRef OopLlvmImplementation::checkedStage42Allocation(
    LLVMValueRef memory,
    std::string_view stem,
    std::string& error)
{
        if (memory == nullptr) return nullptr;
        LLVMValueRef notNull = LLVMBuildICmp(
            builder_, LLVMIntNE, memory, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)),
            (std::string(stem) + ".nonnull").c_str());
        return emitGuard(notNull, LlvmTrapReason::AllocationFailure, stem, error) ? memory : nullptr;
    }

LLVMValueRef OopLlvmImplementation::allocateDelegateCell(
        LLVMValueRef code,
        LLVMValueRef receiver,
        LLVMValueRef contextDestroy,
        std::string_view stem,
        std::string& error)
{
        // Native ABI 1.0, delegate ABI 1:
        //   +0  code pointer
        //   +8  receiver/closure environment
        //   +16 optional context destroy function
        //   +24 cell free function
        // Both callbacks are addresses from the PRODUCER module. A consumer may therefore own and
        // destroy a delegate returned by a DLL without guessing which allocator created either
        // allocation. The old 16-byte cell had no route back to its producer and leaked closures.
        LLVMValueRef bytes = LLVMConstInt(
            LLVMInt64TypeInContext(context_), interop::kManagedDelegateCellSize, 0);
        LLVMValueRef allocArgs[] = {bytes};
        LLVMValueRef cell = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, allocArgs, 1,
            (std::string(stem) + ".cell").c_str());
        cell = checkedStage42Allocation(cell, std::string(stem) + ".alloc", error);
        if (cell == nullptr) return nullptr;
        LLVMBuildStore(builder_, code, cell);
        LLVMValueRef receiverAddress = byteOffset(cell, 8, (std::string(stem) + ".receiver.addr").c_str());
        LLVMBuildStore(builder_, receiver, receiverAddress);
        LLVMValueRef contextDestroyAddress = byteOffset(
            cell, 16, (std::string(stem) + ".context.destroy.addr").c_str());
        LLVMBuildStore(builder_, contextDestroy, contextDestroyAddress);
        LLVMValueRef cellFreeAddress = byteOffset(
            cell, 24, (std::string(stem) + ".cell.free.addr").c_str());
        LLVMBuildStore(builder_, deallocationFunction_, cellFreeAddress);
        return cell;
    }

bool OopLlvmImplementation::emitDelegateDestroy(LLVMValueRef cell, std::string_view stem, std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMBasicBlockRef destroyBlock = appendBlock(std::string(stem) + ".destroy");
        LLVMBasicBlockRef doneBlock = appendBlock(std::string(stem) + ".done");
        LLVMValueRef isNull = LLVMBuildICmp(builder_, LLVMIntEQ, cell, LLVMConstNull(ptr),
                                            (std::string(stem) + ".isnull").c_str());
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);
        enterBlock(destroyBlock);

        LLVMValueRef context = LLVMBuildLoad2(
            builder_, ptr, byteOffset(cell, 8, "delegate.destroy.context.addr"),
            "delegate.destroy.context");
        LLVMValueRef destroyContext = LLVMBuildLoad2(
            builder_, ptr, byteOffset(cell, 16, "delegate.destroy.callback.addr"),
            "delegate.destroy.callback");
        LLVMValueRef hasContext = LLVMBuildICmp(
            builder_, LLVMIntNE, context, LLVMConstNull(ptr), "delegate.destroy.has.context");
        LLVMValueRef hasDestroy = LLVMBuildICmp(
            builder_, LLVMIntNE, destroyContext, LLVMConstNull(ptr), "delegate.destroy.has.callback");
        LLVMValueRef destroyEnvironment = LLVMBuildAnd(
            builder_, hasContext, hasDestroy, "delegate.destroy.environment");
        LLVMBasicBlockRef environmentBlock = appendBlock(std::string(stem) + ".environment");
        LLVMBasicBlockRef freeCellBlock = appendBlock(std::string(stem) + ".cell");
        LLVMBuildCondBr(builder_, destroyEnvironment, environmentBlock, freeCellBlock);

        enterBlock(environmentBlock);
        LLVMValueRef contextArguments[] = {context};
        buildNoUnwindCall(
            deallocationFunctionType_, destroyContext, contextArguments, 1, "");
        LLVMBuildBr(builder_, freeCellBlock);

        enterBlock(freeCellBlock);
        LLVMValueRef freeCell = LLVMBuildLoad2(
            builder_, ptr, byteOffset(cell, 24, "delegate.destroy.cell.callback.addr"),
            "delegate.destroy.cell.callback");
        LLVMValueRef callbackPresent = LLVMBuildICmp(
            builder_, LLVMIntNE, freeCell, LLVMConstNull(ptr), "delegate.destroy.cell.callback.present");
        LLVMBasicBlockRef callbackBlock = appendBlock(std::string(stem) + ".cell.callback");
        LLVMBuildCondBr(builder_, callbackPresent, callbackBlock, doneBlock);
        enterBlock(callbackBlock);
        LLVMValueRef cellArguments[] = {cell};
        buildNoUnwindCall(deallocationFunctionType_, freeCell, cellArguments, 1, "");
        LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return error.empty();
    }

LLVMValueRef OopLlvmImplementation::closureCaptureCell(const frontend::ProjectOopMirNode* node, std::string& error)
{
        return closureCaptureCellAt(static_cast<std::size_t>(node->immediate), error);
    }

LLVMValueRef OopLlvmImplementation::closureCaptureCellAt(std::size_t capture, std::string& error)
{
        if (currentMirFunction_ == nullptr || !currentMirFunction_->isLambda ||
            !currentMirFunction_->hasClosureEnvironment || closureEnvironment_ == nullptr)
        {
            return failValue(error, "E11 closure capture has no environment receiver");
        }
        if (capture >= currentMirFunction_->captureTypeKeys.size())
            return failValue(error, "E11 closure capture ordinal is invalid");
        LLVMValueRef slot = byteOffset(
            closureEnvironment_, static_cast<std::uint64_t>(capture) * 8U,
            "closure.capture.cell.addr");
        return LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0), slot, "closure.capture.cell");
    }

LLVMValueRef OopLlvmImplementation::closureCaptureAddress(const frontend::ProjectOopMirNode* node, std::string& error)
{
        const std::size_t capture = static_cast<std::size_t>(node->immediate);
        if (const auto cached = closureCaptureAddresses_.find(capture);
            cached != closureCaptureAddresses_.end())
            return cached->second;
        return closureCaptureValueAddress(capture, node->typeKey, error);
    }

LLVMValueRef OopLlvmImplementation::closureCaptureValueAddress(
        std::size_t capture,
        std::string_view typeKey,
        std::string& error)
{
        LLVMValueRef cell = closureCaptureCellAt(capture, error);
        if (cell == nullptr) return nullptr;
        const auto [valueSize, valueAlignment] = captureValueLayout(typeKey, error);
        if (valueSize == 0 || valueAlignment == 0) return nullptr;
        const std::uint64_t valueOffset =
            ((8U + valueAlignment - 1U) / valueAlignment) * valueAlignment;
        return byteOffset(cell, valueOffset, "closure.capture.value.addr");
    }

// ES: La dirección de cada captura se calcula aquí, al entrar a la lambda, y la reutilizan todos
// los accesos (closureCaptureAddress); así sigue viva en todo el cuerpo y la variable de
// depuración tiene ubicación en todo él.
// EN: Each capture's address is computed here, on entry to the lambda, and every access reuses it
// (closureCaptureAddress); it therefore stays live across the whole body and the debug variable
// has a location throughout it.
//
// ES: Antes, la variable de depuración de una captura se declaraba en su primer uso, sobre una
// dirección calculada ahí, y cada lectura añadía además un dbg.value con el valor cargado. LLVM
// mezclaba ambas descripciones: en DeepFunctional, dentro de la lambda, `calls` aparecía como
// `<optimized out>` y `bias` no aparecía antes de su primera lectura. Ahora cada captura se declara
// una sola vez al entrar a la lambda, sobre la dirección de su valor dentro de la celda compartida,
// así el depurador lee el valor vivo (las capturas son por referencia) en todo el cuerpo.
// EN: The debug variable of a capture used to be declared at its first use, over an address computed
// there, and every read also added a dbg.value with the loaded value. LLVM mixed both descriptions:
// in DeepFunctional, inside the lambda, `calls` showed as `<optimized out>` and `bias` did not exist
// before its first read. Each capture is now declared once on entry to the lambda, over the address
// of its value inside the shared cell, so the debugger reads the live value (captures are by
// reference) throughout the body.
void OopLlvmImplementation::emitClosureCaptureDebugVariables(
        const frontend::ProjectOopMirFunction& function)
{
        if (!function.isLambda || !function.hasClosureEnvironment || closureEnvironment_ == nullptr)
            return;
        for (std::size_t capture = 0; capture < function.captureTypeKeys.size(); ++capture)
        {
            std::string ignored;
            LLVMValueRef address =
                closureCaptureValueAddress(capture, function.captureTypeKeys[capture], ignored);
            if (address != nullptr) closureCaptureAddresses_.emplace(capture, address);
        }
        if (debugInformation_ != DebugInformationKind::Full || currentDebugSubprogram_ == nullptr ||
            function.body == nullptr || function.body->syntax == nullptr ||
            !function.body->syntax->range().isValid())
            return;
        const SourceRange range = function.body->syntax->range();
        LLVMMetadataRef file = debugFile(range.begin());
        if (file == nullptr) return;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        // ES: En orden de captura, para que la salida sea determinista.
        // EN: In capture order, so the output is deterministic.
        for (std::size_t capture = 0; capture < function.captureTypeKeys.size(); ++capture)
        {
            const auto cached = closureCaptureAddresses_.find(capture);
            if (cached == closureCaptureAddresses_.end()) continue;
            LLVMValueRef address = cached->second;
            const std::string name = capture < function.captureNames.size() &&
                    !function.captureNames[capture].empty()
                ? function.captureNames[capture]
                : "capture#" + std::to_string(capture);
            LLVMMetadataRef type = debugType(function.captureTypeKeys[capture]);
            if (type == nullptr) continue;
            // ES: La dirección se guarda en una ranura del bloque de entrada y la variable se declara
            // sobre esa ranura con DW_OP_deref: la variable vive en la dirección guardada. Así tiene
            // ubicación en memoria durante todo el cuerpo (una dirección en registro dura sólo hasta
            // que el registro se reutiliza) y CodeView la describe como referencia al tipo de la
            // captura, que es lo que es: las capturas son por referencia.
            // EN: The address is stored in an entry-block slot and the variable is declared over that
            // slot with DW_OP_deref: the variable lives at the stored address. It thus has a memory
            // location throughout the body (an address held in a register only lasts until the
            // register is reused) and CodeView describes it as a reference to the capture's type,
            // which is what it is: captures are by reference.
            LLVMValueRef slot = entryBlockAlloca(
                LLVMPointerTypeInContext(context_, 0), 8,
                ("capture" + std::to_string(capture) + ".debug.addr").c_str());
            LLVMBuildStore(builder_, address, slot);
            LLVMMetadataRef variable = LLVMDIBuilderCreateAutoVariable(
                debugBuilder_, currentDebugSubprogram_, name.data(), name.size(), file,
                position.line, type, 1, LLVMDIFlagZero, 0);
            std::uint64_t dereference[] = {0x06};
            LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder_, dereference, 1);
            LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
                context_, position.line, position.column, currentDebugSubprogram_, nullptr);
            if (variable == nullptr || expression == nullptr || location == nullptr ||
                LLVMDIBuilderInsertDeclareRecordAtEnd(
                    debugBuilder_, slot, variable, expression, location,
                    LLVMGetInsertBlock(builder_)) == nullptr)
                continue;
            debugCaptureVariables_.emplace(capture, variable);
        }
    }

LLVMValueRef OopLlvmImplementation::lowerClosureCaptureLoad(const frontend::ProjectOopMirNode* node, std::string& error)
{
        LLVMValueRef address = closureCaptureAddress(node, error);
        if (address == nullptr) return nullptr;
        const std::size_t capture = static_cast<std::size_t>(node->immediate);
        LLVMTypeRef type = llvmType(currentMirFunction_->captureTypeKeys[capture], error);
        if (type == nullptr || type == LLVMVoidTypeInContext(context_))
            return failValue(error, "E11 closure capture has invalid LLVM type");
        return LLVMBuildLoad2(builder_, type, address, "closure.capture.load");
    }

LLVMValueRef OopLlvmImplementation::lowerCaptureCellReference(
    const frontend::ProjectOopMirNode* node,
    std::string& error)
{
        if (node->localOrdinal.has_value())
        {
            const auto found = localCaptureCells_.find(*node->localOrdinal);
            if (found != localCaptureCells_.end()) return found->second;
        }
        else if (node->parameterOrdinal.has_value())
        {
            const auto found = parameterCaptureCells_.find(*node->parameterOrdinal);
            if (found != parameterCaptureCells_.end()) return found->second;
        }
        else if (thisCaptureCell_ != nullptr)
        {
            return thisCaptureCell_;
        }
        return failValue(error, "E11 lambda creation cannot find the shared capture cell");
    }

LLVMValueRef OopLlvmImplementation::lowerLambdaCreate(const frontend::ProjectOopMirNode* node, std::string& error)
{
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end() || found->second.function == nullptr)
            return failValue(error, "Stage 43 lambda generated callable is unavailable: " + node->calleeName);

        if (language::isAnonymousLambdaTypeIdentity(node->typeKey))
        {
            if (!node->children.empty())
                return failValue(error, "T08-F02 anonymous lambda reached LLVM with captured state");
            LLVMTypeRef zeroSize = llvmType(node->typeKey, error);
            return zeroSize == nullptr ? nullptr : LLVMConstNull(zeroSize);
        }

        if (isExternCDelegateType(node->typeKey))
        {
            if (!node->children.empty())
                return failValue(error, "E11 extern C lambda reached LLVM with captured state");
            return callbackGuardThunk(
                node->calleeName, error, isVectorCallDelegateType(node->typeKey));
        }

        LLVMValueRef receiver = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        if (!node->children.empty())
        {
            receiver = createLambdaCaptureEnvironment(node, error);
            if (receiver == nullptr) return nullptr;
        }
        LLVMValueRef contextDestroy = node->children.empty()
            ? LLVMConstNull(LLVMPointerTypeInContext(context_, 0))
            : closureContextDestroy(node->calleeName, node->children.size(), error);
        if (contextDestroy == nullptr && !node->children.empty()) return nullptr;
        return allocateDelegateCell(
            found->second.function, receiver, contextDestroy, "lambda.delegate", error);
    }

LLVMValueRef OopLlvmImplementation::createLambdaCaptureEnvironment(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        const std::uint64_t bytesCount = static_cast<std::uint64_t>(node->children.size()) * 8U;
        LLVMValueRef bytes = LLVMConstInt(LLVMInt64TypeInContext(context_), bytesCount, 0);
        LLVMValueRef args[] = {bytes};
        LLVMValueRef receiver = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, args, 1, "closure.env");
        receiver = checkedStage42Allocation(receiver, "closure.env.alloc", error);
        if (receiver == nullptr) return nullptr;

        for (std::size_t index = 0; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* capture = node->children[index].get();
            LLVMValueRef sourceCell = lowerExpression(capture, error);
            if (sourceCell == nullptr) return nullptr;
            LLVMValueRef value = sourceCell;
            if (capture->captureMode == language::CaptureMode::Copy ||
                capture->captureMode == language::CaptureMode::Move)
            {
                LLVMValueRef sourceAddress = captureCellValueAddress(
                    sourceCell, capture->typeKey, "closure.capture.source", error);
                LLVMValueRef destinationAddress = nullptr;
                value = allocateCaptureCell(
                    capture->typeKey, destinationAddress,
                    "closure.capture.snapshot", error);
                LLVMTypeRef captureType = llvmType(capture->typeKey, error);
                if (sourceAddress == nullptr || destinationAddress == nullptr ||
                    captureType == nullptr || captureType == LLVMVoidTypeInContext(context_))
                    return nullptr;
                LLVMValueRef snapshot = LLVMBuildLoad2(
                    builder_, captureType, sourceAddress, "closure.capture.snapshot.load");
                LLVMBuildStore(builder_, snapshot, destinationAddress);
                if (capture->captureMode == language::CaptureMode::Move && capture->booleanValue)
                {
                    LLVMBuildStore(builder_, LLVMConstNull(captureType), sourceAddress);
                    if (capture->localOrdinal.has_value())
                    {
                        const auto flag = localValueDropFlags_.find(*capture->localOrdinal);
                        if (flag != localValueDropFlags_.end())
                            LLVMBuildStore(
                                builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0),
                                flag->second);
                    }
                    else if (capture->parameterOrdinal.has_value())
                    {
                        const auto flag = parameterValueDropFlags_.find(*capture->parameterOrdinal);
                        if (flag != parameterValueDropFlags_.end())
                            LLVMBuildStore(
                                builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0),
                                flag->second);
                    }
                }
            }
            else
            {
                retainCaptureCell(value);
            }
            LLVMValueRef address = byteOffset(
                receiver, static_cast<std::uint64_t>(index) * 8U, "closure.capture.store.addr");
            LLVMBuildStore(builder_, value, address);
        }
        return receiver;
    }

LLVMValueRef OopLlvmImplementation::lowerDelegateCreate(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() > 1)
            return failValue(error, "Stage 42 delegate-create accepts at most one receiver");

        const bool dynamicTarget = node->virtualSlot.has_value() || node->interfaceSlot.has_value();
        const auto found = functions_.find(node->calleeName);
        if (!dynamicTarget && (found == functions_.end() || found->second.function == nullptr))
            return failValue(error, "Stage 42 delegate target function is unavailable: " + node->calleeName);

        // Interop Foundation (callbacks C -> NativeSharp): an extern C delegate is a raw C
        // function pointer END TO END, so binding a static method to one yields the function's
        // ADDRESS -- no cell. MEASURED before this branch existed: the cell pointer was stored
        // where C expected code, and invoking it executed the cell's bytes (exit 5 for a
        // fixture whose answer is 42). No thunk is needed: since Stage 40.2 the internal
        // function's LLVM signature IS the Win64 C ABI for every C-expressible type (indirect
        // aggregates included), so the pointer a host receives is directly callable from
        // MSVC-compiled code -- certified by execution in the cross-host suite. Instance
        // methods and lambdas are rejected in semantic analysis (a C function pointer has no
        // receiver/environment channel); the null-receiver check here is the backend belt to
        // that suspender.
        if (isExternCDelegateType(node->typeKey))
        {
            if (!node->children.empty())
                return failValue(error,
                    "Stage 47 extern C delegate cannot capture a receiver; only static methods convert");
            const auto* imported = found->second.mirFunction;
            // EN: T08-D18: a C function keeps its identity, as in C: the pointer is the C symbol's address (or
            //     the address bound at run time for a TLS alias), so `s->op == mul` holds in C. C code cannot
            //     raise a NativeSharp exception, so it needs no callback boundary.
            // ES: T08-D18: una funcion C conserva su identidad, como en C: el puntero es la direccion del simbolo C
            //     (o la que se enlaza en ejecucion para un alias TLS), asi que `s->op == mul` se cumple en C. El
            //     codigo C no lanza excepciones NativeSharp, asi que no necesita frontera de callback.
            if (directCFunctionAddress(imported) && !imported->importRequiresRuntimeBinding)
                return found->second.function;
            if (directCFunctionAddress(imported) && imported->importRequiresRuntimeBinding &&
                imported->mangledName.starts_with("__nsc_cpp_tls_import_"))
            {
                LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
                LLVMValueRef library = cStringGlobal(imported->importDllName, "dll");
                LLVMValueRef symbol = cStringGlobal(imported->cAbiName, "sym");
                if (library == nullptr || symbol == nullptr)
                    return failValue(error, "TLS import lost its library or symbol identity");
                LLVMTypeRef parameters[] = {ptr, ptr};
                LLVMTypeRef helperType = LLVMFunctionType(ptr, parameters, 2, 0);
                LLVMValueRef arguments[] = {library, symbol};
                LLVMValueRef target = LLVMBuildCall2(builder_, helperType,
                    cppDllTlsExportHelper(context_, module_), arguments, 2, "import.address");
                if (!emitGuard(LLVMBuildICmp(builder_, LLVMIntNE, target, LLVMConstNull(ptr), "import.bound"),
                               LlvmTrapReason::ImportBindingFailure, "import.binding", error)) return nullptr;
                return target;
            }
            // C1.3 "callback exception boundary": what C receives is the GUARD THUNK's address,
            // not the target's. The thunk has the target's exact signature -- which is already the
            // Win64 C ABI -- so nothing about the call changes for the host; what changes is that
            // a NativeSharp exception reaching the outer edge of the callback traps there instead
            // of unwinding into foreign frames. defineCallbackGuardThunks emitted it before any
            // body was lowered, and it is cached per target, so two conversions of the same method
            // still yield the same pointer.
            return callbackGuardThunk(
                node->calleeName, error, isVectorCallDelegateType(node->typeKey));
        }

        LLVMValueRef receiver = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        if (!node->children.empty())
        {
            receiver = lowerExpression(node->children.front().get(), error);
            if (receiver == nullptr) return nullptr;
            receiver = checkedReceiver(receiver, error);
            if (receiver == nullptr) return nullptr;
        }

        LLVMValueRef code = dynamicTarget
            ? (node->interfaceSlot.has_value()
                ? loadInterfaceTarget(node, receiver, error)
                : loadVirtualTarget(node, receiver, error))
            : found->second.function;
        if (code == nullptr) return nullptr;

        LLVMValueRef contextDestroy = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        if (node->ownership == sema::OwnershipKind::Owned)
        {
            if (descriptorType_ == nullptr)
                return failValue(error, "T08-F05 owning method value has no class descriptor ABI");
            LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
            LLVMValueRef descriptor = LLVMBuildLoad2(
                builder_, ptr, receiver, "delegate.receiver.descriptor");
            LLVMValueRef entryAddress = LLVMBuildStructGEP2(
                builder_, descriptorType_, descriptor, 5, "delegate.receiver.destroy.addr");
            contextDestroy = LLVMBuildLoad2(
                builder_, ptr, entryAddress, "delegate.receiver.destroy");
        }
        return allocateDelegateCell(code, receiver, contextDestroy, "delegate", error);
    }

bool OopLlvmImplementation::prepareDelegateCallTypes(
        const std::vector<std::string>& argumentTypeKeys,
        std::string_view returnTypeKey,
        LLVMValueRef& sretSlot,
        LLVMTypeRef& returnType,
        LLVMTypeRef& staticFunctionType,
        LLVMTypeRef& instanceFunctionType,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Win64 aggregate ABI: the reconstructed target types must match declareFunctions --
        // pointer-typed indirect parameters (the caller already wrapped the values), and an
        // indirect return through an sret slot: leading for a static target, after the
        // receiver for an instance target. Both branches store into the SAME slot, so the
        // merge reads the slot instead of phi-ing an aggregate.
        const bool sretReturn = isIndirectStructTypeKey(returnTypeKey);
        sretSlot = nullptr;
        if (sretReturn)
        {
            LLVMTypeRef carrier = llvmType(returnTypeKey, error);
            if (carrier == nullptr) return false;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(returnTypeKey), "abi.sret.slot");
        }
        returnType = sretReturn
            ? LLVMVoidTypeInContext(context_)
            : llvmType(returnTypeKey, error);
        if (returnType == nullptr) return false;
        std::vector<LLVMTypeRef> staticTypes;
        staticTypes.reserve(argumentTypeKeys.size() + 1);
        if (sretReturn) staticTypes.push_back(ptr);
        for (const std::string& typeKey : argumentTypeKeys)
        {
            LLVMTypeRef type = abiParameterType(typeKey, error);
            if (type == nullptr || type == LLVMVoidTypeInContext(context_))
            {
                failValue(error, "Stage 42 delegate has invalid parameter type '" + typeKey + "'");
                return false;
            }
            staticTypes.push_back(type);
        }
        std::vector<LLVMTypeRef> instanceTypes;
        instanceTypes.reserve(staticTypes.size() + 1);
        instanceTypes.push_back(ptr);
        instanceTypes.insert(instanceTypes.end(), staticTypes.begin(), staticTypes.end());
        staticFunctionType = LLVMFunctionType(
            returnType, staticTypes.empty() ? nullptr : staticTypes.data(),
            static_cast<unsigned>(staticTypes.size()), 0);
        instanceFunctionType = LLVMFunctionType(
            returnType, instanceTypes.data(), static_cast<unsigned>(instanceTypes.size()), 0);
        return true;
    }

LLVMValueRef OopLlvmImplementation::emitDelegateCall(
        LLVMValueRef delegateCell,
        const std::vector<LLVMValueRef>& argumentValues,
        const std::vector<std::string>& argumentTypeKeys,
        std::string_view returnTypeKey,
        std::string& error)
{
        if (argumentValues.size() != argumentTypeKeys.size())
            return failValue(error, "Stage 42 delegate invocation lost signature argument types");
        delegateCell = checkedReceiver(delegateCell, error);
        if (delegateCell == nullptr) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef code = LLVMBuildLoad2(builder_, ptr, delegateCell, "delegate.code");
        LLVMValueRef receiverAddress = byteOffset(delegateCell, 8, "delegate.receiver.addr");
        LLVMValueRef receiver = LLVMBuildLoad2(builder_, ptr, receiverAddress, "delegate.receiver");
        LLVMValueRef hasReceiver = LLVMBuildICmp(
            builder_, LLVMIntNE, receiver, LLVMConstNull(ptr), "delegate.has.receiver");
        LLVMValueRef sretSlot = nullptr;
        LLVMTypeRef returnType = nullptr;
        LLVMTypeRef staticFunctionType = nullptr;
        LLVMTypeRef instanceFunctionType = nullptr;
        if (!prepareDelegateCallTypes(
                argumentTypeKeys, returnTypeKey, sretSlot, returnType,
                staticFunctionType, instanceFunctionType, error)) return nullptr;

        LLVMBasicBlockRef instanceBlock = appendBlock("delegate.instance");
        LLVMBasicBlockRef staticBlock = appendBlock("delegate.static");
        LLVMBasicBlockRef mergeBlock = appendBlock("delegate.merge");
        LLVMBuildCondBr(builder_, hasReceiver, instanceBlock, staticBlock);
        LLVMBasicBlockRef staticIncoming = nullptr;
        LLVMValueRef staticResult = emitDelegateBranchCall(
            staticBlock, mergeBlock, staticFunctionType, code, nullptr, sretSlot,
            argumentValues, returnType, returnTypeKey, "delegate.static.result", 0U,
            staticIncoming, error);
        if (staticResult == nullptr) return nullptr;
        LLVMBasicBlockRef instanceIncoming = nullptr;
        LLVMValueRef instanceResult = emitDelegateBranchCall(
            instanceBlock, mergeBlock, instanceFunctionType, code, receiver, sretSlot,
            argumentValues, returnType, returnTypeKey, "delegate.instance.result", 1U,
            instanceIncoming, error);
        if (instanceResult == nullptr) return nullptr;

        enterBlock(mergeBlock);
        if (sretSlot != nullptr)
        {
            LLVMTypeRef carrier = llvmType(returnTypeKey, error);
            if (carrier == nullptr) return nullptr;
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (returnType == LLVMVoidTypeInContext(context_))
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        LLVMValueRef phi = LLVMBuildPhi(builder_, returnType, "delegate.result");
        LLVMValueRef values[] = {staticResult, instanceResult};
        LLVMBasicBlockRef blocks[] = {staticIncoming, instanceIncoming};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::emitDelegateBranchCall(
        LLVMBasicBlockRef branchBlock,
        LLVMBasicBlockRef mergeBlock,
        LLVMTypeRef functionType,
        LLVMValueRef code,
        LLVMValueRef receiver,
        LLVMValueRef sretSlot,
        const std::vector<LLVMValueRef>& argumentValues,
        LLVMTypeRef returnType,
        std::string_view returnTypeKey,
        const char* resultName,
        unsigned sretPosition,
        LLVMBasicBlockRef& incomingBlock,
        std::string& error)
{
        enterBlock(branchBlock);
        std::vector<LLVMValueRef> arguments;
        arguments.reserve(argumentValues.size() + 2);
        if (receiver != nullptr) arguments.push_back(receiver);
        if (sretSlot != nullptr) arguments.push_back(sretSlot);
        arguments.insert(arguments.end(), argumentValues.begin(), argumentValues.end());
        LLVMValueRef result = buildPotentiallyThrowingCall(
            functionType, code, arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()),
            returnType == LLVMVoidTypeInContext(context_) ? "" : resultName, error);
        if (result == nullptr) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(result, attributeIndex, attribute); },
                sretPosition, returnTypeKey);
        }
        LLVMBuildBr(builder_, mergeBlock);
        incomingBlock = LLVMGetInsertBlock(builder_);
        return result;
    }

bool OopLlvmImplementation::appendExternCDelegateParameterTypes(
        const std::vector<std::string>& argumentTypeKeys,
        std::size_t fixedParameterCount,
        bool sretReturn,
        const std::vector<VectorCallPassing>& passing,
        std::vector<LLVMTypeRef>& parameterTypes,
        std::string& error)
{
        parameterTypes.reserve(fixedParameterCount + 1);
        if (sretReturn) parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
        for (std::size_t index = 0; index < fixedParameterCount; ++index)
        {
            const std::string& typeKey = argumentTypeKeys[index];
            const VectorCallPassing kind = index < passing.size() ? passing[index] : VectorCallPassing::Ordinary;
            LLVMTypeRef type = nullptr;
            if (kind == VectorCallPassing::Vector || kind == VectorCallPassing::HvaInReg)
                type = vectorCallAbiType(typeKey);
            else if (kind == VectorCallPassing::HvaIndirect) type = LLVMPointerTypeInContext(context_, 0);
            else type = abiParameterType(typeKey, error);
            if (type == nullptr || type == LLVMVoidTypeInContext(context_))
            {
                failValue(error, "Stage 47 extern C delegate has invalid parameter type '" + typeKey + "'");
                return false;
            }
            parameterTypes.push_back(type);
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::emitExternCDelegateCall(
        LLVMValueRef functionPointer,
        const std::vector<LLVMValueRef>& argumentValues,
        const std::vector<std::string>& argumentTypeKeys,
        std::string_view returnTypeKey,
        std::string_view delegateTypeKey,
        bool vectorCall,
        std::string& error)
{
        if (argumentValues.size() != argumentTypeKeys.size())
            return failValue(error, "Stage 47 extern C delegate invocation lost signature argument types");
        functionPointer = checkedReceiver(functionPointer, error);
        if (functionPointer == nullptr) return nullptr;

        // Ordinary extern-C delegates use the normal Win64 aggregate ABI.  Item 345 specializes
        // the vectorcall boundary: Float32x4 and HVAs are direct vector-shaped values, including
        // returns, while every other aggregate keeps the established pointer/sret contract.
        // EN: T08-D22: HVAs follow Clang's register count (vectorCallPassing), HFAs of float/double included.
        // ES: T08-D22: las HVA siguen el conteo de registros de Clang, incluidas las HFA de float/double.
        LLVMTypeRef vectorReturnType = vectorCall ? vectorCallAbiType(returnTypeKey) : nullptr;
        const bool sretReturn = isIndirectStructTypeKey(returnTypeKey) && vectorReturnType == nullptr;
        LLVMValueRef sretSlot = nullptr;
        if (sretReturn)
        {
            LLVMTypeRef carrier = llvmType(returnTypeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(returnTypeKey), "abi.sret.slot");
        }
        LLVMTypeRef returnType = sretReturn
            ? LLVMVoidTypeInContext(context_)
            : (vectorReturnType != nullptr ? vectorReturnType : llvmType(returnTypeKey, error));
        if (returnType == nullptr) return nullptr;
        const std::optional<language::FunctionType> structuralFunction =
            language::parseFunctionTypeIdentity(delegateTypeKey);
        const bool sysvCall = structuralFunction && structuralFunction->callingConvention == language::CallingConvention::SysV;
        const bool regCall = structuralFunction && structuralFunction->callingConvention == language::CallingConvention::RegCall;
        const bool preserveMost = structuralFunction && structuralFunction->callingConvention == language::CallingConvention::PreserveMost;
        const bool preserveAll = structuralFunction && structuralFunction->callingConvention == language::CallingConvention::PreserveAll;
        const auto preserveArgument = [](std::string_view key) {
            return key == "builtin:int" || key == "builtin:uint" || key == "builtin:long" ||
                key == "builtin:short" || key == "builtin:bool" || key == "builtin:float" ||
                key == "builtin:double" || key == "builtin:int*";
        };
        if ((preserveMost || preserveAll) && (structuralFunction->variadic ||
            (returnTypeKey != "builtin:int" && returnTypeKey != "builtin:long" && returnTypeKey != "builtin:double") ||
            std::any_of(argumentTypeKeys.begin(), argumentTypeKeys.end(),
                [&](const std::string& key) { return !preserveArgument(key); })))
            return failValue(error, "preserve convention function pointer signature is not yet supported");
        const auto regCallScalar = [](std::string_view key) {
            return key == "builtin:int" || key == "builtin:uint" || key == "builtin:long" ||
                key == "builtin:ulong" || key == "builtin:float" || key == "builtin:sbyte" ||
                key == "builtin:byte" || key == "builtin:short" || key == "builtin:ushort" || key == "builtin:bool";
        };
        if (regCall && (!structuralFunction || structuralFunction->variadic ||
            (!regCallScalar(returnTypeKey) && returnTypeKey != "builtin:void" && returnTypeKey != "builtin:int*") ||
            std::any_of(argumentTypeKeys.begin(), argumentTypeKeys.end(), [&](const std::string& key) {
                return !regCallScalar(key) && key != "builtin:int*";
            })))
            return failValue(error, "regcall function pointer signature is not yet supported");
        const auto sysvScalar = [](std::string_view key) {
            return key == "builtin:int" || key == "builtin:uint" || key == "builtin:long" ||
                key == "builtin:ulong" || key == "builtin:float" || key == "builtin:double" ||
                key == "builtin:sbyte" || key == "builtin:byte" || key == "builtin:short" ||
                key == "builtin:ushort" || key == "builtin:bool" || key.ends_with('*');
        };
        if (sysvCall && ((returnTypeKey != "builtin:void" && !sysvScalar(returnTypeKey)) ||
            std::any_of(argumentTypeKeys.begin(), argumentTypeKeys.end(),
                [&](const std::string& key) { return !sysvScalar(key); })))
            return failValue(error, "sysv_abi function pointer signature is not yet supported");
        const bool variadic = structuralFunction.has_value() && structuralFunction->variadic;
        const std::size_t fixedParameterCount = variadic
            ? structuralFunction->parameters.size() : argumentTypeKeys.size();
        if (fixedParameterCount > argumentTypeKeys.size())
            return failValue(error, "variadic function pointer invocation lost a fixed argument");
        if (sysvCall && variadic)
        {
            for (std::size_t index = fixedParameterCount; index < argumentTypeKeys.size(); ++index)
            {
                const auto& key = argumentTypeKeys[index];
                if (key != "builtin:int" && key != "builtin:uint" && key != "builtin:long" &&
                    key != "builtin:ulong" && key != "builtin:double" && !key.ends_with('*'))
                    return failValue(error, "sysv_abi variadic function pointer argument is not yet supported");
            }
        }
        const std::vector<VectorCallPassing> passing = vectorCall
            ? vectorCallPassing(argumentTypeKeys, fixedParameterCount) : std::vector<VectorCallPassing>{};
        std::vector<LLVMTypeRef> parameterTypes;
        if (!appendExternCDelegateParameterTypes(
                argumentTypeKeys, fixedParameterCount, sretReturn, passing, parameterTypes, error))
            return nullptr;
        LLVMTypeRef functionType = LLVMFunctionType(
            returnType, parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()), variadic ? 1 : 0);
        std::vector<LLVMValueRef> callArguments;
        callArguments.reserve(argumentValues.size() + 1);
        if (sretSlot != nullptr) callArguments.push_back(sretSlot);
        for (std::size_t index = 0; index < argumentValues.size(); ++index)
        {
            LLVMValueRef value = argumentValues[index];
            // EN: the caller left vectors and HVAs as carriers; their passing decides the physical value.
            // ES: el llamador dejo los vectores y las HVA como portadores; su paso decide el valor fisico.
            if (vectorCall && index < passing.size() && vectorCallAbiType(argumentTypeKeys[index]) != nullptr)
            {
                value = vectorCallArgument(value, argumentTypeKeys[index], passing[index], error);
                if (value == nullptr) return nullptr;
            }
            callArguments.push_back(value);
        }
        LLVMValueRef call = LLVMBuildCall2(
            builder_, functionType, functionPointer,
            callArguments.empty() ? nullptr : callArguments.data(),
            static_cast<unsigned>(callArguments.size()),
            returnType == LLVMVoidTypeInContext(context_) ? "" : "cabi.callback.result");
        auto addBoolAttribute = [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddCallSiteAttribute(call, index, attribute); };
        const auto applySysVExtension = [&](std::string_view key, unsigned index) {
            const std::string_view name = key == "builtin:sbyte" || key == "builtin:short" ? "signext" :
                key == "builtin:byte" || key == "builtin:ushort" ? "zeroext" : "";
            if (name.empty()) return;
            const unsigned kind = llvm_api::LLVMGetEnumAttributeKindForName(name.data(), name.size());
            if (kind == 0) return;
            const auto attribute = llvm_api::LLVMCreateEnumAttribute(context_, kind, 0);
            if (attribute != nullptr) LLVMAddCallSiteAttribute(call, index, attribute);
        };
        if (sysvCall) applySysVExtension(returnTypeKey, 0);
        if (returnTypeKey == "builtin:bool" && returnType == LLVMInt1TypeInContext(context_))
            applyBoolZeroExtension(context_, 0, addBoolAttribute);
        for (std::size_t index = 0; index < fixedParameterCount; ++index)
        {
            const std::size_t position = index + (sretReturn ? 1U : 0U);
            if (sysvCall) applySysVExtension(argumentTypeKeys[index], static_cast<unsigned>(position + 1));
            if (argumentTypeKeys[index] == "builtin:bool" &&
                LLVMTypeOf(callArguments[position]) == LLVMInt1TypeInContext(context_))
                applyBoolZeroExtension(context_, static_cast<unsigned>(position + 1), addBoolAttribute);
        }
        if (vectorCall) LLVMSetInstructionCallConv(call, LLVMX86VectorCallCallConv);
        if (sysvCall) LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
        if (regCall) LLVMSetInstructionCallConv(call, LLVMX86RegCallCallConv);
        if (preserveMost) LLVMSetInstructionCallConv(call, LLVMPreserveMostCallConv);
        if (preserveAll) LLVMSetInstructionCallConv(call, LLVMPreserveAllCallConv);
        if (vectorCall)
        {
            applyVectorCallInRegAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, attributeIndex, attribute); },
                sretReturn ? 1U : 0U, passing);
        }
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, attributeIndex, attribute); },
                0U, returnTypeKey);
            LLVMTypeRef carrier = llvmType(returnTypeKey, error);
            if (carrier == nullptr) return nullptr;
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (vectorReturnType != nullptr)
            return vectorCallAbiToCarrier(call, returnTypeKey, error);
        return call;
    }

LLVMValueRef OopLlvmImplementation::lowerDelegateInvoke(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef delegateOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node->children.empty() || node->children.size() != node->argumentCount + 1)
            return failValue(error, "Stage 42 delegate invocation has invalid MIR children");
        LLVMValueRef delegateValue = delegateOverride != nullptr
            ? delegateOverride
            : lowerExpression(node->children.front().get(), error);
        if (delegateValue == nullptr) return nullptr;
        const bool externC = isExternCDelegateType(node->elementTypeKey);
        const bool vectorCall = externC && isVectorCallDelegateType(node->elementTypeKey);
        const auto recordArgument = [](std::string_view key) {
            return key.starts_with("type:") && !key.ends_with('*');
        };
        if (externC && node->cppVariadicCallPlan &&
            (node->typeKey == "builtin:int" || node->typeKey == "builtin:uint" || node->typeKey == "builtin:ulong" ||
             node->typeKey == "builtin:sbyte" || node->typeKey == "builtin:ushort" ||
             node->typeKey == "builtin:byte" || node->typeKey == "builtin:short" ||
             node->typeKey == "builtin:long" || node->typeKey == "builtin:float" || node->typeKey == "builtin:double" ||
             node->typeKey == "builtin:void" || node->typeKey == "builtin:int*") &&
            node->argumentCount >= 2 && node->argumentCount <= 1025 &&
            node->argumentTypeKeys.size() == node->argumentCount &&
            std::any_of(node->argumentTypeKeys.begin(), node->argumentTypeKeys.end(), recordArgument) &&
            std::all_of(node->argumentTypeKeys.begin(), node->argumentTypeKeys.end(),
                [&](const auto& key) { return recordArgument(key) || key == "builtin:int" ||
                    key == "builtin:double" || key == "builtin:int*" || key == "builtin:long" ||
                    key == "builtin:uint" || key == "builtin:ulong"; }))
        {
            using namespace interop::abi_plan;
            const auto& plan = *node->cppVariadicCallPlan;
            const auto signature = language::parseFunctionTypeIdentity(node->elementTypeKey);
            if (!signature || !signature->variadic || signature->callingConvention != language::CallingConvention::SysV ||
                signature->parameters.empty() || signature->parameters.size() >= node->argumentCount ||
                !interop::validateCppVariadicSysVCallPlan(plan, "__nsc_variadic_pointer_abi_probe", signature->parameters.size()) ||
                plan.arguments.size() != node->argumentCount ||
                ((node->typeKey == "builtin:int" || node->typeKey == "builtin:uint") && (plan.physicalReturn.kind != TypeKind::Integer ||
                                                     plan.physicalReturn.widthOrCount != 32)) ||
                ((node->typeKey == "builtin:ulong" || node->typeKey == "builtin:long") && (plan.physicalReturn.kind != TypeKind::Integer ||
                                                       plan.physicalReturn.widthOrCount != 64)) ||
                (node->typeKey == "builtin:sbyte" && (plan.physicalReturn.kind != TypeKind::Integer ||
                    plan.physicalReturn.widthOrCount != 8 || !plan.returnSignExtend || plan.returnZeroExtend)) ||
                (node->typeKey == "builtin:short" && (plan.physicalReturn.kind != TypeKind::Integer ||
                    plan.physicalReturn.widthOrCount != 16 || !plan.returnSignExtend || plan.returnZeroExtend)) ||
                (node->typeKey == "builtin:byte" && (plan.physicalReturn.kind != TypeKind::Integer ||
                    plan.physicalReturn.widthOrCount != 8 || plan.returnSignExtend || !plan.returnZeroExtend)) ||
                (node->typeKey == "builtin:ushort" && (plan.physicalReturn.kind != TypeKind::Integer ||
                    plan.physicalReturn.widthOrCount != 16 || plan.returnSignExtend || !plan.returnZeroExtend)) ||
                (node->typeKey == "builtin:float" && plan.physicalReturn.kind != TypeKind::Float) ||
                (node->typeKey == "builtin:double" && plan.physicalReturn.kind != TypeKind::Double) ||
                (node->typeKey == "builtin:void" && plan.physicalReturn.kind != TypeKind::Void) ||
                (node->typeKey == "builtin:int*" && plan.physicalReturn.kind != TypeKind::Pointer))
                return failValue(error, "sysv_abi variadic function pointer record plan is not supported");
            std::vector<LLVMValueRef> arguments;
            std::vector<LLVMTypeRef> fixedTypes;
            struct VariadicByValue { unsigned ordinal; LLVMTypeRef carrier; unsigned alignment; };
            std::vector<VariadicByValue> byValues;
            for (std::size_t i = 0; i < node->argumentCount; ++i)
            {
                const auto& key = node->argumentTypeKeys[i];
                const auto& logical = plan.arguments[i];
                const auto physical = logical.firstPhysical;
                if (key == "builtin:int" || key == "builtin:double" || key == "builtin:int*" ||
                    key == "builtin:long" || key == "builtin:uint" || key == "builtin:ulong")
                {
                    if (logical.passing != Passing::Direct || logical.physicalCount != 1 ||
                        physical != arguments.size() || physical >= plan.physicalParameters.size() ||
                        ((key == "builtin:int" || key == "builtin:uint") && (plan.physicalParameters[physical].type.kind != TypeKind::Integer ||
                         plan.physicalParameters[physical].type.widthOrCount != 32)) ||
                        (key == "builtin:double" && plan.physicalParameters[physical].type.kind != TypeKind::Double) ||
                        ((key == "builtin:long" || key == "builtin:ulong") && (plan.physicalParameters[physical].type.kind != TypeKind::Integer ||
                         plan.physicalParameters[physical].type.widthOrCount != 64)) ||
                        (key == "builtin:int*" && plan.physicalParameters[physical].type.kind != TypeKind::Pointer))
                        return failValue(error, "sysv_abi variadic function pointer scalar plan is not supported");
                    LLVMValueRef value = argumentOverrides ? (*argumentOverrides)[i] : lowerExpression(node->children[i + 1].get(), error);
                    if (!value || terminated()) return value;
                    LLVMTypeRef expected = (key == "builtin:int" || key == "builtin:uint") ? LLVMInt32TypeInContext(context_) :
                        (key == "builtin:long" || key == "builtin:ulong") ? LLVMInt64TypeInContext(context_) :
                        key == "builtin:double" ? LLVMDoubleTypeInContext(context_) : LLVMPointerTypeInContext(context_, 0);
                    if (LLVMTypeOf(value) != expected)
                        return failValue(error, "sysv_abi variadic function pointer scalar does not match its plan");
                    if (i < signature->parameters.size())
                    {
                        if (signature->parameters[i] != key)
                            return failValue(error, "sysv_abi variadic function pointer fixed argument does not match its signature");
                        fixedTypes.push_back(expected);
                    }
                    arguments.push_back(value);
                    continue;
                }
                const bool fixedRecord = i < signature->parameters.size();
                if (fixedRecord && signature->parameters[i] != key)
                    return failValue(error, "sysv_abi variadic function pointer fixed record does not match its signature");
                const auto* layout = findStructLayout(mirModel_, key);
                const auto ordinal = nameLookup_.typeOrdinalForIdentity(std::string_view(key).substr(5));
                const auto* imported = ordinal ? nameLookup_.cppImportedType(*ordinal) : nullptr;
                if (!layout || !imported || imported->isIncomplete ||
                    !imported->deferredImportError.empty() || !imported->objectLayout ||
                    (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
                     imported->origin != frontend::CppImportedTypeOrigin::CUnion) ||
                    imported->objectLayout->sizeBytes != layout->size || imported->objectLayout->alignmentBytes != layout->alignment)
                    return failValue(error, "sysv_abi variadic function pointer record plan is not supported");
                LLVMValueRef record = argumentOverrides ? (*argumentOverrides)[i] : lowerExpression(node->children[i + 1].get(), error);
                if (!record || terminated()) return record;
                LLVMTypeRef carrier = llvmType(key, error);
                if (!carrier || LLVMTypeOf(record) != carrier)
                    return failValue(error, "sysv_abi variadic function pointer argument does not match its plan");
                if (logical.passing == Passing::Indirect)
                {
                    if (logical.physicalCount != 1 ||
                        physical != arguments.size() || physical >= plan.physicalParameters.size() ||
                        !((layout->size == 20 && layout->alignment == 4) ||
                          (layout->size == 16 && layout->alignment == 8)) ||
                        !plan.physicalParameters[physical].byValue || plan.physicalParameters[physical].alignment != 8)
                        return failValue(error, "sysv_abi variadic function pointer byval plan is not supported");
                    LLVMValueRef slot = entryBlockAlloca(carrier, 8, "sysv.varptr.byval");
                    LLVMValueRef store = LLVMBuildStore(builder_, record, slot);
                    LLVMSetAlignment(store, 8);
                    byValues.push_back({static_cast<unsigned>(arguments.size()), carrier, 8});
                    if (fixedRecord) fixedTypes.push_back(LLVMPointerTypeInContext(context_, 0));
                    arguments.push_back(slot);
                    continue;
                }
                if (logical.passing != Passing::Direct || logical.physicalCount < 1 || logical.physicalCount > 2 ||
                    physical != arguments.size() || physical + logical.physicalCount > plan.physicalParameters.size() ||
                    layout->alignment == 0 || layout->alignment > (1ULL << 29) ||
                    (layout->alignment & (layout->alignment - 1)) != 0)
                    return failValue(error, "sysv_abi variadic function pointer record plan is not supported");
                const auto alignment = static_cast<unsigned>(layout->alignment);
                LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "sysv.varptr.record");
                LLVMValueRef store = LLVMBuildStore(builder_, record, slot);
                LLVMSetAlignment(store, alignment);
                for (std::size_t piece = 0; piece < logical.physicalCount; ++piece)
                {
                    const auto& type = plan.physicalParameters[physical + piece].type;
                    LLVMTypeRef llvmPiece = nullptr;
                    std::uint64_t bytes = 0;
                    if (type.kind == TypeKind::Integer)
                    {
                        llvmPiece = LLVMIntTypeInContext(context_, static_cast<unsigned>(type.widthOrCount));
                        bytes = (type.widthOrCount + 7) / 8;
                    }
                    else if (type.kind == TypeKind::Float) { llvmPiece = LLVMFloatTypeInContext(context_); bytes = 4; }
                    else if (type.kind == TypeKind::Double) { llvmPiece = LLVMDoubleTypeInContext(context_); bytes = 8; }
                    else if (type.kind == TypeKind::Vector) { llvmPiece = LLVMVectorType(LLVMFloatTypeInContext(context_), 2); bytes = 8; }
                    const std::uint64_t offset = piece * 8;
                    if (!llvmPiece || bytes == 0 || offset > layout->size || bytes > layout->size - offset)
                        return failValue(error, "sysv_abi variadic function pointer record piece exceeds its layout");
                    LLVMValueRef address = offset ? byteOffset(slot, offset, "sysv.varptr.piece.addr") : slot;
                    LLVMValueRef value = LLVMBuildLoad2(builder_, llvmPiece, address, "sysv.varptr.piece");
                    LLVMSetAlignment(value, alignment < 8 ? alignment : 8);
                    if (fixedRecord) fixedTypes.push_back(llvmPiece);
                    arguments.push_back(value);
                }
            }
            if (arguments.size() != plan.physicalParameters.size())
                return failValue(error, "sysv_abi variadic function pointer physical argument count is invalid");
            releaseTransferredArguments(node);
            delegateValue = checkedReceiver(delegateValue, error);
            if (!delegateValue) return nullptr;
            LLVMTypeRef returnType = (node->typeKey == "builtin:int" || node->typeKey == "builtin:uint") ? LLVMInt32TypeInContext(context_) :
                (node->typeKey == "builtin:sbyte" || node->typeKey == "builtin:byte") ? LLVMInt8TypeInContext(context_) :
                (node->typeKey == "builtin:ushort" || node->typeKey == "builtin:short") ? LLVMInt16TypeInContext(context_) :
                (node->typeKey == "builtin:ulong" || node->typeKey == "builtin:long") ? LLVMInt64TypeInContext(context_) :
                node->typeKey == "builtin:float" ? LLVMFloatTypeInContext(context_) :
                node->typeKey == "builtin:double" ? LLVMDoubleTypeInContext(context_) :
                node->typeKey == "builtin:int*" ? LLVMPointerTypeInContext(context_, 0) : LLVMVoidTypeInContext(context_);
            LLVMTypeRef functionType = LLVMFunctionType(returnType, fixedTypes.data(),
                static_cast<unsigned>(fixedTypes.size()), 1);
            LLVMValueRef call = LLVMBuildCall2(builder_, functionType, delegateValue, arguments.data(),
                static_cast<unsigned>(arguments.size()), node->typeKey == "builtin:void" ? "" : "sysv.varptr.result");
            LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
            if (plan.returnSignExtend || plan.returnZeroExtend)
            {
                const std::string_view name = plan.returnSignExtend ? "signext" : "zeroext";
                const auto kind = llvm_api::LLVMGetEnumAttributeKindForName(name.data(), name.size());
                if (!kind) return failValue(error, "sysv_abi variadic function pointer return extension is unavailable");
                const auto attribute = llvm_api::LLVMCreateEnumAttribute(context_, kind, 0);
                if (!attribute) return failValue(error, "sysv_abi variadic function pointer return extension is unavailable");
                LLVMAddCallSiteAttribute(call, 0, attribute);
            }
            for (const auto& value : byValues)
                applySysVRecordByValue(context_, value.carrier, value.alignment, value.ordinal,
                    [&](LLVMAttributeIndex index, LLVMAttributeRef attribute) { LLVMAddCallSiteAttribute(call, index, attribute); });
            return call;
        }
        const bool hasRecord = std::any_of(node->argumentTypeKeys.begin(), node->argumentTypeKeys.end(), recordArgument);
        const bool recordReturn = recordArgument(node->typeKey);
        if (externC && node->cppFunctionPointerPlan && (hasRecord || recordReturn) && (node->typeKey == "builtin:int" || recordReturn))
        {
            using namespace interop::abi_plan;
            const auto& plan = *node->cppFunctionPointerPlan;
            const auto signature = language::parseFunctionTypeIdentity(node->elementTypeKey);
            if (!interop::validateCppSysVFunctionPointerPlan(plan) || !signature ||
                signature->callingConvention != language::CallingConvention::SysV || signature->variadic ||
                signature->parameters.size() != node->argumentCount ||
                node->argumentTypeKeys.size() != node->argumentCount ||
                plan.logicalParameters.size() != node->argumentCount)
                return failValue(error, "sysv_abi function pointer record plan is not supported");
            LLVMTypeRef returnType = LLVMInt32TypeInContext(context_);
            LLVMTypeRef returnCarrier = nullptr;
            bool indirectReturn = false;
            if (recordReturn)
            {
                const auto* returnLayout = findStructLayout(mirModel_, node->typeKey);
                const auto ordinal = nameLookup_.typeOrdinalForIdentity(std::string_view(node->typeKey).substr(5));
                const auto* imported = ordinal ? nameLookup_.cppImportedType(*ordinal) : nullptr;
                const auto& physical = plan.physicalReturn;
                if (!returnLayout || returnLayout->alignment != 4 || !imported || imported->isIncomplete ||
                    !imported->deferredImportError.empty() || !imported->objectLayout ||
                    (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
                     imported->origin != frontend::CppImportedTypeOrigin::CUnion) ||
                    imported->objectLayout->sizeBytes != returnLayout->size ||
                    imported->objectLayout->alignmentBytes != returnLayout->alignment)
                    return failValue(error, "sysv_abi function pointer record return is not supported");
                indirectReturn = returnLayout->size == 20 && plan.logicalReturn.passing == Passing::Indirect &&
                    plan.logicalReturn.alignment == 4 && physical.kind == TypeKind::Void;
                const bool directReturn = returnLayout->size == 12 && plan.logicalReturn.passing == Passing::Direct &&
                    physical.kind == TypeKind::Structure && !physical.packed && physical.elements.size() == 2 &&
                    physical.elements[0].kind == TypeKind::Integer && physical.elements[0].widthOrCount == 64 &&
                    physical.elements[1].kind == TypeKind::Integer && physical.elements[1].widthOrCount == 32;
                if (!indirectReturn && !directReturn)
                    return failValue(error, "sysv_abi function pointer record return is not supported");
                returnCarrier = llvmType(node->typeKey, error);
                if (!returnCarrier) return nullptr;
                LLVMTypeRef pieces[] = {LLVMInt64TypeInContext(context_), LLVMInt32TypeInContext(context_)};
                returnType = indirectReturn ? LLVMVoidTypeInContext(context_) : LLVMStructTypeInContext(context_, pieces, 2, 0);
            }
            else if (plan.logicalReturn.passing != Passing::Direct || plan.physicalReturn.kind != TypeKind::Integer ||
                plan.physicalReturn.widthOrCount != 32)
                return failValue(error, "sysv_abi function pointer return plan is not supported");
            const auto plain = [](const PhysicalParameter& p) {
                return !p.pointee && !p.structureReturn && !p.byValue && !p.signExtend &&
                    !p.zeroExtend && !p.inRegister && !p.noAlias;
            };
            const auto integer = [&](const PhysicalParameter& p, std::uint64_t width) {
                return plain(p) && p.type.kind == TypeKind::Integer && p.type.widthOrCount == width;
            };
            std::vector<LLVMValueRef> evaluated;
            evaluated.reserve(node->argumentCount);
            for (std::size_t i = 0; i < node->argumentCount; ++i)
            {
                LLVMValueRef value = argumentOverrides ? (*argumentOverrides)[i] :
                    lowerExpression(node->children[i + 1].get(), error);
                if (!value || terminated()) return value;
                evaluated.push_back(value);
            }
            std::vector<LLVMTypeRef> physicalTypes;
            std::vector<LLVMValueRef> physicalArguments;
            LLVMValueRef returnSlot = nullptr;
            if (indirectReturn)
            {
                returnSlot = entryBlockAlloca(returnCarrier, 4, "sysv.callback.sret");
                physicalTypes.push_back(LLVMPointerTypeInContext(context_, 0));
                physicalArguments.push_back(returnSlot);
            }
            struct ByValueAttribute { unsigned ordinal; LLVMTypeRef carrier; unsigned alignment; };
            std::vector<ByValueAttribute> byValues;
            for (std::size_t i = 0; i < node->argumentCount; ++i)
            {
                const auto& key = node->argumentTypeKeys[i];
                const auto& logical = plan.logicalParameters[i];
                if (logical.offset != 0)
                    return failValue(error, "sysv_abi function pointer argument plan is not supported");
                if (!recordArgument(key))
                {
                    if (key != "builtin:int" || logical.passing != Passing::Direct || logical.physicalCount != 1 ||
                        !integer(plan.physicalParameters[logical.firstPhysical], 32) ||
                        LLVMTypeOf(evaluated[i]) != LLVMInt32TypeInContext(context_))
                        return failValue(error, "sysv_abi function pointer scalar plan is not supported");
                    physicalTypes.push_back(LLVMInt32TypeInContext(context_));
                    physicalArguments.push_back(evaluated[i]);
                    continue;
                }
                const auto* layout = findStructLayout(mirModel_, key);
                const auto ordinal = nameLookup_.typeOrdinalForIdentity(std::string_view(key).substr(5));
                const auto* imported = ordinal ? nameLookup_.cppImportedType(*ordinal) : nullptr;
                if (!layout || !imported || imported->isIncomplete || !imported->deferredImportError.empty() ||
                    !imported->objectLayout ||
                    (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
                     imported->origin != frontend::CppImportedTypeOrigin::CUnion) ||
                    imported->objectLayout->sizeBytes != layout->size ||
                    imported->objectLayout->alignmentBytes != layout->alignment)
                    return failValue(error, "sysv_abi function pointer record provenance is invalid");
                LLVMTypeRef carrier = llvmType(key, error);
                if (!carrier || LLVMTypeOf(evaluated[i]) != carrier)
                    return failValue(error, "sysv_abi function pointer record carrier does not match its plan");
                if (logical.passing == Passing::Indirect)
                {
                    if (logical.physicalCount != 1 || layout->size != 20 || layout->alignment != 4 ||
                        !plan.physicalParameters[logical.firstPhysical].byValue ||
                        plan.physicalParameters[logical.firstPhysical].alignment != 8)
                        return failValue(error, "sysv_abi function pointer byval plan is not supported");
                    LLVMValueRef slot = entryBlockAlloca(carrier, 8, "sysv.callback.byval");
                    LLVMValueRef store = LLVMBuildStore(builder_, evaluated[i], slot);
                    LLVMSetAlignment(store, 8);
                    byValues.push_back({static_cast<unsigned>(physicalArguments.size()), carrier, 8});
                    physicalTypes.push_back(LLVMPointerTypeInContext(context_, 0));
                    physicalArguments.push_back(slot);
                    continue;
                }
                if (logical.passing != Passing::Direct || logical.physicalCount != 2)
                    return failValue(error, "sysv_abi function pointer record plan is not supported");
                const auto& first = plan.physicalParameters[logical.firstPhysical];
                const auto& second = plan.physicalParameters[logical.firstPhysical + 1];
                const bool integerShape = layout->alignment == 4 && (layout->size == 12 || layout->size == 16) &&
                    integer(first, 64) && integer(second, layout->size == 12 ? 32 : 64);
                const bool mixedShape = layout->alignment == 8 && layout->size == 16 && plain(first) &&
                    first.type.kind == TypeKind::Double && integer(second, 32);
                if (!integerShape && !mixedShape)
                    return failValue(error, "sysv_abi function pointer record plan is not supported");
                const auto alignment = static_cast<unsigned>(layout->alignment);
                LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "sysv.callback.record");
                LLVMValueRef store = LLVMBuildStore(builder_, evaluated[i], slot);
                LLVMSetAlignment(store, alignment);
                LLVMTypeRef types[] = {mixedShape ? LLVMDoubleTypeInContext(context_) : LLVMInt64TypeInContext(context_),
                    mixedShape || layout->size == 12 ? LLVMInt32TypeInContext(context_) : LLVMInt64TypeInContext(context_)};
                LLVMValueRef pieces[] = {
                    LLVMBuildLoad2(builder_, types[0], slot, "sysv.callback.first"),
                    LLVMBuildLoad2(builder_, types[1], byteOffset(slot, 8, "sysv.callback.second.addr"),
                        "sysv.callback.second")};
                LLVMSetAlignment(pieces[0], alignment);
                LLVMSetAlignment(pieces[1], alignment);
                physicalTypes.insert(physicalTypes.end(), std::begin(types), std::end(types));
                physicalArguments.insert(physicalArguments.end(), std::begin(pieces), std::end(pieces));
            }
            if (physicalArguments.size() != plan.physicalParameters.size())
                return failValue(error, "sysv_abi function pointer physical argument count is invalid");
            releaseTransferredArguments(node);
            delegateValue = checkedReceiver(delegateValue, error);
            if (!delegateValue) return nullptr;
            LLVMTypeRef functionType = LLVMFunctionType(returnType, physicalTypes.data(),
                static_cast<unsigned>(physicalTypes.size()), 0);
            LLVMValueRef call = LLVMBuildCall2(builder_, functionType, delegateValue, physicalArguments.data(),
                static_cast<unsigned>(physicalArguments.size()), indirectReturn ? "" : "sysv.callback.result");
            LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
            if (indirectReturn)
                applySretAttributes([&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                    { LLVMAddCallSiteAttribute(call, index, attribute); }, 0, node->typeKey);
            for (const auto& value : byValues)
                applySysVRecordByValue(context_, value.carrier, value.alignment, value.ordinal,
                    [&](LLVMAttributeIndex index, LLVMAttributeRef attribute) { LLVMAddCallSiteAttribute(call, index, attribute); });
            if (recordReturn)
            {
                if (indirectReturn)
                {
                    LLVMValueRef result = LLVMBuildLoad2(builder_, returnCarrier, returnSlot, "sysv.callback.sret.value");
                    LLVMSetAlignment(result, 4);
                    return result;
                }
                LLVMValueRef slot = entryBlockAlloca(returnCarrier, 4, "sysv.callback.return");
                LLVMValueRef first = LLVMBuildStore(builder_, LLVMBuildExtractValue(builder_, call, 0, "sysv.callback.return.first"), slot);
                LLVMValueRef second = LLVMBuildStore(builder_, LLVMBuildExtractValue(builder_, call, 1, "sysv.callback.return.second"),
                    byteOffset(slot, 8, "sysv.callback.return.second.addr"));
                LLVMSetAlignment(first, 4);
                LLVMSetAlignment(second, 4);
                LLVMValueRef result = LLVMBuildLoad2(builder_, returnCarrier, slot, "sysv.callback.return.value");
                LLVMSetAlignment(result, 4);
                return result;
            }
            return call;
        }
        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->argumentCount);
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index - 1]
                : lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            // Win64 aggregate ABI: indirect struct arguments cross ANY call boundary --
            // delegate targets included -- as pointers to per-call copies. Wrapped once here
            // so the static/instance/extern-C emitters all see ABI-ready values. Measured
            // before this existed: a delegate taking a 64-byte struct read garbage (1 at -O0,
            // 5 at -O2, for a fixture whose answer is 4).
            if (index - 1 < node->argumentTypeKeys.size())
            {
                const std::string& typeKey = node->argumentTypeKeys[index - 1];
                // A vectorcall vector/HVA remains a carrier until emitExternCDelegateCall turns
                // it into the direct external ABI value.  All other aggregates keep the normal
                // per-call pointer copy.
                if (!vectorCall || vectorCallAbiType(typeKey) == nullptr)
                    value = wrapIndirectArgument(value, typeKey, error);
                if (value == nullptr) return nullptr;
            }
            arguments.push_back(value);
        }
        releaseTransferredArguments(node);
        if (externC)
        {
            return emitExternCDelegateCall(
                delegateValue, arguments, node->argumentTypeKeys, node->typeKey,
                node->elementTypeKey, vectorCall, error);
        }
        return emitDelegateCall(delegateValue, arguments, node->argumentTypeKeys, node->typeKey, error);
    }

LLVMValueRef OopLlvmImplementation::delegateCellsEqual(LLVMValueRef left, LLVMValueRef right)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef leftCode = LLVMBuildLoad2(builder_, ptr, left, "event.left.code");
        LLVMValueRef rightCode = LLVMBuildLoad2(builder_, ptr, right, "event.right.code");
        LLVMValueRef leftReceiver = LLVMBuildLoad2(
            builder_, ptr, byteOffset(left, 8, "event.left.receiver.addr"), "event.left.receiver");
        LLVMValueRef rightReceiver = LLVMBuildLoad2(
            builder_, ptr, byteOffset(right, 8, "event.right.receiver.addr"), "event.right.receiver");
        LLVMValueRef codeEqual = LLVMBuildICmp(builder_, LLVMIntEQ, leftCode, rightCode, "event.code.equal");
        LLVMValueRef receiverEqual = LLVMBuildICmp(
            builder_, LLVMIntEQ, leftReceiver, rightReceiver, "event.receiver.equal");
        return LLVMBuildAnd(builder_, codeEqual, receiverEqual, "event.handler.equal");
    }

LLVMValueRef OopLlvmImplementation::lowerEventAdd(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 2)
            return failValue(error, "Stage 42 event-add requires backing address and handler");
        LLVMValueRef backingAddress = lowerExpression(node->children[0].get(), error);
        LLVMValueRef handler = backingAddress != nullptr ? lowerExpression(node->children[1].get(), error) : nullptr;
        if (backingAddress == nullptr || handler == nullptr) return nullptr;
        handler = checkedReceiver(handler, error);
        if (handler == nullptr) return nullptr;

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef bytes = LLVMConstInt(LLVMInt64TypeInContext(context_), 16, 0);
        LLVMValueRef allocArgs[] = {bytes};
        LLVMValueRef eventNode = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, allocArgs, 1, "event.node");
        eventNode = checkedStage42Allocation(eventNode, "event.add.alloc", error);
        if (eventNode == nullptr) return nullptr;
        LLVMBuildStore(builder_, handler, eventNode);
        LLVMValueRef nextAddress = byteOffset(eventNode, 8, "event.next.addr");
        LLVMBuildStore(builder_, LLVMConstNull(ptr), nextAddress);

        // Event invocation order is subscription order. Keep a singly-linked FIFO list and
        // append the new node; removing then adding a handler naturally moves it to the tail.
        LLVMValueRef head = LLVMBuildLoad2(builder_, ptr, backingAddress, "event.head");
        LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef emptyBlock = appendBlock("event.add.empty");
        LLVMBasicBlockRef loopBlock = appendBlock("event.add.loop");
        LLVMBasicBlockRef appendBlockRef = appendBlock("event.add.append");
        LLVMBasicBlockRef doneBlock = appendBlock("event.add.done");
        LLVMValueRef empty = LLVMBuildICmp(
            builder_, LLVMIntEQ, head, LLVMConstNull(ptr), "event.add.is.empty");
        LLVMBuildCondBr(builder_, empty, emptyBlock, loopBlock);

        enterBlock(emptyBlock);
        LLVMBuildStore(builder_, eventNode, backingAddress);
        LLVMBuildBr(builder_, doneBlock);

        enterBlock(loopBlock);
        LLVMValueRef current = LLVMBuildPhi(builder_, ptr, "event.add.current");
        LLVMValueRef initialValues[] = {head};
        LLVMBasicBlockRef initialBlocks[] = {preheader};
        LLVMAddIncoming(current, initialValues, initialBlocks, 1);
        LLVMValueRef currentNextAddress = byteOffset(current, 8, "event.add.current.next.addr");
        LLVMValueRef currentNext = LLVMBuildLoad2(
            builder_, ptr, currentNextAddress, "event.add.current.next");
        LLVMValueRef atTail = LLVMBuildICmp(
            builder_, LLVMIntEQ, currentNext, LLVMConstNull(ptr), "event.add.at.tail");
        LLVMBasicBlockRef advanceBlock = appendBlock("event.add.advance");
        LLVMBuildCondBr(builder_, atTail, appendBlockRef, advanceBlock);

        enterBlock(advanceBlock);
        LLVMBuildBr(builder_, loopBlock);
        LLVMValueRef nextValues[] = {currentNext};
        LLVMBasicBlockRef nextBlocks[] = {advanceBlock};
        LLVMAddIncoming(current, nextValues, nextBlocks, 1);

        enterBlock(appendBlockRef);
        LLVMBuildStore(builder_, eventNode, currentNextAddress);
        LLVMBuildBr(builder_, doneBlock);

        enterBlock(doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

LLVMValueRef OopLlvmImplementation::lowerEventRemove(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 2)
            return failValue(error, "Stage 42 event-remove requires backing address and handler");
        LLVMValueRef backingAddress = lowerExpression(node->children[0].get(), error);
        LLVMValueRef target = backingAddress != nullptr ? lowerExpression(node->children[1].get(), error) : nullptr;
        if (backingAddress == nullptr || target == nullptr) return nullptr;
        target = checkedReceiver(target, error);
        if (target == nullptr) return nullptr;

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef initial = LLVMBuildLoad2(builder_, ptr, backingAddress, "event.remove.head");
        LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef loopBlock = appendBlock("event.remove.loop");
        LLVMBasicBlockRef bodyBlock = appendBlock("event.remove.body");
        LLVMBasicBlockRef removeBlock = appendBlock("event.remove.match");
        LLVMBasicBlockRef advanceBlock = appendBlock("event.remove.advance");
        LLVMBasicBlockRef doneBlock = appendBlock("event.remove.done");
        LLVMBuildBr(builder_, loopBlock);

        enterBlock(loopBlock);
        LLVMValueRef currentPhi = LLVMBuildPhi(builder_, ptr, "event.remove.current");
        LLVMValueRef linkPhi = LLVMBuildPhi(builder_, ptr, "event.remove.link");
        LLVMValueRef initialValues[] = {initial};
        LLVMBasicBlockRef initialBlocks[] = {preheader};
        LLVMAddIncoming(currentPhi, initialValues, initialBlocks, 1);
        LLVMValueRef initialLinks[] = {backingAddress};
        LLVMAddIncoming(linkPhi, initialLinks, initialBlocks, 1);
        LLVMValueRef hasCurrent = LLVMBuildICmp(
            builder_, LLVMIntNE, currentPhi, LLVMConstNull(ptr), "event.remove.has.current");
        LLVMBuildCondBr(builder_, hasCurrent, bodyBlock, doneBlock);

        enterBlock(bodyBlock);
        LLVMValueRef currentHandler = LLVMBuildLoad2(builder_, ptr, currentPhi, "event.remove.handler");
        LLVMValueRef equal = delegateCellsEqual(currentHandler, target);
        LLVMBuildCondBr(builder_, equal, removeBlock, advanceBlock);

        enterBlock(removeBlock);
        LLVMValueRef removeNextAddress = byteOffset(currentPhi, 8, "event.remove.next.addr");
        LLVMValueRef removeNext = LLVMBuildLoad2(builder_, ptr, removeNextAddress, "event.remove.next");
        LLVMBuildStore(builder_, removeNext, linkPhi);
        LLVMValueRef freeArgs[] = {currentPhi};
        buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, freeArgs, 1, "");
        LLVMBuildBr(builder_, doneBlock);

        enterBlock(advanceBlock);
        LLVMValueRef nextAddress = byteOffset(currentPhi, 8, "event.advance.next.addr");
        LLVMValueRef next = LLVMBuildLoad2(builder_, ptr, nextAddress, "event.advance.next");
        LLVMBuildBr(builder_, loopBlock);
        LLVMValueRef nextValues[] = {next};
        LLVMBasicBlockRef advanceIncoming[] = {advanceBlock};
        LLVMAddIncoming(currentPhi, nextValues, advanceIncoming, 1);
        LLVMValueRef linkValues[] = {nextAddress};
        LLVMAddIncoming(linkPhi, linkValues, advanceIncoming, 1);

        enterBlock(doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

LLVMValueRef OopLlvmImplementation::lowerEventInvoke(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef headOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node->children.empty() || node->children.size() != node->argumentCount + 1 ||
            node->typeKey != "builtin:void")
            return failValue(error, "Stage 42 event invocation requires void delegate semantics");
        LLVMValueRef head = headOverride != nullptr
            ? headOverride
            : lowerExpression(node->children.front().get(), error);
        if (head == nullptr) return nullptr;
        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->argumentCount);
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index - 1]
                : lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            arguments.push_back(value);
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef loopBlock = appendBlock("event.invoke.loop");
        LLVMBasicBlockRef bodyBlock = appendBlock("event.invoke.body");
        LLVMBasicBlockRef doneBlock = appendBlock("event.invoke.done");
        LLVMBuildBr(builder_, loopBlock);
        enterBlock(loopBlock);
        LLVMValueRef currentPhi = LLVMBuildPhi(builder_, ptr, "event.invoke.current");
        LLVMValueRef initialValues[] = {head};
        LLVMBasicBlockRef initialBlocks[] = {preheader};
        LLVMAddIncoming(currentPhi, initialValues, initialBlocks, 1);
        LLVMValueRef hasCurrent = LLVMBuildICmp(
            builder_, LLVMIntNE, currentPhi, LLVMConstNull(ptr), "event.invoke.has.current");
        LLVMBuildCondBr(builder_, hasCurrent, bodyBlock, doneBlock);

        enterBlock(bodyBlock);
        LLVMValueRef handler = LLVMBuildLoad2(builder_, ptr, currentPhi, "event.invoke.handler");
        LLVMValueRef next = LLVMBuildLoad2(
            builder_, ptr, byteOffset(currentPhi, 8, "event.invoke.next.addr"), "event.invoke.next");
        // An event is a sequence of delegate calls. Apply the Win64 aggregate adaptation inside
        // the loop so every subscriber gets a fresh by-value copy, whether its target is static
        // or receives a hidden instance receiver.
        std::vector<LLVMValueRef> abiArguments;
        abiArguments.reserve(arguments.size());
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            LLVMValueRef value = arguments[index];
            if (index < node->argumentTypeKeys.size())
            {
                value = wrapIndirectArgument(value, node->argumentTypeKeys[index], error);
                if (value == nullptr) return nullptr;
            }
            abiArguments.push_back(value);
        }
        if (emitDelegateCall(handler, abiArguments, node->argumentTypeKeys, "builtin:void", error) == nullptr)
            return nullptr;
        LLVMBasicBlockRef afterCall = LLVMGetInsertBlock(builder_);
        LLVMBuildBr(builder_, loopBlock);
        LLVMValueRef nextValues[] = {next};
        LLVMBasicBlockRef nextBlocks[] = {afterCall};
        LLVMAddIncoming(currentPhi, nextValues, nextBlocks, 1);

        enterBlock(doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

}
