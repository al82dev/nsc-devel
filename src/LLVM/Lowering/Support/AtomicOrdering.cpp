#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/DebugMetadataVersion.h"

#include "../../LlvmCApi.h"
#include "../../SignedDivisionCorrection.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"
#include "nativesharp/LLVM/LlvmDebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmModuleMetadata.h"
#include "nativesharp/LLVM/LlvmRuntimeNames.h"
#include "nativesharp/LLVM/LlvmTypeClassification.h"
#include "nativesharp/LLVM/LlvmTypeLayout.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// SHR-022. Una sola funcion para las cuatro salidas de consola: el segundo parametro es una
// mascara de dos bits -- bit 0 anade el salto de linea, bit 1 escribe por error estandar.
// Stage 53.1 "Time" -- see src/Runtime/WindowsRuntimeBuilder.cpp for what these three wrap.
// Stage 53.2 "Environment" -- see src/Runtime/WindowsRuntimeBuilder.cpp for what these three wrap.
// Stage 53.2b "Environment (variables)" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
// Stage 53.3 "Filesystem/Paths" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
// Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
// Stage 53.4 "Filesystem/Paths (cierre)" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
// Stage 53.5 "Command-line arguments" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
// Stage 53.6 "Enumeracion de directorios" -- see src/Runtime/WindowsRuntimeBuilder.cpp. Four
// distinct exported symbols at this LLVM level (one per core operation, same discipline as
// File/Directory Exists/Delete above), even though the C++ side shares one pair of internal
// helper functions -- that sharing is a runtime-side implementation detail, invisible here.
// Stage 54.1 "Threads" -- see src/Runtime/WindowsRuntimeBuilder.cpp. ThreadCreate returns a real
// meaningful i64 (the HANDLE, as nint) -- like the four Directory operations above, it does NOT
// need the "dummy i1 result" pattern void-returning core operations use, because its result is
// genuinely used by NativeSharp code. ThreadJoin/ThreadDetach DO return void at the NativeSharp
// level, so they DO need that dummy-i1 pattern (same as FileDelete/DirectoryDelete above).
// Stage 54.2 "Atomics" -- see src/Runtime/WindowsRuntimeBuilder.cpp. All ten wrap a real Win32
// kernel32.dll Interlocked* export and return a real meaningful value (the new value for
// Increment/Decrement/Add, the old value for Exchange/CompareExchange, matching C#'s documented
// System.Threading.Interlocked contract) -- none of these need the "dummy i1 result" pattern,
// same as ThreadCreate above.
// Stage 54.3 "Mutex" -- see src/Runtime/WindowsRuntimeBuilder.cpp. MutexCreate returns a real
// meaningful i64 (the HANDLE, as nint), same as ThreadCreate above -- no dummy-i1 pattern needed.
// MutexLock/MutexUnlock/MutexClose return void at the NativeSharp level, so they DO need the
// dummy-i1 pattern, same as ThreadJoin/ThreadDetach.
// Stage 54.3 "Mutex" (fix). NOT tied to any MethodCoreOperationKind/NativeSharp source syntax --
// this is an internal implementation-detail hook the COMPILER's own generated code calls (the
// type-initializer/"cctor" guard's same-thread-recursion check, see defineTypeInitializers).
// Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll return void at the NativeSharp
// level (same dummy-i1 pattern as MutexLock/MutexUnlock/MutexClose above); Wait returns a REAL
// builtin:bool, no dummy wrapping needed.
// Stage 54.5 "Otras primitivas de sincronizacion". SemaphoreWait/EventWait share the SAME
// runtime hook (kWaitForHandleTimeoutFunctionName) -- see buildWaitForHandleTimeout's own header
// comment in WindowsRuntimeBuilder.cpp for why. Release/Close/Set/Reset all return void at the
// NativeSharp level (dummy-i1 pattern again); Create returns a real builtin:nint HANDLE.
// Stage 55.1 "Process". All five return builtin:long; SetPriorityClass returns void (dummy-i1
// pattern, same as every other void core operation in this file).
// Stage 55.2 "Hardware / System Info". All six return builtin:long.
// Stage 55.3 "Diagnostics". IsDebuggerPresent/CaptureStackFrameCount return builtin:long;
// OutputString returns void (dummy-i1 pattern).
// Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29). ThreadSleep returns void at the
// NativeSharp level (dummy-i1 pattern, one builtin:int argument). FileCopy returns a REAL
// builtin:bool (no dummy wrapping), three arguments (source ptr, dest ptr, overwrite i1).
// File.MoveRaw and Directory.MoveRaw share ONE runtime hook (kMoveFileFunctionName) -- Win32's
// MoveFileA works identically for files and directories, same "one wrapper, two MIR node kinds"
// precedent SemaphoreWait/EventWait established in Stage 54.5 (see buildMoveFile's own header
// comment in WindowsRuntimeBuilder.cpp) -- also a REAL builtin:bool return, two arguments.
std::optional<LLVMAtomicOrdering> orderedAtomicLoadOrdering(std::uint64_t encoded)
{
    if (encoded == 0U) return std::nullopt;
    const std::optional<language::MemoryOrder> order =
        language::decodeMemoryOrder(encoded - 1U);
    if (!order.has_value() ||
        !language::validMemoryOrder(language::AtomicOperation::Load, *order))
        return std::nullopt;
    switch (*order)
    {
        case language::MemoryOrder::Relaxed: return LLVMAtomicOrderingMonotonic;
        case language::MemoryOrder::Acquire: return LLVMAtomicOrderingAcquire;
        case language::MemoryOrder::SequentiallyConsistent:
            return LLVMAtomicOrderingSequentiallyConsistent;
        default: return std::nullopt;
    }
}

std::optional<LLVMAtomicOrdering> orderedAtomicStoreOrdering(std::uint64_t encoded)
{
    if (encoded == 0U) return std::nullopt;
    const std::optional<language::MemoryOrder> order =
        language::decodeMemoryOrder(encoded - 1U);
    if (!order.has_value() ||
        !language::validMemoryOrder(language::AtomicOperation::Store, *order))
        return std::nullopt;
    switch (*order)
    {
        case language::MemoryOrder::Relaxed: return LLVMAtomicOrderingMonotonic;
        case language::MemoryOrder::Release: return LLVMAtomicOrderingRelease;
        case language::MemoryOrder::SequentiallyConsistent:
            return LLVMAtomicOrderingSequentiallyConsistent;
        default: return std::nullopt;
    }
}

std::optional<LLVMAtomicOrdering> orderedAtomicRmwOrdering(std::uint64_t encoded)
{
    if (encoded == 0U) return std::nullopt;
    const std::optional<language::MemoryOrder> order =
        language::decodeMemoryOrder(encoded - 1U);
    if (!order.has_value() ||
        !language::validMemoryOrder(language::AtomicOperation::ReadModifyWrite, *order))
        return std::nullopt;
    switch (*order)
    {
        case language::MemoryOrder::Relaxed: return LLVMAtomicOrderingMonotonic;
        case language::MemoryOrder::Acquire: return LLVMAtomicOrderingAcquire;
        case language::MemoryOrder::Release: return LLVMAtomicOrderingRelease;
        case language::MemoryOrder::AcquireRelease: return LLVMAtomicOrderingAcquireRelease;
        case language::MemoryOrder::SequentiallyConsistent:
            return LLVMAtomicOrderingSequentiallyConsistent;
        default: return std::nullopt;
    }
}

LLVMValueRef buildOrderedAtomicRmw(
    LLVMBuilderRef builder, LLVMAtomicRMWBinOp operation, LLVMValueRef location,
    LLVMValueRef operand, LLVMAtomicOrdering ordering, unsigned alignment,
    bool returnNewValue, const char* resultName)
{
    LLVMValueRef oldValue =
        LLVMBuildAtomicRMW(builder, operation, location, operand, ordering, 0);
    LLVMSetAlignment(oldValue, alignment);
    return returnNewValue
        ? LLVMBuildAdd(builder, oldValue, operand, resultName)
        : oldValue;
}

LLVMValueRef buildOrderedAtomicCompareExchange(
    LLVMBuilderRef builder, LLVMValueRef location, LLVMValueRef value,
    LLVMValueRef comparand, LLVMAtomicOrdering successOrder,
    LLVMAtomicOrdering failureOrder, unsigned alignment, const char* resultName)
{
    LLVMValueRef pair = LLVMBuildAtomicCmpXchg(
        builder, location, comparand, value, successOrder, failureOrder, 0);
    LLVMSetAlignment(pair, alignment);
    return LLVMBuildExtractValue(builder, pair, 0U, resultName);
}

}
