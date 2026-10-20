#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "MirConstantEvaluator.h"
#include "Internal/MirByteReader.h"
#include "MirVerifier.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Language/CoroutineSemantics.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <string_view>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
const char* projectOopMirAbiPassingName(ProjectOopMirAbiPassing passing) noexcept
{
    return passing == ProjectOopMirAbiPassing::Indirect ? "win64-indirect" : "win64-direct";
}

const char* projectOopMirCleanupKindName(ProjectOopMirCleanupKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirCleanupKind::LocalOwner: return "local-owner";
        case ProjectOopMirCleanupKind::TemporaryOwner: return "temporary-owner";
        case ProjectOopMirCleanupKind::OwnedParameter: return "owned-parameter";
        case ProjectOopMirCleanupKind::Using: return "using";
        case ProjectOopMirCleanupKind::Finally: return "finally";
    }
    return "unknown";
}

const char* projectOopMirNonlocalExitKindName(ProjectOopMirNonlocalExitKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNonlocalExitKind::Return: return "return";
        case ProjectOopMirNonlocalExitKind::Break: return "break";
        case ProjectOopMirNonlocalExitKind::Continue: return "continue";
        case ProjectOopMirNonlocalExitKind::Throw: return "throw";
        case ProjectOopMirNonlocalExitKind::Rethrow: return "rethrow";
        case ProjectOopMirNonlocalExitKind::Panic: return "panic";
        case ProjectOopMirNonlocalExitKind::CleanupContinuation: return "cleanup-continuation";
    }
    return "unknown";
}

namespace
{
const char* projectOopMirNodeKindNameGroup01(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::Block: return "block";
        case ProjectOopMirNodeKind::BlockExpression: return "block-expression";
        case ProjectOopMirNodeKind::LocalDeclaration: return "local-declaration";
        case ProjectOopMirNodeKind::AggregateInitialize: return "aggregate-initialize";
        case ProjectOopMirNodeKind::AggregateSpreadStore: return "aggregate-spread-store";
        case ProjectOopMirNodeKind::AggregateElementStore: return "aggregate-element-store";
        case ProjectOopMirNodeKind::TupleDeconstruct: return "tuple-deconstruct";
        case ProjectOopMirNodeKind::TupleDeconstructElementStore: return "tuple-deconstruct-element-store";
        case ProjectOopMirNodeKind::ParameterPack: return "parameter-pack";
        case ProjectOopMirNodeKind::ExpressionStatement: return "expression-statement";
        case ProjectOopMirNodeKind::InlineAssembly: return "inline-assembly";
        case ProjectOopMirNodeKind::InlineAssemblyLine: return "inline-assembly-line";
        case ProjectOopMirNodeKind::InlineAssemblyInput: return "inline-assembly-input";
        case ProjectOopMirNodeKind::InlineAssemblyOutput: return "inline-assembly-output";
        case ProjectOopMirNodeKind::InlineAssemblyClobber: return "inline-assembly-clobber";
        case ProjectOopMirNodeKind::If: return "if";
        case ProjectOopMirNodeKind::While: return "while";
        case ProjectOopMirNodeKind::DoWhile: return "do-while";
        case ProjectOopMirNodeKind::For: return "for";
        case ProjectOopMirNodeKind::Switch: return "switch";
        case ProjectOopMirNodeKind::SwitchSection: return "switch-section";
        case ProjectOopMirNodeKind::Break: return "break";
        case ProjectOopMirNodeKind::Continue: return "continue";
        case ProjectOopMirNodeKind::Return: return "return";
        case ProjectOopMirNodeKind::TryRegion: return "try-region";
        case ProjectOopMirNodeKind::CatchRegion: return "catch-region";
        case ProjectOopMirNodeKind::FinallyRegion: return "finally-region";
        case ProjectOopMirNodeKind::UsingRegion: return "using-region";
        case ProjectOopMirNodeKind::Throw: return "throw";
        case ProjectOopMirNodeKind::Rethrow: return "rethrow";
        case ProjectOopMirNodeKind::CleanupContinuation: return "cleanup-continuation";
        case ProjectOopMirNodeKind::OwnedParameterCleanup: return "owned-parameter-cleanup";
        case ProjectOopMirNodeKind::MoveValue: return "move-value";
        case ProjectOopMirNodeKind::MoveArrayElement: return "move-array-element";
        case ProjectOopMirNodeKind::MoveOwnedField: return "move-owned-field";
        case ProjectOopMirNodeKind::MoveIteratorCurrent: return "move-iterator-current";
        case ProjectOopMirNodeKind::OwnedTemporaryBorrow: return "owned-temporary-borrow";
        case ProjectOopMirNodeKind::FullExpressionCleanup: return "full-expression-cleanup";
        case ProjectOopMirNodeKind::PointerLoad: return "pointer-load";
        case ProjectOopMirNodeKind::RawElementAddress: return "raw-element-address";
    }
    return nullptr;
}

const char* projectOopMirNodeKindNameGroup02(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::StackAllocation: return "stack-allocation";
        case ProjectOopMirNodeKind::RawAllocation: return "raw-allocation";
        case ProjectOopMirNodeKind::PointerArithmetic: return "pointer-arithmetic";
        case ProjectOopMirNodeKind::CompoundStore: return "compound-store";
        case ProjectOopMirNodeKind::ValueConstruct: return "value-construct";
        case ProjectOopMirNodeKind::PlacementConstruct: return "placement-construct";
        case ProjectOopMirNodeKind::TemporaryAddress: return "temporary-address";
        case ProjectOopMirNodeKind::AddressValue: return "address-value";
        case ProjectOopMirNodeKind::BorrowValue: return "borrow-value";
        case ProjectOopMirNodeKind::ThisValue: return "this";
        case ProjectOopMirNodeKind::ParameterValue: return "parameter-value";
        case ProjectOopMirNodeKind::LocalValue: return "local-value";
        case ProjectOopMirNodeKind::FieldLoad: return "field-load";
        case ProjectOopMirNodeKind::NullConditionalFieldLoad: return "null-conditional-field-load";
        case ProjectOopMirNodeKind::NullConditionalFieldStore: return "null-conditional-field-store";
        case ProjectOopMirNodeKind::NullConditionalArrayLoad: return "null-conditional-array-load";
        case ProjectOopMirNodeKind::NullConditionalArrayStore: return "null-conditional-array-store";
        case ProjectOopMirNodeKind::BitFieldLoad: return "bit-field-load";
        case ProjectOopMirNodeKind::StaticFieldLoad: return "static-field-load";
        case ProjectOopMirNodeKind::NamespaceStateLoad: return "namespace-state-load";
        case ProjectOopMirNodeKind::ParameterAddress: return "parameter-address";
        case ProjectOopMirNodeKind::LocalAddress: return "local-address";
        case ProjectOopMirNodeKind::FieldAddress: return "field-address";
        case ProjectOopMirNodeKind::BitFieldAddress: return "bit-field-address";
        case ProjectOopMirNodeKind::FutureFieldAddress: return "future-field-address";
        case ProjectOopMirNodeKind::AwaitFuture: return "await-future";
        case ProjectOopMirNodeKind::SuspendPoint: return "suspend-point";
        case ProjectOopMirNodeKind::PollAddress: return "poll-address";
        case ProjectOopMirNodeKind::YieldValue: return "yield-value";
        case ProjectOopMirNodeKind::YieldBreak: return "yield-break";
        case ProjectOopMirNodeKind::GeneratorFrameInit: return "generator-frame-init";
        case ProjectOopMirNodeKind::IteratorCurrent: return "iterator-current";
        case ProjectOopMirNodeKind::IteratorMoveNext: return "iterator-move-next";
        case ProjectOopMirNodeKind::IteratorGetEnumerator: return "iterator-get-enumerator";
        case ProjectOopMirNodeKind::IteratorDrop: return "iterator-drop";
        case ProjectOopMirNodeKind::IteratorMoveNextAsync: return "iterator-move-next-async";
        case ProjectOopMirNodeKind::IteratorCancel: return "iterator-cancel";
        case ProjectOopMirNodeKind::AsyncYieldValue: return "async-yield-value";
        case ProjectOopMirNodeKind::DropFrame: return "drop-frame";
        case ProjectOopMirNodeKind::StaticFieldAddress: return "static-field-address";
    }
    return nullptr;
}

const char* projectOopMirNodeKindNameGroup03(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::NamespaceStateAddress: return "namespace-state-address";
        case ProjectOopMirNodeKind::IntegerConstant: return "integer-constant";
        case ProjectOopMirNodeKind::FloatingConstant: return "floating-constant";
        case ProjectOopMirNodeKind::BooleanConstant: return "boolean-constant";
        case ProjectOopMirNodeKind::NullConstant: return "null-constant";
        case ProjectOopMirNodeKind::DefaultValue: return "default-value";
        case ProjectOopMirNodeKind::StringLiteral: return "string-literal";
        case ProjectOopMirNodeKind::Utf8StringLiteral: return "utf8-string-literal";
        case ProjectOopMirNodeKind::ArrayAllocate: return "array-allocate";
        case ProjectOopMirNodeKind::RangeArrayAllocate: return "range-array-allocate";
        case ProjectOopMirNodeKind::ArrayLength: return "array-length";
        case ProjectOopMirNodeKind::ArrayElementLoad: return "array-element-load";
        case ProjectOopMirNodeKind::ArrayElementAddress: return "array-element-address";
        case ProjectOopMirNodeKind::RangeArrayElementLoad: return "range-array-element-load";
        case ProjectOopMirNodeKind::RangeArrayElementAddress: return "range-array-element-address";
        case ProjectOopMirNodeKind::RangeSlice: return "range-slice";
        case ProjectOopMirNodeKind::StringLength: return "string-length";
        case ProjectOopMirNodeKind::StringElementLoad: return "string-element-load";
        case ProjectOopMirNodeKind::EnumName: return "enum-name";
        case ProjectOopMirNodeKind::StringConcat: return "string-concat";
        case ProjectOopMirNodeKind::StringEquals: return "string-equals";
        case ProjectOopMirNodeKind::StringSubstring: return "string-substring";
        case ProjectOopMirNodeKind::StringCharacterPointer: return "string-character-pointer";
        case ProjectOopMirNodeKind::StringFromCharArray: return "string-from-char-array";
        case ProjectOopMirNodeKind::StringViewFromString: return "string-view-from-string";
        case ProjectOopMirNodeKind::SpanFromArray: return "span-from-array";
        case ProjectOopMirNodeKind::ReadOnlySpanFromArray: return "readonly-span-from-array";
        case ProjectOopMirNodeKind::SpanFromFixedArray: return "span-from-fixed-array";
        case ProjectOopMirNodeKind::ReadOnlySpanFromFixedArray: return "readonly-span-from-fixed-array";
        case ProjectOopMirNodeKind::CoreConsoleWriteLine: return "core-console-writeline";
        case ProjectOopMirNodeKind::CoreConsoleReadLine: return "core-console-readline";
        case ProjectOopMirNodeKind::AggregateEquals: return "aggregate-equals";
        case ProjectOopMirNodeKind::AggregateHash: return "aggregate-hash";
        case ProjectOopMirNodeKind::CoreObjectTypeName: return "core-object-type-name";
        case ProjectOopMirNodeKind::CoreObjectReferenceHash: return "core-object-reference-hash";
        case ProjectOopMirNodeKind::CoreClockQueryCounter: return "core-clock-query-counter";
        case ProjectOopMirNodeKind::CoreClockQueryFrequency: return "core-clock-query-frequency";
        case ProjectOopMirNodeKind::CoreClockQuerySystemFileTime: return "core-clock-query-system-filetime";
        case ProjectOopMirNodeKind::CoreEnvironmentTickCount64: return "core-environment-tick-count64";
        case ProjectOopMirNodeKind::CoreEnvironmentProcessorCount: return "core-environment-processor-count";
    }
    return nullptr;
}

const char* projectOopMirNodeKindNameGroup04(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::CoreEnvironmentExit: return "core-environment-exit";
        case ProjectOopMirNodeKind::CoreEnvironmentGetVariable: return "core-environment-get-variable";
        case ProjectOopMirNodeKind::CoreEnvironmentSetVariable: return "core-environment-set-variable";
        case ProjectOopMirNodeKind::CoreFileExists: return "core-file-exists";
        case ProjectOopMirNodeKind::CoreFileDelete: return "core-file-delete";
        case ProjectOopMirNodeKind::CoreDirectoryExists: return "core-directory-exists";
        case ProjectOopMirNodeKind::CoreDirectoryCreate: return "core-directory-create";
        case ProjectOopMirNodeKind::CoreFileReadAllText: return "core-file-read-all-text";
        case ProjectOopMirNodeKind::CoreFileGetSize: return "core-file-get-size";
        case ProjectOopMirNodeKind::CoreFileReadAllBytes: return "core-file-read-all-bytes";
        case ProjectOopMirNodeKind::CoreFileReadBytes: return "core-file-read-bytes";
        case ProjectOopMirNodeKind::CoreFileWriteAllBytes: return "core-file-write-all-bytes";
        case ProjectOopMirNodeKind::CoreEnvironmentCurrentDirectory: return "core-environment-current-directory";
        case ProjectOopMirNodeKind::CoreProcessRun: return "core-process-run";
        case ProjectOopMirNodeKind::CoreFileWriteAllText: return "core-file-write-all-text";
        case ProjectOopMirNodeKind::CoreFileAppendAllText: return "core-file-append-all-text";
        case ProjectOopMirNodeKind::CoreDirectoryDelete: return "core-directory-delete";
        case ProjectOopMirNodeKind::CorePathGetFullPath: return "core-path-get-full-path";
        case ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount:
            return "core-environment-get-command-line-arg-count";
        case ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs: return "core-environment-get-command-line-args";
        case ProjectOopMirNodeKind::CoreDirectoryGetFileCount: return "core-directory-get-file-count";
        case ProjectOopMirNodeKind::CoreDirectoryGetFiles: return "core-directory-get-files";
        case ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount: return "core-directory-get-directory-count";
        case ProjectOopMirNodeKind::CoreDirectoryGetDirectories: return "core-directory-get-directories";
        case ProjectOopMirNodeKind::CoreThreadCreate: return "core-thread-create";
        case ProjectOopMirNodeKind::CoreThreadJoin: return "core-thread-join";
        case ProjectOopMirNodeKind::CoreThreadDetach: return "core-thread-detach";
        case ProjectOopMirNodeKind::CoreAtomicLoadInt32: return "core-atomic-load-int32";
        case ProjectOopMirNodeKind::CoreAtomicStoreInt32: return "core-atomic-store-int32";
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt32: return "core-interlocked-increment-int32";
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt32: return "core-interlocked-decrement-int32";
        case ProjectOopMirNodeKind::CoreInterlockedAddInt32: return "core-interlocked-add-int32";
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt32: return "core-interlocked-exchange-int32";
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32:
            return "core-interlocked-compare-exchange-int32";
        case ProjectOopMirNodeKind::CoreAtomicLoadInt64: return "core-atomic-load-int64";
        case ProjectOopMirNodeKind::CoreAtomicStoreInt64: return "core-atomic-store-int64";
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt64: return "core-interlocked-increment-int64";
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt64: return "core-interlocked-decrement-int64";
        case ProjectOopMirNodeKind::CoreInterlockedAddInt64: return "core-interlocked-add-int64";
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt64: return "core-interlocked-exchange-int64";
    }
    return nullptr;
}

const char* projectOopMirNodeKindNameGroup05(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64:
            return "core-interlocked-compare-exchange-int64";
        case ProjectOopMirNodeKind::CoreAtomicLoadPointer: return "core-atomic-load-pointer";
        case ProjectOopMirNodeKind::CoreAtomicStorePointer: return "core-atomic-store-pointer";
        case ProjectOopMirNodeKind::CoreInterlockedExchangePointer: return "core-interlocked-exchange-pointer";
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer:
            return "core-interlocked-compare-exchange-pointer";
        case ProjectOopMirNodeKind::CoreAtomicThreadFence: return "core-atomic-thread-fence";
        case ProjectOopMirNodeKind::CoreAtomicCompilerFence: return "core-atomic-compiler-fence";
        case ProjectOopMirNodeKind::CoreSimdFloat32x4Add: return "core-simd-float32x4-add";
        case ProjectOopMirNodeKind::CoreSimdFloat32x4Subtract: return "core-simd-float32x4-subtract";
        case ProjectOopMirNodeKind::CoreSimdFloat32x4Multiply: return "core-simd-float32x4-multiply";
        case ProjectOopMirNodeKind::CoreSimdInt128Add: return "core-simd-int128-add";
        case ProjectOopMirNodeKind::CoreSimdInt128Subtract: return "core-simd-int128-subtract";
        case ProjectOopMirNodeKind::CoreSimdInt128Multiply: return "core-simd-int128-multiply";
        case ProjectOopMirNodeKind::CoreSimdFloat256Add: return "core-simd-float256-add";
        case ProjectOopMirNodeKind::CoreSimdFloat256Subtract: return "core-simd-float256-subtract";
        case ProjectOopMirNodeKind::CoreSimdFloat256Multiply: return "core-simd-float256-multiply";
        case ProjectOopMirNodeKind::CoreSimdInt256Add: return "core-simd-int256-add";
        case ProjectOopMirNodeKind::CoreSimdInt256Subtract: return "core-simd-int256-subtract";
        case ProjectOopMirNodeKind::CoreSimdInt256Multiply: return "core-simd-int256-multiply";
        case ProjectOopMirNodeKind::CoreSimdLoadAligned128: return "core-simd-load-aligned-128";
        case ProjectOopMirNodeKind::CoreSimdStoreAligned128: return "core-simd-store-aligned-128";
        case ProjectOopMirNodeKind::CoreSimdLoadUnaligned128: return "core-simd-load-unaligned-128";
        case ProjectOopMirNodeKind::CoreSimdStoreUnaligned128: return "core-simd-store-unaligned-128";
        case ProjectOopMirNodeKind::CoreHalfFromFloat: return "core-half-from-float";
        case ProjectOopMirNodeKind::CoreHalfToFloat: return "core-half-to-float";
        case ProjectOopMirNodeKind::CoreSimdSwizzleWzyx: return "core-simd-swizzle-wzyx";
        case ProjectOopMirNodeKind::CoreSimdShuffleLowHigh: return "core-simd-shuffle-low-high";
        case ProjectOopMirNodeKind::CoreSimdSqrt: return "core-simd-sqrt";
        case ProjectOopMirNodeKind::CoreSimdReciprocalSqrt: return "core-simd-reciprocal-sqrt";
        case ProjectOopMirNodeKind::CoreSimdFusedMultiplyAdd: return "core-simd-fused-multiply-add";
        case ProjectOopMirNodeKind::CoreBitPopulationCount: return "core-bit-popcount";
        case ProjectOopMirNodeKind::CoreBitLeadingZeroCount: return "core-bit-clz";
        case ProjectOopMirNodeKind::CoreBitTrailingZeroCount: return "core-bit-ctz";
        case ProjectOopMirNodeKind::CoreBitRotateLeft: return "core-bit-rotate-left";
        case ProjectOopMirNodeKind::CoreBitRotateRight: return "core-bit-rotate-right";
        case ProjectOopMirNodeKind::CoreBitByteSwap: return "core-bit-byte-swap";
        case ProjectOopMirNodeKind::CoreCpuPrefetchRead: return "core-cpu-prefetch-read";
        case ProjectOopMirNodeKind::CoreCpuPause: return "core-cpu-pause";
        case ProjectOopMirNodeKind::CoreCpuReadTimestamp: return "core-cpu-read-timestamp";
        case ProjectOopMirNodeKind::CoreMutexCreate: return "core-mutex-create";
    }
    return nullptr;
}

const char* projectOopMirNodeKindNameGroup06(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::CoreMutexCreateNamed: return "core-mutex-create-named";
        case ProjectOopMirNodeKind::CoreMutexLock: return "core-mutex-lock";
        case ProjectOopMirNodeKind::CoreMutexUnlock: return "core-mutex-unlock";
        case ProjectOopMirNodeKind::CoreMutexClose: return "core-mutex-close";
        case ProjectOopMirNodeKind::CoreMonitorEnter: return "core-monitor-enter";
        case ProjectOopMirNodeKind::CoreMonitorExit: return "core-monitor-exit";
        case ProjectOopMirNodeKind::CoreMonitorWait: return "core-monitor-wait";
        case ProjectOopMirNodeKind::CoreMonitorPulse: return "core-monitor-pulse";
        case ProjectOopMirNodeKind::CoreMonitorPulseAll: return "core-monitor-pulse-all";
        case ProjectOopMirNodeKind::CoreSemaphoreCreate: return "core-semaphore-create";
        case ProjectOopMirNodeKind::CoreSemaphoreCreateNamed: return "core-semaphore-create-named";
        case ProjectOopMirNodeKind::CoreSemaphoreWait: return "core-semaphore-wait";
        case ProjectOopMirNodeKind::CoreSemaphoreRelease: return "core-semaphore-release";
        case ProjectOopMirNodeKind::CoreSemaphoreClose: return "core-semaphore-close";
        case ProjectOopMirNodeKind::CoreEventCreate: return "core-event-create";
        case ProjectOopMirNodeKind::CoreEventCreateNamed: return "core-event-create-named";
        case ProjectOopMirNodeKind::CoreEventSet: return "core-event-set";
        case ProjectOopMirNodeKind::CoreEventReset: return "core-event-reset";
        case ProjectOopMirNodeKind::CoreEventWait: return "core-event-wait";
        case ProjectOopMirNodeKind::CoreEventClose: return "core-event-close";
        case ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId: return "core-process-get-current-process-id";
        case ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize: return "core-process-get-working-set-size";
        case ProjectOopMirNodeKind::CoreProcessGetKernelTime: return "core-process-get-kernel-time";
        case ProjectOopMirNodeKind::CoreProcessGetUserTime: return "core-process-get-user-time";
        case ProjectOopMirNodeKind::CoreProcessGetPriorityClass: return "core-process-get-priority-class";
        case ProjectOopMirNodeKind::CoreProcessSetPriorityClass: return "core-process-set-priority-class";
        case ProjectOopMirNodeKind::CoreSystemInfoGetPageSize: return "core-system-info-get-page-size";
        case ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture:
            return "core-system-info-get-processor-architecture";
        case ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity:
            return "core-system-info-get-allocation-granularity";
        case ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes:
            return "core-system-info-get-total-physical-memory-bytes";
        case ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes:
            return "core-system-info-get-available-physical-memory-bytes";
        case ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent:
            return "core-system-info-get-memory-load-percent";
        case ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent: return "core-debug-is-debugger-present";
        case ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount: return "core-debug-capture-stack-frame-count";
        case ProjectOopMirNodeKind::CoreDebugOutputString: return "core-debug-output-string";
        case ProjectOopMirNodeKind::CoreThreadSleep: return "core-thread-sleep";
        case ProjectOopMirNodeKind::CoreFileCopy: return "core-file-copy";
        case ProjectOopMirNodeKind::CoreFileMove: return "core-file-move";
        case ProjectOopMirNodeKind::CoreDirectoryMove: return "core-directory-move";
        case ProjectOopMirNodeKind::DelegateCreate: return "delegate-create";
    }
    return nullptr;
}

const char* projectOopMirNodeKindNameGroup07(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::LambdaCreate: return "lambda-create";
        case ProjectOopMirNodeKind::ClosureCaptureLoad: return "closure-capture-load";
        case ProjectOopMirNodeKind::ClosureCaptureAddress: return "closure-capture-address";
        case ProjectOopMirNodeKind::CaptureCellReference: return "capture-cell-reference";
        case ProjectOopMirNodeKind::ClosureCaptureCellReference: return "closure-capture-cell-reference";
        case ProjectOopMirNodeKind::DelegateInvoke: return "delegate-invoke";
        case ProjectOopMirNodeKind::EventAdd: return "event-add";
        case ProjectOopMirNodeKind::EventRemove: return "event-remove";
        case ProjectOopMirNodeKind::EventInvoke: return "event-invoke";
        case ProjectOopMirNodeKind::Unary: return "unary";
        case ProjectOopMirNodeKind::Binary: return "binary";
        case ProjectOopMirNodeKind::Conditional: return "conditional";
        case ProjectOopMirNodeKind::NullCoalescing: return "null-coalescing";
        case ProjectOopMirNodeKind::OptionalCoalescing: return "optional-coalescing";
        case ProjectOopMirNodeKind::NullCoalescingStore: return "null-coalescing-store";
        case ProjectOopMirNodeKind::Store: return "store";
        case ProjectOopMirNodeKind::Convert: return "convert";
        case ProjectOopMirNodeKind::RuntimeTypeTest: return "runtime-type-test";
        case ProjectOopMirNodeKind::SafeReferenceCast: return "safe-reference-cast";
        case ProjectOopMirNodeKind::CheckedReferenceCast: return "checked-reference-cast";
        case ProjectOopMirNodeKind::DirectInstanceCall: return "direct-instance-call";
        case ProjectOopMirNodeKind::DirectStaticCall: return "direct-static-call";
        case ProjectOopMirNodeKind::VirtualInstanceCall: return "virtual-instance-call";
        case ProjectOopMirNodeKind::InterfaceInstanceCall: return "interface-instance-call";
        case ProjectOopMirNodeKind::AllocateAndConstruct: return "allocate-and-construct";
        case ProjectOopMirNodeKind::Deallocate: return "deallocate";
        case ProjectOopMirNodeKind::ObjectDestroy: return "object-destroy";
        case ProjectOopMirNodeKind::Assert: return "assert";
        case ProjectOopMirNodeKind::Unreachable: return "unreachable";
        case ProjectOopMirNodeKind::Assume: return "assume";
        case ProjectOopMirNodeKind::Panic: return "panic";
        case ProjectOopMirNodeKind::ResultPropagation: return "result-propagation";
        case ProjectOopMirNodeKind::LoopExpression: return "loop-expression";
        case ProjectOopMirNodeKind::MatchExpression: return "match-expression";
        case ProjectOopMirNodeKind::MatchArm: return "match-arm";
        case ProjectOopMirNodeKind::MatchValue: return "match-value";
        case ProjectOopMirNodeKind::OverflowPolicy: return "overflow-policy";
    }
    return nullptr;
}

}

const char* projectOopMirNodeKindName(ProjectOopMirNodeKind kind) noexcept
{
    if (const char* name = projectOopMirNodeKindNameGroup01(kind)) return name;
    if (const char* name = projectOopMirNodeKindNameGroup02(kind)) return name;
    if (const char* name = projectOopMirNodeKindNameGroup03(kind)) return name;
    if (const char* name = projectOopMirNodeKindNameGroup04(kind)) return name;
    if (const char* name = projectOopMirNodeKindNameGroup05(kind)) return name;
    if (const char* name = projectOopMirNodeKindNameGroup06(kind)) return name;
    if (const char* name = projectOopMirNodeKindNameGroup07(kind)) return name;
    return "unknown";
}

}
