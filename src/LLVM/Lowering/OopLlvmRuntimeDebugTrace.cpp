#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/LlvmInstrumentationSwitches.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::ensureRuntimeDebugTrace(std::string& error)
{
        if (!kRuntimeDebugTraceEnabled || debugInformation_ != DebugInformationKind::Full ||
            runtimeDebugTraceFunction_ != nullptr)
            return true;

        LLVMTypeRef voidType = LLVMVoidTypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);

        LLVMTypeRef traceParameters[] = {i32, ptr, i32};
        runtimeDebugTraceFunctionType_ = LLVMFunctionType(voidType, traceParameters, 3, 0);
        runtimeDebugTraceFunction_ = LLVMAddFunction(
            module_, "__nsc_runtime_debug_trace", runtimeDebugTraceFunctionType_);
        if (runtimeDebugTraceFunction_ == nullptr)
            return fail(error, "INSTRUMENTATION CODE could not declare the runtime Debug trace sink");
        LLVMSetLinkage(runtimeDebugTraceFunction_, LLVMInternalLinkage);
        applyFunctionAttribute(runtimeDebugTraceFunction_, "nounwind");

        runtimeDebugTraceFlags_ = LLVMAddGlobal(
            module_, i32, "__nsc_runtime_debug_trace_flags");
        if (runtimeDebugTraceFlags_ == nullptr)
            return fail(error, "INSTRUMENTATION CODE could not declare the runtime Debug trace flags");
        // Bit 0: callable entry. Bit 1: allocation/deallocation. Bit 2: field stores. Set to zero
        // to park the trace without deleting it after the heap defect has been isolated.
        LLVMSetInitializer(runtimeDebugTraceFlags_, LLVMConstInt(i32, 7U, 0));
        LLVMSetLinkage(runtimeDebugTraceFlags_, LLVMPrivateLinkage);

        LLVMValueRef path = internStringLiteral(
            "A:\\temp\\nsc-runtime-debug-trace.log", error);
        if (path == nullptr)
            return fail(error, "INSTRUMENTATION CODE could not materialize the runtime Debug trace path");

        LLVMTypeRef appendParameters[] = {ptr, ptr};
        fileAppendAllTextFunctionType_ = LLVMFunctionType(voidType, appendParameters, 2, 0);
        fileAppendAllTextFunction_ = LLVMGetNamedFunction(
            module_, kFileAppendAllTextFunctionName);
        if (fileAppendAllTextFunction_ == nullptr)
        {
            fileAppendAllTextFunction_ = LLVMAddFunction(
                module_, kFileAppendAllTextFunctionName, fileAppendAllTextFunctionType_);
        }
        else if (LLVMGlobalGetValueType(fileAppendAllTextFunction_) !=
                 fileAppendAllTextFunctionType_)
        {
            return fail(error, "INSTRUMENTATION CODE found an incompatible runtime append hook");
        }

        return buildRuntimeDebugTraceBody(i32, path, error);
    }

bool OopLlvmImplementation::buildRuntimeDebugTraceBody(
        LLVMTypeRef i32,
        LLVMValueRef path,
        std::string& error)
{
        LLVMBuilderRef traceBuilder = LLVMCreateBuilderInContext(context_);
        if (traceBuilder == nullptr)
            return fail(error, "INSTRUMENTATION CODE could not create the runtime Debug trace builder");

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            context_, runtimeDebugTraceFunction_, "entry");
        LLVMBasicBlockRef write = LLVMAppendBasicBlockInContext(
            context_, runtimeDebugTraceFunction_, "write");
        LLVMBasicBlockRef done = LLVMAppendBasicBlockInContext(
            context_, runtimeDebugTraceFunction_, "done");

        LLVMPositionBuilderAtEnd(traceBuilder, entry);
        LLVMValueRef flags = LLVMBuildLoad2(
            traceBuilder, i32, runtimeDebugTraceFlags_, "trace.flags");
        LLVMValueRef category = LLVMGetParam(runtimeDebugTraceFunction_, 0);
        LLVMValueRef selected = LLVMBuildAnd(traceBuilder, flags, category, "trace.selected");
        LLVMValueRef enabled = LLVMBuildICmp(
            traceBuilder, LLVMIntNE, selected, LLVMConstInt(i32, 0, 0), "trace.enabled");
        LLVMBuildCondBr(traceBuilder, enabled, write, done);

        LLVMPositionBuilderAtEnd(traceBuilder, write);
        LLVMValueRef appendArguments[] = {
            path, LLVMGetParam(runtimeDebugTraceFunction_, 1)};
        LLVMBuildCall2(
            traceBuilder, fileAppendAllTextFunctionType_, fileAppendAllTextFunction_,
            appendArguments, 2, "");
        LLVMBuildBr(traceBuilder, done);

        LLVMPositionBuilderAtEnd(traceBuilder, done);
        LLVMBuildRetVoid(traceBuilder);
        LLVMDisposeBuilder(traceBuilder);
        return true;
    }

bool OopLlvmImplementation::emitRuntimeDebugTrace(
        std::uint32_t category,
        std::string_view text,
        std::string& error)
{
        if (!kRuntimeDebugTraceEnabled || debugInformation_ != DebugInformationKind::Full) return true;
        if (!ensureRuntimeDebugTrace(error)) return false;
        LLVMValueRef message = internStringLiteral(text, error);
        if (message == nullptr)
            return fail(error, "INSTRUMENTATION CODE could not materialize a runtime Debug trace record");
        LLVMValueRef arguments[] = {
            LLVMConstInt(LLVMInt32TypeInContext(context_), category, 0),
            message,
            LLVMConstInt(LLVMInt32TypeInContext(context_), text.size(), 0)};
        return buildNoUnwindCall(
                   runtimeDebugTraceFunctionType_, runtimeDebugTraceFunction_, arguments, 3, "") != nullptr;
    }

std::string OopLlvmImplementation::runtimeDebugCallableRecord(
        const frontend::ProjectOopMirFunction& function,
        std::string_view event) const
{
        std::string kind = function.constructor
            ? (function.isStatic ? "static-constructor" : "constructor")
            : function.destructor
                ? "destructor"
                : function.isLambda
                    ? "lambda"
                    : function.namespaceFunctionOrdinal.has_value()
                        ? "namespace-function"
                        : function.isStatic ? "static-method" : "instance-method";
        std::string owner = "<namespace>";
        if (!function.namespaceFunctionOrdinal.has_value() &&
            function.declaringTypeOrdinal < nameLookup_.namedTypes().size())
        {
            owner = nameLookup_.namedTypes()[function.declaringTypeOrdinal]
                .identity.fullyQualifiedName();
        }
        const std::string& callable = function.debugDisplayName.empty()
            ? function.mangledName
            : function.debugDisplayName;
        return "[NSC RUNTIME DEBUG INSTRUMENTATION CODE] event=" + std::string(event) +
            "|kind=" + kind +
            "|class=" + owner + "|function=" + callable + "|symbol=" +
            function.mangledName + "\r\n";
    }

std::string OopLlvmImplementation::runtimeDebugFieldStoreRecord(
        const frontend::ProjectOopMirNode& store,
        std::string_view event) const
{
        if (store.children.empty() || store.children.front() == nullptr ||
            memberModel_ == nullptr || currentMirFunction_ == nullptr)
        {
            return {};
        }
        const frontend::ProjectOopMirNode& target = *store.children.front();
        if ((target.kind != frontend::ProjectOopMirNodeKind::FieldAddress &&
             target.kind != frontend::ProjectOopMirNodeKind::BitFieldAddress &&
             target.kind != frontend::ProjectOopMirNodeKind::StaticFieldAddress) ||
            !target.memberOrdinal.has_value() ||
            *target.memberOrdinal >= memberModel_->members().size())
        {
            return {};
        }

        const frontend::ClassMemberSymbol& field =
            memberModel_->members()[*target.memberOrdinal];
        std::string owner = "<unknown>";
        if (field.declaringTypeOrdinal < nameLookup_.namedTypes().size())
        {
            owner = nameLookup_.namedTypes()[field.declaringTypeOrdinal]
                .identity.fullyQualifiedName();
        }
        const char* storage = target.kind == frontend::ProjectOopMirNodeKind::StaticFieldAddress
            ? "static"
            : target.kind == frontend::ProjectOopMirNodeKind::BitFieldAddress
                ? "bit-field"
                : "instance";
        const std::string& callable = currentMirFunction_->debugDisplayName.empty()
            ? currentMirFunction_->mangledName
            : currentMirFunction_->debugDisplayName;
        return "[NSC RUNTIME DEBUG INSTRUMENTATION CODE] event=" + std::string(event) +
            "|storage=" + storage + "|class=" + owner + "|field=" + field.name +
            "|member-ordinal=" + std::to_string(*target.memberOrdinal) +
            "|offset=" + std::to_string(target.immediate) + "|type=" + store.typeKey +
            "|function=" + callable + "|symbol=" + currentMirFunction_->mangledName + "\r\n";
    }

std::string OopLlvmImplementation::runtimeDebugFieldLoadRecord(
        const frontend::ProjectOopMirNode& field,
        std::string_view event) const
{
        if (memberModel_ == nullptr || currentMirFunction_ == nullptr ||
            (field.kind != frontend::ProjectOopMirNodeKind::FieldLoad &&
             field.kind != frontend::ProjectOopMirNodeKind::BitFieldLoad) ||
            !field.memberOrdinal.has_value() ||
            *field.memberOrdinal >= memberModel_->members().size())
        {
            return {};
        }
        const frontend::ClassMemberSymbol& member =
            memberModel_->members()[*field.memberOrdinal];
        std::string owner = "<unknown>";
        if (member.declaringTypeOrdinal < nameLookup_.namedTypes().size())
        {
            owner = nameLookup_.namedTypes()[member.declaringTypeOrdinal]
                .identity.fullyQualifiedName();
        }
        const std::string& callable = currentMirFunction_->debugDisplayName.empty()
            ? currentMirFunction_->mangledName
            : currentMirFunction_->debugDisplayName;
        return "[NSC RUNTIME DEBUG INSTRUMENTATION CODE] event=" + std::string(event) +
            "|storage=instance|class=" + owner + "|field=" + member.name +
            "|member-ordinal=" + std::to_string(*field.memberOrdinal) +
            "|offset=" + std::to_string(field.immediate) + "|type=" + field.typeKey +
            "|function=" + callable + "|symbol=" + currentMirFunction_->mangledName + "\r\n";
    }

std::string OopLlvmImplementation::runtimeDebugArrayRecord(
        const frontend::ProjectOopMirNode& node,
        std::string_view event) const
{
        if (currentMirFunction_ == nullptr) return {};
        const std::string& callable = currentMirFunction_->debugDisplayName.empty()
            ? currentMirFunction_->mangledName
            : currentMirFunction_->debugDisplayName;
        const std::string elementType = node.elementTypeKey.empty()
            ? node.typeKey
            : node.elementTypeKey;
        return "[NSC RUNTIME DEBUG INSTRUMENTATION CODE] event=" + std::string(event) +
            "|element-type=" + elementType + "|element-size=" +
            std::to_string(node.immediate) + "|alignment=" + std::to_string(node.alignment) +
            "|function=" + callable + "|symbol=" + currentMirFunction_->mangledName + "\r\n";
    }

}
