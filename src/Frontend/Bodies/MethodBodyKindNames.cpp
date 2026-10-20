#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

namespace nativesharp::frontend
{
const char* methodScopeKindName(MethodScopeKind kind) noexcept
{
    switch (kind)
    {
        case MethodScopeKind::Method: return "method";
        case MethodScopeKind::Constructor: return "constructor";
        case MethodScopeKind::Destructor: return "destructor";
        case MethodScopeKind::FieldInitializer: return "field-initializer";
        case MethodScopeKind::Block: return "block";
        case MethodScopeKind::For: return "for";
        case MethodScopeKind::Foreach: return "foreach";
        case MethodScopeKind::Catch: return "catch";
        case MethodScopeKind::Using: return "using";
    }
    return "method";
}

const char* methodNameBindingKindName(MethodNameBindingKind kind) noexcept
{
    switch (kind)
    {
        case MethodNameBindingKind::Parameter: return "parameter";
        case MethodNameBindingKind::Local: return "local";
        case MethodNameBindingKind::ImplicitField: return "implicit-field";
        case MethodNameBindingKind::ImplicitMethodGroup: return "implicit-method-group";
        case MethodNameBindingKind::ThisReference: return "this";
        case MethodNameBindingKind::BaseReference: return "base";
        case MethodNameBindingKind::ExplicitThisField: return "this-field";
        case MethodNameBindingKind::ExplicitThisMethodGroup: return "this-method-group";
        case MethodNameBindingKind::ExplicitObjectField: return "object-field";
        case MethodNameBindingKind::ExplicitObjectMethodGroup: return "object-method-group";
        case MethodNameBindingKind::ExplicitBaseField: return "base-field";
        case MethodNameBindingKind::ExplicitBaseMethodGroup: return "base-method-group";
        case MethodNameBindingKind::StaticField: return "static-field";
        case MethodNameBindingKind::NamespaceState: return "namespace-state";
        case MethodNameBindingKind::StaticMethodGroup: return "static-method-group";
        case MethodNameBindingKind::ImplicitEvent: return "implicit-event";
        case MethodNameBindingKind::ExplicitThisEvent: return "this-event";
        case MethodNameBindingKind::ExplicitObjectEvent: return "object-event";
        case MethodNameBindingKind::StaticEvent: return "static-event";
        case MethodNameBindingKind::TypeReference: return "type-reference";
    }
    return "local";
}

const char* methodShadowingKindName(MethodShadowingKind kind) noexcept
{
    switch (kind)
    {
        case MethodShadowingKind::ParameterShadowsMember: return "parameter-shadows-member";
        case MethodShadowingKind::LocalShadowsMember: return "local-shadows-member";
        case MethodShadowingKind::LocalShadowsParameter: return "local-shadows-parameter";
        case MethodShadowingKind::LocalShadowsLocal: return "local-shadows-local";
    }
    return "local-shadows-local";
}

const char* methodCallResolutionKindName(MethodCallResolutionKind kind) noexcept
{
    switch (kind)
    {
        case MethodCallResolutionKind::Resolved: return "resolved";
        case MethodCallResolutionKind::NoViableCandidate: return "no-viable-candidate";
        case MethodCallResolutionKind::Ambiguous: return "ambiguous";
    }
    return "no-viable-candidate";
}

const char* methodRuntimeTypeOperationKindName(MethodRuntimeTypeOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodRuntimeTypeOperationKind::TypeTest: return "is";
        case MethodRuntimeTypeOperationKind::SafeCast: return "as";
        case MethodRuntimeTypeOperationKind::CheckedCast: return "checked-cast";
    }
    return "is";
}

const char* methodExceptionOperationKindName(MethodExceptionOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodExceptionOperationKind::Catch: return "catch";
        case MethodExceptionOperationKind::Throw: return "throw";
        case MethodExceptionOperationKind::Rethrow: return "rethrow";
    }
    return "throw";
}

const char* methodCleanupRegionKindName(MethodCleanupRegionKind kind) noexcept
{
    switch (kind)
    {
        case MethodCleanupRegionKind::Finally: return "finally";
        case MethodCleanupRegionKind::UsingDispose: return "using-dispose";
        case MethodCleanupRegionKind::RaiiDestroy: return "raii-destroy";
        case MethodCleanupRegionKind::OwnedParameterDestroy: return "owned-parameter-destroy";
        case MethodCleanupRegionKind::ValueLocalDrop: return "value-local-drop";
        case MethodCleanupRegionKind::ValueParameterDrop: return "value-parameter-drop";
    }
    return "finally";
}

const char* methodStructuredExitKindName(MethodStructuredExitKind kind) noexcept
{
    switch (kind)
    {
        case MethodStructuredExitKind::Return: return "return";
        case MethodStructuredExitKind::Break: return "break";
        case MethodStructuredExitKind::Continue: return "continue";
    }
    return "return";
}

const char* methodCleanupContinuationKindName(MethodCleanupContinuationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCleanupContinuationKind::Normal: return "normal";
        case MethodCleanupContinuationKind::Return: return "return";
        case MethodCleanupContinuationKind::Break: return "break";
        case MethodCleanupContinuationKind::Continue: return "continue";
        case MethodCleanupContinuationKind::Exception: return "exception";
    }
    return "normal";
}

const char* methodOwnershipTransferKindName(MethodOwnershipTransferKind kind) noexcept
{
    switch (kind)
    {
        case MethodOwnershipTransferKind::MoveLocalToOwnedParameter: return "move-local-to-owned-parameter";
        case MethodOwnershipTransferKind::MoveParameterToOwnedParameter: return "move-parameter-to-owned-parameter";
        case MethodOwnershipTransferKind::OwnedReturnToOwnedParameter: return "owned-return-to-owned-parameter";
        case MethodOwnershipTransferKind::MoveLocalToOwnedReturn: return "move-local-to-owned-return";
        case MethodOwnershipTransferKind::MoveParameterToOwnedReturn: return "move-parameter-to-owned-return";
        case MethodOwnershipTransferKind::OwnedReturnForward: return "owned-return-forward";
    }
    return "unknown";
}

const char* methodLambdaCaptureModeName(language::CaptureMode mode) noexcept
{
    switch (mode)
    {
        case language::CaptureMode::Copy: return "copy";
        case language::CaptureMode::Move: return "move";
        case language::CaptureMode::Reference: return "ref";
        case language::CaptureMode::MutableReference: return "ref-mut";
    }
    return "unknown";
}

const char* methodLambdaCallableTraitName(language::CallableTrait trait) noexcept
{
    switch (trait)
    {
        case language::CallableTrait::Fn: return "Fn";
        case language::CallableTrait::FnMut: return "FnMut";
        case language::CallableTrait::FnOnce: return "FnOnce";
    }
    return "unknown";
}

const char* methodBorrowKindName(MethodBorrowKind kind) noexcept
{
    switch (kind)
    {
        case MethodBorrowKind::OwnerLocalToBorrowedParameter: return "owner-local-to-borrowed-parameter";
        case MethodBorrowKind::OwnedParameterToBorrowedParameter: return "owned-parameter-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedParameterToBorrowedParameter: return "borrowed-parameter-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedLocalToBorrowedParameter: return "borrowed-local-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedReturnToBorrowedParameter: return "borrowed-return-to-borrowed-parameter";
        case MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter:
            return "fresh-owned-temporary-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedParameterToBorrowedReturn: return "borrowed-parameter-to-borrowed-return";
        case MethodBorrowKind::BorrowedLocalToBorrowedReturn: return "borrowed-local-to-borrowed-return";
        case MethodBorrowKind::BorrowedReturnForward: return "borrowed-return-forward";
    }
    return "unknown";
}

const char* methodLocalBorrowSourceKindName(MethodLocalBorrowSourceKind kind) noexcept
{
    switch (kind)
    {
        case MethodLocalBorrowSourceKind::OwnerLocal: return "owner-local";
        case MethodLocalBorrowSourceKind::OwnedParameter: return "owned-parameter";
        case MethodLocalBorrowSourceKind::BorrowedParameter: return "borrowed-parameter";
        case MethodLocalBorrowSourceKind::BorrowedLocal: return "borrowed-local";
    }
    return "unknown";
}

const char* methodBorrowStateName(MethodBorrowState state) noexcept
{
    switch (state)
    {
        case MethodBorrowState::Valid: return "valid";
        case MethodBorrowState::Invalidated: return "invalidated";
        case MethodBorrowState::MaybeInvalid: return "maybe-invalid";
    }
    return "unknown";
}

namespace
{
// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart01(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::ArrayAllocation:
            return "array-allocation";
        case MethodCoreOperationKind::RangeArrayAllocation:
            return "range-array-allocation";
        case MethodCoreOperationKind::ArrayLength:
            return "array-length";
        case MethodCoreOperationKind::FixedArrayLength:
            return "fixed-array-length";
        case MethodCoreOperationKind::ArrayElement:
            return "array-element";
        case MethodCoreOperationKind::RangeArrayElement:
            return "range-array-element";
        case MethodCoreOperationKind::NullConditionalArrayElement:
            return "null-conditional-array-element";
        case MethodCoreOperationKind::RawElement:
            return "raw-element";
        case MethodCoreOperationKind::StringLiteral:
            return "string-literal";
        case MethodCoreOperationKind::Utf8StringLiteral:
            return "utf8-string-literal";
        case MethodCoreOperationKind::StringLength:
            return "string-length";
        case MethodCoreOperationKind::StringElement:
            return "string-element";
        case MethodCoreOperationKind::StringConcat:
            return "string-concat";
        case MethodCoreOperationKind::StringEquals:
            return "string-equals";
        case MethodCoreOperationKind::StringSubstring:
            return "string-substring";
        case MethodCoreOperationKind::StringCharacterPointer:
            return "string-character-pointer";
        case MethodCoreOperationKind::StringFromCharArray:
            return "string-from-char-array";
        case MethodCoreOperationKind::StringViewFromString:
            return "string-view-from-string";
        case MethodCoreOperationKind::SpanFromArray:
            return "span-from-array";
        case MethodCoreOperationKind::ReadOnlySpanFromArray:
            return "readonly-span-from-array";
        case MethodCoreOperationKind::SpanFromFixedArray:
            return "span-from-fixed-array";
        case MethodCoreOperationKind::ReadOnlySpanFromFixedArray:
            return "readonly-span-from-fixed-array";
        case MethodCoreOperationKind::ConsoleWriteLine:
            return "console-writeline";
        default:
            break;
    }
    return nullptr;
}

// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart02(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::ConsoleWrite:
            return "console-write";
        case MethodCoreOperationKind::ConsoleErrorWriteLine:
            return "console-error-writeline";
        case MethodCoreOperationKind::ConsoleErrorWrite:
            return "console-error-write";
        case MethodCoreOperationKind::ConsoleReadLine:
            return "console-readline";
        case MethodCoreOperationKind::StructToString:
            return "struct-to-string";
        case MethodCoreOperationKind::EnumToString:
            return "enum-to-string";
        case MethodCoreOperationKind::StructEquals:
            return "struct-equals";
        case MethodCoreOperationKind::StructHashCode:
            return "struct-hash-code";
        case MethodCoreOperationKind::StructClone:
            return "struct-clone";
        case MethodCoreOperationKind::ObjectTypeName:
            return "object-type-name";
        case MethodCoreOperationKind::ObjectReferenceHash:
            return "object-reference-hash";
        case MethodCoreOperationKind::SizeofConstant:
            return "sizeof-constant";
        case MethodCoreOperationKind::AlignofConstant:
            return "alignof-constant";
        case MethodCoreOperationKind::OffsetofConstant:
            return "offsetof-constant";
        case MethodCoreOperationKind::StackAllocation:
            return "stack-allocation";
        case MethodCoreOperationKind::RawAllocation:
            return "raw-allocation";
        case MethodCoreOperationKind::PointerArithmetic:
            return "pointer-arithmetic";
        case MethodCoreOperationKind::ClockQueryCounter:
            return "clock-query-counter";
        case MethodCoreOperationKind::ClockQueryFrequency:
            return "clock-query-frequency";
        case MethodCoreOperationKind::ClockQuerySystemFileTime:
            return "clock-query-system-filetime";
        case MethodCoreOperationKind::EnvironmentTickCount64:
            return "environment-tick-count64";
        case MethodCoreOperationKind::EnvironmentProcessorCount:
            return "environment-processor-count";
        case MethodCoreOperationKind::EnvironmentExit:
            return "environment-exit";
        default:
            break;
    }
    return nullptr;
}

// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart03(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::EnvironmentGetVariable:
            return "environment-get-variable";
        case MethodCoreOperationKind::EnvironmentSetVariable:
            return "environment-set-variable";
        case MethodCoreOperationKind::FileExists:
            return "file-exists";
        case MethodCoreOperationKind::FileDelete:
            return "file-delete";
        case MethodCoreOperationKind::DirectoryExists:
            return "directory-exists";
        case MethodCoreOperationKind::DirectoryCreate:
            return "directory-create";
        case MethodCoreOperationKind::FileReadAllText:
            return "file-read-all-text";
        case MethodCoreOperationKind::FileGetSize:
            return "file-get-size";
        case MethodCoreOperationKind::FileReadAllBytes:
            return "file-read-all-bytes";
        case MethodCoreOperationKind::FileReadBytes:
            return "file-read-bytes";
        case MethodCoreOperationKind::FileWriteAllBytes:
            return "file-write-all-bytes";
        case MethodCoreOperationKind::EnvironmentCurrentDirectory:
            return "environment-current-directory";
        case MethodCoreOperationKind::ProcessRun:
            return "process-run";
        case MethodCoreOperationKind::Assert:
            return "assert";
        case MethodCoreOperationKind::Unreachable:
            return "unreachable";
        case MethodCoreOperationKind::Assume:
            return "assume";
        case MethodCoreOperationKind::Panic:
            return "panic";
        case MethodCoreOperationKind::IteratorMoveNext:
            return "iterator-move-next";
        case MethodCoreOperationKind::IteratorCurrent:
            return "iterator-current";
        case MethodCoreOperationKind::IteratorGetEnumerator:
            return "iterator-get-enumerator";
        case MethodCoreOperationKind::IteratorDrop:
            return "iterator-drop";
        case MethodCoreOperationKind::IteratorMoveNextAsync:
            return "iterator-move-next-async";
        case MethodCoreOperationKind::IteratorCancel:
            return "iterator-cancel";
        default:
            break;
    }
    return nullptr;
}

// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart04(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::CollectionBuilder:
            return "collection-builder";
        case MethodCoreOperationKind::InterpolationHandler:
            return "interpolation-handler";
        case MethodCoreOperationKind::NullConditionalUserIndex:
            return "null-conditional-user-index";
        case MethodCoreOperationKind::FileWriteAllText:
            return "file-write-all-text";
        case MethodCoreOperationKind::FileAppendAllText:
            return "file-append-all-text";
        case MethodCoreOperationKind::DirectoryDelete:
            return "directory-delete";
        case MethodCoreOperationKind::PathGetFullPath:
            return "path-get-full-path";
        case MethodCoreOperationKind::EnvironmentGetCommandLineArgCount:
            return "environment-get-command-line-arg-count";
        case MethodCoreOperationKind::EnvironmentGetCommandLineArgs:
            return "environment-get-command-line-args";
        case MethodCoreOperationKind::DirectoryGetFileCount:
            return "directory-get-file-count";
        case MethodCoreOperationKind::DirectoryGetFiles:
            return "directory-get-files";
        case MethodCoreOperationKind::DirectoryGetDirectoryCount:
            return "directory-get-directory-count";
        case MethodCoreOperationKind::DirectoryGetDirectories:
            return "directory-get-directories";
        case MethodCoreOperationKind::ThreadCreate:
            return "thread-create";
        case MethodCoreOperationKind::ThreadJoin:
            return "thread-join";
        case MethodCoreOperationKind::ThreadDetach:
            return "thread-detach";
        case MethodCoreOperationKind::AtomicLoadInt32:
            return "atomic-load-int32";
        case MethodCoreOperationKind::AtomicStoreInt32:
            return "atomic-store-int32";
        case MethodCoreOperationKind::InterlockedIncrementInt32:
            return "interlocked-increment-int32";
        case MethodCoreOperationKind::InterlockedDecrementInt32:
            return "interlocked-decrement-int32";
        case MethodCoreOperationKind::InterlockedAddInt32:
            return "interlocked-add-int32";
        case MethodCoreOperationKind::InterlockedExchangeInt32:
            return "interlocked-exchange-int32";
        case MethodCoreOperationKind::InterlockedCompareExchangeInt32:
            return "interlocked-compare-exchange-int32";
        default:
            break;
    }
    return nullptr;
}

// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart05(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::AtomicLoadInt64:
            return "atomic-load-int64";
        case MethodCoreOperationKind::AtomicStoreInt64:
            return "atomic-store-int64";
        case MethodCoreOperationKind::InterlockedIncrementInt64:
            return "interlocked-increment-int64";
        case MethodCoreOperationKind::InterlockedDecrementInt64:
            return "interlocked-decrement-int64";
        case MethodCoreOperationKind::InterlockedAddInt64:
            return "interlocked-add-int64";
        case MethodCoreOperationKind::InterlockedExchangeInt64:
            return "interlocked-exchange-int64";
        case MethodCoreOperationKind::InterlockedCompareExchangeInt64:
            return "interlocked-compare-exchange-int64";
        case MethodCoreOperationKind::AtomicLoadPointer:
            return "atomic-load-pointer";
        case MethodCoreOperationKind::AtomicStorePointer:
            return "atomic-store-pointer";
        case MethodCoreOperationKind::InterlockedExchangePointer:
            return "interlocked-exchange-pointer";
        case MethodCoreOperationKind::InterlockedCompareExchangePointer:
            return "interlocked-compare-exchange-pointer";
        case MethodCoreOperationKind::AtomicThreadFence:
            return "atomic-thread-fence";
        case MethodCoreOperationKind::AtomicCompilerFence:
            return "atomic-compiler-fence";
        case MethodCoreOperationKind::SimdFloat32x4Add:
            return "simd-float32x4-add";
        case MethodCoreOperationKind::SimdFloat32x4Subtract:
            return "simd-float32x4-subtract";
        case MethodCoreOperationKind::SimdFloat32x4Multiply:
            return "simd-float32x4-multiply";
        case MethodCoreOperationKind::SimdInt128Add:
            return "simd-int128-add";
        case MethodCoreOperationKind::SimdInt128Subtract:
            return "simd-int128-subtract";
        case MethodCoreOperationKind::SimdInt128Multiply:
            return "simd-int128-multiply";
        case MethodCoreOperationKind::SimdFloat256Add:
            return "simd-float256-add";
        case MethodCoreOperationKind::SimdFloat256Subtract:
            return "simd-float256-subtract";
        case MethodCoreOperationKind::SimdFloat256Multiply:
            return "simd-float256-multiply";
        case MethodCoreOperationKind::SimdInt256Add:
            return "simd-int256-add";
        default:
            break;
    }
    return nullptr;
}

// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart06(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::SimdInt256Subtract:
            return "simd-int256-subtract";
        case MethodCoreOperationKind::SimdInt256Multiply:
            return "simd-int256-multiply";
        case MethodCoreOperationKind::SimdLoadAligned128:
            return "simd-load-aligned-128";
        case MethodCoreOperationKind::SimdStoreAligned128:
            return "simd-store-aligned-128";
        case MethodCoreOperationKind::SimdLoadUnaligned128:
            return "simd-load-unaligned-128";
        case MethodCoreOperationKind::SimdStoreUnaligned128:
            return "simd-store-unaligned-128";
        case MethodCoreOperationKind::HalfFromFloat:
            return "half-from-float";
        case MethodCoreOperationKind::HalfToFloat:
            return "half-to-float";
        case MethodCoreOperationKind::SimdSwizzleWzyx:
            return "simd-swizzle-wzyx";
        case MethodCoreOperationKind::SimdShuffleLowHigh:
            return "simd-shuffle-low-high";
        case MethodCoreOperationKind::SimdSqrt:
            return "simd-sqrt";
        case MethodCoreOperationKind::SimdReciprocalSqrt:
            return "simd-reciprocal-sqrt";
        case MethodCoreOperationKind::SimdFusedMultiplyAdd:
            return "simd-fused-multiply-add";
        case MethodCoreOperationKind::BitPopulationCount:
            return "bit-popcount";
        case MethodCoreOperationKind::BitLeadingZeroCount:
            return "bit-clz";
        case MethodCoreOperationKind::BitTrailingZeroCount:
            return "bit-ctz";
        case MethodCoreOperationKind::BitRotateLeft:
            return "bit-rotate-left";
        case MethodCoreOperationKind::BitRotateRight:
            return "bit-rotate-right";
        case MethodCoreOperationKind::BitByteSwap:
            return "bit-byte-swap";
        case MethodCoreOperationKind::CpuPrefetchRead:
            return "cpu-prefetch-read";
        case MethodCoreOperationKind::CpuPause:
            return "cpu-pause";
        case MethodCoreOperationKind::CpuReadTimestamp:
            return "cpu-read-timestamp";
        case MethodCoreOperationKind::MutexCreate:
            return "mutex-create";
        default:
            break;
    }
    return nullptr;
}

// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart07(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::MutexCreateNamed:
            return "mutex-create-named";
        case MethodCoreOperationKind::MutexLock:
            return "mutex-lock";
        case MethodCoreOperationKind::MutexUnlock:
            return "mutex-unlock";
        case MethodCoreOperationKind::MutexClose:
            return "mutex-close";
        case MethodCoreOperationKind::MonitorEnter:
            return "monitor-enter";
        case MethodCoreOperationKind::MonitorExit:
            return "monitor-exit";
        case MethodCoreOperationKind::MonitorWait:
            return "monitor-wait";
        case MethodCoreOperationKind::MonitorPulse:
            return "monitor-pulse";
        case MethodCoreOperationKind::MonitorPulseAll:
            return "monitor-pulse-all";
        case MethodCoreOperationKind::SemaphoreCreate:
            return "semaphore-create";
        case MethodCoreOperationKind::SemaphoreCreateNamed:
            return "semaphore-create-named";
        case MethodCoreOperationKind::SemaphoreWait:
            return "semaphore-wait";
        case MethodCoreOperationKind::SemaphoreRelease:
            return "semaphore-release";
        case MethodCoreOperationKind::SemaphoreClose:
            return "semaphore-close";
        case MethodCoreOperationKind::EventCreate:
            return "event-create";
        case MethodCoreOperationKind::EventCreateNamed:
            return "event-create-named";
        case MethodCoreOperationKind::EventSet:
            return "event-set";
        case MethodCoreOperationKind::EventReset:
            return "event-reset";
        case MethodCoreOperationKind::EventWait:
            return "event-wait";
        case MethodCoreOperationKind::EventClose:
            return "event-close";
        case MethodCoreOperationKind::ProcessGetCurrentProcessIdRaw:
            return "process-get-current-process-id";
        case MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw:
            return "process-get-working-set-size";
        case MethodCoreOperationKind::ProcessGetKernelTimeRaw:
            return "process-get-kernel-time";
        default:
            break;
    }
    return nullptr;
}

// ES: Busca el nombre estable en una partición acotada del catálogo de operaciones.
// EN: Looks up the stable name in one bounded partition of the operation catalog.
// Parámetro / Parameter: kind conserva el enumerador original sin transformarlo.
// Retorno / Return: nombre estable o nullptr; sin efectos; orden constante acotado.
const char* methodCoreOperationKindNamePart08(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::ProcessGetUserTimeRaw:
            return "process-get-user-time";
        case MethodCoreOperationKind::ProcessGetPriorityClassRaw:
            return "process-get-priority-class";
        case MethodCoreOperationKind::ProcessSetPriorityClassRaw:
            return "process-set-priority-class";
        case MethodCoreOperationKind::SystemInfoGetPageSizeRaw:
            return "system-info-get-page-size";
        case MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw:
            return "system-info-get-processor-architecture";
        case MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw:
            return "system-info-get-allocation-granularity";
        case MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw:
            return "system-info-get-total-physical-memory-bytes";
        case MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw:
            return "system-info-get-available-physical-memory-bytes";
        case MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw:
            return "system-info-get-memory-load-percent";
        case MethodCoreOperationKind::DebugIsDebuggerPresentRaw:
            return "debug-is-debugger-present";
        case MethodCoreOperationKind::DebugCaptureStackFrameCountRaw:
            return "debug-capture-stack-frame-count";
        case MethodCoreOperationKind::DebugOutputStringRaw:
            return "debug-output-string";
        case MethodCoreOperationKind::ThreadSleepRaw:
            return "thread-sleep";
        case MethodCoreOperationKind::FileCopyRaw:
            return "file-copy";
        case MethodCoreOperationKind::FileMoveRaw:
            return "file-move";
        case MethodCoreOperationKind::DirectoryMoveRaw:
            return "directory-move";
        default:
            break;
    }
    return nullptr;
}

}

const char* methodCoreOperationKindName(MethodCoreOperationKind kind) noexcept
{
    if (const char* name = methodCoreOperationKindNamePart01(kind)) return name;
    if (const char* name = methodCoreOperationKindNamePart02(kind)) return name;
    if (const char* name = methodCoreOperationKindNamePart03(kind)) return name;
    if (const char* name = methodCoreOperationKindNamePart04(kind)) return name;
    if (const char* name = methodCoreOperationKindNamePart05(kind)) return name;
    if (const char* name = methodCoreOperationKindNamePart06(kind)) return name;
    if (const char* name = methodCoreOperationKindNamePart07(kind)) return name;
    if (const char* name = methodCoreOperationKindNamePart08(kind)) return name;
    return "array-allocation";
}

const char* methodLambdaCaptureKindName(MethodLambdaCaptureKind kind) noexcept
{
    switch (kind)
    {
        case MethodLambdaCaptureKind::Local: return "local";
        case MethodLambdaCaptureKind::Parameter: return "parameter";
        case MethodLambdaCaptureKind::ThisReference: return "this";
    }
    return "unknown";
}

const char* methodLambdaCaptureSourceKindName(MethodLambdaCaptureSourceKind kind) noexcept
{
    switch (kind)
    {
        case MethodLambdaCaptureSourceKind::OriginalLocal: return "original-local";
        case MethodLambdaCaptureSourceKind::OriginalParameter: return "original-parameter";
        case MethodLambdaCaptureSourceKind::OriginalThis: return "original-this";
        case MethodLambdaCaptureSourceKind::ParentLocal: return "parent-local";
        case MethodLambdaCaptureSourceKind::ParentParameter: return "parent-parameter";
        case MethodLambdaCaptureSourceKind::ParentCapture: return "parent-capture";
    }
    return "unknown";
}

const char* methodDelegateOperationKindName(MethodDelegateOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodDelegateOperationKind::Create: return "create";
        case MethodDelegateOperationKind::Invoke: return "invoke";
    }
    return "create";
}

const char* methodEventOperationKindName(MethodEventOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodEventOperationKind::Add: return "add";
        case MethodEventOperationKind::Remove: return "remove";
        case MethodEventOperationKind::Invoke: return "invoke";
    }
    return "add";
}
}
