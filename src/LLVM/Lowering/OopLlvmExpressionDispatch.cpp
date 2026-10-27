#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

#define NSC_LOWER_EXPRESSION_GROUP_01 \
    case frontend::ProjectOopMirNodeKind::ThisValue: \
    case frontend::ProjectOopMirNodeKind::ParameterValue: \
    case frontend::ProjectOopMirNodeKind::LocalValue: \
    case frontend::ProjectOopMirNodeKind::IntegerConstant: \
    case frontend::ProjectOopMirNodeKind::FloatingConstant: \
    case frontend::ProjectOopMirNodeKind::BooleanConstant: \
    case frontend::ProjectOopMirNodeKind::NullConstant: \
    case frontend::ProjectOopMirNodeKind::DefaultValue: \
    case frontend::ProjectOopMirNodeKind::StringLiteral: \
    case frontend::ProjectOopMirNodeKind::Utf8StringLiteral: \
    case frontend::ProjectOopMirNodeKind::EnumName: \
    case frontend::ProjectOopMirNodeKind::ParameterAddress: \
    case frontend::ProjectOopMirNodeKind::LocalAddress: \
    case frontend::ProjectOopMirNodeKind::FieldAddress: \
    case frontend::ProjectOopMirNodeKind::BitFieldAddress: \
    case frontend::ProjectOopMirNodeKind::StaticFieldAddress: \
    case frontend::ProjectOopMirNodeKind::NamespaceStateAddress: \
    case frontend::ProjectOopMirNodeKind::FutureFieldAddress: \
    case frontend::ProjectOopMirNodeKind::FieldLoad: \
    case frontend::ProjectOopMirNodeKind::BitFieldLoad: \
    case frontend::ProjectOopMirNodeKind::StaticFieldLoad: \
    case frontend::ProjectOopMirNodeKind::NamespaceStateLoad: \
    case frontend::ProjectOopMirNodeKind::ArrayElementLoad: \
    case frontend::ProjectOopMirNodeKind::RangeArrayElementLoad: \
    case frontend::ProjectOopMirNodeKind::StringElementLoad: \
    case frontend::ProjectOopMirNodeKind::Throw: \
    case frontend::ProjectOopMirNodeKind::Unreachable: \
    case frontend::ProjectOopMirNodeKind::Panic: \
    case frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow: \
    case frontend::ProjectOopMirNodeKind::FullExpressionCleanup: \
    case frontend::ProjectOopMirNodeKind::Conditional: \
    case frontend::ProjectOopMirNodeKind::LoopExpression: \
    case frontend::ProjectOopMirNodeKind::MatchExpression: \
    case frontend::ProjectOopMirNodeKind::MatchValue: \
    case frontend::ProjectOopMirNodeKind::OverflowPolicy: \
    case frontend::ProjectOopMirNodeKind::NullCoalescing: \
    case frontend::ProjectOopMirNodeKind::OptionalCoalescing: \
    case frontend::ProjectOopMirNodeKind::NullCoalescingStore: \
    case frontend::ProjectOopMirNodeKind::BorrowValue: \
    case frontend::ProjectOopMirNodeKind::IteratorCurrent: \
    case frontend::ProjectOopMirNodeKind::IteratorMoveNext: \
    case frontend::ProjectOopMirNodeKind::IteratorGetEnumerator: \
    case frontend::ProjectOopMirNodeKind::IteratorDrop: \
    case frontend::ProjectOopMirNodeKind::IteratorMoveNextAsync: \
    case frontend::ProjectOopMirNodeKind::IteratorCancel:

#define NSC_LOWER_EXPRESSION_GROUP_02 \
    case frontend::ProjectOopMirNodeKind::MoveValue: \
    case frontend::ProjectOopMirNodeKind::MoveArrayElement:

#define NSC_LOWER_EXPRESSION_GROUP_03 \
    case frontend::ProjectOopMirNodeKind::AddressValue: \
    case frontend::ProjectOopMirNodeKind::RawElementAddress: \
    case frontend::ProjectOopMirNodeKind::StackAllocation: \
    case frontend::ProjectOopMirNodeKind::RawAllocation: \
    case frontend::ProjectOopMirNodeKind::PointerArithmetic: \
    case frontend::ProjectOopMirNodeKind::CompoundStore: \
    case frontend::ProjectOopMirNodeKind::PointerLoad: \
    case frontend::ProjectOopMirNodeKind::MoveOwnedField: \
    case frontend::ProjectOopMirNodeKind::MoveIteratorCurrent: \
    case frontend::ProjectOopMirNodeKind::NullConditionalFieldLoad: \
    case frontend::ProjectOopMirNodeKind::NullConditionalFieldStore: \
    case frontend::ProjectOopMirNodeKind::NullConditionalArrayLoad: \
    case frontend::ProjectOopMirNodeKind::NullConditionalArrayStore: \
    case frontend::ProjectOopMirNodeKind::AwaitFuture: \
    case frontend::ProjectOopMirNodeKind::PollAddress:

#define NSC_LOWER_EXPRESSION_GROUP_04 \
    case frontend::ProjectOopMirNodeKind::ArrayAllocate: \
    case frontend::ProjectOopMirNodeKind::RangeArrayAllocate: \
    case frontend::ProjectOopMirNodeKind::ArrayLength: \
    case frontend::ProjectOopMirNodeKind::ArrayElementAddress: \
    case frontend::ProjectOopMirNodeKind::RangeArrayElementAddress: \
    case frontend::ProjectOopMirNodeKind::StringLength: \
    case frontend::ProjectOopMirNodeKind::StringConcat: \
    case frontend::ProjectOopMirNodeKind::StringEquals: \
    case frontend::ProjectOopMirNodeKind::StringCharacterPointer: \
    case frontend::ProjectOopMirNodeKind::StringViewFromString: \
    case frontend::ProjectOopMirNodeKind::SpanFromArray: \
    case frontend::ProjectOopMirNodeKind::ReadOnlySpanFromArray: \
    case frontend::ProjectOopMirNodeKind::SpanFromFixedArray: \
    case frontend::ProjectOopMirNodeKind::ReadOnlySpanFromFixedArray: \
    case frontend::ProjectOopMirNodeKind::StringSubstring: \
    case frontend::ProjectOopMirNodeKind::StringFromCharArray: \
    case frontend::ProjectOopMirNodeKind::CoreConsoleWriteLine: \
    case frontend::ProjectOopMirNodeKind::CoreConsoleReadLine: \
    case frontend::ProjectOopMirNodeKind::CoreClockQueryCounter:

#define NSC_LOWER_EXPRESSION_GROUP_05 \
    case frontend::ProjectOopMirNodeKind::AggregateEquals: \
    case frontend::ProjectOopMirNodeKind::AggregateHash: \
    case frontend::ProjectOopMirNodeKind::CoreObjectTypeName: \
    case frontend::ProjectOopMirNodeKind::CoreObjectReferenceHash: \
    case frontend::ProjectOopMirNodeKind::CoreClockQueryFrequency: \
    case frontend::ProjectOopMirNodeKind::CoreClockQuerySystemFileTime: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentTickCount64: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentProcessorCount: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentExit: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetVariable: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentSetVariable:

#define NSC_LOWER_EXPRESSION_GROUP_06 \
    case frontend::ProjectOopMirNodeKind::CoreFileExists: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryExists: \
    case frontend::ProjectOopMirNodeKind::CoreFileDelete: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryCreate: \
    case frontend::ProjectOopMirNodeKind::CoreFileReadAllText: \
    case frontend::ProjectOopMirNodeKind::CoreFileGetSize: \
    case frontend::ProjectOopMirNodeKind::CoreFileReadAllBytes: \
    case frontend::ProjectOopMirNodeKind::CoreFileReadBytes: \
    case frontend::ProjectOopMirNodeKind::CoreFileWriteAllBytes: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentCurrentDirectory: \
    case frontend::ProjectOopMirNodeKind::CoreProcessRun: \
    case frontend::ProjectOopMirNodeKind::CoreFileWriteAllText: \
    case frontend::ProjectOopMirNodeKind::CoreFileAppendAllText: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryDelete: \
    case frontend::ProjectOopMirNodeKind::CorePathGetFullPath: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryGetFileCount: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount: \
    case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryGetFiles: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectories:

#define NSC_LOWER_EXPRESSION_GROUP_07 \
    case frontend::ProjectOopMirNodeKind::CoreThreadCreate: \
    case frontend::ProjectOopMirNodeKind::CoreThreadJoin: \
    case frontend::ProjectOopMirNodeKind::CoreThreadDetach: \
    case frontend::ProjectOopMirNodeKind::CoreAtomicThreadFence: \
    case frontend::ProjectOopMirNodeKind::CoreAtomicCompilerFence:

#define NSC_LOWER_EXPRESSION_GROUP_08 \
    case frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt32: \
    case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Add: \
    case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Subtract: \
    case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Multiply: \
    case frontend::ProjectOopMirNodeKind::CoreHalfFromFloat: \
    case frontend::ProjectOopMirNodeKind::CoreHalfToFloat: \
    case frontend::ProjectOopMirNodeKind::CoreSimdSwizzleWzyx: \
    case frontend::ProjectOopMirNodeKind::CoreSimdShuffleLowHigh: \
    case frontend::ProjectOopMirNodeKind::CoreSimdSqrt: \
    case frontend::ProjectOopMirNodeKind::CoreSimdReciprocalSqrt: \
    case frontend::ProjectOopMirNodeKind::CoreSimdFusedMultiplyAdd: \
    case frontend::ProjectOopMirNodeKind::CoreCpuPrefetchRead: \
    case frontend::ProjectOopMirNodeKind::CoreCpuPause: \
    case frontend::ProjectOopMirNodeKind::CoreCpuReadTimestamp: \
    case frontend::ProjectOopMirNodeKind::CoreBitRotateLeft: \
    case frontend::ProjectOopMirNodeKind::CoreBitRotateRight: \
    case frontend::ProjectOopMirNodeKind::CoreBitByteSwap: \
    case frontend::ProjectOopMirNodeKind::CoreBitPopulationCount: \
    case frontend::ProjectOopMirNodeKind::CoreBitLeadingZeroCount: \
    case frontend::ProjectOopMirNodeKind::CoreBitTrailingZeroCount: \
    case frontend::ProjectOopMirNodeKind::CoreSimdInt128Add: \
    case frontend::ProjectOopMirNodeKind::CoreSimdInt128Subtract: \
    case frontend::ProjectOopMirNodeKind::CoreSimdInt128Multiply: \
    case frontend::ProjectOopMirNodeKind::CoreSimdFloat256Add: \
    case frontend::ProjectOopMirNodeKind::CoreSimdFloat256Subtract: \
    case frontend::ProjectOopMirNodeKind::CoreSimdFloat256Multiply: \
    case frontend::ProjectOopMirNodeKind::CoreSimdInt256Add: \
    case frontend::ProjectOopMirNodeKind::CoreSimdInt256Subtract: \
    case frontend::ProjectOopMirNodeKind::CoreSimdInt256Multiply: \
    case frontend::ProjectOopMirNodeKind::CoreSimdLoadAligned128: \
    case frontend::ProjectOopMirNodeKind::CoreSimdStoreAligned128: \
    case frontend::ProjectOopMirNodeKind::CoreSimdLoadUnaligned128: \
    case frontend::ProjectOopMirNodeKind::CoreSimdStoreUnaligned128:

#define NSC_LOWER_EXPRESSION_GROUP_09 \
    case frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt32: \
    case frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt64: \
    case frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt64: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt32: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt32: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt64: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt64: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt32: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt32: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt64: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt64: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64: \
    case frontend::ProjectOopMirNodeKind::CoreAtomicLoadPointer: \
    case frontend::ProjectOopMirNodeKind::CoreAtomicStorePointer: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangePointer: \
    case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer:

#define NSC_LOWER_EXPRESSION_GROUP_10 \
    case frontend::ProjectOopMirNodeKind::CoreMutexCreate: \
    case frontend::ProjectOopMirNodeKind::CoreMutexCreateNamed: \
    case frontend::ProjectOopMirNodeKind::CoreMutexLock: \
    case frontend::ProjectOopMirNodeKind::CoreMutexUnlock: \
    case frontend::ProjectOopMirNodeKind::CoreMutexClose: \
    case frontend::ProjectOopMirNodeKind::CoreMonitorEnter: \
    case frontend::ProjectOopMirNodeKind::CoreMonitorExit: \
    case frontend::ProjectOopMirNodeKind::CoreMonitorPulse: \
    case frontend::ProjectOopMirNodeKind::CoreMonitorPulseAll: \
    case frontend::ProjectOopMirNodeKind::CoreMonitorWait: \
    case frontend::ProjectOopMirNodeKind::CoreSemaphoreCreate: \
    case frontend::ProjectOopMirNodeKind::CoreSemaphoreCreateNamed: \
    case frontend::ProjectOopMirNodeKind::CoreSemaphoreWait: \
    case frontend::ProjectOopMirNodeKind::CoreEventWait: \
    case frontend::ProjectOopMirNodeKind::CoreSemaphoreRelease: \
    case frontend::ProjectOopMirNodeKind::CoreSemaphoreClose: \
    case frontend::ProjectOopMirNodeKind::CoreEventSet: \
    case frontend::ProjectOopMirNodeKind::CoreEventReset: \
    case frontend::ProjectOopMirNodeKind::CoreEventClose: \
    case frontend::ProjectOopMirNodeKind::CoreEventCreate: \
    case frontend::ProjectOopMirNodeKind::CoreEventCreateNamed:

#define NSC_LOWER_EXPRESSION_GROUP_11 \
    case frontend::ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId: \
    case frontend::ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize: \
    case frontend::ProjectOopMirNodeKind::CoreProcessGetKernelTime: \
    case frontend::ProjectOopMirNodeKind::CoreProcessGetUserTime: \
    case frontend::ProjectOopMirNodeKind::CoreProcessGetPriorityClass: \
    case frontend::ProjectOopMirNodeKind::CoreProcessSetPriorityClass:

#define NSC_LOWER_EXPRESSION_GROUP_12 \
    case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetPageSize: \
    case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture: \
    case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity: \
    case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes: \
    case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes: \
    case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent: \
    case frontend::ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent: \
    case frontend::ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount: \
    case frontend::ProjectOopMirNodeKind::CoreDebugOutputString: \
    case frontend::ProjectOopMirNodeKind::CoreThreadSleep: \
    case frontend::ProjectOopMirNodeKind::CoreFileCopy: \
    case frontend::ProjectOopMirNodeKind::CoreFileMove: \
    case frontend::ProjectOopMirNodeKind::CoreDirectoryMove:

#define NSC_LOWER_EXPRESSION_GROUP_13 \
    case frontend::ProjectOopMirNodeKind::LambdaCreate: \
    case frontend::ProjectOopMirNodeKind::ClosureCaptureLoad: \
    case frontend::ProjectOopMirNodeKind::ClosureCaptureAddress: \
    case frontend::ProjectOopMirNodeKind::CaptureCellReference: \
    case frontend::ProjectOopMirNodeKind::ClosureCaptureCellReference: \
    case frontend::ProjectOopMirNodeKind::DelegateCreate: \
    case frontend::ProjectOopMirNodeKind::DelegateInvoke: \
    case frontend::ProjectOopMirNodeKind::EventAdd: \
    case frontend::ProjectOopMirNodeKind::EventRemove: \
    case frontend::ProjectOopMirNodeKind::EventInvoke:

#define NSC_LOWER_EXPRESSION_GROUP_14 \
    case frontend::ProjectOopMirNodeKind::Unary: \
    case frontend::ProjectOopMirNodeKind::Binary: \
    case frontend::ProjectOopMirNodeKind::TupleDeconstruct: \
    case frontend::ProjectOopMirNodeKind::TupleDeconstructElementStore: \
    case frontend::ProjectOopMirNodeKind::Store: \
    case frontend::ProjectOopMirNodeKind::Convert: \
    case frontend::ProjectOopMirNodeKind::RuntimeTypeTest: \
    case frontend::ProjectOopMirNodeKind::SafeReferenceCast: \
    case frontend::ProjectOopMirNodeKind::CheckedReferenceCast: \
    case frontend::ProjectOopMirNodeKind::RangeSlice: \
    case frontend::ProjectOopMirNodeKind::DirectInstanceCall: \
    case frontend::ProjectOopMirNodeKind::DirectStaticCall: \
    case frontend::ProjectOopMirNodeKind::VirtualInstanceCall: \
    case frontend::ProjectOopMirNodeKind::InterfaceInstanceCall: \
    case frontend::ProjectOopMirNodeKind::TemporaryAddress: \
    case frontend::ProjectOopMirNodeKind::ValueConstruct: \
    case frontend::ProjectOopMirNodeKind::PlacementConstruct: \
    case frontend::ProjectOopMirNodeKind::ParameterPack: \
    case frontend::ProjectOopMirNodeKind::AggregateInitialize: \
    case frontend::ProjectOopMirNodeKind::AggregateSpreadStore: \
    case frontend::ProjectOopMirNodeKind::AggregateElementStore: \
    case frontend::ProjectOopMirNodeKind::AllocateAndConstruct: \
    case frontend::ProjectOopMirNodeKind::Deallocate: \
    case frontend::ProjectOopMirNodeKind::ObjectDestroy: \
    case frontend::ProjectOopMirNodeKind::Assert: \
    case frontend::ProjectOopMirNodeKind::Assume: \
    case frontend::ProjectOopMirNodeKind::ResultPropagation: \
    case frontend::ProjectOopMirNodeKind::BlockExpression:

#define NSC_LOWER_EXPRESSION_GROUP_15 \
    case frontend::ProjectOopMirNodeKind::Block: \
    case frontend::ProjectOopMirNodeKind::LocalDeclaration: \
    case frontend::ProjectOopMirNodeKind::ExpressionStatement: \
    case frontend::ProjectOopMirNodeKind::If: \
    case frontend::ProjectOopMirNodeKind::While: \
    case frontend::ProjectOopMirNodeKind::DoWhile: \
    case frontend::ProjectOopMirNodeKind::For: \
    case frontend::ProjectOopMirNodeKind::Switch: \
    case frontend::ProjectOopMirNodeKind::SwitchSection: \
    case frontend::ProjectOopMirNodeKind::Break: \
    case frontend::ProjectOopMirNodeKind::Continue: \
    case frontend::ProjectOopMirNodeKind::Return: \
    case frontend::ProjectOopMirNodeKind::TryRegion: \
    case frontend::ProjectOopMirNodeKind::CatchRegion: \
    case frontend::ProjectOopMirNodeKind::FinallyRegion: \
    case frontend::ProjectOopMirNodeKind::UsingRegion: \
    case frontend::ProjectOopMirNodeKind::Rethrow: \
    case frontend::ProjectOopMirNodeKind::CleanupContinuation: \
    case frontend::ProjectOopMirNodeKind::OwnedParameterCleanup: \
    case frontend::ProjectOopMirNodeKind::InlineAssembly: \
    case frontend::ProjectOopMirNodeKind::InlineAssemblyLine: \
    case frontend::ProjectOopMirNodeKind::InlineAssemblyInput: \
    case frontend::ProjectOopMirNodeKind::InlineAssemblyOutput: \
    case frontend::ProjectOopMirNodeKind::InlineAssemblyClobber: \
    case frontend::ProjectOopMirNodeKind::MatchArm: \
    case frontend::ProjectOopMirNodeKind::SuspendPoint: \
    case frontend::ProjectOopMirNodeKind::YieldValue: \
    case frontend::ProjectOopMirNodeKind::YieldBreak: \
    case frontend::ProjectOopMirNodeKind::GeneratorFrameInit: \
    case frontend::ProjectOopMirNodeKind::DropFrame: \
    case frontend::ProjectOopMirNodeKind::AsyncYieldValue:

LLVMValueRef OopLlvmImplementation::lowerExpression(const frontend::ProjectOopMirNode* node, std::string& error)
{
    if (node == nullptr) return nullptr;
    // EN: Freeze 3.0 D-01: an operand with a replay slot is evaluated once per statement pass.
    // ES: Congelamiento 3.0 D-01: un operando con slot de repeticion se evalua una vez por pasada.
    if (OopLlvmReplaySpill spill; beginCoroutineReplaySpill(node, spill, error))
        return finishCoroutineReplaySpill(spill, lowerExpression(node, error), error);
    FloatingPointModeScope sourceModeScope;
    if (node->syntax != nullptr && node->syntax->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
    {
        sourceModeScope.setState(sourceFloatingPointState(
            node->syntax->floatingPointMode(), captureFloatingPointState()));
    }
    ScopedDebugLocation scopedDebugLocation(builder_, debugLocation(node));
    switch (node->kind)
    {
        NSC_LOWER_EXPRESSION_GROUP_01
            return lowerExpressionGroup01(node, error);
        NSC_LOWER_EXPRESSION_GROUP_02
            return lowerExpressionGroup02(node, error);
        NSC_LOWER_EXPRESSION_GROUP_03
            return lowerExpressionGroup03(node, error);
        NSC_LOWER_EXPRESSION_GROUP_04
            return lowerExpressionGroup04(node, error);
        NSC_LOWER_EXPRESSION_GROUP_05
            return lowerExpressionGroup05(node, error);
        NSC_LOWER_EXPRESSION_GROUP_06
            return lowerExpressionGroup06(node, error);
        NSC_LOWER_EXPRESSION_GROUP_07
            return lowerExpressionGroup07(node, error);
        NSC_LOWER_EXPRESSION_GROUP_08
            return lowerExpressionGroup08(node, error);
        NSC_LOWER_EXPRESSION_GROUP_09
            return lowerExpressionGroup09(node, error);
        NSC_LOWER_EXPRESSION_GROUP_10
            return lowerExpressionGroup10(node, error);
        NSC_LOWER_EXPRESSION_GROUP_11
            return lowerExpressionGroup11(node, error);
        NSC_LOWER_EXPRESSION_GROUP_12
            return lowerExpressionGroup12(node, error);
        NSC_LOWER_EXPRESSION_GROUP_13
            return lowerExpressionGroup13(node, error);
        NSC_LOWER_EXPRESSION_GROUP_14
            return lowerExpressionGroup14(node, error);
        NSC_LOWER_EXPRESSION_GROUP_15
            return lowerExpressionGroup15(node, error);
    }
}

#undef NSC_LOWER_EXPRESSION_GROUP_01
#undef NSC_LOWER_EXPRESSION_GROUP_02
#undef NSC_LOWER_EXPRESSION_GROUP_03
#undef NSC_LOWER_EXPRESSION_GROUP_04
#undef NSC_LOWER_EXPRESSION_GROUP_05
#undef NSC_LOWER_EXPRESSION_GROUP_06
#undef NSC_LOWER_EXPRESSION_GROUP_07
#undef NSC_LOWER_EXPRESSION_GROUP_08
#undef NSC_LOWER_EXPRESSION_GROUP_09
#undef NSC_LOWER_EXPRESSION_GROUP_10
#undef NSC_LOWER_EXPRESSION_GROUP_11
#undef NSC_LOWER_EXPRESSION_GROUP_12
#undef NSC_LOWER_EXPRESSION_GROUP_13
#undef NSC_LOWER_EXPRESSION_GROUP_14
#undef NSC_LOWER_EXPRESSION_GROUP_15

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup01(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;
            // NSC305-L04: the leaf family (literals/names) lives in lowerLeafExpression.
            case frontend::ProjectOopMirNodeKind::ThisValue:
            case frontend::ProjectOopMirNodeKind::ParameterValue:
            case frontend::ProjectOopMirNodeKind::LocalValue:
            case frontend::ProjectOopMirNodeKind::IntegerConstant:
            case frontend::ProjectOopMirNodeKind::FloatingConstant:
            case frontend::ProjectOopMirNodeKind::BooleanConstant:
            case frontend::ProjectOopMirNodeKind::NullConstant:
            case frontend::ProjectOopMirNodeKind::DefaultValue:
            case frontend::ProjectOopMirNodeKind::StringLiteral:
            case frontend::ProjectOopMirNodeKind::Utf8StringLiteral:
            case frontend::ProjectOopMirNodeKind::EnumName:
                return lowerLeafExpression(node, error);
            // NSC305-L04: the address sub-family (member/index) lives in lowerAddressExpression.
            case frontend::ProjectOopMirNodeKind::ParameterAddress:
            case frontend::ProjectOopMirNodeKind::LocalAddress:
            case frontend::ProjectOopMirNodeKind::FieldAddress:
            case frontend::ProjectOopMirNodeKind::BitFieldAddress:
            case frontend::ProjectOopMirNodeKind::StaticFieldAddress:
            case frontend::ProjectOopMirNodeKind::NamespaceStateAddress:
            case frontend::ProjectOopMirNodeKind::FutureFieldAddress:
                return lowerAddressExpression(node, error);
            // NSC305-L04: the load sub-family (member/index) lives in lowerLoadExpression.
            case frontend::ProjectOopMirNodeKind::FieldLoad:
            case frontend::ProjectOopMirNodeKind::BitFieldLoad:
            case frontend::ProjectOopMirNodeKind::StaticFieldLoad:
            case frontend::ProjectOopMirNodeKind::NamespaceStateLoad:
            case frontend::ProjectOopMirNodeKind::ArrayElementLoad:
            case frontend::ProjectOopMirNodeKind::RangeArrayElementLoad:
            case frontend::ProjectOopMirNodeKind::StringElementLoad:
                return lowerLoadExpression(node, error);
            // NSC305-L04: the exception sub-family (family 6/6) lives in lowerExceptionExpression.
            case frontend::ProjectOopMirNodeKind::Throw:
            case frontend::ProjectOopMirNodeKind::Unreachable:
            case frontend::ProjectOopMirNodeKind::Panic:
                return lowerExceptionExpression(node, error);
            // NSC305-L04: the cleanup sub-family (family 6/6) lives in lowerCleanupExpression.
            case frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow:
            case frontend::ProjectOopMirNodeKind::FullExpressionCleanup:
            case frontend::ProjectOopMirNodeKind::Conditional:
            case frontend::ProjectOopMirNodeKind::LoopExpression:
            case frontend::ProjectOopMirNodeKind::MatchExpression:
            case frontend::ProjectOopMirNodeKind::MatchValue:
            case frontend::ProjectOopMirNodeKind::OverflowPolicy:
            case frontend::ProjectOopMirNodeKind::NullCoalescing:
            case frontend::ProjectOopMirNodeKind::OptionalCoalescing:
            case frontend::ProjectOopMirNodeKind::NullCoalescingStore:
            case frontend::ProjectOopMirNodeKind::BorrowValue:
                return lowerCleanupExpression(node, error);
            // NSC305-L04: the iterator sub-family (family 6/6) lives in lowerIteratorExpression.
            case frontend::ProjectOopMirNodeKind::IteratorCurrent:
            case frontend::ProjectOopMirNodeKind::IteratorMoveNext:
            case frontend::ProjectOopMirNodeKind::IteratorGetEnumerator:
            case frontend::ProjectOopMirNodeKind::IteratorDrop:
            case frontend::ProjectOopMirNodeKind::IteratorMoveNextAsync:
            case frontend::ProjectOopMirNodeKind::IteratorCancel:
                return lowerIteratorExpression(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup02(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;
            case frontend::ProjectOopMirNodeKind::MoveValue:
            {
                if (node->localOrdinal.has_value() == node->parameterOrdinal.has_value())
                    return failValue(error, "Stage 50.8 move value must identify exactly one owner storage location");
                LLVMValueRef storage = nullptr;
                const char* loadName = nullptr;
                if (node->localOrdinal.has_value())
                {
                    const auto found = localStorage_.find(*node->localOrdinal);
                    if (found == localStorage_.end() || found->second == nullptr)
                        return failValue(error, "Stage 50.8 move value cannot find source local storage");
                    storage = found->second;
                    loadName = "move.load";
                }
                else
                {
                    const auto found = parameterStorage_.find(*node->parameterOrdinal);
                    if (found == parameterStorage_.end() || found->second == nullptr)
                        return failValue(error, "Stage 50.8 move value cannot find source parameter storage");
                    storage = found->second;
                    loadName = "move.param.load";
                }
                // Stage 52 fix: this unconditionally treated the storage slot as a pointer,
                // which was correct for every move() source Stage 49/50 ever produced (always a
                // reference-typed RAII owner) but breaks once a generic T can monomorphize a
                // move()'d local/parameter to a plain value type (int, bool, a struct, ...) --
                // e.g. LinkedList<int>.AddFirst's "move(value)" on an owned int parameter, which
                // loaded the i32 storage slot AS a pointer and failed LLVM verification with a
                // call-parameter type mismatch. Load/store using the value's REAL type instead,
                // and only null out the slot for an actually reference-like source -- nulling an
                // int/bool/struct slot would be harmless in itself, but is also pointless (there
                // is no ownership to protect) and would incorrectly zero a value that legitimate
                // remaining reads of a value-typed "moved-from" owner may still observe.
                LLVMTypeRef valueType = llvmType(node->typeKey, error);
                if (valueType == nullptr) return nullptr;
                LLVMValueRef value = LLVMBuildLoad2(builder_, valueType, storage, loadName);
                if (node->booleanValue)
                {
                    LLVMBuildStore(builder_, LLVMConstNull(valueType), storage);
                    // EN: Freeze 3.0 DEC-10: `move(e)` of a caught exception transfers it, so the
                    //     catch that owns it gives it up and does not release it at its end. Only
                    //     the caught object itself matches an owner slot.
                    // ES: Congelamiento 3.0 DEC-10: `move(e)` de una excepcion atrapada la
                    //     transfiere, asi el catch que la posee la suelta y no la libera al final.
                    //     Solo el mismo objeto atrapado coincide con un slot de dueno.
                    if (valueType == LLVMPointerTypeInContext(context_, 0))
                    {
                        const std::vector<ActiveCatchBody> frames = activeCatchExceptions_;
                        for (const ActiveCatchBody& frame : frames)
                            if (frame.ownerSlot != nullptr) emitCatchOwnershipHandover(value, frame);
                    }
                }
                if (node->localOrdinal.has_value())
                {
                    const auto flag = localValueDropFlags_.find(*node->localOrdinal);
                    if (flag != localValueDropFlags_.end())
                        LLVMBuildStore(
                            builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0),
                            flag->second);
                }
                else if (node->parameterOrdinal.has_value())
                {
                    const auto flag = parameterValueDropFlags_.find(*node->parameterOrdinal);
                    if (flag != parameterValueDropFlags_.end())
                        LLVMBuildStore(
                            builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0),
                            flag->second);
                }
                return value;
            }
            case frontend::ProjectOopMirNodeKind::MoveArrayElement:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup03(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;
            case frontend::ProjectOopMirNodeKind::AddressValue:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::RawElementAddress:
                return lowerExpressionIterative(node, error);
            // CC1 fase 4B -- `stackalloc T[n]`.
            //
            // Se reserva en BYTES (`alloca i8, n * elementSize`) y no como `alloca <T>, n`, por la
            // misma razon por la que RawElementAddress calcula sus direcciones con un GEP de i8:
            // una sola regla de tamano para todo el nivel crudo. El tamano viene del MIR, que lo
            // saco de `elementLayout` -- el mismo del que sale el stride de `p[i]`.
            //
            // El alloca se emite DONDE ESTA la expresion, no en el bloque de entrada. Es lo que
            // `stackalloc` significa: si esta dentro de un bucle, cada vuelta reserva. LLVM no
            // promociona un alloca fuera del bloque de entrada, que es justo el comportamiento
            // correcto aqui y no una limitacion.
            case frontend::ProjectOopMirNodeKind::StackAllocation:
                return lowerExpressionIterative(node, error);

            // CC1 fase 4C -- `new T` escalar. Un bloque del monton del tamano de UN elemento, por
            // el mismo allocador que ya usa todo lo demas, y sin construccion: el nivel crudo no
            // inicializa. Se libera con `delete`, que ya existia y no cambia.
            case frontend::ProjectOopMirNodeKind::RawAllocation:
            {
                if (node->immediate == 0)
                    return failValue(error, "CC1 raw allocation requires an element size");
                LLVMValueRef size = LLVMConstInt(
                    LLVMInt64TypeInContext(context_), node->immediate, 0);
                LLVMValueRef arguments[] = {size};
                LLVMValueRef block = buildNoUnwindCall(
                    allocationFunctionType_, allocationFunction_, arguments, 1, "raw.new");
                if (block == nullptr)
                    return failValue(error, "CC1 raw allocation failed to emit its allocator call");
                return block;
            }

            // CC1 fase 4D -- `p + i`, `i + p`, `p - i` y `p - q`.
            //
            // El desplazamiento es EL MISMO GEP de bytes que RawElementAddress, y eso no es una
            // coincidencia que convenga: `p + i` y `&p[i]` son la misma direccion, y la fixture
            // c12 comprueba que dan el mismo valor. Si algun dia dejan de compartir la forma,
            // dejan de ser lo mismo.
            case frontend::ProjectOopMirNodeKind::PointerArithmetic:
                return lowerExpressionIterative(node, error);

            // IDs 79 y 81 -- `x op= e`, `++x`, `x++`. children[0] es la DIRECCION del destino,
            // bajada UNA sola vez; children[1] el operando derecho. Carga, opera, guarda.
            //
            // NADA DE ARITMETICA PROPIA: la operacion entera se delega en applyBinaryOperation,
            // el mismo codigo que emite un `Binary` corriente. Por eso `x /= y` con `uint` divide
            // sin signo y `x >>= n` con `int` desplaza aritmeticamente sin que este caso tenga que
            // saber nada de eso.
            case frontend::ProjectOopMirNodeKind::CompoundStore:
                return lowerExpressionIterative(node, error);

            case frontend::ProjectOopMirNodeKind::PointerLoad:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::MoveOwnedField:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::MoveIteratorCurrent:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::NullConditionalFieldLoad:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::NullConditionalFieldStore:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::NullConditionalArrayLoad:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::NullConditionalArrayStore:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::AwaitFuture:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::PollAddress:
            {
                // NS304-T05-F09: the poll function's address, stored into the future's pollFn.
                const auto found = functions_.find(node->stringValue);
                if (found == functions_.end() || found->second.function == nullptr)
                    return failValue(error, "async poll target was not declared: " + node->stringValue);
                return found->second.function;
            }
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup04(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;
            case frontend::ProjectOopMirNodeKind::ArrayAllocate:
                return lowerArrayAllocate(node, error);
            case frontend::ProjectOopMirNodeKind::RangeArrayAllocate:
                return lowerRangeArrayAllocate(node, error);
            case frontend::ProjectOopMirNodeKind::ArrayLength:
                return lowerNativeLength(node, false, error);
            case frontend::ProjectOopMirNodeKind::ArrayElementAddress:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::RangeArrayElementAddress:
                return lowerRangeArrayElementAddress(node, error);
            case frontend::ProjectOopMirNodeKind::StringLength:
                return lowerNativeLength(node, true, error);
            case frontend::ProjectOopMirNodeKind::StringConcat:
                return lowerStringConcat(node, error);
            case frontend::ProjectOopMirNodeKind::StringEquals:
                return lowerStringEquals(node, error);
            // SHR-010 "StringView": la direccion del caracter i-esimo. Es EXACTAMENTE la misma
            // llamada que StringElementLoad hace justo arriba, sin el load -- mismo receptor,
            // mismo indice, misma comprobacion de rango, mismo GEP con paso 1 y desplazamiento
            // 8. No hay aritmetica nueva aqui, y eso es deliberado: si algun dia el layout de
            // una cadena cambia, cambia en un sitio y las dos operaciones lo siguen.
            //
            // La comprobacion de rango SE QUEDA. Un puntero fuera de rango es peor que un
            // caracter fuera de rango, no mejor. Una vista vacia se construye con puntero nulo
            // en la biblioteca, no pidiendo la direccion de un caracter que no existe.
            case frontend::ProjectOopMirNodeKind::StringCharacterPointer:
                return lowerExpressionIterative(node, error);
            // NS304-T03-F03: list-pattern rest capture over a native string synthesizes a
            // StringView `{ char* start; int count }` (16-byte indirect aggregate carrier)
            // without calling StringView.Of. The data pointer is the same bounds-checked GEP
            // as StringCharacterPointer; the count is the length operand.
            case frontend::ProjectOopMirNodeKind::StringViewFromString:
                return lowerExpressionIterative(node, error);
            // NS304-T03-F03: list-pattern rest capture over a native array -> Span<T>/ReadOnlySpan<T>
            // (`{ T* data; int count }`). Same no-copy synthesis as StringView, but the data pointer
            // is the array element address and the element size comes from `elementTypeKey`.
            case frontend::ProjectOopMirNodeKind::SpanFromArray:
            case frontend::ProjectOopMirNodeKind::ReadOnlySpanFromArray:
                return lowerExpressionIterative(node, error);
            // NS304-T03-F03: fixed-array rest capture is a borrowed Span view over inline storage.
            // Unlike a native array there is no object header: the receiver is already the address
            // of the fixed array, and the extent is carried by its canonical type identity.
            case frontend::ProjectOopMirNodeKind::SpanFromFixedArray:
            case frontend::ProjectOopMirNodeKind::ReadOnlySpanFromFixedArray:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::StringSubstring:
                return lowerStringSubstring(node, error);
            case frontend::ProjectOopMirNodeKind::StringFromCharArray:
                return lowerStringFromCharArray(node, error);
            case frontend::ProjectOopMirNodeKind::CoreConsoleWriteLine:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreConsoleReadLine:
                return lowerExpressionIterative(node, error);

            // Stage 53.1 "Time": three sibling zero-argument, zero-branch calls. No argument
            // lowering (there are no children to lower), no result post-processing -- the raw
            // i64 the runtime hook returns IS the NativeSharp `long` value, unmodified.
            case frontend::ProjectOopMirNodeKind::CoreClockQueryCounter:
                if (clockQueryCounterFunction_ == nullptr)
                    return failValue(error, "Stage 53.1 NativeClock.QueryCounterRaw requires its runtime hook");
                return buildNoUnwindCall(
                    clockQueryCounterFunctionType_,
                    clockQueryCounterFunction_, nullptr, 0, "clock.counter");
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup05(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;
            // C8 "type root": the two primitives NativeSharp.Core.Object's virtual methods rest
            // on. Both take the object reference as their single operand.
            case frontend::ProjectOopMirNodeKind::AggregateEquals:
                return lowerAggregateEquals(node, error);
            case frontend::ProjectOopMirNodeKind::AggregateHash:
                return lowerAggregateHash(node, error);
            case frontend::ProjectOopMirNodeKind::CoreObjectTypeName:
                return lowerObjectTypeName(node, error);
            case frontend::ProjectOopMirNodeKind::CoreObjectReferenceHash:
                return lowerObjectReferenceHash(node, error);

            case frontend::ProjectOopMirNodeKind::CoreClockQueryFrequency:
                if (clockQueryFrequencyFunction_ == nullptr)
                    return failValue(error, "Stage 53.1 NativeClock.QueryFrequencyRaw requires its runtime hook");
                return buildNoUnwindCall(
                    clockQueryFrequencyFunctionType_,
                    clockQueryFrequencyFunction_, nullptr, 0, "clock.frequency");
            case frontend::ProjectOopMirNodeKind::CoreClockQuerySystemFileTime:
                if (clockQuerySystemFileTimeFunction_ == nullptr)
                    return failValue(error, "Stage 53.1 NativeClock.QuerySystemFileTimeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    clockQuerySystemFileTimeFunctionType_,
                    clockQuerySystemFileTimeFunction_, nullptr, 0, "clock.filetime");

            // Stage 53.2 "Environment": TickCount64/ProcessorCount are zero-argument calls,
            // same as the NativeClock trio -- the raw i64/i32 the runtime hook returns IS the
            // NativeSharp long/int value, unmodified.
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentTickCount64:
                if (environmentTickCount64Function_ == nullptr)
                    return failValue(error, "Stage 53.2 Environment.TickCount64 requires its runtime hook");
                return buildNoUnwindCall(
                    environmentTickCount64FunctionType_,
                    environmentTickCount64Function_, nullptr, 0, "environment.tickcount64");
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentProcessorCount:
                if (environmentProcessorCountFunction_ == nullptr)
                    return failValue(error, "Stage 53.2 Environment.ProcessorCount requires its runtime hook");
                return buildNoUnwindCall(
                    environmentProcessorCountFunctionType_,
                    environmentProcessorCountFunction_, nullptr, 0, "environment.processorcount");
            // Exit: one argument, void return -- same shape as CoreConsoleWriteLine above, just
            // an i32 value instead of a string pointer, and there is nothing meaningful to
            // return afterward since ExitProcess never returns control (see buildEnvironmentExit
            // in WindowsRuntimeBuilder.cpp, which ends in "unreachable" rather than a ret).
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentExit:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetVariable:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentSetVariable:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup06(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            // Stage 53.3 "Filesystem/Paths": File.Exists/Directory.Exists take one string
            // argument (a path) and return the runtime hook's i1 result directly -- no result
            // post-processing needed, the hook already returns exactly the NativeSharp
            // builtin:bool this HIR node claims as its type (see buildFileExists/
            // buildDirectoryExists in WindowsRuntimeBuilder.cpp).
            case frontend::ProjectOopMirNodeKind::CoreFileExists:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryExists:
            case frontend::ProjectOopMirNodeKind::CoreFileDelete:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryCreate:
                return lowerExpressionIterative(node, error);

            // Stage 53.3b "Filesystem/Paths (contenido de archivo)": ReadAllText takes one
            // string argument (a path) and returns the freshly-allocated string the runtime
            // hook builds -- same shape as CoreEnvironmentGetVariable above (all the size/
            // allocation work happens on the runtime side, in
            // WindowsRuntimeBuilder.cpp's buildFileReadAllText).
            case frontend::ProjectOopMirNodeKind::CoreFileReadAllText:
            case frontend::ProjectOopMirNodeKind::CoreFileGetSize:
            case frontend::ProjectOopMirNodeKind::CoreFileReadAllBytes:
                return lowerExpressionIterative(node, error);
            // ReadBytes: la ruta y el arreglo del llamador; devuelve cuantos bytes se leyeron.
            case frontend::ProjectOopMirNodeKind::CoreFileReadBytes:
                return lowerExpressionIterative(node, error);
            // WriteAllBytes: ruta, arreglo y cuantos bytes de el escribir. Devuelve i1.
            case frontend::ProjectOopMirNodeKind::CoreFileWriteAllBytes:
                return lowerExpressionIterative(node, error);
            // CurrentDirectory: cero operandos, una cadena recien asignada de vuelta.
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentCurrentDirectory:
            {
                if (environmentCurrentDirectoryFunction_ == nullptr)
                    return failValue(error, "Bloque 4 Environment.CurrentDirectory requires its runtime hook");
                return buildNoUnwindCall(
                    environmentCurrentDirectoryFunctionType_, environmentCurrentDirectoryFunction_,
                    nullptr, 0, "environment.currentdirectory");
            }
            // Bloque 4, fase C -- Process.Run. Tres operandos, i32 de vuelta.
            case frontend::ProjectOopMirNodeKind::CoreProcessRun:
                return lowerExpressionIterative(node, error);
            // WriteAllText/AppendAllText: two string arguments (path, content), void return --
            // same "call the hook, dummy i1 result" shape CoreEnvironmentSetVariable uses.
            case frontend::ProjectOopMirNodeKind::CoreFileWriteAllText:
            case frontend::ProjectOopMirNodeKind::CoreFileAppendAllText:
                return lowerExpressionIterative(node, error);

            // Stage 53.4 "Filesystem/Paths (cierre)": Directory.Delete takes one string argument
            // (a path), void return -- same "call the hook, dummy i1 result" shape CoreFileDelete/
            // CoreDirectoryCreate above use.
            case frontend::ProjectOopMirNodeKind::CoreDirectoryDelete:
            case frontend::ProjectOopMirNodeKind::CorePathGetFullPath:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetFileCount:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount:
            {
                if (node->children.size() != 0 || environmentGetCommandLineArgCountFunction_ == nullptr)
                    return failValue(
                        error,
                        "Stage 53.5 Environment.GetCommandLineArgCount takes no arguments and requires runtime hook");
                return buildNoUnwindCall(
                    environmentGetCommandLineArgCountFunctionType_, environmentGetCommandLineArgCountFunction_,
                    nullptr, 0, "environment.getcommandlineargcount");
            }
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs:
                return lowerExpressionIterative(node, error);
            // GetFiles/GetDirectories take two operands -- the path, then the caller-provided
            // builtin:string[] buffer (same "buffer provided by caller" pattern
            // CoreEnvironmentGetCommandLineArgs established) -- and return the i32 the runtime
            // hook computes (how many elements were actually written).
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetFiles:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectories:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup07(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            // Stage 54.1 "Threads". ThreadCreate takes one operand -- a ThreadStart delegate
            // value (already fully constructed by the time it reaches this static call: whoever
            // wrote `new Thread(someDelegate)` in NativeSharp already went through the normal
            // delegate-creation machinery, either lowerDelegateCreate for a static/instance
            // method group or lowerLambdaCreate for a lambda literal -- ThreadCreate's operand
            // here is just an ORDINARY expression of delegate type, lowered the ordinary way via
            // lowerExpression, exactly like any other reference-typed operand). lowerExpression on
            // a delegate-typed expression already yields the `ptr` to its 16-byte heap cell (see
            // lowerDelegateCreate/lowerLambdaCreate/emitDelegateCall above in this file for that
            // representation), so this cell pointer is handed to CreateThread's lpParameter
            // as-is, with zero additional marshaling on this side -- all of the "read code/
            // receiver from the cell and branch" logic lives in the trampoline function inside
            // WindowsRuntimeBuilder.cpp (buildThreadTrampolineFunction), not here. The result is
            // the real, meaningful i64 HANDLE (as nint) -- no dummy-i1 wrapping, same as
            // CoreDirectoryGetFileCount/CoreEnvironmentGetCommandLineArgCount above.
            case frontend::ProjectOopMirNodeKind::CoreThreadCreate:
                return lowerExpressionIterative(node, error);
            // ThreadJoin/ThreadDetach take one operand -- a builtin:nint (the HANDLE the caller
            // already holds, saved by Thread's constructor from ThreadCreate's result) -- and
            // return void at the NativeSharp level. Same "runtime function genuinely returns
            // void, this case block synthesizes its own harmless i1 dummy value after the call"
            // pattern CoreDirectoryDelete/CoreFileDelete above already use.
            case frontend::ProjectOopMirNodeKind::CoreThreadJoin:
            case frontend::ProjectOopMirNodeKind::CoreThreadDetach:
                return lowerExpressionIterative(node, error);

            // Stage 54.2 "Atomics". Increment/Decrement take one operand -- a builtin:int*/
            // builtin:long* location -- and return a real i32/i64 result (the new value). No
            // dummy-i1 wrapping: unlike ThreadJoin/ThreadDetach above, these genuinely return a
            // meaningful value at the NativeSharp level too.
            case frontend::ProjectOopMirNodeKind::CoreAtomicThreadFence:
            case frontend::ProjectOopMirNodeKind::CoreAtomicCompilerFence:
            {
                if (!node->children.empty())
                    return failValue(error, "AtomicFence cannot carry runtime operands");
                const std::optional<LLVMAtomicOrdering> ordering =
                    orderedAtomicRmwOrdering(node->integerValue);
                if (!ordering.has_value() || *ordering == LLVMAtomicOrderingMonotonic)
                    return failValue(error, "AtomicFence carries an invalid MemoryOrder");
                const bool compilerOnly =
                    node->kind == frontend::ProjectOopMirNodeKind::CoreAtomicCompilerFence;
                LLVMBuildFence(builder_, *ordering, compilerOnly ? 1 : 0, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup08(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            case frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt32:
                return lowerExpressionIterative(node, error);
            // Entrega 228: Float32x4 conserva su carrier agregado publico, pero la operacion
            // interna se realiza sobre <4 x float>. Con punteros opacos basta un alloca del
            // carrier para reinterpretar sin cambiar firmas, layout, PDB ni metadata.
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Add:
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Subtract:
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Multiply:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreHalfFromFloat:
            case frontend::ProjectOopMirNodeKind::CoreHalfToFloat:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSimdSwizzleWzyx:
            case frontend::ProjectOopMirNodeKind::CoreSimdShuffleLowHigh:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSimdSqrt:
            case frontend::ProjectOopMirNodeKind::CoreSimdReciprocalSqrt:
            case frontend::ProjectOopMirNodeKind::CoreSimdFusedMultiplyAdd:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreCpuPrefetchRead:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreCpuPause:
            {
                if (!node->children.empty()) return failValue(error, "Pause cannot have operands");
                LLVMTypeRef type = LLVMFunctionType(LLVMVoidTypeInContext(context_), nullptr, 0U, 0);
                LLVMValueRef intrinsic = LLVMGetNamedFunction(module_, "llvm.x86.sse2.pause");
                if (intrinsic == nullptr) intrinsic = LLVMAddFunction(module_, "llvm.x86.sse2.pause", type);
                LLVMBuildCall2(builder_, type, intrinsic, nullptr, 0U, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreCpuReadTimestamp:
            {
                if (!node->children.empty())
                    return failValue(error, "ReadTimestamp cannot have operands");
                LLVMTypeRef type = LLVMFunctionType(LLVMInt64TypeInContext(context_), nullptr, 0U, 0);
                LLVMValueRef intrinsic = LLVMGetNamedFunction(module_, "llvm.readcyclecounter");
                if (intrinsic == nullptr) intrinsic = LLVMAddFunction(module_, "llvm.readcyclecounter", type);
                return LLVMBuildCall2(builder_, type, intrinsic, nullptr, 0U, "cpu.timestamp");
            }
            case frontend::ProjectOopMirNodeKind::CoreBitRotateLeft:
            case frontend::ProjectOopMirNodeKind::CoreBitRotateRight:
            case frontend::ProjectOopMirNodeKind::CoreBitByteSwap:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreBitPopulationCount:
            case frontend::ProjectOopMirNodeKind::CoreBitLeadingZeroCount:
            case frontend::ProjectOopMirNodeKind::CoreBitTrailingZeroCount:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSimdInt128Add:
            case frontend::ProjectOopMirNodeKind::CoreSimdInt128Subtract:
            case frontend::ProjectOopMirNodeKind::CoreSimdInt128Multiply:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat256Add:
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat256Subtract:
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat256Multiply:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSimdInt256Add:
            case frontend::ProjectOopMirNodeKind::CoreSimdInt256Subtract:
            case frontend::ProjectOopMirNodeKind::CoreSimdInt256Multiply:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSimdLoadAligned128:
            case frontend::ProjectOopMirNodeKind::CoreSimdStoreAligned128:
            case frontend::ProjectOopMirNodeKind::CoreSimdLoadUnaligned128:
            case frontend::ProjectOopMirNodeKind::CoreSimdStoreUnaligned128:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup09(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;
            case frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt32:
            case frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt64:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt64:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt32:
            case frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt32:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt64:
            case frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt64:
                return lowerExpressionIterative(node, error);
            // Add/Exchange take two operands -- location, then the value -- and return a real
            // i32/i64 result (the new value for Add, the old value for Exchange -- the difference
            // is handled entirely on the runtime side, see WindowsRuntimeBuilder.cpp; this lowerer
            // only forwards arguments and returns whatever the runtime function computed).
            case frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt32:
            case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt32:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt64:
            case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt64:
                return lowerExpressionIterative(node, error);
            // CompareExchange takes three operands -- location, value, comparand -- same order as
            // Win32's InterlockedCompareExchange(Destination, Exchange, Comparand) and C#'s
            // Interlocked.CompareExchange(location, value, comparand). Returns the OLD value
            // regardless of whether the comparison matched, forwarded directly from Win32.
            case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32:
            case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64:
                return lowerExpressionIterative(node, error);

            case frontend::ProjectOopMirNodeKind::CoreAtomicLoadPointer:
            case frontend::ProjectOopMirNodeKind::CoreAtomicStorePointer:
            case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangePointer:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup10(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            // Stage 54.3 "Mutex". MutexCreate takes ZERO operands (the first zero-operand core
            // operation in the project) and returns the real, meaningful i64 HANDLE (as nint) --
            // no dummy-i1 wrapping, same discipline as CoreThreadCreate above.
            case frontend::ProjectOopMirNodeKind::CoreMutexCreate:
            {
                if (!node->children.empty() || mutexCreateFunction_ == nullptr)
                    return failValue(
                        error, "Stage 54.3 Mutex.MutexCreate takes no arguments and requires its runtime hook");
                return buildNoUnwindCall(
                    mutexCreateFunctionType_, mutexCreateFunction_, nullptr, 0, "mutex.create");
            }
            case frontend::ProjectOopMirNodeKind::CoreMutexCreateNamed:
            case frontend::ProjectOopMirNodeKind::CoreMutexLock:
            case frontend::ProjectOopMirNodeKind::CoreMutexUnlock:
            case frontend::ProjectOopMirNodeKind::CoreMutexClose:
                return lowerExpressionIterative(node, error);

            // Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll: one FieldAddress child
            // (built in ProjectOopHirModel.cpp, resolved to a real pointer by `fieldAddress()`
            // already, INCLUDING its own null-receiver trap) -- exact same "lower the one child,
            // forward the pointer, dummy-i1 return" shape as CoreMutexLock/Unlock/Close above.
            case frontend::ProjectOopMirNodeKind::CoreMonitorEnter:
            case frontend::ProjectOopMirNodeKind::CoreMonitorExit:
            case frontend::ProjectOopMirNodeKind::CoreMonitorPulse:
            case frontend::ProjectOopMirNodeKind::CoreMonitorPulseAll:
                return lowerExpressionIterative(node, error);
            // CoreMonitorWait: the ONE case in this round that lowers its receiver child exactly
            // ONCE and then GEPs it TWICE (using `immediate`/`secondaryImmediate`, the cv_/lock_
            // byte offsets ProjectOopMirModel.cpp already resolved) -- see this node kind's own
            // header comment in ProjectOopHirModel.h/MethodCoreOperationKind::MonitorWait's
            // comment in ProjectMethodBodyModel.h for the full "why not two FieldAddress children"
            // rationale. checkedReceiver runs ONCE, shared by both GEPs -- Monitor's receiver is
            // never re-validated twice for the same call, unlike two independent FieldAddress
            // nodes would each do.
            case frontend::ProjectOopMirNodeKind::CoreMonitorWait:
                return lowerExpressionIterative(node, error);

            // Stage 54.5 "Otras primitivas de sincronizacion". SemaphoreCreate: two explicit i32
            // operands (initialCount, maximumCount), a REAL i64 HANDLE result -- no dummy-i1
            // wrapping, same discipline as CoreMutexCreate/CoreThreadCreate.
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreCreate:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreCreateNamed:
                return lowerExpressionIterative(node, error);
            // SemaphoreWait/EventWait BOTH lower to the SAME shared runtime hook
            // (waitForHandleTimeoutFunction_) -- see buildWaitForHandleTimeout's header comment in
            // WindowsRuntimeBuilder.cpp for why sharing one wrapper across two
            // MethodCoreOperationKind values is safe here (the underlying Win32 operation is
            // byte-for-byte identical: WaitForSingleObject(handle, ms) collapsed to a bool).
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreWait:
            case frontend::ProjectOopMirNodeKind::CoreEventWait:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreRelease:
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreClose:
            case frontend::ProjectOopMirNodeKind::CoreEventSet:
            case frontend::ProjectOopMirNodeKind::CoreEventReset:
            case frontend::ProjectOopMirNodeKind::CoreEventClose:
                return lowerExpressionIterative(node, error);
            // EventCreate: two explicit i1 operands (manualReset, initialState), a REAL i64
            // HANDLE result.
            case frontend::ProjectOopMirNodeKind::CoreEventCreate:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::CoreEventCreateNamed:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup11(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            // Stage 55.1 "Process". Every getter is the simplest possible core-operation shape
            // (zero arguments, one call, return its result) -- same as CoreClockQueryCounter.
            case frontend::ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId:
                if (processGetCurrentProcessIdFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.GetCurrentProcessIdRaw requires its runtime hook");
                return buildNoUnwindCall(
                    processGetCurrentProcessIdFunctionType_,
                    processGetCurrentProcessIdFunction_, nullptr, 0, "process.currentprocessid");
            case frontend::ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize:
                if (processGetWorkingSetSizeFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.GetWorkingSetSizeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    processGetWorkingSetSizeFunctionType_,
                    processGetWorkingSetSizeFunction_, nullptr, 0, "process.workingsetsize");
            case frontend::ProjectOopMirNodeKind::CoreProcessGetKernelTime:
                if (processGetKernelTimeFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.GetKernelTimeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    processGetKernelTimeFunctionType_, processGetKernelTimeFunction_, nullptr, 0, "process.kerneltime");
            case frontend::ProjectOopMirNodeKind::CoreProcessGetUserTime:
                if (processGetUserTimeFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.GetUserTimeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    processGetUserTimeFunctionType_, processGetUserTimeFunction_, nullptr, 0, "process.usertime");
            case frontend::ProjectOopMirNodeKind::CoreProcessGetPriorityClass:
                if (processGetPriorityClassFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.GetPriorityClassRaw requires its runtime hook");
                return buildNoUnwindCall(
                    processGetPriorityClassFunctionType_,
                    processGetPriorityClassFunction_, nullptr, 0, "process.priorityclass");
            case frontend::ProjectOopMirNodeKind::CoreProcessSetPriorityClass:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup12(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            // Stage 55.2 "Hardware / System Info". Same zero-argument shape as Process above.
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetPageSize:
                if (systemInfoGetPageSizeFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetPageSizeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetPageSizeFunctionType_,
                    systemInfoGetPageSizeFunction_, nullptr, 0, "systeminfo.pagesize");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture:
                if (systemInfoGetProcessorArchitectureFunction_ == nullptr)
                    return failValue(
                        error, "Stage 55.2 SystemInfo.GetProcessorArchitectureRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetProcessorArchitectureFunctionType_,
                    systemInfoGetProcessorArchitectureFunction_, nullptr, 0,
                    "systeminfo.processorarchitecture");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity:
                if (systemInfoGetAllocationGranularityFunction_ == nullptr)
                    return failValue(
                        error, "Stage 55.2 SystemInfo.GetAllocationGranularityRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetAllocationGranularityFunctionType_,
                    systemInfoGetAllocationGranularityFunction_, nullptr, 0,
                    "systeminfo.allocationgranularity");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes:
                if (systemInfoGetTotalPhysicalMemoryBytesFunction_ == nullptr)
                    return failValue(
                        error, "Stage 55.2 SystemInfo.GetTotalPhysicalMemoryBytesRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetTotalPhysicalMemoryBytesFunctionType_,
                    systemInfoGetTotalPhysicalMemoryBytesFunction_, nullptr, 0,
                    "systeminfo.totalphysicalmemorybytes");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes:
                if (systemInfoGetAvailablePhysicalMemoryBytesFunction_ == nullptr)
                    return failValue(
                        error, "Stage 55.2 SystemInfo.GetAvailablePhysicalMemoryBytesRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetAvailablePhysicalMemoryBytesFunctionType_,
                    systemInfoGetAvailablePhysicalMemoryBytesFunction_, nullptr, 0,
                    "systeminfo.availablephysicalmemorybytes");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent:
                if (systemInfoGetMemoryLoadPercentFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetMemoryLoadPercentRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetMemoryLoadPercentFunctionType_,
                    systemInfoGetMemoryLoadPercentFunction_, nullptr, 0, "systeminfo.memoryloadpercent");

            // Stage 55.3 diagnostics and the Pre-Stage-56 utility round share this group helper.
            case frontend::ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent:
            case frontend::ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount:
            case frontend::ProjectOopMirNodeKind::CoreDebugOutputString:
            case frontend::ProjectOopMirNodeKind::CoreThreadSleep:
            case frontend::ProjectOopMirNodeKind::CoreFileCopy:
            case frontend::ProjectOopMirNodeKind::CoreFileMove:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryMove:
                return lowerExpressionGroup12DiagnosticsAndUtilities(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup12DiagnosticsAndUtilities(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;
            // Stage 55.3 "Diagnostics".
            case frontend::ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent:
                if (debugIsDebuggerPresentFunction_ == nullptr)
                    return failValue(error, "Stage 55.3 Debug.IsDebuggerPresentRaw requires its runtime hook");
                return buildNoUnwindCall(
                    debugIsDebuggerPresentFunctionType_,
                    debugIsDebuggerPresentFunction_, nullptr, 0, "debug.isdebuggerpresent");
            case frontend::ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount:
                if (debugCaptureStackFrameCountFunction_ == nullptr)
                    return failValue(error, "Stage 55.3 Debug.CaptureStackFrameCountRaw requires its runtime hook");
                return buildNoUnwindCall(
                    debugCaptureStackFrameCountFunctionType_,
                    debugCaptureStackFrameCountFunction_, nullptr, 0,
                    "debug.capturestackframecount");
            case frontend::ProjectOopMirNodeKind::CoreDebugOutputString:
                return lowerExpressionIterative(node, error);

            // Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29).
            case frontend::ProjectOopMirNodeKind::CoreThreadSleep:
            case frontend::ProjectOopMirNodeKind::CoreFileCopy:
            case frontend::ProjectOopMirNodeKind::CoreFileMove:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryMove:
                return lowerExpressionIterative(node, error);
    }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup13(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            case frontend::ProjectOopMirNodeKind::LambdaCreate:
                return lowerLambdaCreate(node, error);
            case frontend::ProjectOopMirNodeKind::ClosureCaptureLoad:
                return lowerClosureCaptureLoad(node, error);
            case frontend::ProjectOopMirNodeKind::ClosureCaptureAddress:
                return closureCaptureAddress(node, error);
            case frontend::ProjectOopMirNodeKind::CaptureCellReference:
                return lowerCaptureCellReference(node, error);
            case frontend::ProjectOopMirNodeKind::ClosureCaptureCellReference:
                return closureCaptureCell(node, error);
            case frontend::ProjectOopMirNodeKind::DelegateCreate:
                return lowerDelegateCreate(node, error);
            case frontend::ProjectOopMirNodeKind::DelegateInvoke:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::EventAdd:
                return lowerEventAdd(node, error);
            case frontend::ProjectOopMirNodeKind::EventRemove:
                return lowerEventRemove(node, error);
            case frontend::ProjectOopMirNodeKind::EventInvoke:
                return lowerExpressionIterative(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup14(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            case frontend::ProjectOopMirNodeKind::Unary:
            case frontend::ProjectOopMirNodeKind::Binary:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::TupleDeconstruct:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::TupleDeconstructElementStore:
                return failValue(error, "tuple deconstruction element descriptor escaped its parent");
            case frontend::ProjectOopMirNodeKind::Store:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::Convert:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::RuntimeTypeTest:
                return lowerRuntimeTypeTest(node, error);
            case frontend::ProjectOopMirNodeKind::SafeReferenceCast:
                return lowerSafeReferenceCast(node, error);
            case frontend::ProjectOopMirNodeKind::CheckedReferenceCast:
                return lowerCheckedReferenceCast(node, error);
            case frontend::ProjectOopMirNodeKind::RangeSlice:
                return lowerRangeSlice(node, error);
            case frontend::ProjectOopMirNodeKind::DirectInstanceCall:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::DirectStaticCall:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::VirtualInstanceCall:
            case frontend::ProjectOopMirNodeKind::InterfaceInstanceCall:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::TemporaryAddress:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::ValueConstruct:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::PlacementConstruct:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::ParameterPack:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::AggregateInitialize:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::AggregateSpreadStore:
                return failValue(
                    error, "T01-F05 aggregate spread must be lowered by its owning initializer");
            case frontend::ProjectOopMirNodeKind::AggregateElementStore:
                return failValue(
                    error, "C7 aggregate element store must be lowered by its owning initializer");
            case frontend::ProjectOopMirNodeKind::AllocateAndConstruct:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::Deallocate:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::ObjectDestroy:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::Assert:
            case frontend::ProjectOopMirNodeKind::Assume:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::ResultPropagation:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::BlockExpression:
                return lowerBlockExpression(node, error);
        }
}

LLVMValueRef OopLlvmImplementation::lowerExpressionGroup15(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
    switch (node->kind)
    {
        default:
            return nullptr;

            // LOS VEINTE QUE NO SON EXPRESIONES, escritos uno a uno en vez de un `default:`.
            //
            // POR QUE, y es el septimo sitio de la cadena del doc 100 -- el que faltaba por
            // guardar. Aqui habia `default: return failValue(...)`. Con el `default`, anadir un
            // enumerador nuevo a ProjectOopMirNodeKind y olvidarse de bajarlo NO daba ningun
            // error: caia aqui y el programa fallaba en tiempo de ejecucion con un mensaje que
            // decia "no es una expresion" sobre algo que si lo era. Un enumerador olvidado se
            // parecia exactamente a un enumerador mal colocado.
            //
            // Sin `default`, con -Wswitch y /WX, olvidarse es un ERROR DE COMPILACION QUE NOMBRA
            // EL ENUMERADOR. Es el mismo guardian que ya protege stringCallLowering en el HIR
            // (doc 106) y por el mismo motivo.
            //
            // Estos veinte son SENTENCIAS. Llegar aqui con una es un fallo interno del compilador,
            // no un programa malo del usuario, y por eso comparten un solo mensaje.
            case frontend::ProjectOopMirNodeKind::Block:
            case frontend::ProjectOopMirNodeKind::LocalDeclaration:
            case frontend::ProjectOopMirNodeKind::ExpressionStatement:
            case frontend::ProjectOopMirNodeKind::If:
            case frontend::ProjectOopMirNodeKind::While:
            case frontend::ProjectOopMirNodeKind::DoWhile:
            case frontend::ProjectOopMirNodeKind::For:
            case frontend::ProjectOopMirNodeKind::Switch:
            case frontend::ProjectOopMirNodeKind::SwitchSection:
            case frontend::ProjectOopMirNodeKind::Break:
            case frontend::ProjectOopMirNodeKind::Continue:
            case frontend::ProjectOopMirNodeKind::Return:
            case frontend::ProjectOopMirNodeKind::TryRegion:
            case frontend::ProjectOopMirNodeKind::CatchRegion:
            case frontend::ProjectOopMirNodeKind::FinallyRegion:
            case frontend::ProjectOopMirNodeKind::UsingRegion:
            case frontend::ProjectOopMirNodeKind::Rethrow:
            case frontend::ProjectOopMirNodeKind::CleanupContinuation:
            case frontend::ProjectOopMirNodeKind::OwnedParameterCleanup:
            case frontend::ProjectOopMirNodeKind::InlineAssembly:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyLine:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyInput:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyOutput:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyClobber:
            case frontend::ProjectOopMirNodeKind::MatchArm:
            case frontend::ProjectOopMirNodeKind::SuspendPoint:
            case frontend::ProjectOopMirNodeKind::YieldValue:
            case frontend::ProjectOopMirNodeKind::YieldBreak:
            case frontend::ProjectOopMirNodeKind::GeneratorFrameInit:
            case frontend::ProjectOopMirNodeKind::DropFrame:
            case frontend::ProjectOopMirNodeKind::AsyncYieldValue:
                return failValue(error, "non-expression Stage 34 MIR node reached expression lowering");
    }
}

}
