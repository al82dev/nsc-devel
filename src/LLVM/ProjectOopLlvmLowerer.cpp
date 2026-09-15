#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "LlvmCApi.h"
#include "SignedDivisionCorrection.h"

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
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

namespace
{
constexpr const char* kTargetTriple = "x86_64-pc-windows-msvc";
constexpr const char* kAllocationFunctionName = "__nativesharp_alloc";
constexpr const char* kDeallocationFunctionName = "__nativesharp_free";
constexpr const char* kMemsetIntrinsicName = "llvm.memset.p0.i64";
constexpr const char* kMemcpyIntrinsicName = "llvm.memcpy.p0.p0.i64";
constexpr const char* kTrapFunctionName = "__nativesharp_trap";
// SHR-022. Una sola funcion para las cuatro salidas de consola: el segundo parametro es una
// mascara de dos bits -- bit 0 anade el salto de linea, bit 1 escribe por error estandar.
constexpr const char* kConsoleWriteLineFunctionName = "__nativesharp_console_write";
constexpr const char* kNativeThrowFunctionName = "__nativesharp_throw";
constexpr const char* kExceptionCurrentFunctionName = "__nativesharp_exception_current";
constexpr const char* kExceptionClearFunctionName = "__nativesharp_exception_clear";
constexpr const char* kEhFilterFunctionName = "__nativesharp_eh_filter";
constexpr const char* kSehPersonalityFunctionName = "__C_specific_handler";
// Stage 53.1 "Time" -- see src/Runtime/WindowsRuntimeBuilder.cpp for what these three wrap.
constexpr const char* kClockQueryCounterFunctionName = "__nativesharp_clock_query_counter";
constexpr const char* kClockQueryFrequencyFunctionName = "__nativesharp_clock_query_frequency";
constexpr const char* kClockQuerySystemFileTimeFunctionName = "__nativesharp_clock_query_system_filetime";
// Stage 53.2 "Environment" -- see src/Runtime/WindowsRuntimeBuilder.cpp for what these three wrap.
constexpr const char* kEnvironmentTickCount64FunctionName = "__nativesharp_environment_tick_count64";
constexpr const char* kEnvironmentProcessorCountFunctionName = "__nativesharp_environment_processor_count";
constexpr const char* kEnvironmentExitFunctionName = "__nativesharp_environment_exit";
// Stage 53.2b "Environment (variables)" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
constexpr const char* kEnvironmentGetVariableFunctionName = "__nativesharp_environment_get_variable";
constexpr const char* kEnvironmentSetVariableFunctionName = "__nativesharp_environment_set_variable";
// Stage 53.3 "Filesystem/Paths" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
constexpr const char* kFileExistsFunctionName = "__nativesharp_file_exists";
constexpr const char* kFileDeleteFunctionName = "__nativesharp_file_delete";
constexpr const char* kDirectoryExistsFunctionName = "__nativesharp_directory_exists";
constexpr const char* kDirectoryCreateFunctionName = "__nativesharp_directory_create";
// Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
constexpr const char* kFileReadAllTextFunctionName = "__nativesharp_file_read_all_text";
constexpr const char* kFileGetSizeFunctionName = "__nativesharp_file_get_size";
constexpr const char* kFileReadAllBytesFunctionName = "__nativesharp_file_read_all_bytes";
constexpr const char* kFileReadBytesFunctionName = "__nativesharp_file_read_bytes";
constexpr const char* kFileWriteAllBytesFunctionName = "__nativesharp_file_write_all_bytes";
constexpr const char* kEnvironmentCurrentDirectoryFunctionName = "__nativesharp_environment_current_directory";
constexpr const char* kProcessRunFunctionName = "__nativesharp_process_run";
constexpr const char* kFileWriteAllTextFunctionName = "__nativesharp_file_write_all_text";
constexpr const char* kFileAppendAllTextFunctionName = "__nativesharp_file_append_all_text";
// Stage 53.4 "Filesystem/Paths (cierre)" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
constexpr const char* kDirectoryDeleteFunctionName = "__nativesharp_directory_delete";
constexpr const char* kPathGetFullPathFunctionName = "__nativesharp_path_get_full_path";
// Stage 53.5 "Command-line arguments" -- see src/Runtime/WindowsRuntimeBuilder.cpp.
constexpr const char* kEnvironmentGetCommandLineArgCountFunctionName = "__nativesharp_environment_get_command_line_arg_count";
constexpr const char* kEnvironmentGetCommandLineArgsFunctionName = "__nativesharp_environment_get_command_line_args";
// Stage 53.6 "Enumeracion de directorios" -- see src/Runtime/WindowsRuntimeBuilder.cpp. Four
// distinct exported symbols at this LLVM level (one per core operation, same discipline as
// File/Directory Exists/Delete above), even though the C++ side shares one pair of internal
// helper functions -- that sharing is a runtime-side implementation detail, invisible here.
constexpr const char* kDirectoryGetFileCountFunctionName = "__nativesharp_directory_get_file_count";
constexpr const char* kDirectoryGetFilesFunctionName = "__nativesharp_directory_get_files";
constexpr const char* kDirectoryGetDirectoryCountFunctionName = "__nativesharp_directory_get_directory_count";
constexpr const char* kDirectoryGetDirectoriesFunctionName = "__nativesharp_directory_get_directories";
// Stage 54.1 "Threads" -- see src/Runtime/WindowsRuntimeBuilder.cpp. ThreadCreate returns a real
// meaningful i64 (the HANDLE, as nint) -- like the four Directory operations above, it does NOT
// need the "dummy i1 result" pattern void-returning core operations use, because its result is
// genuinely used by NativeSharp code. ThreadJoin/ThreadDetach DO return void at the NativeSharp
// level, so they DO need that dummy-i1 pattern (same as FileDelete/DirectoryDelete above).
constexpr const char* kThreadCreateFunctionName = "__nativesharp_thread_create";
constexpr const char* kThreadJoinFunctionName = "__nativesharp_thread_join";
constexpr const char* kThreadDetachFunctionName = "__nativesharp_thread_detach";
// Stage 54.2 "Atomics" -- see src/Runtime/WindowsRuntimeBuilder.cpp. All ten wrap a real Win32
// kernel32.dll Interlocked* export and return a real meaningful value (the new value for
// Increment/Decrement/Add, the old value for Exchange/CompareExchange, matching C#'s documented
// System.Threading.Interlocked contract) -- none of these need the "dummy i1 result" pattern,
// same as ThreadCreate above.
constexpr const char* kAtomicLoadInt32FunctionName = "__nativesharp_atomic_load_int32";
constexpr const char* kAtomicStoreInt32FunctionName = "__nativesharp_atomic_store_int32";
constexpr const char* kAtomicLoadInt64FunctionName = "__nativesharp_atomic_load_int64";
constexpr const char* kAtomicStoreInt64FunctionName = "__nativesharp_atomic_store_int64";
std::optional<LLVMAtomicOrdering> orderedAtomicLoadOrdering(std::uint64_t encoded)
{
    if (encoded == 0U) return std::nullopt;
    switch (encoded - 1U)
    {
        case 0U: return LLVMAtomicOrderingMonotonic;
        case 1U: return LLVMAtomicOrderingAcquire;
        case 4U: return LLVMAtomicOrderingSequentiallyConsistent;
        default: return std::nullopt;
    }
}

std::optional<LLVMAtomicOrdering> orderedAtomicStoreOrdering(std::uint64_t encoded)
{
    if (encoded == 0U) return std::nullopt;
    switch (encoded - 1U)
    {
        case 0U: return LLVMAtomicOrderingMonotonic;
        case 2U: return LLVMAtomicOrderingRelease;
        case 4U: return LLVMAtomicOrderingSequentiallyConsistent;
        default: return std::nullopt;
    }
}

std::optional<LLVMAtomicOrdering> orderedAtomicRmwOrdering(std::uint64_t encoded)
{
    if (encoded == 0U) return std::nullopt;
    switch (encoded - 1U)
    {
        case 0U: return LLVMAtomicOrderingMonotonic;
        case 1U: return LLVMAtomicOrderingAcquire;
        case 2U: return LLVMAtomicOrderingRelease;
        case 3U: return LLVMAtomicOrderingAcquireRelease;
        case 4U: return LLVMAtomicOrderingSequentiallyConsistent;
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

constexpr const char* kInterlockedIncrementInt32FunctionName = "__nativesharp_interlocked_increment_int32";
constexpr const char* kInterlockedDecrementInt32FunctionName = "__nativesharp_interlocked_decrement_int32";
constexpr const char* kInterlockedAddInt32FunctionName = "__nativesharp_interlocked_add_int32";
constexpr const char* kInterlockedExchangeInt32FunctionName = "__nativesharp_interlocked_exchange_int32";
constexpr const char* kInterlockedCompareExchangeInt32FunctionName = "__nativesharp_interlocked_compare_exchange_int32";
constexpr const char* kInterlockedIncrementInt64FunctionName = "__nativesharp_interlocked_increment_int64";
constexpr const char* kInterlockedDecrementInt64FunctionName = "__nativesharp_interlocked_decrement_int64";
constexpr const char* kInterlockedAddInt64FunctionName = "__nativesharp_interlocked_add_int64";
constexpr const char* kInterlockedExchangeInt64FunctionName = "__nativesharp_interlocked_exchange_int64";
constexpr const char* kInterlockedCompareExchangeInt64FunctionName = "__nativesharp_interlocked_compare_exchange_int64";
// Stage 54.3 "Mutex" -- see src/Runtime/WindowsRuntimeBuilder.cpp. MutexCreate returns a real
// meaningful i64 (the HANDLE, as nint), same as ThreadCreate above -- no dummy-i1 pattern needed.
// MutexLock/MutexUnlock/MutexClose return void at the NativeSharp level, so they DO need the
// dummy-i1 pattern, same as ThreadJoin/ThreadDetach.
constexpr const char* kMutexCreateFunctionName = "__nativesharp_mutex_create";
constexpr const char* kMutexLockFunctionName = "__nativesharp_mutex_lock";
constexpr const char* kMutexUnlockFunctionName = "__nativesharp_mutex_unlock";
constexpr const char* kMutexCloseFunctionName = "__nativesharp_mutex_close";
// Stage 54.3 "Mutex" (fix). NOT tied to any MethodCoreOperationKind/NativeSharp source syntax --
// this is an internal implementation-detail hook the COMPILER's own generated code calls (the
// type-initializer/"cctor" guard's same-thread-recursion check, see defineTypeInitializers).
constexpr const char* kGetCurrentThreadIdFunctionName = "__nativesharp_get_current_thread_id";
// Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll return void at the NativeSharp
// level (same dummy-i1 pattern as MutexLock/MutexUnlock/MutexClose above); Wait returns a REAL
// builtin:bool, no dummy wrapping needed.
constexpr const char* kMonitorEnterFunctionName = "__nativesharp_monitor_enter";
constexpr const char* kMonitorExitFunctionName = "__nativesharp_monitor_exit";
constexpr const char* kMonitorPulseFunctionName = "__nativesharp_monitor_pulse";
constexpr const char* kMonitorPulseAllFunctionName = "__nativesharp_monitor_pulse_all";
constexpr const char* kMonitorWaitFunctionName = "__nativesharp_monitor_wait";
// Stage 54.5 "Otras primitivas de sincronizacion". SemaphoreWait/EventWait share the SAME
// runtime hook (kWaitForHandleTimeoutFunctionName) -- see buildWaitForHandleTimeout's own header
// comment in WindowsRuntimeBuilder.cpp for why. Release/Close/Set/Reset all return void at the
// NativeSharp level (dummy-i1 pattern again); Create returns a real builtin:nint HANDLE.
constexpr const char* kSemaphoreCreateFunctionName = "__nativesharp_semaphore_create";
constexpr const char* kWaitForHandleTimeoutFunctionName = "__nativesharp_wait_for_handle_timeout";
constexpr const char* kSemaphoreReleaseFunctionName = "__nativesharp_semaphore_release";
constexpr const char* kSemaphoreCloseFunctionName = "__nativesharp_semaphore_close";
constexpr const char* kEventCreateFunctionName = "__nativesharp_event_create";
constexpr const char* kEventSetFunctionName = "__nativesharp_event_set";
constexpr const char* kEventResetFunctionName = "__nativesharp_event_reset";
constexpr const char* kEventCloseFunctionName = "__nativesharp_event_close";

// Stage 55.1 "Process". All five return builtin:long; SetPriorityClass returns void (dummy-i1
// pattern, same as every other void core operation in this file).
constexpr const char* kProcessGetCurrentProcessIdFunctionName = "__nativesharp_process_get_current_process_id";
constexpr const char* kProcessGetWorkingSetSizeFunctionName = "__nativesharp_process_get_working_set_size";
constexpr const char* kProcessGetKernelTimeFunctionName = "__nativesharp_process_get_kernel_time";
constexpr const char* kProcessGetUserTimeFunctionName = "__nativesharp_process_get_user_time";
constexpr const char* kProcessGetPriorityClassFunctionName = "__nativesharp_process_get_priority_class";
constexpr const char* kProcessSetPriorityClassFunctionName = "__nativesharp_process_set_priority_class";
// Stage 55.2 "Hardware / System Info". All six return builtin:long.
constexpr const char* kSystemInfoGetPageSizeFunctionName = "__nativesharp_system_info_get_page_size";
constexpr const char* kSystemInfoGetProcessorArchitectureFunctionName = "__nativesharp_system_info_get_processor_architecture";
constexpr const char* kSystemInfoGetAllocationGranularityFunctionName = "__nativesharp_system_info_get_allocation_granularity";
constexpr const char* kSystemInfoGetTotalPhysicalMemoryBytesFunctionName = "__nativesharp_system_info_get_total_physical_memory_bytes";
constexpr const char* kSystemInfoGetAvailablePhysicalMemoryBytesFunctionName = "__nativesharp_system_info_get_available_physical_memory_bytes";
constexpr const char* kSystemInfoGetMemoryLoadPercentFunctionName = "__nativesharp_system_info_get_memory_load_percent";
// Stage 55.3 "Diagnostics". IsDebuggerPresent/CaptureStackFrameCount return builtin:long;
// OutputString returns void (dummy-i1 pattern).
constexpr const char* kDebugIsDebuggerPresentFunctionName = "__nativesharp_debug_is_debugger_present";
constexpr const char* kDebugCaptureStackFrameCountFunctionName = "__nativesharp_debug_capture_stack_frame_count";
constexpr const char* kDebugOutputStringFunctionName = "__nativesharp_debug_output_string";

// Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29). ThreadSleep returns void at the
// NativeSharp level (dummy-i1 pattern, one builtin:int argument). FileCopy returns a REAL
// builtin:bool (no dummy wrapping), three arguments (source ptr, dest ptr, overwrite i1).
// File.MoveRaw and Directory.MoveRaw share ONE runtime hook (kMoveFileFunctionName) -- Win32's
// MoveFileA works identically for files and directories, same "one wrapper, two MIR node kinds"
// precedent SemaphoreWait/EventWait established in Stage 54.5 (see buildMoveFile's own header
// comment in WindowsRuntimeBuilder.cpp) -- also a REAL builtin:bool return, two arguments.
constexpr const char* kThreadSleepFunctionName = "__nativesharp_thread_sleep";
constexpr const char* kFileCopyFunctionName = "__nativesharp_file_copy";
constexpr const char* kMoveFileFunctionName = "__nativesharp_move_file";

bool isFloating(std::string_view type) noexcept
{
    return type == "builtin:float" || type == "builtin:double";
}

// LA TERCERA Y LA CUARTA COPIA de la tabla de tipos enteros, encontradas al arreglar la division
// el 2026-09-03 y quitadas en el mismo sitio. Estas dos funciones repetian los anchos y los
// signos que ya estaban en include/nativesharp/Frontend/BuiltinIntegerTypes.h -- y esta copia era
// la mas peligrosa de todas, porque es la que decide sdiv contra udiv, ashr contra lshr y sext
// contra zext. Un desacuerdo aqui no da un error: da CODIGO GENERADO QUE HACE OTRA COSA.
//
// ID 89 sigue siendo el motivo de que `sbyte` este en la tabla como el unico entero de 8 bits con
// signo; ahora esa decision se lee de un solo sitio en vez de estar escrita cuatro veces.
bool isSignedInteger(std::string_view type) noexcept
{
    const frontend::builtins::IntegerTypeInfo* info = frontend::builtins::findIntegerType(type);
    return info != nullptr && info->isSigned;
}


unsigned integerWidth(std::string_view type) noexcept
{
    // `bool` no es un entero incorporado a efectos de promocion ni de plegado -- la tabla lo deja
    // fuera a proposito y lo fija con un static_assert-- pero SI tiene un ancho LLVM: i1.
    if (type == "builtin:bool") return 1;
    const frontend::builtins::IntegerTypeInfo* info = frontend::builtins::findIntegerType(type);
    return info != nullptr ? info->width : 0;
}

bool isComparison(lexer::TokenKind kind) noexcept
{
    return kind == lexer::TokenKind::EqualEqual || kind == lexer::TokenKind::BangEqual ||
        kind == lexer::TokenKind::Less || kind == lexer::TokenKind::LessEqual ||
        kind == lexer::TokenKind::Greater || kind == lexer::TokenKind::GreaterEqual;
}

LLVMIntPredicate integerPredicate(lexer::TokenKind kind, bool signedOperands)
{
    switch (kind)
    {
        case lexer::TokenKind::EqualEqual: return LLVMIntEQ;
        case lexer::TokenKind::BangEqual: return LLVMIntNE;
        case lexer::TokenKind::Less: return signedOperands ? LLVMIntSLT : LLVMIntULT;
        case lexer::TokenKind::LessEqual: return signedOperands ? LLVMIntSLE : LLVMIntULE;
        case lexer::TokenKind::Greater: return signedOperands ? LLVMIntSGT : LLVMIntUGT;
        case lexer::TokenKind::GreaterEqual: return signedOperands ? LLVMIntSGE : LLVMIntUGE;
        default: return LLVMIntEQ;
    }
}

LLVMRealPredicate floatingPredicate(lexer::TokenKind kind)
{
    switch (kind)
    {
        case lexer::TokenKind::EqualEqual: return LLVMRealOEQ;
        case lexer::TokenKind::BangEqual: return LLVMRealUNE;
        case lexer::TokenKind::Less: return LLVMRealOLT;
        case lexer::TokenKind::LessEqual: return LLVMRealOLE;
        case lexer::TokenKind::Greater: return LLVMRealOGT;
        case lexer::TokenKind::GreaterEqual: return LLVMRealOGE;
        default: return LLVMRealOEQ;
    }
}

std::string portablePath(const std::filesystem::path& path)
{
    const std::u8string encoded = path.lexically_normal().generic_u8string();
    std::string result(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    std::replace(result.begin(), result.end(), '\\', '/');
    if (result.size() >= 2 && result[1] == ':' && result[0] >= 'a' && result[0] <= 'z')
        result[0] = static_cast<char>(result[0] - 'a' + 'A');
    while (result.size() > 1 && result.back() == '/' &&
           !(result.size() == 3 && result[1] == ':'))
        result.pop_back();
    return result;
}

std::string foldedWindowsPath(std::string path)
{
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char value) {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a')
                                            : static_cast<char>(value);
    });
    return path;
}

bool pathHasPrefix(std::string_view path, std::string_view prefix)
{
    if (!path.starts_with(prefix)) return false;
    if (path.size() == prefix.size() || prefix.ends_with('/')) return true;
    return path[prefix.size()] == '/';
}

std::string remapDebugSourcePath(
    const std::filesystem::path& sourcePath,
    std::span<const DebugPrefixMap> mappings)
{
    std::error_code filesystemError;
    std::filesystem::path absolute = std::filesystem::absolute(sourcePath, filesystemError);
    if (filesystemError) absolute = sourcePath;
    const std::string source = portablePath(absolute);
    const std::string foldedSource = foldedWindowsPath(source);

    std::size_t selectedLength = 0;
    std::string selectedReplacement;
    for (const DebugPrefixMap& mapping : mappings)
    {
        const std::string prefix = portablePath(mapping.sourcePrefix);
        const std::string foldedPrefix = foldedWindowsPath(prefix);
        if (!pathHasPrefix(foldedSource, foldedPrefix) || prefix.size() < selectedLength) continue;
        selectedLength = prefix.size();
        selectedReplacement = portablePath(mapping.replacementPrefix);
    }
    if (selectedLength == 0) return source;

    const std::string_view suffix(source.data() + selectedLength, source.size() - selectedLength);
    if (selectedReplacement.empty())
        return !suffix.empty() && suffix.front() == '/' ? std::string(suffix.substr(1))
                                                        : std::string(suffix);
    if (!suffix.empty() && selectedReplacement.back() == '/' && suffix.front() == '/')
        selectedReplacement.pop_back();
    else if (!suffix.empty() && selectedReplacement.back() != '/' && suffix.front() != '/')
        selectedReplacement.push_back('/');
    selectedReplacement.append(suffix);
    return selectedReplacement;
}
}

class OopLlvmImplementation final
{
public:
    OopLlvmImplementation(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel* memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager* sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::span<const DebugPrefixMap> debugPrefixMaps = {},
        bool partitioned = false,
        bool ownsModuleGlobals = true,
        std::span<const std::size_t> functionOrdinals = {},
        std::string_view moduleName = "NativeSharp.Stage34.OOP")
        : nameLookup_(nameLookup),
          memberModel_(memberModel),
          mirModel_(mirModel),
          sourceManager_(sourceManager),
          debugInformation_(debugInformation),
          optimizationLevel_(optimizationLevel),
          debugPrefixMaps_(debugPrefixMaps.begin(), debugPrefixMaps.end()),
          partitioned_(partitioned),
          ownsModuleGlobals_(ownsModuleGlobals),
          functionDefinitions_(mirModel.functions().size(), false),
          moduleName_(moduleName)
    {
        for (const std::size_t ordinal : functionOrdinals)
        {
            if (ordinal < functionDefinitions_.size()) functionDefinitions_[ordinal] = true;
            else invalidFunctionOrdinal_ = ordinal;
        }
    }

    bool lower(std::string& error)
    {
        error.clear();
        if (invalidFunctionOrdinal_.has_value())
        {
            error = "LLVM code partition refers to invalid MIR function ordinal " +
                std::to_string(*invalidFunctionOrdinal_);
            return false;
        }
        context_ = LLVMContextCreate();
        module_ = LLVMModuleCreateWithNameInContext(moduleName_.c_str(), context_);
        builder_ = LLVMCreateBuilderInContext(context_);
        if (context_ == nullptr || module_ == nullptr || builder_ == nullptr)
        {
            error = "Stage 34 failed to create LLVM context/module/builder";
            dispose();
            return false;
        }
        LLVMSetTarget(module_, kTargetTriple);

        if (!initializeDebugInformation(error))
        {
            dispose();
            return false;
        }

        if (!declareRuntime(error) || !declareStaticFields(error, !partitioned_ || ownsModuleGlobals_) ||
            !declareFunctions(error) ||
            !declareTypeInitializers(error, !partitioned_ || ownsModuleGlobals_) ||
            !declareDestructionEntries(error) ||
            !emitDispatchMetadata(error, !partitioned_ || ownsModuleGlobals_) ||
            !defineRttiHelpers(error) || !defineTypedCatchFilters(error) ||
            ((!partitioned_ || ownsModuleGlobals_) &&
             (!defineTypeInitializers(error) || !defineStaticFieldAccessors(error))) ||
            !defineCallbackGuardThunks(error, !partitioned_ || ownsModuleGlobals_) ||
            !defineFunctions(error) ||
            ((!partitioned_ || ownsModuleGlobals_) &&
             (!defineCAbiWrappers(error) || !defineDestructionEntries(error))))
        {
            dispose();
            return false;
        }

        if (debugBuilder_ != nullptr)
        {
            LLVMDIBuilderFinalize(debugBuilder_);
            LLVMDisposeDIBuilder(debugBuilder_);
            debugBuilder_ = nullptr;
        }

        LLVMDisposeBuilder(builder_);
        builder_ = nullptr;
        char* verificationMessage = nullptr;
        const int broken = LLVMVerifyModule(module_, LLVMReturnStatusAction, &verificationMessage);
        if (broken != 0)
        {
            error = "Stage 34 LLVM verifier rejected OOP module";
            if (verificationMessage != nullptr)
            {
                error += ": ";
                error += verificationMessage;
            }
            if (verificationMessage != nullptr) LLVMDisposeMessage(verificationMessage);
            dispose();
            return false;
        }
        if (verificationMessage != nullptr) LLVMDisposeMessage(verificationMessage);
        return true;
    }

    LLVMContextRef releaseContext() noexcept
    {
        LLVMContextRef result = context_;
        context_ = nullptr;
        return result;
    }

    LLVMModuleRef releaseModule() noexcept
    {
        LLVMModuleRef result = module_;
        module_ = nullptr;
        return result;
    }

private:
    struct FunctionInfo final
    {
        LLVMValueRef function = nullptr;
        LLVMTypeRef functionType = nullptr;
        LLVMMetadataRef subprogram = nullptr;
    };

    enum class FuncletKind
    {
        Catch,
        Cleanup
    };

    // One frame per construct that `break` or `continue` can name. A loop pushes a frame whose two
    // targets are its own; a SWITCH pushes one whose break target is the switch end and whose
    // CONTINUE TARGET IS INHERITED from the enclosing loop, so `continue` inside a case continues
    // that loop. That inheritance is why the funclet depth is stored twice: the break target sits
    // at the depth of the construct that owns it, while the continue target sits at the depth of a
    // loop that may be several funclets further out.
    struct LoopTargets final
    {
        LLVMBasicBlockRef breakTarget = nullptr;
        LLVMBasicBlockRef continueTarget = nullptr;
        std::size_t funcletDepth = 0;
        std::size_t continueFuncletDepth = 0;
    };

    struct FuncletContext final
    {
        LLVMValueRef pad = nullptr;
        FuncletKind kind = FuncletKind::Catch;
        std::size_t cleanupDepthAtEntry = 0;
        LLVMValueRef caughtException = nullptr;
        LLVMBasicBlockRef unwindDestination = nullptr;
    };

    // CC1 EH-001. One entry per `catch` clause body currently being lowered, innermost last.
    // Deliberately NOT a field on FuncletContext: since the B-006/B-007 fix, clause bodies are
    // ordinary parent blocks and nothing ever pushes a FuncletKind::Catch entry onto funcletStack_
    // any more (the only push site builds Cleanup entries), so an obligation hung there would be
    // unreachable code that closes the finding on paper and measures nothing.
    struct ActiveCatchBody final
    {
        LLVMValueRef exception = nullptr;
        std::size_t loopDepthAtEntry = 0;
    };

    // A cleanup region (a RAII local's scope-exit, a `using`, an owned-parameter destroy, or a
    // `finally` body) can be reached by more than one exceptional unwind chain within the same
    // method -- for example a local declared before two sibling try blocks is unwound both by
    // "no catch in try #1 matched" and, independently, by "try #2 itself needs to unwind past
    // it". Before this cache existed, emitExceptionalCleanupChain() lowered that SAME MIR region
    // fresh for every chain that passed through it, producing two independent `cleanuppad within
    // none` instructions for one logical scope.
    //
    // ACCURACY NOTE (external audit item "comentarios contradictorios sobre la causa raíz",
    // 2026-08-29): this comment used to assert that the duplicate pads were "what
    // BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md's crash came from" -- i.e. that
    // this cache was the fix for bug B-001. That claim was never checked by running anything, and
    // it is FALSE on two counts, both established by real Wine execution: (1) the cache was already
    // in the built binary while B-001's access violation still reproduced identically, and (2)
    // disabling the cache outright (always lowering a fresh cleanuppad per chain) changes NOTHING
    // about the EH bugs known today -- it was measured against both the B-006 double-destruction
    // and the B-007 nested-catch repro, with byte-identical results. Emitting one pad per logical
    // scope is still the right thing to do (it is what the source's scope nesting actually means,
    // and duplicate "within none" pads for one scope are simply wrong IR), so the cache stays --
    // but as correctness hygiene, NOT as the fix for any known bug. B-001's actual fix is the
    // per-function mem2reg legalization in src/LLVM/LlvmOptimizer.cpp. Caching by ordinal makes sure
    // each cleanup region gets exactly one cleanuppad, reused by every chain that needs it,
    // matching its real (unique, static) position in the source's scope nesting. `parentPad` is
    // stored alongside the block because the same ordinal COULD legitimately need two different
    // funclet parents if it were ever reached from within two different enclosing funclets (a
    // cleanup call made from inside a catch handler's own body would need a different parent
    // than the same ordinal reached from top-level code) -- the cache is only reused when the
    // parent matches, so a genuinely different context still falls back to lowering fresh rather
    // than reusing an incompatible pad.
    struct CachedCleanup final
    {
        LLVMValueRef parentPad = nullptr;
        LLVMBasicBlockRef next = nullptr;
        LLVMBasicBlockRef block = nullptr;
    };

    struct ExceptionHandlerContext final
    {
        std::size_t regionOrdinal = 0;
        LLVMBasicBlockRef dispatchBlock = nullptr;
        std::size_t retainedCleanupDepth = 0;
        std::size_t funcletDepthAtEntry = 0;
    };

    void dispose() noexcept
    {
        if (debugBuilder_ != nullptr)
        {
            LLVMDisposeDIBuilder(debugBuilder_);
            debugBuilder_ = nullptr;
        }
        if (builder_ != nullptr)
        {
            LLVMDisposeBuilder(builder_);
            builder_ = nullptr;
        }
        if (module_ != nullptr)
        {
            LLVMDisposeModule(module_);
            module_ = nullptr;
        }
        if (context_ != nullptr)
        {
            LLVMContextDispose(context_);
            context_ = nullptr;
        }
    }

    class ScopedDebugLocation final
    {
    public:
        ScopedDebugLocation(LLVMBuilderRef builder, LLVMMetadataRef location)
            : builder_(builder)
        {
            if (builder_ == nullptr || location == nullptr) return;
            previous_ = LLVMGetCurrentDebugLocation2(builder_);
            LLVMSetCurrentDebugLocation2(builder_, location);
            active_ = true;
        }

        ~ScopedDebugLocation()
        {
            if (active_) LLVMSetCurrentDebugLocation2(builder_, previous_);
        }

        ScopedDebugLocation(const ScopedDebugLocation&) = delete;
        ScopedDebugLocation& operator=(const ScopedDebugLocation&) = delete;

    private:
        LLVMBuilderRef builder_ = nullptr;
        LLVMMetadataRef previous_ = nullptr;
        bool active_ = false;
    };

    class ScopedDebugScope final
    {
    public:
        ScopedDebugScope(LLVMMetadataRef& slot, LLVMMetadataRef scope)
            : slot_(slot), previous_(slot)
        {
            if (scope != nullptr) slot_ = scope;
        }

        ~ScopedDebugScope()
        {
            slot_ = previous_;
        }

        ScopedDebugScope(const ScopedDebugScope&) = delete;
        ScopedDebugScope& operator=(const ScopedDebugScope&) = delete;

    private:
        LLVMMetadataRef& slot_;
        LLVMMetadataRef previous_ = nullptr;
    };

    bool initializeDebugInformation(std::string& error)
    {
        if (debugInformation_ == DebugInformationKind::None) return true;
        if (sourceManager_ == nullptr || memberModel_ == nullptr)
            return fail(error, "debug information requires project members and a source manager");

        debugBuilder_ = LLVMCreateDIBuilder(module_);
        if (debugBuilder_ == nullptr)
            return fail(error, "failed to create the LLVM debug information builder");
        LLVMSetIsNewDbgInfoFormat(module_, 1);

        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMMetadataRef debugVersion = LLVMValueAsMetadata(
            LLVMConstInt(i32, LLVMDebugMetadataVersion(), 0));
        LLVMMetadataRef codeView = LLVMValueAsMetadata(LLVMConstInt(i32, 1, 0));
        LLVMAddModuleFlag(
            module_, LLVMModuleFlagBehaviorWarning, "Debug Info Version", 18, debugVersion);
        LLVMAddModuleFlag(
            module_, LLVMModuleFlagBehaviorWarning, "CodeView", 8, codeView);

        LLVMMetadataRef primaryFile = nullptr;
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            if (function.memberOrdinal >= memberModel_->members().size()) continue;
            const frontend::ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
            if (!member.range.isValid()) continue;
            primaryFile = debugFile(member.range.begin());
            if (primaryFile != nullptr) break;
        }
        if (primaryFile == nullptr)
        {
            static constexpr std::string_view unknown = "unknown.ns";
            primaryFile = LLVMDIBuilderCreateFile(
                debugBuilder_, unknown.data(), unknown.size(), "", 0);
        }
        if (primaryFile == nullptr)
            return fail(error, "failed to create the primary LLVM debug source file");

        static constexpr std::string_view producer = "NativeSharp nsc 1.0.1";
        const LLVMDWARFEmissionKind emission =
            debugInformation_ == DebugInformationKind::LineTablesOnly
            ? LLVMDWARFEmissionLineTablesOnly
            : LLVMDWARFEmissionFull;
        debugCompileUnit_ = LLVMDIBuilderCreateCompileUnit(
            debugBuilder_, LLVMDWARFSourceLanguageC_sharp, primaryFile,
            producer.data(), producer.size(),
            optimizationLevel_ == LlvmOptimizationLevel::O0 ? 0 : 1,
            "", 0, 0, "", 0, emission, 0, 0, 0, "", 0, "", 0);
        if (debugCompileUnit_ == nullptr)
            return fail(error, "failed to create the LLVM debug compile unit");
        return true;
    }

    LLVMMetadataRef debugFile(SourceLocation location)
    {
        if (debugBuilder_ == nullptr || sourceManager_ == nullptr || !location.isValid())
            return nullptr;
        const std::uint32_t fileKey = location.fileId().value();
        const auto existing = debugFiles_.find(fileKey);
        if (existing != debugFiles_.end()) return existing->second;
        const source::SourceFile* sourceFile = sourceManager_->getFile(location.fileId());
        if (sourceFile == nullptr) return nullptr;
        const std::string normalized = remapDebugSourcePath(sourceFile->path(), debugPrefixMaps_);
        const std::size_t separator = normalized.find_last_of('/');
        const std::string filename = separator == std::string::npos
            ? normalized
            : normalized.substr(separator + 1);
        const std::string directory = separator == std::string::npos
            ? std::string()
            : normalized.substr(0, separator == 0 ? 1 : separator);
        const std::string_view sourceBytes = sourceFile->buffer();
        const auto bytes = std::span(
            reinterpret_cast<const std::uint8_t*>(sourceBytes.data()), sourceBytes.size());
        const std::string checksum = basic::sha256Hex(basic::sha256(bytes));
        LLVMMetadataRef file = LLVMDIBuilderCreateFileWithChecksum(
            debugBuilder_, filename.data(), filename.size(), directory.data(), directory.size(),
            CSK_SHA256, checksum.data(), checksum.size(), nullptr, 0);
        if (file != nullptr)
        {
            debugFiles_.emplace(fileKey, file);
            // LLVM's CodeView streamer numbers files in the same first-use order in which the
            // lowering creates their DIFile records. Integrated assembly uses that stable number
            // in generated .cv_loc directives so every physical instruction line remains
            // step-able without splitting one asm contract into semantically different calls.
            debugCodeViewFileNumbers_.emplace(
                fileKey, static_cast<unsigned>(debugCodeViewFileNumbers_.size() + 1));
        }
        return file;
    }

    unsigned debugCodeViewFileNumber(SourceLocation location)
    {
        if (debugBuilder_ == nullptr || !location.isValid()) return 0;
        const std::uint32_t fileKey = location.fileId().value();
        if (debugFiles_.find(fileKey) == debugFiles_.end() && debugFile(location) == nullptr)
            return 0;
        const auto found = debugCodeViewFileNumbers_.find(fileKey);
        return found == debugCodeViewFileNumbers_.end() ? 0 : found->second;
    }

    LLVMMetadataRef createDebugSubprogram(const frontend::ProjectOopMirFunction& function)
    {
        if (debugBuilder_ == nullptr || function.isImportedC || function.isImportedNative)
            return nullptr;
        if (function.isSyntheticImplicitConstructor)
        {
            if (function.declaringTypeOrdinal >= nameLookup_.namedTypes().size()) return nullptr;
            const frontend::NamedTypeDeclaration& type =
                nameLookup_.namedTypes()[function.declaringTypeOrdinal];
            const std::vector<std::string> noParameters;
            return createSynthesizedDebugSubprogram(
                type.range,
                type.identity.fullyQualifiedName() + ".implicit-ctor",
                function.mangledName,
                "builtin:void",
                noParameters,
                false,
                false);
        }
        if (memberModel_ == nullptr || function.memberOrdinal >= memberModel_->members().size())
            return nullptr;
        const frontend::ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
        SourceRange range = member.range;
        if (function.isLambda && function.body != nullptr && function.body->syntax != nullptr &&
            function.body->syntax->range().isValid())
            range = function.body->syntax->range();
        if (!range.isValid()) return nullptr;
        LLVMMetadataRef file = debugFile(range.begin());
        if (file == nullptr) return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        std::string displayName =
            nameLookup_.namedTypes()[function.declaringTypeOrdinal].identity.fullyQualifiedName() +
            "." + member.name;
        if (function.isLambda)
            displayName += ".lambda#" + std::to_string(function.lambdaOrdinal.value_or(0));
        std::vector<LLVMMetadataRef> signatureTypes;
        signatureTypes.reserve(function.parameterTypeKeys.size() + 1);
        signatureTypes.push_back(debugType(
            function.referenceReturn ? function.returnTypeKey + "*" : function.returnTypeKey));
        for (const std::string& parameterType : function.parameterTypeKeys)
            signatureTypes.push_back(debugType(parameterType));
        LLVMMetadataRef functionType = LLVMDIBuilderCreateSubroutineType(
            debugBuilder_, file, signatureTypes.data(),
            static_cast<unsigned>(signatureTypes.size()), LLVMDIFlagZero);
        if (functionType == nullptr) return nullptr;
        return LLVMDIBuilderCreateFunction(
            debugBuilder_, file,
            displayName.data(), displayName.size(),
            function.mangledName.data(), function.mangledName.size(),
            file, position.line, functionType,
            function.isExportedC ? 0 : 1, 1, position.line,
            LLVMDIFlagZero,
            optimizationLevel_ == LlvmOptimizationLevel::O0 ? 0 : 1);
    }

    LLVMMetadataRef createSynthesizedDebugSubprogram(
        SourceRange range,
        std::string_view displayName,
        std::string_view linkageName,
        std::string_view returnTypeKey,
        const std::vector<std::string>& parameterTypeKeys,
        bool localToUnit,
        bool thunk)
    {
        if (debugBuilder_ == nullptr || sourceManager_ == nullptr)
            return nullptr;
        if (!range.isValid()) return nullptr;
        LLVMMetadataRef file = debugFile(range.begin());
        if (file == nullptr) return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        std::vector<LLVMMetadataRef> signatureTypes;
        signatureTypes.reserve(parameterTypeKeys.size() + 1);
        signatureTypes.push_back(debugType(returnTypeKey));
        for (const std::string& parameterType : parameterTypeKeys)
            signatureTypes.push_back(debugType(parameterType));
        LLVMMetadataRef functionType = LLVMDIBuilderCreateSubroutineType(
            debugBuilder_, file, signatureTypes.data(),
            static_cast<unsigned>(signatureTypes.size()), LLVMDIFlagZero);
        if (functionType == nullptr) return nullptr;
        LLVMDIFlags flags = LLVMDIFlagArtificial;
        if (thunk)
            flags = static_cast<LLVMDIFlags>(flags | LLVMDIFlagThunk);
        return LLVMDIBuilderCreateFunction(
            debugBuilder_, file,
            displayName.data(), displayName.size(),
            linkageName.data(), linkageName.size(),
            file, position.line, functionType,
            localToUnit ? 1 : 0, 1, position.line, flags,
            optimizationLevel_ == LlvmOptimizationLevel::O0 ? 0 : 1);
    }

    LLVMMetadataRef createGeneratedDebugSubprogram(
        const frontend::ProjectOopMirFunction& origin,
        std::string_view role,
        std::string_view linkageName,
        bool localToUnit)
    {
        if (debugBuilder_ == nullptr || memberModel_ == nullptr ||
            origin.memberOrdinal >= memberModel_->members().size() ||
            origin.declaringTypeOrdinal >= nameLookup_.namedTypes().size())
            return nullptr;
        const frontend::ClassMemberSymbol& member = memberModel_->members()[origin.memberOrdinal];
        SourceRange range = member.range;
        if (origin.isLambda && origin.body != nullptr && origin.body->syntax != nullptr &&
            origin.body->syntax->range().isValid())
            range = origin.body->syntax->range();
        std::string displayName =
            nameLookup_.namedTypes()[origin.declaringTypeOrdinal].identity.fullyQualifiedName() +
            "." + member.name + " [generated: " + std::string(role) + "]";
        return createSynthesizedDebugSubprogram(
            range, displayName, linkageName, origin.returnTypeKey,
            origin.parameterTypeKeys, localToUnit, true);
    }

    LLVMMetadataRef debugSubprogramLocation(SourceRange range, LLVMMetadataRef subprogram)
    {
        if (subprogram == nullptr || sourceManager_ == nullptr || !range.isValid())
            return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        return LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, subprogram, nullptr);
    }

    LLVMMetadataRef debugSubprogramLocation(
        const frontend::ProjectOopMirFunction& origin,
        LLVMMetadataRef subprogram)
    {
        if (subprogram == nullptr || sourceManager_ == nullptr || memberModel_ == nullptr ||
            origin.memberOrdinal >= memberModel_->members().size())
            return nullptr;
        SourceRange range = memberModel_->members()[origin.memberOrdinal].range;
        if (origin.isLambda && origin.body != nullptr && origin.body->syntax != nullptr &&
            origin.body->syntax->range().isValid())
            range = origin.body->syntax->range();
        return debugSubprogramLocation(range, subprogram);
    }

    LLVMMetadataRef debugLocation(const frontend::ProjectOopMirNode* node)
    {
        if (debugBuilder_ == nullptr || sourceManager_ == nullptr ||
            currentDebugSubprogram_ == nullptr || node == nullptr || node->syntax == nullptr)
            return nullptr;
        const SourceRange range = node->syntax->range();
        if (!range.isValid()) return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        LLVMMetadataRef scope = currentDebugScope_ != nullptr
            ? currentDebugScope_
            : currentDebugSubprogram_;
        const std::uint32_t fileKey = range.begin().fileId().value();
        if (fileKey != currentDebugFileKey_)
        {
            if (LLVMMetadataRef file = debugFile(range.begin()); file != nullptr)
            {
                scope = LLVMDIBuilderCreateLexicalBlockFile(
                    debugBuilder_, scope, file, 0);
            }
        }
        return LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, scope, nullptr);
    }

    const frontend::NamedTypeDeclaration* debugNamedType(std::string_view typeKey) const noexcept
    {
        if (!typeKey.starts_with("type:")) return nullptr;
        const std::string_view key = typeKey.substr(5);
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() == key) return &type;
        }
        return nullptr;
    }

    std::string debugNamedDisplayName(const frontend::NamedTypeDeclaration& named) const
    {
        constexpr std::string_view specializationPrefix = "stage44-specialization:";
        if (named.syntax == nullptr ||
            !named.syntax->auxiliaryText().starts_with(specializationPrefix))
            return named.identity.fullyQualifiedName();

        std::string display =
            named.syntax->auxiliaryText().substr(specializationPrefix.size());
        // A specialization key starts with the project/module identity.  CodeView already scopes
        // this compile unit to that module, so publish the NativeSharp namespace/type spelling.
        if (const std::size_t module = display.find("::"); module != std::string::npos)
            display.erase(0, module + 2);
        // `Box`1<int>` is the canonical arity-bearing key.  The arity is identity machinery, not
        // source syntax; the PDB display name is the closed spelling users wrote: `Box<int>`.
        for (std::size_t tick = display.find('`'); tick != std::string::npos;
             tick = display.find('`', tick))
        {
            std::size_t end = tick + 1;
            while (end < display.size() && display[end] >= '0' && display[end] <= '9') ++end;
            display.erase(tick, end - tick);
        }
        return display;
    }

    static LLVMDIFlags debugMemberFlags(frontend::ClassMemberAccessibility accessibility) noexcept
    {
        switch (accessibility)
        {
            case frontend::ClassMemberAccessibility::Private: return LLVMDIFlagPrivate;
            case frontend::ClassMemberAccessibility::Protected: return LLVMDIFlagProtected;
            case frontend::ClassMemberAccessibility::Public: return LLVMDIFlagPublic;
            case frontend::ClassMemberAccessibility::Internal: return LLVMDIFlagZero;
        }
        return LLVMDIFlagZero;
    }

    LLVMMetadataRef debugClassPointee(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named,
        const frontend::ProjectOopMirClassLayout& layout)
    {
        const std::string cacheKey(typeKey);
        if (const auto cached = debugPointeeTypes_.find(cacheKey);
            cached != debugPointeeTypes_.end())
            return cached->second;

        const std::string name = debugNamedDisplayName(named);
        LLVMMetadataRef file = debugFile(named.range.begin());
        const unsigned line = named.range.isValid()
            ? sourceManager_->getPosition(named.range.begin()).line
            : 0;
        const std::string uniqueId = "nativesharp:class:" + named.identity.key();
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagClassType, name.data(), name.size(),
            debugCompileUnit_, file, line, 0, layout.size * 8,
            static_cast<std::uint32_t>(layout.alignment * 8), LLVMDIFlagZero,
            uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return nullptr;
        debugPointeeTypes_[cacheKey] = temporary;

        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, temporary, 64, 64, 0, name.data(), name.size());
        if (pointer == nullptr) return nullptr;
        debugTypes_[cacheKey] = pointer;

        LLVMMetadataRef derivedFrom = nullptr;
        if (layout.baseTypeOrdinal.has_value() &&
            *layout.baseTypeOrdinal < nameLookup_.namedTypes().size())
        {
            const frontend::NamedTypeDeclaration& base =
                nameLookup_.namedTypes()[*layout.baseTypeOrdinal];
            const std::string baseKey = "type:" + base.identity.key();
            (void)debugType(baseKey);
            if (const auto found = debugPointeeTypes_.find(baseKey);
                found != debugPointeeTypes_.end())
                derivedFrom = found->second;
        }

        std::vector<LLVMMetadataRef> members;
        members.reserve(layout.fields.size() + (derivedFrom != nullptr ? 1U : 0U));
        if (derivedFrom != nullptr)
        {
            LLVMMetadataRef inheritance = LLVMDIBuilderCreateInheritance(
                debugBuilder_, temporary, derivedFrom, 0, 0, LLVMDIFlagPublic);
            if (inheritance != nullptr) members.push_back(inheritance);
        }
        for (const frontend::ObjectFieldLayout& field : layout.fields)
        {
            std::string fieldName = "field" + std::to_string(field.memberOrdinal);
            LLVMMetadataRef fieldFile = file;
            unsigned fieldLine = line;
            LLVMDIFlags fieldFlags = LLVMDIFlagZero;
            if (memberModel_ != nullptr && field.memberOrdinal < memberModel_->members().size())
            {
                const frontend::ClassMemberSymbol& member =
                    memberModel_->members()[field.memberOrdinal];
                fieldName = member.name;
                fieldFlags = debugMemberFlags(member.accessibility);
                if (member.range.isValid())
                {
                    fieldFile = debugFile(member.range.begin());
                    fieldLine = sourceManager_->getPosition(member.range.begin()).line;
                }
            }
            LLVMMetadataRef fieldType = debugType(field.typeKey);
            if (fieldType == nullptr)
                fieldType = LLVMDIBuilderCreateUnspecifiedType(
                    debugBuilder_, field.typeKey.data(), field.typeKey.size());
            LLVMMetadataRef member = LLVMDIBuilderCreateMemberType(
                debugBuilder_, temporary, fieldName.data(), fieldName.size(), fieldFile,
                fieldLine, field.size * 8,
                static_cast<std::uint32_t>(field.alignment * 8), field.offset * 8,
                fieldFlags, fieldType);
            if (member != nullptr) members.push_back(member);
        }

        LLVMMetadataRef complete = LLVMDIBuilderCreateClassType(
            debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
            layout.size * 8, static_cast<std::uint32_t>(layout.alignment * 8), 0,
            LLVMDIFlagZero, derivedFrom, members.data(),
            static_cast<unsigned>(members.size()), nullptr, nullptr,
            uniqueId.data(), uniqueId.size());
        if (complete == nullptr) return nullptr;
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        debugPointeeTypes_[cacheKey] = complete;
        return complete;
    }

    LLVMMetadataRef debugHeaderReference(
        std::string_view cacheKeyView,
        std::string_view displayName,
        std::string_view elementTypeKey)
    {
        const std::string cacheKey(cacheKeyView);
        const auto cached = debugTypes_.find(cacheKey);
        if (cached != debugTypes_.end()) return cached->second;

        const std::string uniqueId = "nativesharp:header:" + cacheKey;
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagStructureType, displayName.data(), displayName.size(),
            debugCompileUnit_, nullptr, 0, 0, 64, 64, LLVMDIFlagZero,
            uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return nullptr;
        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, temporary, 64, 64, 0, displayName.data(), displayName.size());
        if (pointer == nullptr) return nullptr;
        debugTypes_[cacheKey] = pointer;
        debugPointeeTypes_[cacheKey] = temporary;

        LLVMMetadataRef lengthType = debugType("builtin:long");
        LLVMMetadataRef elementType = debugType(elementTypeKey);
        if (elementType == nullptr)
            elementType = LLVMDIBuilderCreateUnspecifiedType(
                debugBuilder_, elementTypeKey.data(), elementTypeKey.size());
        LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(debugBuilder_, 0, 0);
        LLVMMetadataRef flexibleArray = LLVMDIBuilderCreateArrayType(
            debugBuilder_, 0, 8, elementType, &subrange, 1);
        LLVMMetadataRef members[] = {
            LLVMDIBuilderCreateMemberType(
                debugBuilder_, temporary, "Length", 6, nullptr, 0, 64, 64, 0,
                LLVMDIFlagPublic, lengthType),
            LLVMDIBuilderCreateMemberType(
                debugBuilder_, temporary, "Data", 4, nullptr, 0, 0, 8, 64,
                LLVMDIFlagPublic, flexibleArray)};
        LLVMMetadataRef complete = LLVMDIBuilderCreateStructType(
            debugBuilder_, debugCompileUnit_, displayName.data(), displayName.size(),
            nullptr, 0, 64, 64, LLVMDIFlagZero, nullptr, members, 2, 0, nullptr,
            uniqueId.data(), uniqueId.size());
        if (complete == nullptr) return nullptr;
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        debugPointeeTypes_[cacheKey] = complete;
        return pointer;
    }

    LLVMMetadataRef debugDelegateReference(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named)
    {
        const std::string cacheKey(typeKey);
        const auto cached = debugTypes_.find(cacheKey);
        if (cached != debugTypes_.end()) return cached->second;
        const std::string name = debugNamedDisplayName(named);
        const std::string cellName = name + ".delegate-cell";
        const std::string uniqueId = "nativesharp:delegate:" + named.identity.key();
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagStructureType, cellName.data(), cellName.size(),
            debugCompileUnit_, debugFile(named.range.begin()), 0, 0, 256, 64,
            LLVMDIFlagArtificial, uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return nullptr;
        LLVMMetadataRef pointerType = debugType("builtin:void*");
        LLVMMetadataRef fields[] = {
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "Code", 4, nullptr, 0,
                64, 64, 0, LLVMDIFlagPublic, pointerType),
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "Receiver", 8, nullptr, 0,
                64, 64, 64, LLVMDIFlagPublic, pointerType),
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "ContextDestroy", 14, nullptr, 0,
                64, 64, 128, LLVMDIFlagPublic, pointerType),
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "CellFree", 8, nullptr, 0,
                64, 64, 192, LLVMDIFlagPublic, pointerType)};
        LLVMMetadataRef complete = LLVMDIBuilderCreateStructType(
            debugBuilder_, debugCompileUnit_, cellName.data(), cellName.size(), nullptr, 0,
            256, 64, LLVMDIFlagArtificial, nullptr, fields, 4, 0, nullptr,
            uniqueId.data(), uniqueId.size());
        if (temporary == nullptr || complete == nullptr) return nullptr;
        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, complete, 64, 64, 0, name.data(), name.size());
        if (pointer == nullptr) return nullptr;
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        debugTypes_[cacheKey] = pointer;
        debugPointeeTypes_[cacheKey] = complete;
        return pointer;
    }

    LLVMMetadataRef debugType(std::string_view typeKey)
    {
        if (debugBuilder_ == nullptr || typeKey == "builtin:void") return nullptr;
        const std::string cacheKey(typeKey);
        const auto cached = debugTypes_.find(cacheKey);
        if (cached != debugTypes_.end()) return cached->second;

        const auto basic = [&](std::string_view name, std::uint64_t bits,
                               LLVMDWARFTypeEncoding encoding)
        {
            return LLVMDIBuilderCreateBasicType(
                debugBuilder_, name.data(), name.size(), bits, encoding, LLVMDIFlagZero);
        };
        LLVMMetadataRef type = nullptr;
        if (typeKey == "builtin:bool")
            type = basic("bool", 8, LLVMDWARFTypeEncodingBoolean);
        else if (typeKey == "builtin:sbyte")
            type = basic("sbyte", 8, LLVMDWARFTypeEncodingSignedChar);
        else if (typeKey == "builtin:byte")
            type = basic("byte", 8, LLVMDWARFTypeEncodingUnsignedChar);
        else if (typeKey == "builtin:char")
            type = basic("char", 8, LLVMDWARFTypeEncodingUnsignedChar);
        else if (typeKey == "builtin:short")
            type = basic("short", 16, LLVMDWARFTypeEncodingSigned);
        else if (typeKey == "builtin:ushort")
            type = basic("ushort", 16, LLVMDWARFTypeEncodingUnsigned);
        else if (typeKey == "builtin:int")
            type = basic("int", 32, LLVMDWARFTypeEncodingSigned);
        else if (typeKey == "builtin:uint")
            type = basic("uint", 32, LLVMDWARFTypeEncodingUnsigned);
        else if (typeKey == "builtin:long" || typeKey == "builtin:nint")
            type = basic(typeKey == "builtin:long" ? "long" : "nint", 64,
                         LLVMDWARFTypeEncodingSigned);
        else if (typeKey == "builtin:ulong" || typeKey == "builtin:nuint")
            type = basic(typeKey == "builtin:ulong" ? "ulong" : "nuint", 64,
                         LLVMDWARFTypeEncodingUnsigned);
        else if (typeKey == "builtin:float")
            type = basic("float", 32, LLVMDWARFTypeEncodingFloat);
        else if (typeKey == "builtin:double")
            type = basic("double", 64, LLVMDWARFTypeEncodingFloat);
        else if (!typeKey.empty() && typeKey.back() == '*')
        {
            const std::string_view pointeeKey = typeKey.substr(0, typeKey.size() - 1);
            LLVMMetadataRef pointee = pointeeKey == "builtin:void"
                ? LLVMDIBuilderCreateUnspecifiedType(debugBuilder_, "void", 4)
                : debugType(pointeeKey);
            const std::string name = std::string(typeKey);
            type = LLVMDIBuilderCreatePointerType(
                debugBuilder_, pointee, 64, 64, 0, name.data(), name.size());
        }
        else if (const frontend::NamedTypeDeclaration* named = debugNamedType(typeKey);
                 named != nullptr)
        {
            const std::string name = debugNamedDisplayName(*named);
            if (const frontend::builtins::IntegerTypeInfo* enumInfo = enumUnderlyingInfo(typeKey);
                enumInfo != nullptr)
            {
                std::vector<LLVMMetadataRef> enumerators;
                if (memberModel_ != nullptr)
                {
                    for (const frontend::ClassMemberSymbol& member : memberModel_->members())
                    {
                        if (!member.isEnumConstant ||
                            member.declaringTypeOrdinal !=
                                static_cast<std::size_t>(named - nameLookup_.namedTypes().data()))
                            continue;
                        LLVMMetadataRef enumerator = LLVMDIBuilderCreateEnumerator(
                            debugBuilder_, member.name.data(), member.name.size(),
                            member.enumConstantValue, enumInfo->isSigned ? 0 : 1);
                        if (enumerator != nullptr) enumerators.push_back(enumerator);
                    }
                }
                LLVMMetadataRef underlying = basic(
                    name + ".underlying", enumInfo->width,
                    enumInfo->isSigned ? LLVMDWARFTypeEncodingSigned
                                       : LLVMDWARFTypeEncodingUnsigned);
                LLVMMetadataRef file = debugFile(named->range.begin());
                const unsigned line = named->range.isValid()
                    ? sourceManager_->getPosition(named->range.begin()).line : 0;
                type = LLVMDIBuilderCreateEnumerationType(
                    debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
                    enumInfo->width, enumInfo->width, enumerators.data(),
                    static_cast<unsigned>(enumerators.size()), underlying);
            }
            else if (const frontend::ProjectOopMirStructLayout* layout = structLayout(typeKey);
                     layout != nullptr)
            {
                LLVMMetadataRef file = debugFile(named->range.begin());
                const unsigned line = named->range.isValid()
                    ? sourceManager_->getPosition(named->range.begin()).line
                    : 0;
                const std::string uniqueId = "nativesharp:" + named->identity.key();
                LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
                    debugBuilder_, LLVMDWARFTagStructureType, name.data(), name.size(),
                    debugCompileUnit_, file, line, 0, layout->size * 8,
                    static_cast<std::uint32_t>(layout->alignment * 8), LLVMDIFlagZero,
                    uniqueId.data(), uniqueId.size());
                if (temporary == nullptr) return nullptr;
                debugTypes_.emplace(cacheKey, temporary);

                std::vector<LLVMMetadataRef> members;
                members.reserve(layout->fields.size());
                for (const frontend::ObjectFieldLayout& field : layout->fields)
                {
                    std::string fieldName = "field" + std::to_string(field.memberOrdinal);
                    LLVMMetadataRef fieldFile = file;
                    unsigned fieldLine = line;
                    LLVMDIFlags fieldFlags = LLVMDIFlagZero;
                    if (memberModel_ != nullptr && field.memberOrdinal < memberModel_->members().size())
                    {
                        const frontend::ClassMemberSymbol& member =
                            memberModel_->members()[field.memberOrdinal];
                        fieldName = member.name;
                        fieldFlags = debugMemberFlags(member.accessibility);
                        if (member.range.isValid())
                        {
                            fieldFile = debugFile(member.range.begin());
                            fieldLine = sourceManager_->getPosition(member.range.begin()).line;
                        }
                    }
                    LLVMMetadataRef fieldType = debugType(field.typeKey);
                    if (fieldType == nullptr)
                    {
                        fieldType = LLVMDIBuilderCreateUnspecifiedType(
                            debugBuilder_, field.typeKey.data(), field.typeKey.size());
                    }
                    LLVMMetadataRef member = LLVMDIBuilderCreateMemberType(
                        debugBuilder_, temporary, fieldName.data(), fieldName.size(), fieldFile,
                        fieldLine, field.size * 8,
                        static_cast<std::uint32_t>(field.alignment * 8), field.offset * 8,
                        fieldFlags, fieldType);
                    if (member != nullptr) members.push_back(member);
                }

                LLVMMetadataRef complete = layout->isUnion
                    ? LLVMDIBuilderCreateUnionType(
                        debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
                        layout->size * 8, static_cast<std::uint32_t>(layout->alignment * 8),
                        LLVMDIFlagZero, members.data(), static_cast<unsigned>(members.size()),
                        0, uniqueId.data(), uniqueId.size())
                    : LLVMDIBuilderCreateStructType(
                        debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
                        layout->size * 8, static_cast<std::uint32_t>(layout->alignment * 8),
                        LLVMDIFlagZero, nullptr, members.data(),
                        static_cast<unsigned>(members.size()), 0, nullptr,
                        uniqueId.data(), uniqueId.size());
                if (complete == nullptr) return nullptr;
                LLVMMetadataReplaceAllUsesWith(temporary, complete);
                debugTypes_[cacheKey] = complete;
                type = complete;
            }
            else if (named->kind == frontend::NamedTypeDeclarationKind::Class)
            {
                if (const frontend::ProjectOopMirClassLayout* layout = classLayout(typeKey);
                    layout != nullptr)
                {
                    LLVMMetadataRef pointee = debugClassPointee(typeKey, *named, *layout);
                    const auto pointer = debugTypes_.find(cacheKey);
                    type = pointee != nullptr && pointer != debugTypes_.end()
                        ? pointer->second : nullptr;
                }
            }
            else if (named->kind == frontend::NamedTypeDeclarationKind::Delegate)
            {
                type = debugDelegateReference(typeKey, *named);
            }
            else
            {
                LLVMMetadataRef pointee = LLVMDIBuilderCreateUnspecifiedType(
                    debugBuilder_, name.data(), name.size());
                type = LLVMDIBuilderCreatePointerType(
                    debugBuilder_, pointee, 64, 64, 0, name.data(), name.size());
            }
        }
        else
        {
            const std::string name = typeKey == "builtin:string"
                ? "string"
                : std::string(typeKey);
            if (typeKey == "builtin:string" || typeKey == "string-literal")
                return debugHeaderReference(typeKey, name, "builtin:char");
            if (frontend::ProjectTypeConversion::isNativeArray(typeKey))
            {
                const std::optional<std::string> element =
                    frontend::ProjectTypeConversion::nativeArrayElementType(typeKey);
                return debugHeaderReference(
                    typeKey, name, element.has_value() ? std::string_view(*element)
                                                      : std::string_view("builtin:void"));
            }
            LLVMMetadataRef pointee = LLVMDIBuilderCreateUnspecifiedType(
                debugBuilder_, name.data(), name.size());
            if (typeKey == "builtin:string" || typeKey == "string-literal" ||
                frontend::ProjectTypeConversion::isNativeArray(typeKey) ||
                frontend::ProjectTypeConversion::isNativeCoreReference(typeKey))
            {
                type = LLVMDIBuilderCreatePointerType(
                    debugBuilder_, pointee, 64, 64, 0, name.data(), name.size());
            }
            else
            {
                type = pointee;
            }
        }
        if (type != nullptr) debugTypes_.emplace(cacheKey, type);
        return type;
    }

    bool emitDebugParameter(
        const frontend::ProjectOopMirFunction& function,
        std::size_t index,
        LLVMValueRef storage,
        std::string& error)
    {
        if (debugInformation_ != DebugInformationKind::Full || currentDebugSubprogram_ == nullptr)
            return true;
        if (storage == nullptr || index >= function.parameterTypeKeys.size())
            return fail(error, "debug parameter is missing its LLVM storage or type");

        SourceRange range;
        std::string name = index < function.parameterNames.size()
            ? function.parameterNames[index]
            : "p" + std::to_string(index);
        if (!function.isLambda && memberModel_ != nullptr &&
            function.memberOrdinal < memberModel_->members().size())
        {
            const frontend::ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
            range = index < member.parameters.size() ? member.parameters[index].range : member.range;
        }
        else if (function.body != nullptr && function.body->syntax != nullptr)
        {
            range = function.body->syntax->range();
        }
        if (!range.isValid()) return true;
        LLVMMetadataRef file = debugFile(range.begin());
        LLVMMetadataRef type = debugType(function.parameterTypeKeys[index]);
        if (file == nullptr || type == nullptr) return true;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        LLVMMetadataRef variable = LLVMDIBuilderCreateParameterVariable(
            debugBuilder_, currentDebugSubprogram_, name.data(), name.size(),
            static_cast<unsigned>(index + 1), file, position.line, type, 1, LLVMDIFlagZero);
        LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder_, nullptr, 0);
        LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, currentDebugSubprogram_, nullptr);
        if (variable == nullptr || expression == nullptr || location == nullptr)
            return fail(error, "failed to create debug metadata for parameter '" + name + "'");
        if (LLVMDIBuilderInsertDeclareRecordAtEnd(
                debugBuilder_, storage, variable, expression, location,
                LLVMGetInsertBlock(builder_)) == nullptr)
            return fail(error, "failed to emit debug storage for parameter '" + name + "'");
        return true;
    }

    bool emitDebugThis(
        const frontend::ProjectOopMirFunction& function,
        std::string& error)
    {
        if (debugInformation_ != DebugInformationKind::Full || function.isStatic ||
            function.isLambda || currentDebugSubprogram_ == nullptr)
            return true;
        if (thisValue_ == nullptr || function.declaringTypeOrdinal >= nameLookup_.namedTypes().size())
            return fail(error, "debug receiver is missing its LLVM value or declaring type");
        if (memberModel_ == nullptr || function.memberOrdinal >= memberModel_->members().size())
            return true;

        const frontend::ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
        if (!member.range.isValid()) return true;
        const std::string thisTypeKey = "type:" +
            nameLookup_.namedTypes()[function.declaringTypeOrdinal].identity.key();
        LLVMMetadataRef type = debugType(thisTypeKey);
        LLVMMetadataRef file = debugFile(member.range.begin());
        if (type == nullptr || file == nullptr) return true;
        const source::SourcePosition position = sourceManager_->getPosition(member.range.begin());

        LLVMValueRef storage = thisValue_;
        if (!isStructValue(thisTypeKey))
        {
            LLVMTypeRef llvmThisType = llvmType(thisTypeKey, error);
            if (llvmThisType == nullptr) return false;
            storage = LLVMBuildAlloca(builder_, llvmThisType, "this.addr");
            LLVMBuildStore(builder_, thisValue_, storage);
        }
        const auto flags = static_cast<LLVMDIFlags>(
            LLVMDIFlagArtificial | LLVMDIFlagObjectPointer);
        static constexpr std::string_view thisName = "this";
        LLVMMetadataRef variable = LLVMDIBuilderCreateParameterVariable(
            debugBuilder_, currentDebugSubprogram_, thisName.data(), thisName.size(), 0,
            file, position.line, type, 1, flags);
        LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder_, nullptr, 0);
        LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, currentDebugSubprogram_, nullptr);
        if (variable == nullptr || expression == nullptr || location == nullptr)
            return fail(error, "failed to create debug metadata for 'this'");
        if (LLVMDIBuilderInsertDeclareRecordAtEnd(
                debugBuilder_, storage, variable, expression, location,
                LLVMGetInsertBlock(builder_)) == nullptr)
            return fail(error, "failed to emit debug storage for 'this'");
        return true;
    }

    bool emitDebugLocal(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error)
    {
        if (debugInformation_ != DebugInformationKind::Full || currentDebugSubprogram_ == nullptr ||
            node == nullptr || storage == nullptr || node->stringValue.empty() ||
            !node->localOrdinal.has_value())
            return true;
        if (!debugDeclaredLocals_.insert(*node->localOrdinal).second) return true;
        if (node->syntax == nullptr || !node->syntax->range().isValid()) return true;
        const SourceRange range = node->syntax->range();
        LLVMMetadataRef file = debugFile(range.begin());
        const std::string referenceTypeKey =
            node->referenceLocal ? node->typeKey + "*" : std::string{};
        const std::string_view typeKey = node->referenceLocal
            ? std::string_view(referenceTypeKey)
            : (node->kind == frontend::ProjectOopMirNodeKind::UsingRegion ||
               node->kind == frontend::ProjectOopMirNodeKind::CatchRegion
                ? std::string_view(node->elementTypeKey)
                : std::string_view(node->typeKey));
        LLVMMetadataRef type = debugType(typeKey);
        if (file == nullptr || type == nullptr) return true;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        LLVMMetadataRef scope = currentDebugScope_ != nullptr
            ? currentDebugScope_
            : currentDebugSubprogram_;
        LLVMMetadataRef variable = LLVMDIBuilderCreateAutoVariable(
            debugBuilder_, scope, node->stringValue.data(),
            node->stringValue.size(), file, position.line, type, 1, LLVMDIFlagZero,
            0);
        LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder_, nullptr, 0);
        LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, scope, nullptr);
        if (variable == nullptr || expression == nullptr || location == nullptr)
            return fail(error, "failed to create debug metadata for local '" + node->stringValue + "'");
        if (LLVMDIBuilderInsertDeclareRecordAtEnd(
                debugBuilder_, storage, variable, expression, location,
                LLVMGetInsertBlock(builder_)) == nullptr)
            return fail(error, "failed to emit debug storage for local '" + node->stringValue + "'");
        return true;
    }

    bool declareRuntime(std::string& error)
    {
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef trapParams[] = {i32};
        trapFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), trapParams, 1, 0);
        trapFunction_ = LLVMAddFunction(module_, kTrapFunctionName, trapFunctionType_);
        // `nounwind` is both TRUE (the trap terminates the process; it never raises) and
        // load-bearing for exception correctness. Without it, LLVM's inliner rewrites every trap
        // CALL that ends up inside a `try` into an INVOKE, which extends that try's protected
        // instruction range past the trap. The Windows personality treats the byte immediately
        // after a protected range as still inside it (scope-table LabelEnd is "last instruction
        // + 1"), so a catch handler laid out right after the range is seen as INSIDE the enclosing
        // scope -- and an enclosing `finally` or cleanup is then silently skipped. Keeping the trap
        // a plain call leaves its bytes OUTSIDE the range, which is exactly what pushes the handler
        // past LabelEnd. See lowerExceptionalTransfer's comment and
        // BUGS_ENCONTRADOS/bug_finally_saltado_por_layout_de_handler.md for the measurements,
        // including the clang reproduction of the same hazard.
        applyFunctionAttribute(trapFunction_, "nounwind");
        applyFunctionAttribute(trapFunction_, "noreturn");

        LLVMTypeRef allocParams[] = {i64};
        allocationFunctionType_ = LLVMFunctionType(ptr, allocParams, 1, 0);
        allocationFunction_ = LLVMAddFunction(module_, kAllocationFunctionName, allocationFunctionType_);

        LLVMTypeRef freeParams[] = {ptr};
        deallocationFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), freeParams, 1, 0);
        deallocationFunction_ = LLVMAddFunction(module_, kDeallocationFunctionName, deallocationFunctionType_);

        LLVMTypeRef consoleParams[] = {ptr, LLVMInt32TypeInContext(context_)};
        consoleWriteLineFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), consoleParams, 2, 0);
        consoleWriteLineFunction_ = LLVMAddFunction(module_, kConsoleWriteLineFunctionName, consoleWriteLineFunctionType_);

        // Stage 53.1 "Time" -- all three take zero parameters and return i64 (builtin:long).
        // Defined for real in src/Runtime/WindowsRuntimeBuilder.cpp; declared here, exactly
        // like every other __nativesharp_* runtime hook, so the object this lowerer emits has
        // an external reference the linker resolves against the runtime module at link time.
        clockQueryCounterFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        clockQueryCounterFunction_ = LLVMAddFunction(
            module_, kClockQueryCounterFunctionName, clockQueryCounterFunctionType_);
        clockQueryFrequencyFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        clockQueryFrequencyFunction_ = LLVMAddFunction(
            module_, kClockQueryFrequencyFunctionName, clockQueryFrequencyFunctionType_);
        clockQuerySystemFileTimeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        clockQuerySystemFileTimeFunction_ = LLVMAddFunction(
            module_, kClockQuerySystemFileTimeFunctionName, clockQuerySystemFileTimeFunctionType_);

        // Stage 53.2 "Environment" -- TickCount64/ProcessorCount take zero parameters, same as
        // the NativeClock trio above (TickCount64 returns i64, ProcessorCount returns i32 --
        // GetActiveProcessorCount is a real 32-bit DWORD in Win32). Exit takes one i32
        // parameter and returns void, the same shape as consoleWriteLineFunctionType_ above
        // just with an i32 argument instead of a pointer.
        environmentTickCount64FunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        environmentTickCount64Function_ = LLVMAddFunction(
            module_, kEnvironmentTickCount64FunctionName, environmentTickCount64FunctionType_);
        environmentProcessorCountFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 0);
        environmentProcessorCountFunction_ = LLVMAddFunction(
            module_, kEnvironmentProcessorCountFunctionName, environmentProcessorCountFunctionType_);
        LLVMTypeRef environmentExitParams[] = {i32};
        environmentExitFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), environmentExitParams, 1, 0);
        environmentExitFunction_ = LLVMAddFunction(
            module_, kEnvironmentExitFunctionName, environmentExitFunctionType_);

        // Stage 53.2b "Environment (variables)". GetEnvironmentVariable takes one NativeSharp
        // string pointer and returns a freshly allocated one (ptr -> ptr, same shape as any
        // other reference-typed core operation); SetEnvironmentVariable takes two string
        // pointers and returns void.
        LLVMTypeRef environmentGetVariableParams[] = {ptr};
        environmentGetVariableFunctionType_ = LLVMFunctionType(ptr, environmentGetVariableParams, 1, 0);
        environmentGetVariableFunction_ = LLVMAddFunction(
            module_, kEnvironmentGetVariableFunctionName, environmentGetVariableFunctionType_);
        LLVMTypeRef environmentSetVariableParams[] = {ptr, ptr};
        environmentSetVariableFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), environmentSetVariableParams, 2, 0);
        environmentSetVariableFunction_ = LLVMAddFunction(
            module_, kEnvironmentSetVariableFunctionName, environmentSetVariableFunctionType_);

        // Stage 53.3 "Filesystem/Paths". Exists takes one NativeSharp string pointer (a path)
        // and returns i1 (builtin:bool) directly -- no allocation, unlike GetEnvironmentVariable,
        // so the return type is i1 here rather than ptr. Delete/CreateDirectory take one string
        // pointer and return void, same shape as SetEnvironmentVariable minus one parameter.
        LLVMTypeRef boolType = LLVMInt1TypeInContext(context_);
        LLVMTypeRef fileExistsParams[] = {ptr};
        fileExistsFunctionType_ = LLVMFunctionType(boolType, fileExistsParams, 1, 0);
        fileExistsFunction_ = LLVMAddFunction(module_, kFileExistsFunctionName, fileExistsFunctionType_);
        LLVMTypeRef fileDeleteParams[] = {ptr};
        fileDeleteFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), fileDeleteParams, 1, 0);
        fileDeleteFunction_ = LLVMAddFunction(module_, kFileDeleteFunctionName, fileDeleteFunctionType_);
        LLVMTypeRef directoryExistsParams[] = {ptr};
        directoryExistsFunctionType_ = LLVMFunctionType(boolType, directoryExistsParams, 1, 0);
        directoryExistsFunction_ = LLVMAddFunction(
            module_, kDirectoryExistsFunctionName, directoryExistsFunctionType_);
        LLVMTypeRef directoryCreateParams[] = {ptr};
        directoryCreateFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), directoryCreateParams, 1, 0);
        directoryCreateFunction_ = LLVMAddFunction(
            module_, kDirectoryCreateFunctionName, directoryCreateFunctionType_);

        // Stage 53.3b "Filesystem/Paths (contenido de archivo)". ReadAllText takes one string
        // pointer (a path) and returns a freshly allocated one -- same ptr -> ptr shape as
        // GetEnvironmentVariable. WriteAllText/AppendAllText take two string pointers (path,
        // content) and return void -- same shape as SetEnvironmentVariable.
        LLVMTypeRef fileReadAllTextParams[] = {ptr};
        fileReadAllTextFunctionType_ = LLVMFunctionType(ptr, fileReadAllTextParams, 1, 0);
        fileReadAllTextFunction_ = LLVMAddFunction(
            module_, kFileReadAllTextFunctionName, fileReadAllTextFunctionType_);

        // Bloque 4, fase A -- File.GetSize. Un puntero a cadena entra, un i64 sale. Es la primera
        // operacion nucleo de esta familia que devuelve 64 bits: las demas devuelven i1, i32, void
        // o un puntero. Ver MethodCoreOperationKind::FileGetSize para por que no puede ser i32.
        LLVMTypeRef fileGetSizeParams[] = {ptr};
        fileGetSizeFunctionType_ = LLVMFunctionType(
            LLVMInt64TypeInContext(context_), fileGetSizeParams, 1, 0);
        fileGetSizeFunction_ = LLVMAddFunction(
            module_, kFileGetSizeFunctionName, fileGetSizeFunctionType_);

        // Bloque 4, fase A -- E/S binaria. Un `byte[]` es un puntero al MISMO layout que una
        // cadena, { i64 longitud; datos }, asi que en el IR los tres son punteros y la firma no
        // distingue "arreglo de bytes" de "cadena": lo que las separa es la clave de tipo del
        // frontend, no el tipo LLVM.
        LLVMTypeRef fileReadAllBytesParams[] = {ptr};
        fileReadAllBytesFunctionType_ = LLVMFunctionType(ptr, fileReadAllBytesParams, 1, 0);
        fileReadAllBytesFunction_ = LLVMAddFunction(
            module_, kFileReadAllBytesFunctionName, fileReadAllBytesFunctionType_);
        LLVMTypeRef fileReadBytesParams[] = {ptr, ptr};
        fileReadBytesFunctionType_ = LLVMFunctionType(i32, fileReadBytesParams, 2, 0);
        fileReadBytesFunction_ = LLVMAddFunction(
            module_, kFileReadBytesFunctionName, fileReadBytesFunctionType_);
        LLVMTypeRef fileWriteAllBytesParams[] = {ptr, ptr, i32};
        fileWriteAllBytesFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), fileWriteAllBytesParams, 3, 0);
        fileWriteAllBytesFunction_ = LLVMAddFunction(
            module_, kFileWriteAllBytesFunctionName, fileWriteAllBytesFunctionType_);
        environmentCurrentDirectoryFunctionType_ = LLVMFunctionType(ptr, nullptr, 0, 0);
        environmentCurrentDirectoryFunction_ = LLVMAddFunction(
            module_, kEnvironmentCurrentDirectoryFunctionName, environmentCurrentDirectoryFunctionType_);

        // Bloque 4, fase C -- Process.Run. Tres punteros a cadena entran (ejecutable, linea de
        // comandos, ruta de salida), un i32 sale: el codigo de salida del hijo, o -1.
        LLVMTypeRef processRunParams[] = {ptr, ptr, ptr};
        processRunFunctionType_ = LLVMFunctionType(i32, processRunParams, 3, 0);
        processRunFunction_ = LLVMAddFunction(
            module_, kProcessRunFunctionName, processRunFunctionType_);
        LLVMTypeRef fileWriteAllTextParams[] = {ptr, ptr};
        fileWriteAllTextFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), fileWriteAllTextParams, 2, 0);
        fileWriteAllTextFunction_ = LLVMAddFunction(
            module_, kFileWriteAllTextFunctionName, fileWriteAllTextFunctionType_);
        LLVMTypeRef fileAppendAllTextParams[] = {ptr, ptr};
        fileAppendAllTextFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), fileAppendAllTextParams, 2, 0);
        fileAppendAllTextFunction_ = LLVMAddFunction(
            module_, kFileAppendAllTextFunctionName, fileAppendAllTextFunctionType_);

        // Stage 53.4 "Filesystem/Paths (cierre)". DirectoryDelete: one string pointer (a path),
        // void return -- same shape as FileDelete/DirectoryCreate above. PathGetFullPath: one
        // string pointer in, a freshly allocated one out -- same ptr -> ptr shape as
        // GetEnvironmentVariable/ReadAllText above.
        LLVMTypeRef directoryDeleteParams[] = {ptr};
        directoryDeleteFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), directoryDeleteParams, 1, 0);
        directoryDeleteFunction_ = LLVMAddFunction(
            module_, kDirectoryDeleteFunctionName, directoryDeleteFunctionType_);
        LLVMTypeRef pathGetFullPathParams[] = {ptr};
        pathGetFullPathFunctionType_ = LLVMFunctionType(ptr, pathGetFullPathParams, 1, 0);
        pathGetFullPathFunction_ = LLVMAddFunction(
            module_, kPathGetFullPathFunctionName, pathGetFullPathFunctionType_);

        // Stage 53.5 "Command-line arguments". GetCommandLineArgCount takes zero parameters
        // and returns i32 (builtin:int) directly -- same zero-argument shape as
        // environmentProcessorCountFunctionType_ above, just a different wrapped Win32 call.
        // GetCommandLineArgs takes one pointer (the caller-provided builtin:string[] buffer,
        // the "buffer provided by caller" pattern -- see Environment.ns) and also returns i32
        // directly (how many elements were actually written), NOT ptr -> ptr like
        // GetEnvironmentVariable/PathGetFullPath: this is the first core operation whose single
        // pointer argument is an array rather than a string, and whose result is a meaningful
        // integer rather than a freshly allocated NativeSharp object.
        environmentGetCommandLineArgCountFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 0);
        environmentGetCommandLineArgCountFunction_ = LLVMAddFunction(
            module_, kEnvironmentGetCommandLineArgCountFunctionName,
            environmentGetCommandLineArgCountFunctionType_);
        LLVMTypeRef environmentGetCommandLineArgsParams[] = {ptr};
        environmentGetCommandLineArgsFunctionType_ = LLVMFunctionType(
            i32, environmentGetCommandLineArgsParams, 1, 0);
        environmentGetCommandLineArgsFunction_ = LLVMAddFunction(
            module_, kEnvironmentGetCommandLineArgsFunctionName,
            environmentGetCommandLineArgsFunctionType_);

        // Stage 53.6 "Enumeracion de directorios". GetFileCount/GetDirectoryCount: one string
        // pointer (a path) in, i32 out -- same ptr -> i32 shape as
        // environmentGetCommandLineArgCountFunctionType_ but with a real argument this time.
        // GetFiles/GetDirectories: two pointers in (the path, then the caller-provided
        // builtin:string[] buffer -- same buffer-in-second-position idea
        // environmentGetCommandLineArgsFunctionType_ established, just with a path ahead of it),
        // i32 out (how many elements were actually written).
        LLVMTypeRef directoryGetCountParams[] = {ptr};
        directoryGetFileCountFunctionType_ = LLVMFunctionType(i32, directoryGetCountParams, 1, 0);
        directoryGetFileCountFunction_ = LLVMAddFunction(
            module_, kDirectoryGetFileCountFunctionName, directoryGetFileCountFunctionType_);
        directoryGetDirectoryCountFunctionType_ = LLVMFunctionType(i32, directoryGetCountParams, 1, 0);
        directoryGetDirectoryCountFunction_ = LLVMAddFunction(
            module_, kDirectoryGetDirectoryCountFunctionName, directoryGetDirectoryCountFunctionType_);
        LLVMTypeRef directoryGetEntriesParams[] = {ptr, ptr};
        directoryGetFilesFunctionType_ = LLVMFunctionType(i32, directoryGetEntriesParams, 2, 0);
        directoryGetFilesFunction_ = LLVMAddFunction(
            module_, kDirectoryGetFilesFunctionName, directoryGetFilesFunctionType_);
        directoryGetDirectoriesFunctionType_ = LLVMFunctionType(i32, directoryGetEntriesParams, 2, 0);
        directoryGetDirectoriesFunction_ = LLVMAddFunction(
            module_, kDirectoryGetDirectoriesFunctionName, directoryGetDirectoriesFunctionType_);

        // Stage 54.1 "Threads". ThreadCreate: one pointer in (the ThreadStart delegate's 16-byte
        // heap cell -- a delegate value is ALWAYS represented as `ptr` at this LLVM level,
        // regardless of what it points to at the NativeSharp type level, exactly like every other
        // reference-typed operand this lowerer already handles), i64 out (the Win32 HANDLE,
        // reinterpreted as nint) -- a real, meaningful result, so NO "dummy i1" wrapping, same
        // discipline as directoryGetFileCountFunctionType_/directoryGetDirectoriesFunctionType_
        // above. ThreadJoin/ThreadDetach: one i64 in (the HANDLE the caller already holds as
        // nint), void out -- same "runtime function genuinely returns void, the LOWERING CASE
        // BLOCK synthesizes its own i1 dummy value in IR after the call" pattern
        // directoryDeleteFunctionType_/fileDeleteFunctionType_ above already use (see the
        // CoreThreadJoin/CoreThreadDetach case blocks further down for where that i1 actually
        // comes from -- it is NOT part of this function's own return type).
        LLVMTypeRef threadCreateParams[] = {ptr};
        threadCreateFunctionType_ = LLVMFunctionType(i64, threadCreateParams, 1, 0);
        threadCreateFunction_ = LLVMAddFunction(
            module_, kThreadCreateFunctionName, threadCreateFunctionType_);
        LLVMTypeRef threadJoinOrDetachParams[] = {i64};
        threadJoinFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), threadJoinOrDetachParams, 1, 0);
        threadJoinFunction_ = LLVMAddFunction(module_, kThreadJoinFunctionName, threadJoinFunctionType_);
        threadDetachFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), threadJoinOrDetachParams, 1, 0);
        threadDetachFunction_ = LLVMAddFunction(module_, kThreadDetachFunctionName, threadDetachFunctionType_);

        // Stage 54.2 "Atomics". The location parameter is always `ptr` at this LLVM level --
        // NativeSharp's raw-pointer machinery (see ProjectMethodBodyModel.cpp's Interlocked
        // recognition block: the first parameter's typeKey is builtin:int*/builtin:long*) already
        // lowers ANY `*`-suffixed typeKey to plain LLVM `ptr` via isPointerLike(), so there is
        // nothing special to do here beyond picking i32 vs i64 for the value/result type.
        // Increment/Decrement: one ptr in, i32/i64 out (the NEW value -- Win32's
        // InterlockedIncrement/Decrement already return the new value directly, no adjustment
        // needed in the runtime wrapper). Add: ptr + i32/i64 in, i32/i64 out (also the NEW value,
        // but Win32's InterlockedExchangeAdd returns the OLD value -- the runtime wrapper computes
        // old + value itself, see WindowsRuntimeBuilder.cpp). Exchange: ptr + i32/i64 in, i32/i64
        // out (the OLD value, Win32's InterlockedExchange result used directly).
        // CompareExchange: ptr + i32/i64 + i32/i64 in (location, value, comparand -- same order as
        // Win32's InterlockedCompareExchange(Destination, Exchange, Comparand) and C#'s
        // Interlocked.CompareExchange(location, value, comparand)), i32/i64 out (the OLD value,
        // regardless of whether the comparison matched, Win32's result used directly). None of
        // these ten need the "dummy i1" pattern -- every one returns a real, meaningful value.
        LLVMTypeRef interlockedLocationOnlyInt32Params[] = {ptr};
        atomicLoadInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationOnlyInt32Params, 1, 0);
        atomicLoadInt32Function_ = LLVMAddFunction(
            module_, kAtomicLoadInt32FunctionName, atomicLoadInt32FunctionType_);
        interlockedIncrementInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationOnlyInt32Params, 1, 0);
        interlockedIncrementInt32Function_ = LLVMAddFunction(
            module_, kInterlockedIncrementInt32FunctionName, interlockedIncrementInt32FunctionType_);
        interlockedDecrementInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationOnlyInt32Params, 1, 0);
        interlockedDecrementInt32Function_ = LLVMAddFunction(
            module_, kInterlockedDecrementInt32FunctionName, interlockedDecrementInt32FunctionType_);
        LLVMTypeRef interlockedLocationOnlyInt64Params[] = {ptr};
        atomicLoadInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationOnlyInt64Params, 1, 0);
        atomicLoadInt64Function_ = LLVMAddFunction(
            module_, kAtomicLoadInt64FunctionName, atomicLoadInt64FunctionType_);
        interlockedIncrementInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationOnlyInt64Params, 1, 0);
        interlockedIncrementInt64Function_ = LLVMAddFunction(
            module_, kInterlockedIncrementInt64FunctionName, interlockedIncrementInt64FunctionType_);
        interlockedDecrementInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationOnlyInt64Params, 1, 0);
        interlockedDecrementInt64Function_ = LLVMAddFunction(
            module_, kInterlockedDecrementInt64FunctionName, interlockedDecrementInt64FunctionType_);
        LLVMTypeRef interlockedLocationValueInt32Params[] = {ptr, i32};
        atomicStoreInt32FunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), interlockedLocationValueInt32Params, 2, 0);
        atomicStoreInt32Function_ = LLVMAddFunction(
            module_, kAtomicStoreInt32FunctionName, atomicStoreInt32FunctionType_);
        interlockedAddInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationValueInt32Params, 2, 0);
        interlockedAddInt32Function_ = LLVMAddFunction(
            module_, kInterlockedAddInt32FunctionName, interlockedAddInt32FunctionType_);
        interlockedExchangeInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationValueInt32Params, 2, 0);
        interlockedExchangeInt32Function_ = LLVMAddFunction(
            module_, kInterlockedExchangeInt32FunctionName, interlockedExchangeInt32FunctionType_);
        LLVMTypeRef interlockedLocationValueInt64Params[] = {ptr, i64};
        atomicStoreInt64FunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), interlockedLocationValueInt64Params, 2, 0);
        atomicStoreInt64Function_ = LLVMAddFunction(
            module_, kAtomicStoreInt64FunctionName, atomicStoreInt64FunctionType_);
        interlockedAddInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationValueInt64Params, 2, 0);
        interlockedAddInt64Function_ = LLVMAddFunction(
            module_, kInterlockedAddInt64FunctionName, interlockedAddInt64FunctionType_);
        interlockedExchangeInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationValueInt64Params, 2, 0);
        interlockedExchangeInt64Function_ = LLVMAddFunction(
            module_, kInterlockedExchangeInt64FunctionName, interlockedExchangeInt64FunctionType_);
        LLVMTypeRef interlockedCompareExchangeInt32Params[] = {ptr, i32, i32};
        interlockedCompareExchangeInt32FunctionType_ = LLVMFunctionType(
            i32, interlockedCompareExchangeInt32Params, 3, 0);
        interlockedCompareExchangeInt32Function_ = LLVMAddFunction(
            module_, kInterlockedCompareExchangeInt32FunctionName, interlockedCompareExchangeInt32FunctionType_);
        LLVMTypeRef interlockedCompareExchangeInt64Params[] = {ptr, i64, i64};
        interlockedCompareExchangeInt64FunctionType_ = LLVMFunctionType(
            i64, interlockedCompareExchangeInt64Params, 3, 0);
        interlockedCompareExchangeInt64Function_ = LLVMAddFunction(
            module_, kInterlockedCompareExchangeInt64FunctionName, interlockedCompareExchangeInt64FunctionType_);

        // Stage 54.3 "Mutex". Exact same shape as ThreadCreate/ThreadJoin/ThreadDetach above:
        // MutexCreate takes NO parameters (the first zero-parameter runtime hook in the project),
        // i64 out (the HANDLE, as nint) -- a real, meaningful result, no dummy-i1 wrapping.
        // MutexLock/MutexUnlock/MutexClose: one i64 in (the HANDLE), void out -- same "dummy i1
        // synthesized by the lowering case block" pattern as ThreadJoin/ThreadDetach.
        mutexCreateFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        mutexCreateFunction_ = LLVMAddFunction(module_, kMutexCreateFunctionName, mutexCreateFunctionType_);
        LLVMTypeRef mutexHandleParams[] = {i64};
        mutexLockFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), mutexHandleParams, 1, 0);
        mutexLockFunction_ = LLVMAddFunction(module_, kMutexLockFunctionName, mutexLockFunctionType_);
        mutexUnlockFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), mutexHandleParams, 1, 0);
        mutexUnlockFunction_ = LLVMAddFunction(module_, kMutexUnlockFunctionName, mutexUnlockFunctionType_);
        mutexCloseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), mutexHandleParams, 1, 0);
        mutexCloseFunction_ = LLVMAddFunction(module_, kMutexCloseFunctionName, mutexCloseFunctionType_);
        getCurrentThreadIdFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 0);
        getCurrentThreadIdFunction_ = LLVMAddFunction(
            module_, kGetCurrentThreadIdFunctionName, getCurrentThreadIdFunctionType_);

        // Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll: one pointer in (the
        // FieldAddress-computed lock_/cv_ address), void out. Wait: two pointers + one i32 in
        // (cv_ address, lock_ address, millisecondsTimeout), a REAL builtin:bool (i1) out -- no
        // dummy wrapping needed, same discipline as threadCreateFunctionType_ above.
        LLVMTypeRef monitorOnePointerParams[] = {ptr};
        monitorEnterFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorEnterFunction_ = LLVMAddFunction(module_, kMonitorEnterFunctionName, monitorEnterFunctionType_);
        monitorExitFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorExitFunction_ = LLVMAddFunction(module_, kMonitorExitFunctionName, monitorExitFunctionType_);
        monitorPulseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorPulseFunction_ = LLVMAddFunction(module_, kMonitorPulseFunctionName, monitorPulseFunctionType_);
        monitorPulseAllFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorPulseAllFunction_ = LLVMAddFunction(
            module_, kMonitorPulseAllFunctionName, monitorPulseAllFunctionType_);
        LLVMTypeRef monitorWaitParams[] = {ptr, ptr, i32};
        monitorWaitFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), monitorWaitParams, 3, 0);
        monitorWaitFunction_ = LLVMAddFunction(module_, kMonitorWaitFunctionName, monitorWaitFunctionType_);

        // Stage 54.5 "Otras primitivas de sincronizacion". SemaphoreCreate: two i32 in
        // (initialCount, maximumCount), i64 out (a real HANDLE, no dummy wrapping). The shared
        // waitForHandleTimeoutFunction_ both SemaphoreWait and EventWait call: one i64 in (the
        // HANDLE), one i32 in (millisecondsTimeout), a real builtin:bool (i1) out.
        // SemaphoreRelease/SemaphoreClose/EventSet/EventReset/EventClose: one i64 in, void out
        // (dummy-i1 pattern in the LOWERING CASE BLOCK, same as CoreMutexLock/Unlock/Close).
        // EventCreate: two i1 in (manualReset, initialState), i64 out (a real HANDLE).
        LLVMTypeRef semaphoreCreateParams[] = {i32, i32};
        semaphoreCreateFunctionType_ = LLVMFunctionType(i64, semaphoreCreateParams, 2, 0);
        semaphoreCreateFunction_ = LLVMAddFunction(
            module_, kSemaphoreCreateFunctionName, semaphoreCreateFunctionType_);
        LLVMTypeRef waitForHandleTimeoutParams[] = {i64, i32};
        waitForHandleTimeoutFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), waitForHandleTimeoutParams, 2, 0);
        waitForHandleTimeoutFunction_ = LLVMAddFunction(
            module_, kWaitForHandleTimeoutFunctionName, waitForHandleTimeoutFunctionType_);
        LLVMTypeRef oneHandleParams[] = {i64};
        semaphoreReleaseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        semaphoreReleaseFunction_ = LLVMAddFunction(
            module_, kSemaphoreReleaseFunctionName, semaphoreReleaseFunctionType_);
        semaphoreCloseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        semaphoreCloseFunction_ = LLVMAddFunction(
            module_, kSemaphoreCloseFunctionName, semaphoreCloseFunctionType_);
        LLVMTypeRef eventCreateParams[] = {LLVMInt1TypeInContext(context_), LLVMInt1TypeInContext(context_)};
        eventCreateFunctionType_ = LLVMFunctionType(i64, eventCreateParams, 2, 0);
        eventCreateFunction_ = LLVMAddFunction(module_, kEventCreateFunctionName, eventCreateFunctionType_);
        eventSetFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        eventSetFunction_ = LLVMAddFunction(module_, kEventSetFunctionName, eventSetFunctionType_);
        eventResetFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        eventResetFunction_ = LLVMAddFunction(module_, kEventResetFunctionName, eventResetFunctionType_);
        eventCloseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        eventCloseFunction_ = LLVMAddFunction(module_, kEventCloseFunctionName, eventCloseFunctionType_);

        // Stage 55.1 "Process". Five zero-argument builtin:long imports (the CURRENT process
        // HANDLE is obtained inside the runtime wrapper itself, never passed in from here).
        processGetCurrentProcessIdFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetCurrentProcessIdFunction_ = LLVMAddFunction(
            module_, kProcessGetCurrentProcessIdFunctionName, processGetCurrentProcessIdFunctionType_);
        processGetWorkingSetSizeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetWorkingSetSizeFunction_ = LLVMAddFunction(
            module_, kProcessGetWorkingSetSizeFunctionName, processGetWorkingSetSizeFunctionType_);
        processGetKernelTimeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetKernelTimeFunction_ = LLVMAddFunction(
            module_, kProcessGetKernelTimeFunctionName, processGetKernelTimeFunctionType_);
        processGetUserTimeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetUserTimeFunction_ = LLVMAddFunction(
            module_, kProcessGetUserTimeFunctionName, processGetUserTimeFunctionType_);
        processGetPriorityClassFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetPriorityClassFunction_ = LLVMAddFunction(
            module_, kProcessGetPriorityClassFunctionName, processGetPriorityClassFunctionType_);
        // SetPriorityClass: one builtin:int argument (the Win32 DWORD priority class constant),
        // void return -- dummy-i1 pattern at the call site, same as every other void core op.
        LLVMTypeRef setPriorityClassParams[] = {i32};
        processSetPriorityClassFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), setPriorityClassParams, 1, 0);
        processSetPriorityClassFunction_ = LLVMAddFunction(
            module_, kProcessSetPriorityClassFunctionName, processSetPriorityClassFunctionType_);

        // Stage 55.2 "Hardware / System Info". All six zero-argument, builtin:long.
        systemInfoGetPageSizeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetPageSizeFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetPageSizeFunctionName, systemInfoGetPageSizeFunctionType_);
        systemInfoGetProcessorArchitectureFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetProcessorArchitectureFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetProcessorArchitectureFunctionName, systemInfoGetProcessorArchitectureFunctionType_);
        systemInfoGetAllocationGranularityFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetAllocationGranularityFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetAllocationGranularityFunctionName, systemInfoGetAllocationGranularityFunctionType_);
        systemInfoGetTotalPhysicalMemoryBytesFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetTotalPhysicalMemoryBytesFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetTotalPhysicalMemoryBytesFunctionName, systemInfoGetTotalPhysicalMemoryBytesFunctionType_);
        systemInfoGetAvailablePhysicalMemoryBytesFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetAvailablePhysicalMemoryBytesFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetAvailablePhysicalMemoryBytesFunctionName, systemInfoGetAvailablePhysicalMemoryBytesFunctionType_);
        systemInfoGetMemoryLoadPercentFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetMemoryLoadPercentFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetMemoryLoadPercentFunctionName, systemInfoGetMemoryLoadPercentFunctionType_);

        // Stage 55.3 "Diagnostics". Is/CaptureCount: zero-argument, builtin:long. OutputString:
        // one builtin:string pointer argument (marshaled the same way EnvironmentSetVariable's
        // arguments are, via buildNullTerminatedCopy on the runtime side), void return.
        debugIsDebuggerPresentFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        debugIsDebuggerPresentFunction_ = LLVMAddFunction(
            module_, kDebugIsDebuggerPresentFunctionName, debugIsDebuggerPresentFunctionType_);
        debugCaptureStackFrameCountFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        debugCaptureStackFrameCountFunction_ = LLVMAddFunction(
            module_, kDebugCaptureStackFrameCountFunctionName, debugCaptureStackFrameCountFunctionType_);
        LLVMTypeRef debugOutputStringParams[] = {ptr};
        debugOutputStringFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), debugOutputStringParams, 1, 0);
        debugOutputStringFunction_ = LLVMAddFunction(
            module_, kDebugOutputStringFunctionName, debugOutputStringFunctionType_);

        // Pre-Stage-56 "utilidades basicas" round. ThreadSleep: one builtin:int argument, void
        // return (dummy-i1 pattern at the call site, same as ThreadJoin/ThreadDetach). FileCopy:
        // two string pointers + one i1 (overwrite) in, a real i1 out. The shared MoveFile hook
        // both CoreFileMove and CoreDirectoryMove call: two string pointers in, a real i1 out.
        LLVMTypeRef threadSleepParams[] = {i32};
        threadSleepFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), threadSleepParams, 1, 0);
        threadSleepFunction_ = LLVMAddFunction(module_, kThreadSleepFunctionName, threadSleepFunctionType_);
        LLVMTypeRef fileCopyParams[] = {ptr, ptr, LLVMInt1TypeInContext(context_)};
        fileCopyFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), fileCopyParams, 3, 0);
        fileCopyFunction_ = LLVMAddFunction(module_, kFileCopyFunctionName, fileCopyFunctionType_);
        LLVMTypeRef moveFileParams[] = {ptr, ptr};
        moveFileFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), moveFileParams, 2, 0);
        moveFileFunction_ = LLVMAddFunction(module_, kMoveFileFunctionName, moveFileFunctionType_);

        LLVMTypeRef throwParams[] = {ptr};
        nativeThrowFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), throwParams, 1, 0);
        nativeThrowFunction_ = LLVMAddFunction(module_, kNativeThrowFunctionName, nativeThrowFunctionType_);

        exceptionCurrentFunctionType_ = LLVMFunctionType(ptr, nullptr, 0, 0);
        exceptionCurrentFunction_ = LLVMAddFunction(module_, kExceptionCurrentFunctionName, exceptionCurrentFunctionType_);

        exceptionClearFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), nullptr, 0, 0);
        exceptionClearFunction_ = LLVMAddFunction(module_, kExceptionClearFunctionName, exceptionClearFunctionType_);

        LLVMTypeRef filterParams[] = {ptr, ptr};
        ehFilterFunctionType_ = LLVMFunctionType(i32, filterParams, 2, 0);
        ehFilterFunction_ = LLVMAddFunction(module_, kEhFilterFunctionName, ehFilterFunctionType_);

        sehPersonalityFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 1);
        sehPersonalityFunction_ = LLVMAddFunction(module_, kSehPersonalityFunctionName, sehPersonalityFunctionType_);

        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef i1 = LLVMInt1TypeInContext(context_);
        LLVMTypeRef memsetParams[] = {ptr, i8, i64, i1};
        memsetFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), memsetParams, 4, 0);
        memsetFunction_ = LLVMAddFunction(module_, kMemsetIntrinsicName, memsetFunctionType_);
        LLVMTypeRef memcpyParams[] = {ptr, ptr, i64, i1};
        memcpyFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), memcpyParams, 4, 0);
        memcpyFunction_ = LLVMAddFunction(module_, kMemcpyIntrinsicName, memcpyFunctionType_);
        if (trapFunction_ == nullptr || allocationFunction_ == nullptr ||
            deallocationFunction_ == nullptr || consoleWriteLineFunction_ == nullptr ||
            clockQueryCounterFunction_ == nullptr || clockQueryFrequencyFunction_ == nullptr ||
            clockQuerySystemFileTimeFunction_ == nullptr ||
            environmentTickCount64Function_ == nullptr || environmentProcessorCountFunction_ == nullptr ||
            environmentExitFunction_ == nullptr ||
            environmentGetVariableFunction_ == nullptr || environmentSetVariableFunction_ == nullptr ||
            fileExistsFunction_ == nullptr || fileDeleteFunction_ == nullptr ||
            directoryExistsFunction_ == nullptr || directoryCreateFunction_ == nullptr ||
            fileReadAllTextFunction_ == nullptr || fileWriteAllTextFunction_ == nullptr ||
            fileAppendAllTextFunction_ == nullptr || fileGetSizeFunction_ == nullptr ||
            fileReadAllBytesFunction_ == nullptr || fileReadBytesFunction_ == nullptr ||
            fileWriteAllBytesFunction_ == nullptr ||
            environmentCurrentDirectoryFunction_ == nullptr ||
            processRunFunction_ == nullptr ||
            directoryDeleteFunction_ == nullptr || pathGetFullPathFunction_ == nullptr ||
            environmentGetCommandLineArgCountFunction_ == nullptr ||
            environmentGetCommandLineArgsFunction_ == nullptr ||
            directoryGetFileCountFunction_ == nullptr || directoryGetFilesFunction_ == nullptr ||
            directoryGetDirectoryCountFunction_ == nullptr || directoryGetDirectoriesFunction_ == nullptr ||
            threadCreateFunction_ == nullptr || threadJoinFunction_ == nullptr ||
            threadDetachFunction_ == nullptr ||
            atomicLoadInt32Function_ == nullptr || atomicStoreInt32Function_ == nullptr ||
            atomicLoadInt64Function_ == nullptr || atomicStoreInt64Function_ == nullptr ||
            interlockedIncrementInt32Function_ == nullptr || interlockedDecrementInt32Function_ == nullptr ||
            interlockedAddInt32Function_ == nullptr || interlockedExchangeInt32Function_ == nullptr ||
            interlockedCompareExchangeInt32Function_ == nullptr ||
            interlockedIncrementInt64Function_ == nullptr || interlockedDecrementInt64Function_ == nullptr ||
            interlockedAddInt64Function_ == nullptr || interlockedExchangeInt64Function_ == nullptr ||
            interlockedCompareExchangeInt64Function_ == nullptr ||
            mutexCreateFunction_ == nullptr || mutexLockFunction_ == nullptr ||
            mutexUnlockFunction_ == nullptr || mutexCloseFunction_ == nullptr ||
            getCurrentThreadIdFunction_ == nullptr ||
            monitorEnterFunction_ == nullptr || monitorExitFunction_ == nullptr ||
            monitorPulseFunction_ == nullptr || monitorPulseAllFunction_ == nullptr ||
            monitorWaitFunction_ == nullptr ||
            semaphoreCreateFunction_ == nullptr || waitForHandleTimeoutFunction_ == nullptr ||
            semaphoreReleaseFunction_ == nullptr || semaphoreCloseFunction_ == nullptr ||
            eventCreateFunction_ == nullptr || eventSetFunction_ == nullptr ||
            eventResetFunction_ == nullptr || eventCloseFunction_ == nullptr ||
            nativeThrowFunction_ == nullptr || exceptionCurrentFunction_ == nullptr ||
            exceptionClearFunction_ == nullptr ||
            ehFilterFunction_ == nullptr || sehPersonalityFunction_ == nullptr ||
            memsetFunction_ == nullptr || memcpyFunction_ == nullptr)
        {
            error = "Stage 45.10 failed to declare OOP runtime/EH hooks";
            return false;
        }
        return true;
    }

    void applyOdrDefinition(LLVMValueRef value)
    {
        if (value == nullptr) return;
        // weak_odr, rather than linkonce_odr, is required because a public ODR entity may be listed
        // in a DLL export table even after all of its local calls were inlined at O2. The optimizer
        // may discard an unreferenced linkonce definition; weak_odr keeps the external definition
        // while its symbol-specific COMDAT selects one copy at final link.
        LLVMSetLinkage(value, LLVMWeakODRLinkage);
        std::size_t physicalNameLength = 0;
        const char* physicalNameData = LLVMGetValueName2(value, &physicalNameLength);
        if (physicalNameData == nullptr || physicalNameLength == 0) return;
        const std::string physicalName(physicalNameData, physicalNameLength);
        // COFF requires the section-selection key to identify a real definition in the selected
        // section. Using a detached byte global as the key made function unwind sections form a
        // two-level association (.xdata -> function -> artificial leader); NSLink then rejected
        // the auxiliary .xdata section because it had no usable section-definition key.
        // The physical function/global is already unique and is the standard COFF COMDAT leader.
        LLVMComdatRef comdat = LLVMGetOrInsertComdat(module_, physicalName.c_str());
        LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
        LLVMSetComdat(value, comdat);
    }

    void applyGenericOdrDefinition(LLVMValueRef value, const std::string& specializationKey)
    {
        if (specializationKey.empty()) return;
        applyOdrDefinition(value);
    }

    bool declareStaticFields(std::string& error, bool defineStorage)
    {
        for (const frontend::ProjectOopMirStaticField& field : mirModel_.staticFields())
        {
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

            LLVMValueRef initializer = LLVMConstNull(type);
            if (field.isConst)
            {
                if (!field.hasConstantInitializer)
                {
                    return fail(error, "Stage 37 const field lost its compile-time initializer");
                }
                // La clave de tipo de un miembro de enum es la del propio enum, no
                // "builtin:int"/"builtin:uint", asi que isIntegral() sola no lo veria:
                // isEnumConstant es lo que lo marca. El SIGNO sale del tipo subyacente y no de una
                // suposicion -- un `enum : byte` con un miembro en 200 se emite sin signo.
                const frontend::builtins::IntegerTypeInfo* fieldEnum = enumUnderlyingInfo(field.typeKey);
                if (frontend::ProjectTypeConversion::isIntegral(field.typeKey) || field.isEnumConstant)
                {
                    initializer = LLVMConstInt(
                        type,
                        field.constantIntegerValue,
                        (fieldEnum != nullptr ? fieldEnum->isSigned : isSignedInteger(field.typeKey))
                            ? 1
                            : 0);
                }
                else if (field.typeKey == "builtin:bool")
                {
                    initializer = LLVMConstInt(type, field.constantBooleanValue ? 1U : 0U, 0);
                }
                else if (field.typeKey == "builtin:float" || field.typeKey == "builtin:double")
                {
                    initializer = LLVMConstReal(type, field.constantFloatingValue);
                }
                else if (field.typeKey == "builtin:string")
                {
                    initializer = internStringLiteral(field.constantStringValue, error);
                    if (initializer == nullptr) return false;
                }
                else
                {
                    return fail(error, "Stage 37 const field type is not supported by native constant initialization");
                }
                LLVMSetGlobalConstant(global, 1);
            }
            LLVMSetInitializer(global, initializer);
            // Constants are materialized from the same certified value in producers and
            // consumers. Mark both sides ODR: a DLL consumer keeps a local selectable copy,
            // while a static producer and consumer contribute equivalent COMDAT definitions
            // instead of one strong plus one selectable definition that COFF still rejects.
            if (field.isConst) applyOdrDefinition(global);
        }
        return true;
    }

    bool defineStaticFieldAccessors(std::string& error)
    {
        for (const frontend::ProjectOopMirStaticField& field : mirModel_.staticFields())
        {
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

    bool declareTypeInitializers(std::string& error, bool defineStorage)
    {
        std::unordered_map<std::size_t, bool> needsInitializer;
        for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
        {
            if (initializer.isStatic && !initializer.isConst)
            {
                needsInitializer[initializer.declaringTypeOrdinal] = true;
            }
        }

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

    bool defineTypeInitializers(std::string& error)
    {
        for (const auto& [typeOrdinal, info] : staticInitFunctions_)
        {
            currentFunction_ = info.function;
            currentMirFunction_ = nullptr;
            thisValue_ = nullptr;
            closureEnvironment_ = nullptr;
            constructionThisOverride_ = nullptr;
            parameterStorage_.clear();
            localStorage_.clear();
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
            SourceRange debugRange;
            if (typeOrdinal < nameLookup_.namedTypes().size())
            {
                debugRange = nameLookup_.namedTypes()[typeOrdinal].range;
                if (debugRange.isValid()) currentDebugFileKey_ = debugRange.begin().fileId().value();
            }

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
            LLVMBasicBlockRef casAttempt = appendBlock("cctor.cas_attempt");
            LLVMBasicBlockRef recursionCheck = appendBlock("cctor.recursion_check");
            LLVMBasicBlockRef spinWait = appendBlock("cctor.spin_wait");
            LLVMBasicBlockRef initialize = appendBlock("cctor.initialize");
            LLVMBasicBlockRef done = appendBlock("cctor.done");
            LLVMPositionBuilderAtEnd(builder_, entry);
            ScopedDebugLocation generatedLocation(
                builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
            LLVMValueRef stateGlobal = staticInitStateGlobals_[typeOrdinal];
            LLVMValueRef ownerGlobal = staticInitOwnerGlobals_[typeOrdinal];
            LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
            LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
            LLVMValueRef zero = LLVMConstInt(i8, 0, 0);
            LLVMValueRef one = LLVMConstInt(i8, 1, 0);
            LLVMValueRef two = LLVMConstInt(i8, 2, 0);
            // Fast path: an atomic load lets an already-initialized class (the overwhelming
            // majority of calls -- this function runs on EVERY static-member access, not just the
            // first) skip straight to `done` without ever touching the CAS below.
            LLVMValueRef fastState = LLVMBuildLoad2(builder_, i8, stateGlobal, "cctor.state.fast");
            LLVMSetOrdering(fastState, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMValueRef fastDone = LLVMBuildICmp(builder_, LLVMIntEQ, fastState, two, "cctor.fast_done");
            LLVMBuildCondBr(builder_, fastDone, done, casAttempt);

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
            LLVMValueRef won = LLVMBuildExtractValue(builder_, cas, 1, "cctor.cas.won");
            LLVMBuildCondBr(builder_, won, initialize, recursionCheck);

            LLVMPositionBuilderAtEnd(builder_, recursionCheck);
            LLVMValueRef ownerTid = LLVMBuildLoad2(builder_, i32, ownerGlobal, "cctor.owner_tid");
            LLVMSetOrdering(ownerTid, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMValueRef isSelf = LLVMBuildICmp(builder_, LLVMIntEQ, ownerTid, currentTid, "cctor.is_self");
            LLVMBuildCondBr(builder_, isSelf, done, spinWait);

            LLVMPositionBuilderAtEnd(builder_, spinWait);
            // A losing thread (confirmed, via recursionCheck above, to be a genuinely DIFFERENT
            // thread from whichever one is initializing) spins on an atomic load until the winner
            // publishes state == 2. This window is only ever hit by the (rare) small number of
            // threads that race a class's FIRST static-field access at the same instant -- once
            // state reaches 2 it never changes again, so every later call takes the fast path
            // above instead. A busy spin (rather than a real OS wait) keeps this guard self-
            // contained: no risk of the guard itself needing to wait on a primitive (like Mutex)
            // that might not have finished constructing yet.
            LLVMValueRef spinState = LLVMBuildLoad2(builder_, i8, stateGlobal, "cctor.state.spin");
            LLVMSetOrdering(spinState, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMValueRef spinDone = LLVMBuildICmp(builder_, LLVMIntEQ, spinState, two, "cctor.spin_done");
            LLVMBuildCondBr(builder_, spinDone, done, spinWait);

            LLVMPositionBuilderAtEnd(builder_, initialize);
            // Publish this thread's id BEFORE running any field initializer below -- a
            // self-referencing initializer's recursive call (see the header comment above) must
            // see ITS OWN thread id here in order for recursionCheck to recognize it as
            // self-recursion rather than spin-waiting on itself forever.
            LLVMValueRef ownerStore = LLVMBuildStore(builder_, currentTid, ownerGlobal);
            LLVMSetOrdering(ownerStore, LLVMAtomicOrderingSequentiallyConsistent);
            for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
            {
                if (!initializer.isStatic || initializer.isConst ||
                    initializer.declaringTypeOrdinal != typeOrdinal)
                {
                    continue;
                }
                const auto globalFound = staticFieldGlobals_.find(initializer.memberOrdinal);
                if (globalFound == staticFieldGlobals_.end())
                {
                    return fail(error, "Stage 37 runtime static initializer lost its field global");
                }
                LLVMValueRef value = lowerExpression(initializer.value.get(), error);
                if (value == nullptr)
                {
                    return false;
                }
                LLVMBuildStore(builder_, value, globalFound->second);
            }
            LLVMValueRef publish = LLVMBuildStore(builder_, two, stateGlobal);
            LLVMSetOrdering(publish, LLVMAtomicOrderingSequentiallyConsistent);
            if (!terminated()) LLVMBuildBr(builder_, done);

            LLVMPositionBuilderAtEnd(builder_, done);
            LLVMBuildRetVoid(builder_);
        }
        currentDebugSubprogram_ = nullptr;
        currentDebugScope_ = nullptr;
        currentDebugFileKey_ = 0;
        return true;
    }

    bool declareFunctions(std::string& error)
    {
        const std::span<const frontend::ProjectOopMirFunction> mirFunctions = mirModel_.functions();
        for (std::size_t functionOrdinal = 0; functionOrdinal < mirFunctions.size(); ++functionOrdinal)
        {
            const frontend::ProjectOopMirFunction& function = mirFunctions[functionOrdinal];
            // Win64 aggregate ABI (ground truth: clang for x86_64-pc-windows-msvc). An indirect
            // struct return becomes `void` plus a leading sret pointer parameter; an indirect
            // struct parameter becomes a plain pointer to a caller-owned per-call copy. For
            // instance methods the sret slot sits AFTER `this` (RCX = this, RDX = sret) --
            // confirmed against clang's own C++ output, where member functions carry
            // (ptr this, ptr sret) in that order.
            const bool sretReturn = !function.referenceReturn &&
                isIndirectStructTypeKey(function.returnTypeKey);
            LLVMTypeRef returnType = function.referenceReturn
                ? LLVMPointerTypeInContext(context_, 0)
                : sretReturn
                    ? LLVMVoidTypeInContext(context_)
                    : llvmType(function.returnTypeKey, error);
            if (returnType == nullptr)
            {
                return false;
            }
            std::vector<LLVMTypeRef> parameters;
            if (!function.isStatic)
            {
                parameters.push_back(LLVMPointerTypeInContext(context_, 0));
            }
            if (sretReturn)
            {
                parameters.push_back(LLVMPointerTypeInContext(context_, 0));
            }
            for (const std::string& parameter : function.parameterTypeKeys)
            {
                LLVMTypeRef type = abiParameterType(parameter, error);
                if (type == nullptr || type == LLVMVoidTypeInContext(context_))
                {
                    if (error.empty()) error = "invalid Stage 34 method parameter type: " + parameter;
                    return false;
                }
                parameters.push_back(type);
            }
            LLVMTypeRef functionType = LLVMFunctionType(
                returnType,
                parameters.empty() ? nullptr : parameters.data(),
                static_cast<unsigned>(parameters.size()),
                0);
            // C1.2 "import identity": the same triple (library, symbol, signature) may be
            // DECLARED more than once -- two classes may each wrap `kernel32!GetCurrentProcessId`,
            // and the frontend accepts that because it is ONE import, not a conflict. One import
            // is one LLVM declaration: asking LLVM for the name twice would silently mint
            // `GetCurrentProcessId.1` and the link would end with "undefined symbol:
            // GetCurrentProcessId.1" (measured, by this round's own suite). Every member keeps its
            // own MIR record and simply resolves to the shared declaration.
            if (function.isImportedC || function.isImportedNative)
            {
                const auto existing = functions_.find(function.mangledName);
                if (existing != functions_.end()) continue;
            }
            LLVMValueRef llvmFunction = LLVMAddFunction(module_, function.mangledName.c_str(), functionType);
            if (llvmFunction == nullptr)
            {
                error = "Stage 34 failed to declare LLVM function " + function.mangledName;
                return false;
            }
            const bool ownsDefinition = !function.isImportedC && !function.isImportedNative &&
                (!partitioned_ || (!ownsModuleGlobals_ && functionDefinitions_[functionOrdinal]));
            if (!function.isImportedC && !function.isImportedNative)
            {
                if (ownsDefinition && !function.genericSpecializationKey.empty())
                {
                    applyGenericOdrDefinition(llvmFunction, function.genericSpecializationKey);
                }
                else if (ownsDefinition && function.isSyntheticImplicitConstructor)
                {
                    // An implicit constructor is a compiler-owned ODR definition. The same nominal
                    // type can legitimately reach a final native link through a source dependency
                    // and through a static precompiled producer. Both compilations derive the same
                    // physical symbol and semantics from the type declaration; leaving both copies
                    // strong made the ubiquitous NativeSharp.Core.Object() root collide first.
                    // Written constructors remain strong and therefore still diagnose real duplicate
                    // user implementations.
                    applyOdrDefinition(llvmFunction);
                }
                else if (ownsDefinition &&
                    (function.inliningPolicy == frontend::InliningPolicy::Preferred ||
                     function.inliningPolicy == frontend::InliningPolicy::Required))
                {
                    // A public inline body can be materialized by both its producer and any
                    // NSMETA 1.13 consumer. They share one deterministic physical identity and
                    // therefore require symbol-scoped ODR/COMDAT linkage, never two strong defs.
                    applyOdrDefinition(llvmFunction);
                }
            }
            applyUwtableAttribute(llvmFunction);
            if (function.inliningPolicy == frontend::InliningPolicy::Preferred &&
                !function.isImportedC && !function.isImportedNative)
            {
                applyFunctionAttribute(llvmFunction, "inlinehint");
            }
            else if (function.inliningPolicy == frontend::InliningPolicy::Required &&
                !function.isImportedC && !function.isImportedNative)
            {
                applyFunctionAttribute(llvmFunction, "alwaysinline");
            }
            else if (function.inliningPolicy == frontend::InliningPolicy::Forbidden &&
                !function.isImportedC)
            {
                // A precompiled NativeSharp declaration owns no local body, but its producer's
                // noinline contract still constrains every consumer call site. LLVM accepts the
                // attribute on the declaration and preserves the external call boundary.
                applyFunctionAttribute(llvmFunction, "noinline");
            }
            if (sretReturn)
            {
                applySretAttributes(
                    [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                    { LLVMAddAttributeAtIndex(llvmFunction, index, attribute); },
                    function.isStatic ? 0U : 1U,
                    function.returnTypeKey);
            }
            LLVMMetadataRef subprogram = ownsDefinition ? createDebugSubprogram(function) : nullptr;
            if (debugBuilder_ != nullptr && ownsDefinition &&
                subprogram == nullptr)
            {
                error = "failed to create LLVM debug subprogram for " + function.mangledName;
                return false;
            }
            if (subprogram != nullptr) LLVMSetSubprogram(llvmFunction, subprogram);
            functions_[function.mangledName] = FunctionInfo{llvmFunction, functionType, subprogram};
        }
        return true;
    }

    bool declareDestructionEntries(std::string& error)
    {
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef parameters[] = {ptr};
        destructionEntryFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameters, 1, 0);
        if (destructionEntryFunctionType_ == nullptr)
        {
            return fail(error, "Stage 35 failed to create destruction-entry function type");
        }

        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.isImported) continue;
            if (table.destructionEntryName.empty() || table.objectDestructionEntryName.empty())
            {
                return fail(error, "item 319 class descriptor has incomplete destruction-entry names");
            }
            LLVMValueRef function = LLVMAddFunction(
                module_, table.destructionEntryName.c_str(), destructionEntryFunctionType_);
            if (function == nullptr)
            {
                return fail(error, "Stage 35 failed to declare destruction entry " +
                    table.destructionEntryName);
            }
            if (!partitioned_ || ownsModuleGlobals_)
                applyGenericOdrDefinition(function, table.genericSpecializationKey);
            const bool ownsDefinition = !partitioned_ || ownsModuleGlobals_;
            LLVMMetadataRef subprogram = nullptr;
            if (ownsDefinition && table.typeOrdinal < nameLookup_.namedTypes().size())
            {
                const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[table.typeOrdinal];
                const std::vector<std::string> parameters = {table.typeKey};
                const std::string displayName =
                    type.identity.fullyQualifiedName() + ".destruction-entry [generated]";
                subprogram = createSynthesizedDebugSubprogram(
                    type.range, displayName, table.destructionEntryName, "builtin:void",
                    parameters, true, false);
            }
            if (debugBuilder_ != nullptr && ownsDefinition && subprogram == nullptr)
            {
                return fail(error, "failed to create debug subprogram for destruction entry " +
                    table.destructionEntryName);
            }
            if (subprogram != nullptr) LLVMSetSubprogram(function, subprogram);
            destructionFunctions_[table.typeOrdinal] = FunctionInfo{
                function, destructionEntryFunctionType_, subprogram};

            LLVMValueRef objectFunction = LLVMAddFunction(
                module_, table.objectDestructionEntryName.c_str(), destructionEntryFunctionType_);
            if (objectFunction == nullptr)
            {
                return fail(error, "item 319 failed to declare object-only destruction entry " +
                    table.objectDestructionEntryName);
            }
            if (!partitioned_ || ownsModuleGlobals_)
                applyGenericOdrDefinition(objectFunction, table.genericSpecializationKey);
            LLVMMetadataRef objectSubprogram = nullptr;
            if (ownsDefinition && table.typeOrdinal < nameLookup_.namedTypes().size())
            {
                const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[table.typeOrdinal];
                const std::vector<std::string> parameters = {table.typeKey};
                const std::string displayName =
                    type.identity.fullyQualifiedName() + ".object-destruction-entry [generated]";
                objectSubprogram = createSynthesizedDebugSubprogram(
                    type.range, displayName, table.objectDestructionEntryName, "builtin:void",
                    parameters, true, false);
            }
            if (debugBuilder_ != nullptr && ownsDefinition && objectSubprogram == nullptr)
            {
                return fail(error, "failed to create debug subprogram for object-only destruction entry " +
                    table.objectDestructionEntryName);
            }
            if (objectSubprogram != nullptr) LLVMSetSubprogram(objectFunction, objectSubprogram);
            objectDestructionFunctions_[table.typeOrdinal] = FunctionInfo{
                objectFunction, destructionEntryFunctionType_, objectSubprogram};
        }
        return true;
    }

    bool emitDispatchMetadata(std::string& error, bool defineStorage)
    {
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef interfaceEntryFields[] = {i64, ptr};
        interfaceMapEntryType_ = LLVMStructCreateNamed(context_, "nativesharp.interface_map_entry");
        if (interfaceMapEntryType_ == nullptr)
        {
            return fail(error, "Stage 36 failed to create interface-map entry type");
        }
        LLVMStructSetBody(interfaceMapEntryType_, interfaceEntryFields, 2, 0);
        LLVMTypeRef interfaceMapFields[] = {i64, ptr};
        interfaceMapType_ = LLVMStructCreateNamed(context_, "nativesharp.interface_map");
        if (interfaceMapType_ == nullptr)
        {
            return fail(error, "Stage 36 failed to create interface-map header type");
        }
        LLVMStructSetBody(interfaceMapType_, interfaceMapFields, 2, 0);
        // C8 "type root" appends field 6: a pointer to this type's NAME, laid out exactly like a
        // string literal ({i64 length, [N+1 x i8] bytes}) so NativeSharp.Core.Object.ToString()
        // can hand it back as a `string` with a single load and no allocation. Appended rather
        // than inserted so every existing field index in this file stays valid.
        // Item 319 appends field 7. It has the same `void(ptr)` ABI as field 5, but destroys only
        // the object lifetime and deliberately leaves the backing storage untouched.
        LLVMTypeRef descriptorFields[] = {ptr, ptr, i64, i64, ptr, ptr, ptr, ptr};
        descriptorType_ = LLVMStructCreateNamed(context_, "nativesharp.class_descriptor");
        if (descriptorType_ == nullptr)
        {
            return fail(error, "Macro A failed to create NativeSharp class-descriptor type");
        }
        LLVMStructSetBody(descriptorType_, descriptorFields, 8, 0);
        descriptorAccessorFunctionType_ = LLVMFunctionType(ptr, nullptr, 0, 0);
        if (descriptorAccessorFunctionType_ == nullptr)
        {
            return fail(error, "1.0-E1 failed to create class-descriptor accessor type");
        }

        // A local class can inherit (directly or through another local class) from a class whose
        // descriptor lives in a precompiled static library or DLL. NSMETA deliberately publishes
        // an accessor for that descriptor rather than promising that the descriptor DATA symbol
        // is linkable -- the latter is not true for the DLL product. Mark the complete local
        // descendant chain now so its descriptor can be completed at runtime through accessors.
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.isImported) runtimeDescriptorAccessorTypes_.insert(table.typeOrdinal);
        }
        bool addedRuntimeDescriptor = true;
        while (addedRuntimeDescriptor)
        {
            addedRuntimeDescriptor = false;
            for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
            {
                if (table.isImported || !table.baseTypeOrdinal.has_value() ||
                    !runtimeDescriptorAccessorTypes_.contains(*table.baseTypeOrdinal))
                {
                    continue;
                }
                addedRuntimeDescriptor =
                    runtimeDescriptorAccessorTypes_.insert(table.typeOrdinal).second || addedRuntimeDescriptor;
            }
        }

        if (!defineStorage)
        {
            // Function code units need only the externally visible descriptor data and accessor
            // declarations used by construction/casts. Vtables, interface maps, type-name data
            // and accessor bodies have exactly one definition in the coordinator module.
            for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
            {
                if (table.descriptorAccessorName.empty())
                {
                    return fail(error, "partitioned class has no descriptor accessor");
                }
                LLVMValueRef accessor = LLVMAddFunction(
                    module_, table.descriptorAccessorName.c_str(), descriptorAccessorFunctionType_);
                if (accessor == nullptr)
                {
                    return fail(error, "failed to declare partitioned class-descriptor accessor " +
                        table.descriptorAccessorName);
                }
                descriptorAccessorFunctions_[table.typeOrdinal] =
                    FunctionInfo{accessor, descriptorAccessorFunctionType_};
                if (table.isImported) continue;

                LLVMValueRef descriptor = LLVMAddGlobal(
                    module_, descriptorType_, table.descriptorName.c_str());
                if (descriptor == nullptr)
                {
                    return fail(error, "failed to declare partitioned class descriptor " +
                        table.descriptorName);
                }
                LLVMSetLinkage(descriptor, LLVMExternalLinkage);
                descriptorGlobals_[table.typeOrdinal] = descriptor;
            }
            return true;
        }

        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.isImported)
            {
                if (table.descriptorAccessorName.empty())
                {
                    return fail(error, "1.0-E1 imported class has no descriptor accessor");
                }
                LLVMValueRef accessor = LLVMAddFunction(
                    module_, table.descriptorAccessorName.c_str(), descriptorAccessorFunctionType_);
                if (accessor == nullptr)
                {
                    return fail(error, "1.0-E1 failed to declare imported class-descriptor accessor " +
                        table.descriptorAccessorName);
                }
                descriptorAccessorFunctions_[table.typeOrdinal] =
                    FunctionInfo{accessor, descriptorAccessorFunctionType_};
                continue;
            }
            LLVMValueRef vtableGlobal = nullptr;
            if (!table.slotCalleeNames.empty())
            {
                LLVMTypeRef arrayType = LLVMArrayType2(ptr, table.slotCalleeNames.size());
                vtableGlobal = LLVMAddGlobal(module_, arrayType, table.vtableName.c_str());
                if (vtableGlobal == nullptr)
                {
                    return fail(error, "Macro A failed to declare vtable global " + table.vtableName);
                }
                std::vector<LLVMValueRef> entries;
                entries.reserve(table.slotCalleeNames.size());
                for (const std::string& callee : table.slotCalleeNames)
                {
                    if (callee.empty())
                    {
                        entries.push_back(LLVMConstNull(ptr));
                        continue;
                    }
                    const auto found = functions_.find(callee);
                    if (found == functions_.end())
                    {
                        return fail(error, "Stage 35 vtable target was not declared: " + callee);
                    }
                    entries.push_back(found->second.function);
                }
                LLVMValueRef initializer = LLVMConstArray2(
                    ptr, entries.data(), static_cast<std::uint64_t>(entries.size()));
                LLVMSetInitializer(vtableGlobal, initializer);
                LLVMSetGlobalConstant(vtableGlobal, 1);
                applyGenericOdrDefinition(vtableGlobal, table.genericSpecializationKey);
                vtableGlobals_[table.typeOrdinal] = vtableGlobal;
            }

            std::vector<LLVMValueRef> interfaceMapEntries;
            for (const frontend::ProjectOopMirDispatchTable::InterfaceTable& interfaceTable :
                 table.interfaceTables)
            {
                LLVMTypeRef methodArrayType = LLVMArrayType2(ptr, interfaceTable.calleeNames.size());
                LLVMValueRef methodTable = LLVMAddGlobal(
                    module_, methodArrayType, interfaceTable.methodTableName.c_str());
                if (methodTable == nullptr)
                {
                    return fail(error, "Stage 36 failed to declare interface method table " +
                        interfaceTable.methodTableName);
                }
                std::vector<LLVMValueRef> methods;
                methods.reserve(interfaceTable.calleeNames.size());
                for (const std::string& callee : interfaceTable.calleeNames)
                {
                    if (callee.empty())
                    {
                        methods.push_back(LLVMConstNull(ptr));
                        continue;
                    }
                    const auto found = functions_.find(callee);
                    if (found == functions_.end())
                    {
                        return fail(error, "Stage 36 interface implementation was not declared: " + callee);
                    }
                    methods.push_back(found->second.function);
                }
                LLVMValueRef methodInitializer = LLVMConstArray2(
                    ptr, methods.data(), static_cast<std::uint64_t>(methods.size()));
                LLVMSetInitializer(methodTable, methodInitializer);
                LLVMSetGlobalConstant(methodTable, 1);
                applyGenericOdrDefinition(methodTable, table.genericSpecializationKey);

                LLVMValueRef entryFields[] = {
                    LLVMConstInt(i64, interfaceTable.stableInterfaceTypeId, 0),
                    methodTable
                };
                interfaceMapEntries.push_back(
                    LLVMConstNamedStruct(interfaceMapEntryType_, entryFields, 2));
            }
            if (!interfaceMapEntries.empty())
            {
                LLVMTypeRef entriesArrayType = LLVMArrayType2(
                    interfaceMapEntryType_, interfaceMapEntries.size());
                const std::string entriesName = table.interfaceMapName + "__entries";
                LLVMValueRef entriesGlobal = LLVMAddGlobal(
                    module_, entriesArrayType, entriesName.c_str());
                if (entriesGlobal == nullptr)
                {
                    return fail(error, "Stage 36 failed to declare interface-map entries");
                }
                LLVMValueRef entriesInitializer = LLVMConstArray2(
                    interfaceMapEntryType_, interfaceMapEntries.data(),
                    static_cast<std::uint64_t>(interfaceMapEntries.size()));
                LLVMSetInitializer(entriesGlobal, entriesInitializer);
                LLVMSetGlobalConstant(entriesGlobal, 1);
                applyGenericOdrDefinition(entriesGlobal, table.genericSpecializationKey);

                LLVMValueRef mapGlobal = LLVMAddGlobal(
                    module_, interfaceMapType_, table.interfaceMapName.c_str());
                if (mapGlobal == nullptr)
                {
                    return fail(error, "Stage 36 failed to declare interface-map header");
                }
                LLVMValueRef mapFields[] = {
                    LLVMConstInt(i64, interfaceMapEntries.size(), 0),
                    entriesGlobal
                };
                LLVMSetInitializer(mapGlobal, LLVMConstNamedStruct(interfaceMapType_, mapFields, 2));
                LLVMSetGlobalConstant(mapGlobal, 1);
                applyGenericOdrDefinition(mapGlobal, table.genericSpecializationKey);
                interfaceMapGlobals_[table.typeOrdinal] = mapGlobal;
            }

            LLVMValueRef descriptor = LLVMAddGlobal(
                module_, descriptorType_, table.descriptorName.c_str());
            if (descriptor == nullptr)
            {
                return fail(error, "Macro A failed to declare class descriptor " + table.descriptorName);
            }
            descriptorGlobals_[table.typeOrdinal] = descriptor;
            applyGenericOdrDefinition(descriptor, table.genericSpecializationKey);

            if (table.descriptorAccessorName.empty())
            {
                return fail(error, "1.0-E1 local class has no descriptor-accessor name");
            }
            LLVMValueRef accessor = LLVMAddFunction(
                module_, table.descriptorAccessorName.c_str(), descriptorAccessorFunctionType_);
            if (accessor == nullptr)
            {
                return fail(error, "1.0-E1 failed to declare class-descriptor accessor " +
                    table.descriptorAccessorName);
            }
            descriptorAccessorFunctions_[table.typeOrdinal] =
                FunctionInfo{accessor, descriptorAccessorFunctionType_};
            applyGenericOdrDefinition(accessor, table.genericSpecializationKey);
        }

        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.isImported) continue;
            const auto descriptorFound = descriptorGlobals_.find(table.typeOrdinal);
            if (descriptorFound == descriptorGlobals_.end())
            {
                return fail(error, "Macro A lost a class descriptor during initialization");
            }

            LLVMValueRef baseDescriptor = LLVMConstNull(ptr);
            if (table.baseTypeOrdinal.has_value())
            {
                if (!runtimeDescriptorAccessorTypes_.contains(table.typeOrdinal))
                {
                    const auto baseFound = descriptorGlobals_.find(*table.baseTypeOrdinal);
                    if (baseFound == descriptorGlobals_.end())
                    {
                        return fail(error, "Macro A descriptor cannot resolve its local base descriptor");
                    }
                    baseDescriptor = baseFound->second;
                }
            }

            LLVMValueRef vtable = LLVMConstNull(ptr);
            const auto vtableFound = vtableGlobals_.find(table.typeOrdinal);
            if (vtableFound != vtableGlobals_.end())
            {
                vtable = vtableFound->second;
            }

            const auto destructionFound = destructionFunctions_.find(table.typeOrdinal);
            if (destructionFound == destructionFunctions_.end())
            {
                return fail(error, "Stage 35 descriptor cannot resolve its destruction entry");
            }
            const auto objectDestructionFound = objectDestructionFunctions_.find(table.typeOrdinal);
            if (objectDestructionFound == objectDestructionFunctions_.end())
            {
                return fail(error, "item 319 descriptor cannot resolve its object-only destruction entry");
            }

            LLVMValueRef interfaceMap = LLVMConstNull(ptr);
            const auto interfaceMapFound = interfaceMapGlobals_.find(table.typeOrdinal);
            if (interfaceMapFound != interfaceMapGlobals_.end())
            {
                interfaceMap = interfaceMapFound->second;
            }

            LLVMValueRef typeName = internTypeNameConstant(
                table.typeNameGlobalName, table.typeDisplayName);
            if (typeName == nullptr)
            {
                return fail(error, "C8 descriptor cannot materialize its type name constant");
            }
            applyGenericOdrDefinition(typeName, table.genericSpecializationKey);

            LLVMValueRef values[] = {
                baseDescriptor,
                vtable,
                LLVMConstInt(i64, table.slotMemberOrdinals.size(), 0),
                LLVMConstInt(i64, table.stableTypeId, 0),
                interfaceMap,
                destructionFound->second.function,
                typeName,
                objectDestructionFound->second.function
            };
            LLVMValueRef initializer = LLVMConstNamedStruct(descriptorType_, values, 8);
            LLVMSetInitializer(descriptorFound->second, initializer);
            LLVMSetGlobalConstant(
                descriptorFound->second,
                runtimeDescriptorAccessorTypes_.contains(table.typeOrdinal) ? 0 : 1);

            const auto accessorFound = descriptorAccessorFunctions_.find(table.typeOrdinal);
            if (accessorFound == descriptorAccessorFunctions_.end())
            {
                return fail(error, "1.0-E1 lost a declared class-descriptor accessor");
            }
            LLVMValueRef accessor = accessorFound->second.function;
            LLVMBasicBlockRef accessorEntry = LLVMAppendBasicBlockInContext(context_, accessor, "entry");
            LLVMBuilderRef accessorBuilder = LLVMCreateBuilderInContext(context_);
            LLVMPositionBuilderAtEnd(accessorBuilder, accessorEntry);
            if (runtimeDescriptorAccessorTypes_.contains(table.typeOrdinal))
            {
                if (!table.baseTypeOrdinal.has_value())
                {
                    LLVMDisposeBuilder(accessorBuilder);
                    return fail(error, "1.0-E1 runtime descriptor has no base type");
                }
                const auto baseAccessor = descriptorAccessorFunctions_.find(*table.baseTypeOrdinal);
                if (baseAccessor == descriptorAccessorFunctions_.end())
                {
                    LLVMDisposeBuilder(accessorBuilder);
                    return fail(error, "1.0-E1 runtime descriptor cannot resolve its base accessor");
                }
                LLVMValueRef resolvedBase = LLVMBuildCall2(
                    accessorBuilder,
                    baseAccessor->second.functionType,
                    baseAccessor->second.function,
                    nullptr,
                    0,
                    "base.classdesc");
                LLVMValueRef baseField = LLVMBuildStructGEP2(
                    accessorBuilder,
                    descriptorType_,
                    descriptorFound->second,
                    0,
                    "base.classdesc.addr");
                // More than one thread may construct the first object of this type. Publish the
                // identical base pointer exactly once; a plain repeated store would race with
                // another accessor or with RTTI reading the completed descriptor chain.
                LLVMBuildAtomicCmpXchg(
                    accessorBuilder,
                    baseField,
                    LLVMConstNull(ptr),
                    resolvedBase,
                    LLVMAtomicOrderingSequentiallyConsistent,
                    LLVMAtomicOrderingSequentiallyConsistent,
                    0);
            }
            LLVMBuildRet(accessorBuilder, descriptorFound->second);
            LLVMDisposeBuilder(accessorBuilder);
        }
        return true;
    }

    bool defineRttiHelpers(std::string& error)
    {
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr || interfaceMapEntryType_ == nullptr)
        {
            return fail(error, "Stage 39 RTTI helpers require completed descriptor/interface-map LLVM types");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i1 = LLVMInt1TypeInContext(context_);
        LLVMTypeRef parameters[] = {ptr, i64};
        rttiTestFunctionType_ = LLVMFunctionType(i1, parameters, 2, 0);
        rttiClassTestFunction_ = LLVMAddFunction(
            module_, "__nativesharp_rtti_is_class", rttiTestFunctionType_);
        rttiInterfaceTestFunction_ = LLVMAddFunction(
            module_, "__nativesharp_rtti_is_interface", rttiTestFunctionType_);
        if (rttiTestFunctionType_ == nullptr || rttiClassTestFunction_ == nullptr ||
            rttiInterfaceTestFunction_ == nullptr)
        {
            return fail(error, "Stage 39 failed to declare RTTI helper functions");
        }
        // NativeSharp 1.0.1 / composable libraries. These helpers inspect an object's public
        // descriptor shape but are implementation details of the module that emits the test.
        // External linkage made every independently compiled .lib define the same two strong
        // COFF symbols; the first real producer/consumer link therefore failed before main.
        // Keeping them internal is also stronger than COMDAT folding: no cross-module identity is
        // promised and each module may inline or optimize its own copy independently.
        LLVMSetLinkage(rttiClassTestFunction_, LLVMInternalLinkage);
        LLVMSetLinkage(rttiInterfaceTestFunction_, LLVMInternalLinkage);

        currentFunction_ = rttiClassTestFunction_;
        blockCounter_ = 0;
        LLVMBasicBlockRef classEntry = appendBlock("rtti.class.entry");
        LLVMBasicBlockRef classStart = appendBlock("rtti.class.start");
        LLVMBasicBlockRef classLoop = appendBlock("rtti.class.loop");
        LLVMBasicBlockRef classProbe = appendBlock("rtti.class.probe");
        LLVMBasicBlockRef classNext = appendBlock("rtti.class.next");
        LLVMBasicBlockRef classMatch = appendBlock("rtti.class.match");
        LLVMBasicBlockRef classFail = appendBlock("rtti.class.fail");
        LLVMPositionBuilderAtEnd(builder_, classEntry);
        LLVMValueRef classObject = LLVMGetParam(rttiClassTestFunction_, 0);
        LLVMValueRef classTargetId = LLVMGetParam(rttiClassTestFunction_, 1);
        LLVMSetValueName2(classObject, "object", 6);
        LLVMSetValueName2(classTargetId, "target_type_id", 14);
        LLVMValueRef descriptorSlot = LLVMBuildAlloca(builder_, ptr, "descriptor.slot");
        LLVMValueRef classHasObject = LLVMBuildICmp(
            builder_, LLVMIntNE, classObject, LLVMConstNull(ptr), "object.nonnull");
        LLVMBuildCondBr(builder_, classHasObject, classStart, classFail);

        LLVMPositionBuilderAtEnd(builder_, classStart);
        LLVMValueRef firstDescriptor = LLVMBuildLoad2(builder_, ptr, classObject, "descriptor");
        LLVMBuildStore(builder_, firstDescriptor, descriptorSlot);
        LLVMBuildBr(builder_, classLoop);

        LLVMPositionBuilderAtEnd(builder_, classLoop);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, descriptorSlot, "descriptor.current");
        LLVMValueRef hasDescriptor = LLVMBuildICmp(
            builder_, LLVMIntNE, descriptor, LLVMConstNull(ptr), "descriptor.nonnull");
        LLVMBuildCondBr(builder_, hasDescriptor, classProbe, classFail);

        LLVMPositionBuilderAtEnd(builder_, classProbe);
        LLVMValueRef typeIdAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 3, "descriptor.type_id.addr");
        LLVMValueRef typeId = LLVMBuildLoad2(builder_, i64, typeIdAddress, "descriptor.type_id");
        LLVMValueRef classMatches = LLVMBuildICmp(
            builder_, LLVMIntEQ, typeId, classTargetId, "descriptor.matches");
        LLVMBuildCondBr(builder_, classMatches, classMatch, classNext);

        LLVMPositionBuilderAtEnd(builder_, classNext);
        LLVMValueRef baseAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 0, "descriptor.base.addr");
        LLVMValueRef baseDescriptor = LLVMBuildLoad2(builder_, ptr, baseAddress, "descriptor.base");
        LLVMBuildStore(builder_, baseDescriptor, descriptorSlot);
        LLVMBuildBr(builder_, classLoop);

        LLVMPositionBuilderAtEnd(builder_, classMatch);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 1, 0));
        LLVMPositionBuilderAtEnd(builder_, classFail);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 0, 0));

        currentFunction_ = rttiInterfaceTestFunction_;
        blockCounter_ = 0;
        LLVMBasicBlockRef ifaceEntry = appendBlock("rtti.interface.entry");
        LLVMBasicBlockRef ifaceStart = appendBlock("rtti.interface.start");
        LLVMBasicBlockRef ifaceMap = appendBlock("rtti.interface.map");
        LLVMBasicBlockRef ifaceSearch = appendBlock("rtti.interface.search");
        LLVMBasicBlockRef ifaceProbe = appendBlock("rtti.interface.probe");
        LLVMBasicBlockRef ifaceNext = appendBlock("rtti.interface.next");
        LLVMBasicBlockRef ifaceMatch = appendBlock("rtti.interface.match");
        LLVMBasicBlockRef ifaceFail = appendBlock("rtti.interface.fail");
        LLVMPositionBuilderAtEnd(builder_, ifaceEntry);
        LLVMValueRef ifaceObject = LLVMGetParam(rttiInterfaceTestFunction_, 0);
        LLVMValueRef ifaceTargetId = LLVMGetParam(rttiInterfaceTestFunction_, 1);
        LLVMSetValueName2(ifaceObject, "object", 6);
        LLVMSetValueName2(ifaceTargetId, "target_interface_id", 19);
        LLVMValueRef ifaceHasObject = LLVMBuildICmp(
            builder_, LLVMIntNE, ifaceObject, LLVMConstNull(ptr), "object.nonnull");
        LLVMBuildCondBr(builder_, ifaceHasObject, ifaceStart, ifaceFail);

        LLVMPositionBuilderAtEnd(builder_, ifaceStart);
        LLVMValueRef ifaceDescriptor = LLVMBuildLoad2(builder_, ptr, ifaceObject, "descriptor");
        LLVMValueRef mapAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, ifaceDescriptor, 4, "descriptor.interface_map.addr");
        LLVMValueRef map = LLVMBuildLoad2(builder_, ptr, mapAddress, "interface_map");
        LLVMValueRef hasMap = LLVMBuildICmp(
            builder_, LLVMIntNE, map, LLVMConstNull(ptr), "interface_map.nonnull");
        LLVMBuildCondBr(builder_, hasMap, ifaceMap, ifaceFail);

        LLVMPositionBuilderAtEnd(builder_, ifaceMap);
        LLVMValueRef countAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 0, "interface_map.count.addr");
        LLVMValueRef count = LLVMBuildLoad2(builder_, i64, countAddress, "interface_map.count");
        LLVMValueRef entriesAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 1, "interface_map.entries.addr");
        LLVMValueRef entries = LLVMBuildLoad2(builder_, ptr, entriesAddress, "interface_map.entries");
        LLVMBuildBr(builder_, ifaceSearch);

        LLVMPositionBuilderAtEnd(builder_, ifaceSearch);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "interface_map.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {ifaceMap};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef inRange = LLVMBuildICmp(
            builder_, LLVMIntULT, index, count, "interface_map.inrange");
        LLVMBuildCondBr(builder_, inRange, ifaceProbe, ifaceFail);

        LLVMPositionBuilderAtEnd(builder_, ifaceProbe);
        LLVMValueRef entryIndices[] = {index};
        LLVMValueRef entry = LLVMBuildGEP2(
            builder_, interfaceMapEntryType_, entries, entryIndices, 1, "interface_entry");
        LLVMValueRef idAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 0, "interface_entry.id.addr");
        LLVMValueRef id = LLVMBuildLoad2(builder_, i64, idAddress, "interface_entry.id");
        LLVMValueRef ifaceMatches = LLVMBuildICmp(
            builder_, LLVMIntEQ, id, ifaceTargetId, "interface_entry.matches");
        LLVMBuildCondBr(builder_, ifaceMatches, ifaceMatch, ifaceNext);

        LLVMPositionBuilderAtEnd(builder_, ifaceNext);
        LLVMValueRef one = LLVMConstInt(i64, 1, 0);
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, index, one, "interface_map.next");
        LLVMBuildBr(builder_, ifaceSearch);
        LLVMValueRef nextValues[] = {nextIndex};
        LLVMBasicBlockRef nextBlocks[] = {ifaceNext};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        LLVMPositionBuilderAtEnd(builder_, ifaceMatch);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 1, 0));
        LLVMPositionBuilderAtEnd(builder_, ifaceFail);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 0, 0));

        currentFunction_ = nullptr;
        blockCounter_ = 0;
        return true;
    }

    void collectTypedCatchTypeOrdinals(
        const frontend::ProjectOopMirNode* node,
        std::vector<std::size_t>& ordinals) const
    {
        if (node == nullptr) return;
        if (node->kind == frontend::ProjectOopMirNodeKind::CatchRegion &&
            !node->catchAll && node->exceptionTypeOrdinal.has_value() &&
            std::find(ordinals.begin(), ordinals.end(), *node->exceptionTypeOrdinal) == ordinals.end())
        {
            ordinals.push_back(*node->exceptionTypeOrdinal);
        }
        for (const auto& child : node->children)
            collectTypedCatchTypeOrdinals(child.get(), ordinals);
    }

    bool defineTypedCatchFilters(std::string& error)
    {
        if (ehFilterFunctionType_ == nullptr || ehFilterFunction_ == nullptr ||
            exceptionCurrentFunctionType_ == nullptr || exceptionCurrentFunction_ == nullptr ||
            rttiTestFunctionType_ == nullptr || rttiClassTestFunction_ == nullptr)
        {
            return fail(error, "Stage 45.11 typed catch filters require runtime and RTTI helpers");
        }

        std::vector<std::size_t> typedCatchOrdinals;
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            if (function.body == nullptr) continue; // imported C declarations carry no body
            collectTypedCatchTypeOrdinals(function.body.get(), typedCatchOrdinals);
        }

        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        for (std::size_t typeOrdinal : typedCatchOrdinals)
        {
            const frontend::ProjectOopMirDispatchTable* table = nullptr;
            for (const frontend::ProjectOopMirDispatchTable& candidate : mirModel_.dispatchTables())
            {
                if (candidate.typeOrdinal == typeOrdinal)
                {
                    table = &candidate;
                    break;
                }
            }
            if (table == nullptr)
                return fail(error, "Stage 45.11 typed catch has no class descriptor dispatch table");

            const std::string name = "__nativesharp_eh_filter_type_" +
                std::to_string(table->stableTypeId);
            LLVMValueRef function = LLVMAddFunction(module_, name.c_str(), ehFilterFunctionType_);
            if (function == nullptr)
            {
                return fail(error, "Stage 45.11 failed to declare typed catch filter " + name);
            }
            // A typed filter is referenced only by this module's own Win64 EH tables. It is not a
            // runtime ABI entry and must not compete with the identical implementation detail
            // emitted by another precompiled producer or consumer for the same caught type.
            LLVMSetLinkage(function, LLVMInternalLinkage);
            LLVMMetadataRef subprogram = nullptr;
            SourceRange debugRange;
            if (typeOrdinal < nameLookup_.namedTypes().size())
            {
                const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[typeOrdinal];
                debugRange = type.range;
                const std::vector<std::string> parameters = {"builtin:void*", "builtin:void*"};
                const std::string displayName =
                    type.identity.fullyQualifiedName() + ".catch-filter [generated]";
                subprogram = createSynthesizedDebugSubprogram(
                    debugRange, displayName, name, "builtin:int", parameters, true, false);
            }
            if (debugBuilder_ != nullptr && subprogram == nullptr)
            {
                return fail(error, "failed to create debug subprogram for typed catch filter " + name);
            }
            if (subprogram != nullptr) LLVMSetSubprogram(function, subprogram);
            typedCatchFilters_[typeOrdinal] = function;
            // Multi-catch fix (see lowerTryRegion's catchswitch comment): the parent-side clause
            // dispatch tests the caught object with __nativesharp_rtti_is_class, which needs the
            // same stable type id this filter compiles in, so remember it here rather than
            // re-scanning the dispatch tables at every catch site.
            typedCatchTypeIds_[typeOrdinal] = table->stableTypeId;

            currentFunction_ = function;
            currentDebugSubprogram_ = subprogram;
            currentDebugScope_ = currentDebugSubprogram_;
            currentDebugFileKey_ = debugRange.isValid() ? debugRange.begin().fileId().value() : 0;
            blockCounter_ = 0;
            LLVMBasicBlockRef entry = appendBlock("eh.typed.filter.entry");
            LLVMBasicBlockRef typeCheck = appendBlock("eh.typed.filter.type");
            LLVMBasicBlockRef reject = appendBlock("eh.typed.filter.reject");
            LLVMPositionBuilderAtEnd(builder_, entry);
            ScopedDebugLocation generatedLocation(
                builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));

            LLVMValueRef domainArguments[] = {LLVMGetParam(function, 0), LLVMGetParam(function, 1)};
            LLVMValueRef domainResult = LLVMBuildCall2(
                builder_, ehFilterFunctionType_, ehFilterFunction_, domainArguments, 2,
                "eh.domain.match");
            LLVMValueRef domainMatches = LLVMBuildICmp(
                builder_, LLVMIntNE, domainResult, LLVMConstInt(i32, 0, 0),
                "eh.domain.matches");
            LLVMBuildCondBr(builder_, domainMatches, typeCheck, reject);

            LLVMPositionBuilderAtEnd(builder_, typeCheck);
            LLVMValueRef exception = LLVMBuildCall2(
                builder_, exceptionCurrentFunctionType_, exceptionCurrentFunction_,
                nullptr, 0, "eh.filter.exception");
            LLVMValueRef rttiArguments[] = {
                exception,
                LLVMConstInt(i64, table->stableTypeId, 0)};
            LLVMValueRef classMatches = LLVMBuildCall2(
                builder_, rttiTestFunctionType_, rttiClassTestFunction_,
                rttiArguments, 2, "eh.type.matches");
            LLVMValueRef filterResult = LLVMBuildZExt(
                builder_, classMatches, i32, "eh.type.filter.result");
            LLVMBuildRet(builder_, filterResult);

            LLVMPositionBuilderAtEnd(builder_, reject);
            LLVMBuildRet(builder_, LLVMConstInt(i32, 0, 0));
        }

        currentFunction_ = nullptr;
        currentDebugSubprogram_ = nullptr;
        currentDebugScope_ = nullptr;
        currentDebugFileKey_ = 0;
        blockCounter_ = 0;
        return true;
    }

    bool defineFunctions(std::string& error)
    {
        const std::span<const frontend::ProjectOopMirFunction> mirFunctions = mirModel_.functions();
        for (std::size_t functionOrdinal = 0; functionOrdinal < mirFunctions.size(); ++functionOrdinal)
        {
            const frontend::ProjectOopMirFunction& function = mirFunctions[functionOrdinal];
            // Interop Foundation: imported C functions stay DECLARATIONS -- declareFunctions gave
            // them their (ABI-mapped) prototype and the definition lives in the external DLL.
            // C1.2 "import identity" is the one exception: an import whose C symbol name is
            // claimed by two libraries has no import-library slot to be declared against, so it
            // gets a real body that binds the external symbol at first call.
            if (function.isImportedC || function.isImportedNative)
            {
                if (partitioned_ && !ownsModuleGlobals_) continue;
                // Same deduplication as declareFunctions: one identity, one body. The set is what
                // keeps a second declaration of the same import from redefining it.
                if (function.importRequiresRuntimeBinding &&
                    definedRuntimeBoundImports_.insert(function.mangledName).second &&
                    !defineRuntimeBoundImport(function, error))
                {
                    return false;
                }
                continue;
            }
            if (partitioned_ &&
                (ownsModuleGlobals_ || !functionDefinitions_[functionOrdinal]))
            {
                continue;
            }
            if (!defineFunction(function, error))
            {
                return false;
            }
        }
        return true;
    }

    // C1.3 "callback exception boundary".
    //
    // THE CONTRACT, stated once: a NativeSharp exception cannot leave NativeSharp code through a
    // C boundary. It is not a style rule -- a NativeSharp `throw` is a Win64 SEH RaiseException,
    // and letting it continue past the boundary unwinds FOREIGN frames (a Win32 message pump, an
    // OpenGL driver, an SDL event loop) whose cleanup contract this compiler knows nothing about.
    // Whatever the foreign code would then do with a strange exception code, none of it is a
    // behaviour NativeSharp can define, so the boundary refuses to produce it.
    //
    // WHERE THE BOUNDARY IS. Exactly the two places foreign code can enter: a function whose
    // address was handed out as an `extern C delegate` (a callback), and an `export extern C`
    // entry point. Both get the SAME guard, because both are "C called us".
    //
    // WHAT THE GUARD IS. A thunk with the target's own signature -- which since Stage 40.2 IS the
    // Win64 C ABI for every C-expressible type -- that invokes the target inside one catchpad
    // filtered by `__nativesharp_eh_filter` (the filter that accepts this compiler's own
    // exception code and nothing else) and traps there. Two consequences worth being explicit
    // about:
    //
    //   * A FOREIGN SEH exception is NOT caught. The filter declines it, the catchswitch unwinds
    //     to caller, and an access violation raised inside a callback keeps propagating to
    //     whatever the host installed -- the boundary contains this compiler's exceptions, it
    //     does not swallow the operating system's.
    //   * The guard costs nothing on the success path: an invoke with no cleanup between it and
    //     the return, plus the unwind edge in the function's own exception table.
    //
    // The thunk is cached per target so that converting the same method twice yields the SAME
    // function pointer -- a host that compares two callbacks it was handed must still see them as
    // equal.
    bool defineCallbackGuardThunks(std::string& error, bool defineBodies)
    {
        std::vector<std::string> targets;
        const auto visit = [&](const frontend::ProjectOopMirNode* node, auto&& self) -> void
        {
            if (node == nullptr) return;
            if ((node->kind == frontend::ProjectOopMirNodeKind::DelegateCreate ||
                 node->kind == frontend::ProjectOopMirNodeKind::LambdaCreate) &&
                isExternCDelegateType(node->typeKey) && !node->calleeName.empty() &&
                std::find(targets.begin(), targets.end(), node->calleeName) == targets.end())
            {
                targets.push_back(node->calleeName);
            }
            for (const auto& child : node->children) self(child.get(), self);
        };
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            visit(function.body.get(), visit);
        }
        for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
        {
            visit(initializer.value.get(), visit);
        }
        for (const std::string& target : targets)
        {
            if (callbackGuardThunk(target, error, defineBodies) == nullptr) return false;
        }
        return true;
    }

    // C1.3 "callback exception boundary": the handler half, shared by both doors foreign code
    // enters through (an `extern C delegate` target's guard thunk and an `export extern C`
    // wrapper). The builder must already sit in the invoke's unwind block. One catchpad, filtered
    // by __nativesharp_eh_filter -- which accepts this compiler's own exception code and NOTHING
    // else, so a foreign SEH exception (an access violation raised inside the callback, say) is
    // declined here and keeps propagating to whatever the host installed. The catchswitch unwinds
    // to caller for the same reason. Reaching the trap means a NativeSharp exception was about to
    // unwind foreign frames; the process stops here instead, with a defined exit code.
    bool emitCallbackBoundaryHandler(std::string& error)
    {
        if (ehFilterFunction_ == nullptr || trapFunction_ == nullptr)
        {
            return fail(error, "C1.3 callback boundary needs the SEH filter and trap");
        }
        LLVMValueRef catchSwitch = LLVMBuildCatchSwitch(
            builder_, nullptr, nullptr, 1, "boundary.catchswitch");
        if (catchSwitch == nullptr)
        {
            return fail(error, "C1.3 failed to create the callback boundary catchswitch");
        }
        LLVMBasicBlockRef padBlock = appendBlock("boundary.catchpad");
        LLVMAddHandler(catchSwitch, padBlock);
        LLVMPositionBuilderAtEnd(builder_, padBlock);
        LLVMValueRef catchArguments[] = {ehFilterFunction_};
        LLVMValueRef catchPad = LLVMBuildCatchPad(
            builder_, catchSwitch, catchArguments, 1, "boundary.catch");
        if (catchPad == nullptr)
        {
            return fail(error, "C1.3 failed to create the callback boundary catchpad");
        }
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::CallbackExceptionEscape), 0);
        LLVMValueRef trapArguments[] = {reason};
        LLVMValueRef bundleArguments[] = {catchPad};
        LLVMOperandBundleRef bundle = LLVMCreateOperandBundle("funclet", 7, bundleArguments, 1);
        LLVMOperandBundleRef bundles[] = {bundle};
        LLVMBuildCallWithOperandBundles(
            builder_, trapFunctionType_, trapFunction_, trapArguments, 1, bundles, 1, "");
        LLVMDisposeOperandBundle(bundle);
        LLVMBuildUnreachable(builder_);
        return true;
    }

    LLVMValueRef callbackGuardThunk(
        const std::string& targetName,
        std::string& error,
        bool defineBody = true)
    {
        const auto cached = callbackGuardThunks_.find(targetName);
        if (cached != callbackGuardThunks_.end()) return cached->second.function;
        const auto found = functions_.find(targetName);
        if (found == functions_.end() || found->second.function == nullptr)
        {
            return failValue(error, "C1.3 callback boundary target is unavailable: " + targetName);
        }
        if (sehPersonalityFunction_ == nullptr || ehFilterFunction_ == nullptr ||
            trapFunction_ == nullptr)
        {
            return failValue(error, "C1.3 callback boundary needs the SEH personality and filter");
        }
        LLVMTypeRef thunkType = found->second.functionType;
        const std::string thunkName = targetName + "__c_boundary";
        LLVMValueRef thunk = LLVMAddFunction(module_, thunkName.c_str(), thunkType);
        if (thunk == nullptr)
        {
            return failValue(error, "C1.3 failed to declare the callback boundary thunk for " + targetName);
        }
        applyUwtableAttribute(thunk);
        // Whatever ABI attributes the target's declaration carries (an sret return, above all)
        // must be reproduced on the thunk, because the thunk is what C sees.
        const frontend::ProjectOopMirFunction* mirFunction = nullptr;
        for (const frontend::ProjectOopMirFunction& candidate : mirModel_.functions())
        {
            if (candidate.mangledName == targetName) { mirFunction = &candidate; break; }
        }
        if (mirFunction == nullptr)
        {
            return failValue(error, "C1.3 callback boundary lost the target's MIR record: " + targetName);
        }
        LLVMMetadataRef subprogram = nullptr;
        if (defineBody)
        {
            subprogram = createGeneratedDebugSubprogram(
                *mirFunction, "C callback boundary", thunkName, true);
            if (debugBuilder_ != nullptr && subprogram == nullptr)
            {
                return failValue(error, "failed to create debug subprogram for callback boundary " +
                    thunkName);
            }
            if (subprogram != nullptr) LLVMSetSubprogram(thunk, subprogram);
        }
        const bool sretReturn = !mirFunction->referenceReturn &&
            isIndirectStructTypeKey(mirFunction->returnTypeKey);
        if (sretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddAttributeAtIndex(thunk, index, attribute); },
                0U, mirFunction->returnTypeKey);
        }
        if (!defineBody)
        {
            callbackGuardThunks_[targetName] = FunctionInfo{thunk, thunkType, nullptr};
            return thunk;
        }
        LLVMSetPersonalityFn(thunk, sehPersonalityFunction_);

        LLVMValueRef savedFunction = currentFunction_;
        const std::size_t savedBlockCounter = blockCounter_;
        LLVMBasicBlockRef savedBlock = LLVMGetInsertBlock(builder_);
        currentFunction_ = thunk;
        blockCounter_ = 0;
        ScopedDebugLocation generatedDebugLocation(
            builder_, debugSubprogramLocation(*mirFunction, subprogram));

        LLVMBasicBlockRef entryBlock = appendBlock("boundary.entry");
        LLVMBasicBlockRef returnBlock = appendBlock("boundary.return");
        LLVMBasicBlockRef unwindBlock = appendBlock("boundary.unwind");
        LLVMPositionBuilderAtEnd(builder_, entryBlock);
        const unsigned parameterCount = static_cast<unsigned>(
            mirFunction->parameterTypeKeys.size() + (sretReturn ? 1U : 0U) +
            (mirFunction->isStatic ? 0U : 1U));
        std::vector<LLVMValueRef> forwarded(parameterCount);
        for (unsigned index = 0; index < parameterCount; ++index)
        {
            forwarded[index] = LLVMGetParam(thunk, index);
        }
        const bool returnsVoid = sretReturn || mirFunction->returnTypeKey == "builtin:void";
        LLVMValueRef result = LLVMBuildInvoke2(
            builder_, thunkType, found->second.function,
            forwarded.empty() ? nullptr : forwarded.data(), parameterCount,
            returnBlock, unwindBlock, returnsVoid ? "" : "boundary.result");
        if (result == nullptr)
        {
            currentFunction_ = savedFunction;
            blockCounter_ = savedBlockCounter;
            if (savedBlock != nullptr) LLVMPositionBuilderAtEnd(builder_, savedBlock);
            return failValue(error, "C1.3 failed to emit the guarded callback invoke");
        }
        if (sretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(result, index, attribute); },
                0U, mirFunction->returnTypeKey);
        }

        LLVMPositionBuilderAtEnd(builder_, returnBlock);
        if (returnsVoid) { LLVMBuildRetVoid(builder_); }
        else { LLVMBuildRet(builder_, result); }

        LLVMPositionBuilderAtEnd(builder_, unwindBlock);
        if (!emitCallbackBoundaryHandler(error))
        {
            currentFunction_ = savedFunction;
            blockCounter_ = savedBlockCounter;
            if (savedBlock != nullptr) LLVMPositionBuilderAtEnd(builder_, savedBlock);
            return nullptr;
        }

        currentFunction_ = savedFunction;
        blockCounter_ = savedBlockCounter;
        if (savedBlock != nullptr) LLVMPositionBuilderAtEnd(builder_, savedBlock);
        callbackGuardThunks_[targetName] = FunctionInfo{thunk, thunkType, subprogram};
        return thunk;
    }

    // C1.2 "import identity". A NUL-terminated byte array in the module, for the two strings a
    // runtime-bound import needs: its library name and its external symbol name. Deliberately not
    // the NativeSharp string-literal shape (length + bytes) -- LoadLibraryA/GetProcAddress want a
    // plain C string and nothing else.
    LLVMValueRef cStringGlobal(std::string_view text, std::string_view stem)
    {
        const std::string key(text);
        const auto found = cStringGlobals_.find(key);
        if (found != cStringGlobals_.end()) return found->second;
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef bytesType = LLVMArrayType2(i8, text.size() + 1U);
        LLVMValueRef initializer = LLVMConstStringInContext2(context_, text.data(), text.size(), 0);
        const std::string name =
            "__nsc_cstr_" + std::string(stem) + "_" + std::to_string(cStringSequence_++);
        LLVMValueRef global = LLVMAddGlobal(module_, bytesType, name.c_str());
        if (global == nullptr || initializer == nullptr) return nullptr;
        LLVMSetInitializer(global, initializer);
        LLVMSetGlobalConstant(global, 1);
        LLVMSetLinkage(global, LLVMPrivateLinkage);
        cStringGlobals_.emplace(key, global);
        return global;
    }

    // C1.2 "import identity": the body of an import whose C symbol name is claimed by more than
    // one library.
    //
    // WHY A BODY AT ALL. A PE import table has exactly one slot per (import library, symbol
    // NAME), and an import library names its symbol after the C symbol. So `a.dll!Init` and
    // `b.dll!Init` cannot both be resolved statically: the two would be the same linker symbol.
    // MEASURED before this existed -- declaring both produced two LLVM functions asking to be
    // called `Init`, LLVM renamed the second to `Init.1`, and lld ended the build with
    // "undefined symbol: Init.1". The general fix is to stop asking the STATIC import machinery
    // to distinguish two identities it cannot express, and bind that import the way Windows
    // itself binds an ambiguous name: by library handle at run time.
    //
    // WHAT IT COSTS, AND WHO PAYS IT. Only the colliding imports get this. Every import whose
    // name a single library claims -- which is every import in a normal project -- still resolves
    // statically by name through the import library, exactly as the certified user32/kernel32
    // scenarios do, with the same PE import directory. The cache global means the LoadLibrary/
    // GetProcAddress pair runs at most once per import per process.
    //
    // The forwarding call reuses the function's OWN prototype verbatim, which is what keeps this
    // ABI-correct for free: since Stage 40.2 that prototype already IS the Win64 C ABI for every
    // C-expressible type (sret returns, indirect aggregate parameters and all), so forwarding
    // every parameter unchanged forwards the ABI unchanged.
    bool defineRuntimeBoundImport(
        const frontend::ProjectOopMirFunction& function, std::string& error)
    {
        const auto found = functions_.find(function.mangledName);
        if (found == functions_.end() || found->second.function == nullptr)
        {
            return fail(error, "C1.2 runtime-bound import lost its declaration: " + function.mangledName);
        }
        if (function.importDllName.empty() || function.cAbiName.empty())
        {
            return fail(error, "C1.2 runtime-bound import lost its (library, symbol) identity");
        }
        LLVMValueRef importFunction = found->second.function;
        LLVMTypeRef importType = found->second.functionType;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);

        if (loadLibraryFunction_ == nullptr)
        {
            LLVMTypeRef loadLibraryParameters[] = {ptr};
            loadLibraryFunctionType_ = LLVMFunctionType(ptr, loadLibraryParameters, 1, 0);
            loadLibraryFunction_ = LLVMAddFunction(module_, "LoadLibraryA", loadLibraryFunctionType_);
            LLVMTypeRef getProcAddressParameters[] = {ptr, ptr};
            getProcAddressFunctionType_ = LLVMFunctionType(ptr, getProcAddressParameters, 2, 0);
            getProcAddressFunction_ =
                LLVMAddFunction(module_, "GetProcAddress", getProcAddressFunctionType_);
            if (loadLibraryFunction_ == nullptr || getProcAddressFunction_ == nullptr)
            {
                return fail(error, "C1.2 failed to declare the runtime import-binding entry points");
            }
            applyFunctionAttribute(loadLibraryFunction_, "nounwind");
            applyFunctionAttribute(getProcAddressFunction_, "nounwind");
        }

        LLVMValueRef libraryName = cStringGlobal(function.importDllName, "dll");
        LLVMValueRef symbolName = cStringGlobal(function.cAbiName, "sym");
        if (libraryName == nullptr || symbolName == nullptr)
        {
            return fail(error, "C1.2 failed to materialize the import identity strings");
        }
        LLVMValueRef cache = LLVMAddGlobal(module_, ptr, (function.mangledName + "__address").c_str());
        if (cache == nullptr)
        {
            return fail(error, "C1.2 failed to declare the runtime import address cache");
        }
        LLVMSetInitializer(cache, LLVMConstNull(ptr));
        LLVMSetLinkage(cache, LLVMPrivateLinkage);

        currentFunction_ = importFunction;
        blockCounter_ = 0;
        LLVMBasicBlockRef entryBlock = appendBlock("import.entry");
        LLVMBasicBlockRef bindBlock = appendBlock("import.bind");
        LLVMBasicBlockRef callBlock = appendBlock("import.call");
        LLVMPositionBuilderAtEnd(builder_, entryBlock);
        LLVMValueRef cached = LLVMBuildLoad2(builder_, ptr, cache, "import.cached");
        LLVMValueRef needsBinding =
            LLVMBuildICmp(builder_, LLVMIntEQ, cached, LLVMConstNull(ptr), "import.unbound");
        LLVMBuildCondBr(builder_, needsBinding, bindBlock, callBlock);

        LLVMPositionBuilderAtEnd(builder_, bindBlock);
        LLVMValueRef loadArguments[] = {libraryName};
        LLVMValueRef moduleHandle = LLVMBuildCall2(
            builder_, loadLibraryFunctionType_, loadLibraryFunction_, loadArguments, 1,
            "import.module");
        LLVMValueRef procArguments[] = {moduleHandle, symbolName};
        LLVMValueRef address = LLVMBuildCall2(
            builder_, getProcAddressFunctionType_, getProcAddressFunction_, procArguments, 2,
            "import.address");
        LLVMBuildStore(builder_, address, cache);
        LLVMBuildBr(builder_, callBlock);

        LLVMPositionBuilderAtEnd(builder_, callBlock);
        LLVMValueRef target = LLVMBuildPhi(builder_, ptr, "import.target");
        LLVMValueRef incomingValues[] = {cached, address};
        LLVMBasicBlockRef incomingBlocks[] = {entryBlock, bindBlock};
        LLVMAddIncoming(target, incomingValues, incomingBlocks, 2);
        // A null here means the library or the symbol is genuinely absent at run time. Trapping is
        // the only honest answer: an indirect call through null would be an access violation with
        // no attribution, and there is no error channel in the imported function's own C
        // signature to report it through.
        if (!emitGuard(
                LLVMBuildICmp(builder_, LLVMIntNE, target, LLVMConstNull(ptr), "import.bound"),
                LlvmTrapReason::ImportBindingFailure, "import.binding", error))
        {
            return false;
        }

        // The prototype declareFunctions built for this import, reconstructed from the same MIR
        // facts rather than read back off the LLVM function: an import is always static (no
        // `this`), a leading sret pointer when the return travels indirectly, then one parameter
        // per declared parameter. Forwarding every one of them unchanged forwards the ABI
        // unchanged.
        const bool sretReturn = !function.referenceReturn &&
            isIndirectStructTypeKey(function.returnTypeKey);
        const unsigned parameterCount = static_cast<unsigned>(
            function.parameterTypeKeys.size() + (sretReturn ? 1U : 0U));
        std::vector<LLVMValueRef> forwarded(parameterCount);
        for (unsigned index = 0; index < parameterCount; ++index)
        {
            forwarded[index] = LLVMGetParam(importFunction, index);
        }
        const bool returnsVoid = sretReturn || function.returnTypeKey == "builtin:void";
        LLVMValueRef result = LLVMBuildCall2(
            builder_, importType, target, forwarded.empty() ? nullptr : forwarded.data(),
            parameterCount, returnsVoid ? "" : "import.result");
        if (result == nullptr)
        {
            return fail(error, "C1.2 failed to emit the runtime-bound import forwarding call");
        }
        // Win64 aggregate ABI: an indirect struct return travels through a leading sret pointer.
        // The declaration carries the attribute; the forwarding call site must carry it too, so
        // the two agree about which pointer is the return slot.
        if (sretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(result, index, attribute); },
                0U, function.returnTypeKey);
        }
        if (returnsVoid)
        {
            LLVMBuildRetVoid(builder_);
        }
        else
        {
            LLVMBuildRet(builder_, result);
        }
        currentFunction_ = nullptr;
        return true;
    }

    bool isExternCDelegateType(std::string_view typeKey) const noexcept
    {
        if (!typeKey.starts_with("type:")) return false;
        const std::string_view key = typeKey.substr(5);
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() != key) continue;
            return type.kind == frontend::NamedTypeDeclarationKind::Delegate &&
                type.syntax != nullptr && type.syntax->hasModifier(lexer::TokenKind::KwExtern);
        }
        return false;
    }


    bool defineCAbiWrappers(std::string& error)
    {
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            if (!function.isExportedC) continue;
            if (!function.isStatic || function.cAbiName.empty())
            {
                return fail(error, "Stage 47 C ABI wrapper lost static/export metadata");
            }
            const auto internal = functions_.find(function.mangledName);
            if (internal == functions_.end())
            {
                return fail(error, "Stage 47 C ABI wrapper lost internal function " + function.mangledName);
            }

            // Win64 aggregate ABI at the EXPORT boundary: the wrapper's C signature IS the
            // contract an MSVC-compiled host sees, so it takes the same shape clang gives a C
            // function -- indirect struct parameters as plain pointers, an indirect struct
            // return as a leading sret pointer. Verified against a real clang-compiled C host
            // in Stage48HostAbi tests.
            const bool sretReturn = isIndirectStructTypeKey(function.returnTypeKey);
            LLVMTypeRef returnType = sretReturn
                ? LLVMVoidTypeInContext(context_)
                : llvmType(function.returnTypeKey, error);
            if (returnType == nullptr) return false;
            // C1.1 "function-pointer round trip": an extern C delegate IS a raw function pointer
            // (llvmType maps it to `ptr`; lowerDelegateCreate yields the target's own address), so
            // returning one through the wrapper needs no special case -- the hard refusal that
            // used to sit here was the backend half of a frontend rule now narrowed to ordinary
            // (cell-shaped) delegates. See ProjectMemberModel's C ABI return-type check.

            std::vector<LLVMTypeRef> parameterTypes;
            parameterTypes.reserve(function.parameterTypeKeys.size() + 1);
            if (sretReturn) parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
            for (const std::string& typeKey : function.parameterTypeKeys)
            {
                LLVMTypeRef type = abiParameterType(typeKey, error);
                if (type == nullptr || type == LLVMVoidTypeInContext(context_)) return false;
                parameterTypes.push_back(type);
            }
            LLVMTypeRef wrapperType = LLVMFunctionType(
                returnType,
                parameterTypes.empty() ? nullptr : parameterTypes.data(),
                static_cast<unsigned>(parameterTypes.size()),
                0);
            LLVMValueRef wrapper = LLVMAddFunction(module_, function.cAbiName.c_str(), wrapperType);
            if (wrapper == nullptr)
            {
                return fail(error, "Stage 47 failed to declare C ABI wrapper " + function.cAbiName);
            }
            LLVMMetadataRef subprogram = createGeneratedDebugSubprogram(
                function, "C ABI wrapper", function.cAbiName, false);
            if (debugBuilder_ != nullptr && subprogram == nullptr)
            {
                return fail(error, "failed to create debug subprogram for C ABI wrapper " +
                    function.cAbiName);
            }
            if (subprogram != nullptr) LLVMSetSubprogram(wrapper, subprogram);
            if (sretReturn)
            {
                applySretAttributes(
                    [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                    { LLVMAddAttributeAtIndex(wrapper, index, attribute); },
                    0U, function.returnTypeKey);
            }

            currentFunction_ = wrapper;
            currentMirFunction_ = nullptr;
            blockCounter_ = 0;
            ScopedDebugLocation generatedDebugLocation(
                builder_, debugSubprogramLocation(function, subprogram));
            LLVMBasicBlockRef entry = appendBlock("cabi.entry");
            LLVMPositionBuilderAtEnd(builder_, entry);

            std::vector<LLVMValueRef> arguments;
            arguments.reserve(function.parameterTypeKeys.size() + 1);
            // The internal function shares this exact ABI (declareFunctions applies the same
            // mapping), so both the host's argument pointers and the host's sret slot are
            // forwarded as-is: the per-call copies the C caller made belong to this call chain,
            // and nothing here reads them after the internal call returns.
            if (sretReturn) arguments.push_back(LLVMGetParam(wrapper, 0));
            const unsigned wrapperArgumentBase = sretReturn ? 1U : 0U;
            for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
            {
                // extern C delegates are native C function pointers end-to-end.
                // NativeSharp's ordinary delegate cell representation is deliberately not used here.
                arguments.push_back(LLVMGetParam(
                    wrapper, wrapperArgumentBase + static_cast<unsigned>(index)));
            }

            // C1.3 "callback exception boundary": an `export extern C` entry point is the OTHER
            // door foreign code enters through, and it carries exactly the same hazard as a
            // callback -- a NativeSharp exception escaping here would unwind the HOST's frames
            // with this compiler's SEH exception code. The wrapper already exists as its own
            // function, so the guard is the same catchpad the callback thunk uses, emitted right
            // here: invoke instead of call, one catchpad filtered by __nativesharp_eh_filter (so
            // a genuine foreign SEH exception still propagates untouched), trap on catch.
            LLVMBasicBlockRef returnBlock = appendBlock("cabi.return");
            LLVMBasicBlockRef unwindBlock = appendBlock("cabi.unwind");
            LLVMSetPersonalityFn(wrapper, sehPersonalityFunction_);
            LLVMValueRef result = LLVMBuildInvoke2(
                builder_, internal->second.functionType, internal->second.function,
                arguments.empty() ? nullptr : arguments.data(),
                static_cast<unsigned>(arguments.size()),
                returnBlock, unwindBlock,
                returnType == LLVMVoidTypeInContext(context_) ? "" : "cabi.result");
            if (result == nullptr)
            {
                return fail(error, "Stage 47 failed to emit the guarded C ABI wrapper call");
            }
            if (sretReturn)
            {
                applySretAttributes(
                    [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                    { LLVMAddCallSiteAttribute(result, index, attribute); },
                    0U, function.returnTypeKey);
            }
            LLVMPositionBuilderAtEnd(builder_, returnBlock);
            if (returnType == LLVMVoidTypeInContext(context_))
            {
                LLVMBuildRetVoid(builder_);
            }
            else
            {
                LLVMBuildRet(builder_, result);
            }
            LLVMPositionBuilderAtEnd(builder_, unwindBlock);
            if (!emitCallbackBoundaryHandler(error)) return false;
        }
        currentFunction_ = nullptr;
        currentMirFunction_ = nullptr;
        blockCounter_ = 0;
        return true;
    }

    bool defineDestructionEntries(std::string& error)
    {
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            // The imported descriptor already points at the producer's destruction entry. This
            // module neither declares nor defines that private implementation directly.
            if (table.isImported) continue;
            const auto entryFound = destructionFunctions_.find(table.typeOrdinal);
            const auto objectEntryFound = objectDestructionFunctions_.find(table.typeOrdinal);
            if (entryFound == destructionFunctions_.end() ||
                objectEntryFound == objectDestructionFunctions_.end())
            {
                return fail(error, "item 319 lost a declared destruction entry");
            }

            // The object-only entry owns the complete destructor/owned-field/base traversal.
            // The traditional entry below is now a thin wrapper that invokes it and releases
            // the outermost storage exactly once.
            currentFunction_ = objectEntryFound->second.function;
            currentMirFunction_ = nullptr;
            thisValue_ = nullptr;
            closureEnvironment_ = nullptr;
            constructionThisOverride_ = nullptr;
            parameterStorage_.clear();
            localStorage_.clear();
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

            currentDebugSubprogram_ = objectEntryFound->second.subprogram;
            currentDebugScope_ = currentDebugSubprogram_;
            debugDeclaredLocals_.clear();
            currentDebugFileKey_ = 0;
            SourceRange debugRange;
            if (table.typeOrdinal < nameLookup_.namedTypes().size())
            {
                debugRange = nameLookup_.namedTypes()[table.typeOrdinal].range;
                if (debugRange.isValid()) currentDebugFileKey_ = debugRange.begin().fileId().value();
            }

            LLVMBasicBlockRef entry = appendBlock("destroy.entry");
            LLVMPositionBuilderAtEnd(builder_, entry);
            ScopedDebugLocation generatedLocation(
                builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
            LLVMValueRef object = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(object, "object", 6);

            // The destruction-order CONTRACT executes here, step by step, exactly as the
            // frontend froze it: most-derived destructor body, that class's owned fields in
            // reverse declaration order, then the base, up the chain -- destruction is the
            // reverse of construction. The full contract statement and its rationale live on
            // ProjectOopMirDispatchTable::destructionSteps; this loop deliberately contains no
            // ordering decisions of its own, so the order can never quietly diverge between the
            // model and the emitted code. Verified by execution
            // (Stage50_11OwnershipExecutionTests: DestructionOrderContract*).
            for (const frontend::ProjectOopMirDispatchTable::DestructionStep& step : table.destructionSteps)
            {
                if (!step.isFieldRelease)
                {
                    const auto bodyFound = functions_.find(step.destructorCalleeName);
                    if (bodyFound == functions_.end())
                    {
                        return fail(error, "Stage 35 destructor body was not declared: " + step.destructorCalleeName);
                    }
                    LLVMValueRef arguments[] = {object};
                    buildNoUnwindCall(
                        bodyFound->second.functionType, bodyFound->second.function,
                        arguments, 1, "");
                    continue;
                }
                // Release one owned field: null-guarded, cleared before its destructor runs --
                // the same protocol scoped owners use (see emitCleanupGuardBranch). The null
                // guard is what makes this correct when a destructor body already released the
                // field by hand (it stored null through `field = null` or move-out), and it is
                // idempotent under exceptional re-entry for the same reason.
                const frontend::ProjectOopMirDispatchTable::OwnedField& ownedField = step.ownedField;
                LLVMTypeRef ptrType = LLVMPointerTypeInContext(context_, 0);
                LLVMValueRef offset = LLVMConstInt(
                    LLVMInt64TypeInContext(context_), ownedField.offset, 0);
                LLVMValueRef indices[] = {offset};
                LLVMValueRef fieldAddress = LLVMBuildGEP2(
                    builder_, LLVMInt8TypeInContext(context_), object, indices, 1,
                    "owned.field.addr");
                LLVMValueRef fieldValue = LLVMBuildLoad2(
                    builder_, ptrType, fieldAddress, "owned.field.value");
                LLVMValueRef fieldIsNull = LLVMBuildICmp(
                    builder_, LLVMIntEQ, fieldValue, LLVMConstNull(ptrType), "owned.field.isnull");
                LLVMBasicBlockRef destroyBlock = appendBlock("owned.field.destroy");
                LLVMBasicBlockRef doneBlock = appendBlock("owned.field.done");
                LLVMBuildCondBr(builder_, fieldIsNull, doneBlock, destroyBlock);
                LLVMPositionBuilderAtEnd(builder_, destroyBlock);
                LLVMBuildStore(builder_, LLVMConstNull(ptrType), fieldAddress);
                if (ownedField.isNativeString)
                {
                    LLVMValueRef stringArguments[] = {fieldValue};
                    buildNoUnwindCall(
                        deallocationFunctionType_, deallocationFunction_, stringArguments, 1, "");
                }
                else
                {
                    if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
                    {
                        return fail(error, "Bloque 1 owned field has no class-descriptor destruction ABI");
                    }
                    // Dispatched through the field's RUNTIME descriptor, not its declared type, so a
                    // field declared as a base type destroys the derived object it actually holds.
                    LLVMValueRef descriptor = LLVMBuildLoad2(
                        builder_, ptrType, fieldValue, "owned.field.descriptor");
                    LLVMValueRef entryAddress = LLVMBuildStructGEP2(
                        builder_, descriptorType_, descriptor, 5, "owned.field.entry.addr");
                    LLVMValueRef destructionEntry = LLVMBuildLoad2(
                        builder_, ptrType, entryAddress, "owned.field.entry");
                    LLVMValueRef destroyArguments[] = {fieldValue};
                    buildNoUnwindCall(
                        destructionEntryFunctionType_, destructionEntry, destroyArguments, 1, "");
                }
                LLVMBuildBr(builder_, doneBlock);
                LLVMPositionBuilderAtEnd(builder_, doneBlock);
            }

            if (table.delegatedBaseDestructionTypeOrdinal.has_value())
            {
                const auto baseAccessor = descriptorAccessorFunctions_.find(
                    *table.delegatedBaseDestructionTypeOrdinal);
                if (baseAccessor == descriptorAccessorFunctions_.end())
                {
                    return fail(error,
                        "Stage 35 local destruction cannot resolve its precompiled base descriptor accessor");
                }
                LLVMValueRef baseDescriptor = LLVMBuildCall2(
                    builder_, baseAccessor->second.functionType, baseAccessor->second.function,
                    nullptr, 0, "destruction.base.descriptor");
                LLVMValueRef entryAddress = LLVMBuildStructGEP2(
                    builder_, descriptorType_, baseDescriptor, 7,
                    "destruction.base.object.entry.addr");
                LLVMValueRef baseDestructionEntry = LLVMBuildLoad2(
                    builder_, ptr, entryAddress, "destruction.base.object.entry");
                LLVMValueRef baseArguments[] = {object};
                buildNoUnwindCall(
                    destructionEntryFunctionType_, baseDestructionEntry, baseArguments, 1, "");
            }
            LLVMBuildRetVoid(builder_);

            currentFunction_ = entryFound->second.function;
            currentDebugSubprogram_ = entryFound->second.subprogram;
            currentDebugScope_ = currentDebugSubprogram_;
            blockCounter_ = 0;
            LLVMBasicBlockRef releasingEntry = appendBlock("destroy-and-free.entry");
            LLVMPositionBuilderAtEnd(builder_, releasingEntry);
            ScopedDebugLocation releasingLocation(
                builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
            LLVMValueRef releasingObject = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(releasingObject, "object", 6);
            LLVMValueRef destroyArguments[] = {releasingObject};
            buildNoUnwindCall(
                objectEntryFound->second.functionType,
                objectEntryFound->second.function,
                destroyArguments,
                1,
                "");
            LLVMValueRef freeArguments[] = {releasingObject};
            buildNoUnwindCall(
                deallocationFunctionType_, deallocationFunction_, freeArguments, 1, "");
            LLVMBuildRetVoid(builder_);
        }
        currentDebugSubprogram_ = nullptr;
        currentDebugScope_ = nullptr;
        currentDebugFileKey_ = 0;
        (void)ptr;
        return true;
    }

    bool defineFunction(const frontend::ProjectOopMirFunction& function, std::string& error)
    {
        const auto found = functions_.find(function.mangledName);
        if (found == functions_.end())
        {
            error = "Stage 34 internal function declaration missing";
            return false;
        }
        currentFunction_ = found->second.function;
        currentMirFunction_ = &function;
        currentDebugSubprogram_ = found->second.subprogram;
        currentDebugScope_ = currentDebugSubprogram_;
        debugDeclaredLocals_.clear();
        debugCaptureVariables_.clear();
        currentDebugFileKey_ = 0;
        if (memberModel_ != nullptr && function.memberOrdinal < memberModel_->members().size())
            currentDebugFileKey_ = memberModel_->members()[function.memberOrdinal].fileId.value();
        LLVMSetCurrentDebugLocation2(
            builder_,
            currentDebugSubprogram_ == nullptr
                ? nullptr
                : LLVMDIBuilderCreateDebugLocation(
                    context_, 0, 0, currentDebugSubprogram_, nullptr));
        parameterStorage_.clear();
        localStorage_.clear();
        stackObjectStorage_.clear();
        parameterCaptureCells_.clear();
        localCaptureCells_.clear();
        thisCaptureCell_ = nullptr;
        ownedCaptureCells_.clear();
        captureCellExceptionalCleanupBlock_ = nullptr;
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

        LLVMBasicBlockRef entry = appendBlock("entry");
        LLVMPositionBuilderAtEnd(builder_, entry);
        constructionThisOverride_ = nullptr;
        const bool sretReturn = !function.referenceReturn &&
            isIndirectStructTypeKey(function.returnTypeKey);
        const unsigned implicitLead = function.isStatic ? 0U : 1U;
        // Regular parameters sit after `this`/the closure environment AND after the sret slot
        // when the function returns an indirect struct.
        const unsigned parameterBase = implicitLead + (sretReturn ? 1U : 0U);
        if (function.isStatic)
        {
            thisValue_ = nullptr;
            closureEnvironment_ = nullptr;
            if (!function.isLambda && !ensureTypeInitialized(function.declaringTypeOrdinal, error))
            {
                return false;
            }
        }
        else if (function.isLambda)
        {
            thisValue_ = nullptr;
            closureEnvironment_ = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(closureEnvironment_, "closure.env", 11);
        }
        else
        {
            closureEnvironment_ = nullptr;
            thisValue_ = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(thisValue_, "this", 4);
            if (!emitDebugThis(function, error)) return false;
        }

        if (function.capturesThisCell)
        {
            if (thisValue_ == nullptr)
                return fail(error, "E11 captured this has no receiver value");
            LLVMValueRef valueAddress = nullptr;
            const std::string thisTypeKey = "type:" +
                nameLookup_.namedTypes()[function.declaringTypeOrdinal].identity.key();
            thisCaptureCell_ = allocateCaptureCell(
                thisTypeKey, valueAddress, "capture.this", error);
            if (thisCaptureCell_ == nullptr || valueAddress == nullptr) return false;
            LLVMValueRef capturedThis = thisValue_;
            if (isStructValue(thisTypeKey))
            {
                LLVMTypeRef thisType = llvmType(thisTypeKey, error);
                if (thisType == nullptr) return false;
                capturedThis = LLVMBuildLoad2(builder_, thisType, thisValue_, "capture.this.load");
            }
            LLVMBuildStore(builder_, capturedThis, valueAddress);
            ownedCaptureCells_.push_back(thisCaptureCell_);
        }

        sretStorage_ = nullptr;
        if (sretReturn)
        {
            sretStorage_ = LLVMGetParam(currentFunction_, implicitLead);
            LLVMSetValueName2(sretStorage_, "sret.out", 8);
        }
        for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
        {
            LLVMValueRef value = LLVMGetParam(
                currentFunction_, parameterBase + static_cast<unsigned>(index));
            LLVMSetValueName2(value, ("p" + std::to_string(index)).c_str(), ("p" + std::to_string(index)).size());
            const bool captured = std::ranges::find(
                function.capturedParameterOrdinals, index) != function.capturedParameterOrdinals.end();
            if (captured)
            {
                LLVMValueRef valueAddress = nullptr;
                LLVMValueRef cell = allocateCaptureCell(
                    function.parameterTypeKeys[index], valueAddress,
                    "capture.p" + std::to_string(index), error);
                if (cell == nullptr || valueAddress == nullptr) return false;
                if (isIndirectStructTypeKey(function.parameterTypeKeys[index]))
                {
                    LLVMTypeRef type = llvmType(function.parameterTypeKeys[index], error);
                    if (type == nullptr) return false;
                    value = LLVMBuildLoad2(builder_, type, value, "capture.param.aggregate.load");
                }
                LLVMBuildStore(builder_, value, valueAddress);
                parameterStorage_[index] = valueAddress;
                parameterCaptureCells_[index] = cell;
                ownedCaptureCells_.push_back(cell);
                if (!emitDebugParameter(function, index, valueAddress, error)) return false;
                continue;
            }
            if (isIndirectStructTypeKey(function.parameterTypeKeys[index]))
            {
                // Win64 indirect aggregate: the incoming value IS a pointer to a copy the caller
                // made for THIS call, and that memory belongs to the call -- the callee may read
                // and write it freely (clang re-copies when forwarding, which is what makes this
                // safe). So the pointer itself is the parameter home: member GEPs, whole-value
                // loads and stores all work through it unchanged, and no second copy is made.
                parameterStorage_[index] = value;
                if (!emitDebugParameter(function, index, value, error)) return false;
                continue;
            }
            LLVMTypeRef type = llvmType(function.parameterTypeKeys[index], error);
            if (type == nullptr) return false;
            LLVMValueRef storage = LLVMBuildAlloca(builder_, type, ("p" + std::to_string(index) + ".addr").c_str());
            LLVMBuildStore(builder_, value, storage);
            parameterStorage_[index] = storage;
            if (!emitDebugParameter(function, index, storage, error)) return false;
        }

        collectLocalAllocas(function.body.get(), error);
        if (!error.empty()) return false;
        collectCleanupRegions(function.body.get(), error);
        if (!error.empty()) return false;
        const bool hasExceptionalControl = containsExceptionalControl(function.body.get());
        if (hasExceptionalControl || !ownedCaptureCells_.empty())
        {
            LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
        }
        if (!function.constructor && function.returnTypeKey != "builtin:void" &&
            hasExceptionalControl)
        {
            LLVMTypeRef returnType = function.referenceReturn
                ? LLVMPointerTypeInContext(context_, 0)
                : llvmType(function.returnTypeKey, error);
            if (returnType == nullptr) return false;
            returnStorage_ = LLVMBuildAlloca(builder_, returnType, "eh.return.addr");
            const unsigned returnAlignment = structAlignmentFor(function.returnTypeKey);
            if (returnAlignment > 1U) LLVMSetAlignment(returnStorage_, returnAlignment);
        }

        if (!lowerStatement(function.body.get(), error)) return false;
        if (!terminated())
        {
            if (function.constructor || function.returnTypeKey == "builtin:void")
            {
                releaseOwnedCaptureCells();
                LLVMBuildRetVoid(builder_);
            }
            else
            {
                LLVMBuildUnreachable(builder_);
            }
        }
        return true;
    }

    void collectLocalAllocas(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || !error.empty()) return;
        if ((node->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
             node->kind == frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow ||
             node->kind == frontend::ProjectOopMirNodeKind::UsingRegion ||
             node->kind == frontend::ProjectOopMirNodeKind::CatchRegion) &&
            node->localOrdinal.has_value())
        {
            if (!localStorage_.contains(*node->localOrdinal))
            {
                const std::string& typeKey =
                    (node->kind == frontend::ProjectOopMirNodeKind::UsingRegion ||
                     node->kind == frontend::ProjectOopMirNodeKind::CatchRegion)
                    ? node->elementTypeKey
                    : node->typeKey;
                const std::string storageTypeKey =
                    node->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration &&
                        node->referenceLocal
                    ? typeKey + "*"
                    : typeKey;
                LLVMTypeRef type = llvmType(storageTypeKey, error);
                if (type == nullptr) return;
                const bool captured = currentMirFunction_ != nullptr && std::ranges::find(
                    currentMirFunction_->capturedLocalOrdinals,
                    *node->localOrdinal) != currentMirFunction_->capturedLocalOrdinals.end();
                LLVMValueRef slot = nullptr;
                if (captured)
                {
                    LLVMValueRef cell = allocateCaptureCell(
                        typeKey,
                        slot,
                        "capture.local" + std::to_string(*node->localOrdinal),
                        error);
                    if (cell == nullptr || slot == nullptr) return;
                    localCaptureCells_[*node->localOrdinal] = cell;
                    ownedCaptureCells_.push_back(cell);
                }
                else
                {
                    slot = LLVMBuildAlloca(
                        builder_, type,
                        ("local" + std::to_string(*node->localOrdinal) + ".addr").c_str());
                }
                // An indirect-struct carrier is [N x i8], whose alloca defaults to align 1 --
                // but member accesses through the slot do naturally-aligned typed loads and
                // stores, so the slot must carry the struct layout's real alignment.
                const unsigned structAlignment = node->referenceLocal
                    ? 8U
                    : structAlignmentFor(typeKey);
                if (!captured && structAlignment > 1U) LLVMSetAlignment(slot, structAlignment);
                localStorage_[*node->localOrdinal] = slot;
            }
            if (node->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration &&
                node->stackPromotedObject && !stackObjectStorage_.contains(*node->localOrdinal))
            {
                if (node->children.size() != 1 || node->children.front() == nullptr ||
                    node->children.front()->kind !=
                        frontend::ProjectOopMirNodeKind::AllocateAndConstruct ||
                    !node->children.front()->stackPromotedObject ||
                    node->children.front()->localOrdinal != node->localOrdinal ||
                    node->children.front()->immediate == 0 ||
                    node->children.front()->alignment == 0)
                {
                    error = "item 323 stack-promoted local has no matching object layout";
                    return;
                }
                const frontend::ProjectOopMirNode* construction = node->children.front().get();
                LLVMTypeRef bytes = LLVMArrayType2(
                    LLVMInt8TypeInContext(context_), construction->immediate);
                LLVMValueRef object = LLVMBuildAlloca(
                    builder_, bytes,
                    ("local" + std::to_string(*node->localOrdinal) + ".object").c_str());
                if (construction->alignment > 1U)
                    LLVMSetAlignment(object, static_cast<unsigned>(construction->alignment));
                stackObjectStorage_[*node->localOrdinal] = object;
            }
        }
        for (const auto& child : node->children) collectLocalAllocas(child.get(), error);
    }

    // Emits the guarded entry to a cleanup: branches to `doneBlock` when this owner slot is already
    // empty and to `destroyBlock` otherwise, clearing the slot on the way in. Shared by
    // emitRaiiDestroy and emitOwnedParameterDestroy so the two can never drift apart on the
    // exactly-once guarantee.
    //
    // HISTORY -- do NOT re-add a separate "already destroyed" flag here. One was added for bug
    // B-006 (an enclosing cleanup running during the unwind to a same-frame catch, destroying an
    // object still in scope), back when the compiler still depended on that unwind edge and had to
    // survive the cleanup being entered twice. B-006 was then fixed STRUCTURALLY instead -- the
    // catchswitch unwinds to caller and the enclosing cleanups run explicitly on the
    // no-clause-matched path, see lowerTryRegion -- which removed the double entry at its source
    // and left the flag with nothing to protect against.
    //
    // Worse, the flag was actively wrong. It lived per STORAGE SLOT, but a slot legitimately holds
    // a SEQUENCE of owners over its lifetime through move-assignment (`a = move(b)`): destroying
    // the first value set the flag permanently, so the value moved in afterwards was never
    // destroyed at scope exit. That is a silent leak, and it was caught by real execution in
    // tests/unit/Stage50_11OwnershipExecutionTests.cpp, not by any IR-shape check. The owner
    // slot's own null state is the correct guard precisely because storing a new owner into the
    // slot re-arms it automatically.
    void emitCleanupGuardBranch(
        LLVMValueRef ownerStorage,
        LLVMValueRef ownerValue,
        LLVMBasicBlockRef destroyBlock,
        LLVMBasicBlockRef doneBlock,
        const char* namePrefix)
    {
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef skip = LLVMBuildICmp(
            builder_, LLVMIntEQ, ownerValue, LLVMConstNull(ptr),
            (std::string(namePrefix) + ".isnull").c_str());
        LLVMBuildCondBr(builder_, skip, doneBlock, destroyBlock);

        LLVMPositionBuilderAtEnd(builder_, destroyBlock);
        // Cleared BEFORE the destructor runs: that is what makes a cleanup entered twice destroy
        // only once, and it keeps the slot from holding a dangling pointer afterwards on any path
        // that legitimately survives the cleanup.
        LLVMBuildStore(builder_, LLVMConstNull(ptr), ownerStorage);
    }

    bool containsExceptionalControl(const frontend::ProjectOopMirNode* node) const noexcept
    {
        if (node == nullptr) return false;
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::TryRegion:
            case frontend::ProjectOopMirNodeKind::FinallyRegion:
            case frontend::ProjectOopMirNodeKind::UsingRegion:
            case frontend::ProjectOopMirNodeKind::LocalDeclaration:
            case frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow:
            case frontend::ProjectOopMirNodeKind::OwnedParameterCleanup:
                if (node->cleanupRegionOrdinal.has_value()) return true;
                break;
            case frontend::ProjectOopMirNodeKind::Throw:
            case frontend::ProjectOopMirNodeKind::Rethrow:
            // Stage 52 fix: a try/catch needs the full SEH funclet/personality-routine machinery
            // (and therefore the `returnStorage_` spill slot for `return` statements that unwind
            // through an active catch funclet -- see lowerStructuredExit's `complexExit` check)
            // regardless of whether its *protected* body happens to contain a `throw`/`rethrow`
            // or a cleanup-owning local. A catch region proves the enclosing try/catch exists,
            // which is what actually drives funclet nesting at LLVM lowering time -- the previous
            // check only looked for a `cleanupRegionOrdinal` on TryRegion/FinallyRegion/UsingRegion
            // (set only when the protected block owns RAII/using cleanup) or an explicit
            // throw/rethrow, so `try { return x; } catch (...) { ... }` with neither -- a
            // completely ordinary, expected shape -- silently fell through with
            // `hasExceptionalControl == false`, never allocated the spill slot, and then crashed
            // with "Stage 45.10 return through cleanup/catch has no return spill slot" the moment
            // `lowerStructuredExit` saw the catch funclet on the stack. See CatchRegion below.
            case frontend::ProjectOopMirNodeKind::CatchRegion:
                return true;
            default:
                break;
        }
        for (const auto& child : node->children)
        {
            if (containsExceptionalControl(child.get())) return true;
        }
        return false;
    }

    void collectCleanupRegions(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || !error.empty()) return;
        if ((node->kind == frontend::ProjectOopMirNodeKind::FinallyRegion ||
             node->kind == frontend::ProjectOopMirNodeKind::UsingRegion ||
             node->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
             node->kind == frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow ||
             node->kind == frontend::ProjectOopMirNodeKind::OwnedParameterCleanup) &&
            node->cleanupRegionOrdinal.has_value())
        {
            const auto [found, inserted] = cleanupRegions_.emplace(*node->cleanupRegionOrdinal, node);
            if (!inserted && found->second != node)
            {
                error = "Stage 45.10 found duplicate MIR cleanup-region identity";
                return;
            }
        }
        for (const auto& child : node->children) collectCleanupRegions(child.get(), error);
    }

    std::optional<std::size_t> activeCleanupIndex(
        std::size_t ordinal,
        const std::vector<std::size_t>& active) const noexcept
    {
        const auto found = std::find(active.begin(), active.end(), ordinal);
        if (found == active.end()) return std::nullopt;
        return static_cast<std::size_t>(found - active.begin());
    }

    LLVMValueRef activeFuncletPad() const noexcept
    {
        return funcletStack_.empty() ? nullptr : funcletStack_.back().pad;
    }

    LLVMValueRef buildNoUnwindCall(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        const char* name)
    {
        LLVMValueRef pad = activeFuncletPad();
        if (pad == nullptr)
        {
            return LLVMBuildCall2(
                builder_, functionType, function, arguments, argumentCount, name);
        }
        LLVMValueRef bundleArguments[] = {pad};
        LLVMOperandBundleRef bundle = LLVMCreateOperandBundle(
            "funclet", 7, bundleArguments, 1);
        LLVMOperandBundleRef bundles[] = {bundle};
        LLVMValueRef call = LLVMBuildCallWithOperandBundles(
            builder_, functionType, function, arguments, argumentCount,
            bundles, 1, name);
        LLVMDisposeOperandBundle(bundle);
        return call;
    }

    LLVMValueRef buildInvokeInCurrentFunclet(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        LLVMBasicBlockRef normalBlock,
        LLVMBasicBlockRef unwindBlock,
        const char* name)
    {
        LLVMValueRef pad = activeFuncletPad();
        if (pad == nullptr)
        {
            return LLVMBuildInvoke2(
                builder_, functionType, function, arguments, argumentCount,
                normalBlock, unwindBlock, name);
        }
        LLVMValueRef bundleArguments[] = {pad};
        LLVMOperandBundleRef bundle = LLVMCreateOperandBundle(
            "funclet", 7, bundleArguments, 1);
        LLVMOperandBundleRef bundles[] = {bundle};
        LLVMValueRef invoke = LLVMBuildInvokeWithOperandBundles(
            builder_, functionType, function, arguments, argumentCount,
            normalBlock, unwindBlock, bundles, 1, name);
        LLVMDisposeOperandBundle(bundle);
        return invoke;
    }

    LLVMBasicBlockRef emitExceptionalCleanupChain(
        const std::vector<std::size_t>& cleanupPath,
        LLVMBasicBlockRef finalUnwindTarget,
        std::string& error)
    {
        if (cleanupPath.empty()) return finalUnwindTarget;
        LLVMBasicBlockRef sourceBlock = LLVMGetInsertBlock(builder_);
        if (sourceBlock == nullptr)
        {
            error = "Stage 45.10 cannot emit an exceptional cleanup chain without an insertion block";
            return nullptr;
        }

        const std::vector<std::size_t> savedActive = activeCleanupRegions_;
        const std::vector<FuncletContext> savedFunclets = funcletStack_;

        // Pass 1: resolve each ordinal's active-region index and funclet parent. Both are
        // properties of the CALL SITE that started this chain (fixed for the whole chain, the
        // same for every ordinal in it) -- nothing here depends on blocks built later in pass 2,
        // which is exactly what the old single-pass version got wrong: it computed `parentPad`
        // by scanning `savedFunclets` (the state from BEFORE this chain started) even for
        // ordinals later in the path, so an outer ordinal's cleanuppad never actually saw the
        // cleanuppad this same call just built for an inner ordinal as its parent.
        std::vector<std::size_t> regionIndices(cleanupPath.size());
        std::vector<LLVMValueRef> parentPads(cleanupPath.size());
        std::vector<std::size_t> parentPrefixes(cleanupPath.size());
        for (std::size_t pathIndex = 0; pathIndex < cleanupPath.size(); ++pathIndex)
        {
            const std::size_t ordinal = cleanupPath[pathIndex];
            const auto cleanupFound = cleanupRegions_.find(ordinal);
            const std::optional<std::size_t> regionIndex = activeCleanupIndex(ordinal, savedActive);
            if (cleanupFound == cleanupRegions_.end() || cleanupFound->second == nullptr ||
                !regionIndex.has_value())
            {
                LLVMPositionBuilderAtEnd(builder_, sourceBlock);
                error = "Stage 45.10 exceptional cleanup path references an inactive MIR cleanup region";
                return nullptr;
            }
            regionIndices[pathIndex] = *regionIndex;

            LLVMValueRef parentPad = nullptr;
            std::size_t parentPrefix = 0;
            for (std::size_t index = 0; index < savedFunclets.size(); ++index)
            {
                if (*regionIndex >= savedFunclets[index].cleanupDepthAtEntry)
                {
                    parentPad = savedFunclets[index].pad;
                    parentPrefix = index + 1;
                }
            }
            parentPads[pathIndex] = parentPad;
            parentPrefixes[pathIndex] = parentPrefix;
        }

        // Pass 2: build/reuse blocks from OUTERMOST to INNERMOST, so each ordinal's unwind
        // target ("next") is already resolved -- either a previously-cached cleanuppad or a
        // freshly built one -- by the time it is needed. Reusing a cached ordinal here is what
        // gives a cleanup region reached by several unwind chains (a RAII local's own scope-exit,
        // reached both from a try/catch's "no handler matched" edge and, independently, from a
        // sibling try/finally further down the same method) exactly ONE cleanuppad instead of
        // one independent "within none" cleanuppad per chain -- see CachedCleanup's comment and
        // BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md for the crash the duplicate
        // lowering used to produce.
        LLVMBasicBlockRef next = finalUnwindTarget;
        for (std::size_t reverseIndex = 0; reverseIndex < cleanupPath.size(); ++reverseIndex)
        {
            const std::size_t pathIndex = cleanupPath.size() - 1 - reverseIndex;
            const std::size_t ordinal = cleanupPath[pathIndex];
            const std::size_t regionIndex = regionIndices[pathIndex];
            const LLVMValueRef parentPad = parentPads[pathIndex];
            const std::size_t parentPrefix = parentPrefixes[pathIndex];

            const auto cached = cleanupChainCache_.find(ordinal);
            if (cached != cleanupChainCache_.end() && cached->second.parentPad == parentPad &&
                cached->second.next == next)
            {
                next = cached->second.block;
                continue;
            }

            LLVMBasicBlockRef block = appendBlock("eh.cleanup." + std::to_string(ordinal));
            funcletStack_.clear();
            for (std::size_t index = 0; index < parentPrefix; ++index)
            {
                funcletStack_.push_back(savedFunclets[index]);
            }
            activeCleanupRegions_.clear();
            for (std::size_t index = 0; index < regionIndex; ++index)
            {
                activeCleanupRegions_.push_back(savedActive[index]);
            }
            LLVMPositionBuilderAtEnd(builder_, block);
            LLVMValueRef cleanupPad = LLVMBuildCleanupPad(
                builder_, parentPad, nullptr, 0, "eh.cleanuppad");
            if (cleanupPad == nullptr)
            {
                activeCleanupRegions_ = savedActive;
                funcletStack_ = savedFunclets;
                LLVMPositionBuilderAtEnd(builder_, sourceBlock);
                error = "Stage 45.10 failed to create cleanuppad";
                return nullptr;
            }
            funcletStack_.push_back(FuncletContext{
                cleanupPad, FuncletKind::Cleanup, regionIndex, nullptr, next});

            if (!emitCleanupAction(ordinal, error))
            {
                activeCleanupRegions_ = savedActive;
                funcletStack_ = savedFunclets;
                LLVMPositionBuilderAtEnd(builder_, sourceBlock);
                return nullptr;
            }
            if (!terminated())
            {
                LLVMBuildCleanupRet(builder_, cleanupPad, next);
            }

            cleanupChainCache_[ordinal] = CachedCleanup{parentPad, next, block};
            next = block;
        }

        activeCleanupRegions_ = savedActive;
        funcletStack_ = savedFunclets;
        LLVMPositionBuilderAtEnd(builder_, sourceBlock);
        return next;
    }

    LLVMBasicBlockRef buildCurrentUnwindDestination(std::string& error)
    {
        std::size_t retainedDepth = 0;
        LLVMBasicBlockRef finalTarget = nullptr;

        const bool hasLocalHandler = !exceptionHandlerStack_.empty() &&
            exceptionHandlerStack_.back().funcletDepthAtEntry == funcletStack_.size();
        if (hasLocalHandler)
        {
            retainedDepth = std::min(
                exceptionHandlerStack_.back().retainedCleanupDepth,
                activeCleanupRegions_.size());
            finalTarget = exceptionHandlerStack_.back().dispatchBlock;
        }
        else if (!funcletStack_.empty())
        {
            retainedDepth = std::min(
                funcletStack_.back().cleanupDepthAtEntry,
                activeCleanupRegions_.size());
            finalTarget = funcletStack_.back().unwindDestination;
        }

        // A captured local/parameter has one reference owned by the creating activation. If an
        // invoke leaves that activation, normal return lowering never runs, so put that reference
        // behind a function-level SEH cleanup. Local catches deliberately retain the cells: the
        // method is still alive and may continue using both the local and closures after catch.
        if (!hasLocalHandler && finalTarget == nullptr && !ownedCaptureCells_.empty())
        {
            finalTarget = captureCellExceptionalCleanup(error);
            if (finalTarget == nullptr) return nullptr;
        }

        std::vector<std::size_t> path;
        for (std::size_t index = activeCleanupRegions_.size(); index > retainedDepth; --index)
        {
            path.push_back(activeCleanupRegions_[index - 1]);
        }
        return path.empty()
            ? finalTarget
            : emitExceptionalCleanupChain(path, finalTarget, error);
    }

    LLVMValueRef buildPotentiallyThrowingCall(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        const char* name,
        std::string& error)
    {
        LLVMBasicBlockRef unwindBlock = buildCurrentUnwindDestination(error);
        if (!error.empty()) return nullptr;
        if (unwindBlock == nullptr)
        {
            return buildNoUnwindCall(
                functionType, function, arguments, argumentCount, name);
        }
        LLVMBasicBlockRef normalBlock = appendBlock("eh.invoke.cont");
        LLVMValueRef invoke = buildInvokeInCurrentFunclet(
            functionType, function, arguments, argumentCount,
            normalBlock, unwindBlock, name);
        if (invoke == nullptr)
        {
            error = "Stage 45.10 failed to emit invoke for a potentially throwing call";
            return nullptr;
        }
        LLVMPositionBuilderAtEnd(builder_, normalBlock);
        return invoke;
    }

    bool findInterfaceCleanupTarget(
        std::size_t contractMemberOrdinal,
        std::uint64_t& stableInterfaceId,
        std::size_t& slot) const noexcept
    {
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            for (const auto& interfaceTable : table.interfaceTables)
            {
                const auto found = std::find(
                    interfaceTable.interfaceMemberOrdinals.begin(),
                    interfaceTable.interfaceMemberOrdinals.end(),
                    contractMemberOrdinal);
                if (found == interfaceTable.interfaceMemberOrdinals.end()) continue;
                stableInterfaceId = interfaceTable.stableInterfaceTypeId;
                slot = static_cast<std::size_t>(
                    found - interfaceTable.interfaceMemberOrdinals.begin());
                return true;
            }
        }
        return false;
    }

    bool emitInterfaceDispose(
        LLVMValueRef receiver,
        std::size_t contractMemberOrdinal,
        std::string& error)
    {
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr ||
            interfaceMapEntryType_ == nullptr)
        {
            return fail(error, "Stage 45.10 interface Dispose has no interface-map ABI metadata");
        }
        std::uint64_t interfaceId = 0;
        std::size_t slot = 0;
        if (!findInterfaceCleanupTarget(contractMemberOrdinal, interfaceId, slot))
        {
            return fail(error, "Stage 45.10 cannot resolve IDisposable.Dispose interface slot");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "dispose.interface.descriptor");
        LLVMValueRef mapAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 4, "dispose.interface.map.addr");
        LLVMValueRef map = LLVMBuildLoad2(builder_, ptr, mapAddress, "dispose.interface.map");

        LLVMBasicBlockRef mapBlock = appendBlock("dispose.interface.map.ok");
        LLVMBasicBlockRef searchBlock = appendBlock("dispose.interface.search");
        LLVMBasicBlockRef probeBlock = appendBlock("dispose.interface.probe");
        LLVMBasicBlockRef nextBlock = appendBlock("dispose.interface.next");
        LLVMBasicBlockRef foundBlock = appendBlock("dispose.interface.found");
        LLVMBasicBlockRef missingBlock = appendBlock("dispose.interface.missing");
        LLVMBasicBlockRef doneBlock = appendBlock("dispose.interface.done");

        LLVMValueRef hasMap = LLVMBuildICmp(
            builder_, LLVMIntNE, map, LLVMConstNull(ptr), "dispose.interface.map.nonnull");
        LLVMBuildCondBr(builder_, hasMap, mapBlock, missingBlock);

        LLVMPositionBuilderAtEnd(builder_, mapBlock);
        LLVMValueRef countAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 0, "dispose.interface.map.count.addr");
        LLVMValueRef count = LLVMBuildLoad2(builder_, i64, countAddress, "dispose.interface.map.count");
        LLVMValueRef entriesAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 1, "dispose.interface.map.entries.addr");
        LLVMValueRef entries = LLVMBuildLoad2(builder_, ptr, entriesAddress, "dispose.interface.map.entries");
        LLVMBuildBr(builder_, searchBlock);

        LLVMPositionBuilderAtEnd(builder_, searchBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "dispose.interface.map.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {mapBlock};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef inRange = LLVMBuildICmp(
            builder_, LLVMIntULT, index, count, "dispose.interface.map.inrange");
        LLVMBuildCondBr(builder_, inRange, probeBlock, missingBlock);

        LLVMPositionBuilderAtEnd(builder_, probeBlock);
        LLVMValueRef entryIndices[] = {index};
        LLVMValueRef entry = LLVMBuildGEP2(
            builder_, interfaceMapEntryType_, entries, entryIndices, 1, "dispose.interface.entry");
        LLVMValueRef idAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 0, "dispose.interface.entry.id.addr");
        LLVMValueRef id = LLVMBuildLoad2(builder_, i64, idAddress, "dispose.interface.entry.id");
        LLVMValueRef wanted = LLVMConstInt(i64, interfaceId, 0);
        LLVMValueRef matches = LLVMBuildICmp(
            builder_, LLVMIntEQ, id, wanted, "dispose.interface.entry.matches");
        LLVMBuildCondBr(builder_, matches, foundBlock, nextBlock);

        LLVMPositionBuilderAtEnd(builder_, nextBlock);
        LLVMValueRef one = LLVMConstInt(i64, 1, 0);
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, index, one, "dispose.interface.map.next");
        LLVMBuildBr(builder_, searchBlock);
        LLVMValueRef nextValues[] = {nextIndex};
        LLVMBasicBlockRef nextBlocks[] = {nextBlock};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        LLVMPositionBuilderAtEnd(builder_, missingBlock);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::InterfaceDispatchFailure), 0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        LLVMPositionBuilderAtEnd(builder_, foundBlock);
        LLVMValueRef tableAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 1, "dispose.interface.entry.table.addr");
        LLVMValueRef methodTable = LLVMBuildLoad2(builder_, ptr, tableAddress, "dispose.interface.method.table");
        LLVMValueRef slotIndex = LLVMConstInt(i64, slot, 0);
        LLVMValueRef slotIndices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, methodTable, slotIndices, 1, "dispose.interface.slot.addr");
        LLVMValueRef target = LLVMBuildLoad2(builder_, ptr, slotAddress, "dispose.interface.target");
        LLVMTypeRef parameterTypes[] = {ptr};
        LLVMTypeRef disposeType = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameterTypes, 1, 0);
        LLVMValueRef arguments[] = {receiver};
        if (buildPotentiallyThrowingCall(
                disposeType, target, arguments, 1, "", error) == nullptr && !error.empty())
        {
            return false;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return true;
    }

    bool emitUsingDispose(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || node->kind != frontend::ProjectOopMirNodeKind::UsingRegion ||
            !node->localOrdinal.has_value() || !node->disposeContractMemberOrdinal.has_value())
        {
            return fail(error, "Stage 45.10 using cleanup is missing frozen Dispose metadata");
        }
        const auto localFound = localStorage_.find(*node->localOrdinal);
        if (localFound == localStorage_.end() || localFound->second == nullptr)
        {
            return fail(error, "Stage 45.10 using cleanup cannot find its resource local");
        }
        LLVMTypeRef resourceType = llvmType(node->elementTypeKey, error);
        if (resourceType == nullptr) return false;
        LLVMValueRef resource = LLVMBuildLoad2(
            builder_, resourceType, localFound->second, "using.resource.cleanup");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, resource, LLVMConstNull(ptr), "using.resource.isnull");
        LLVMBasicBlockRef disposeBlock = appendBlock("using.dispose");
        LLVMBasicBlockRef doneBlock = appendBlock("using.dispose.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, disposeBlock);

        LLVMPositionBuilderAtEnd(builder_, disposeBlock);
        if (node->interfaceCleanupDispatch)
        {
            if (!emitInterfaceDispose(resource, *node->disposeContractMemberOrdinal, error)) return false;
        }
        else
        {
            if (!node->disposeImplementationMemberOrdinal.has_value())
            {
                return fail(error, "Stage 45.10 direct using cleanup has no Dispose implementation");
            }
            const frontend::ProjectOopMirFunction* disposeFunction =
                mirModel_.functionForMember(*node->disposeImplementationMemberOrdinal);
            if (disposeFunction == nullptr)
            {
                return fail(error, "Stage 45.10 direct using cleanup cannot resolve Dispose MIR function");
            }
            const auto target = functions_.find(disposeFunction->mangledName);
            if (target == functions_.end())
            {
                return fail(error, "Stage 45.10 direct using cleanup cannot resolve Dispose LLVM function");
            }
            LLVMValueRef arguments[] = {resource};
            if (buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function,
                    arguments, 1, "", error) == nullptr && !error.empty())
            {
                return false;
            }
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return true;
    }

    bool emitRaiiDestroy(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr ||
            (node->kind != frontend::ProjectOopMirNodeKind::LocalDeclaration &&
             node->kind != frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow) ||
            !node->localOrdinal.has_value() || !node->cleanupRegionOrdinal.has_value())
        {
            return fail(error, "Stage 49 RAII cleanup is missing local/region identity");
        }
        const auto found = localStorage_.find(*node->localOrdinal);
        if (found == localStorage_.end() || found->second == nullptr)
        {
            return fail(error, "Stage 49 RAII cleanup cannot find owner local storage");
        }
        // Defense in depth (closure checklist P1 item 5/6): semantic analysis already restricts a
        // `raii` local to a class/interface reference or `builtin:string` (see
        // ProjectMethodBodyModel.cpp's "raii local requires a NativeSharp class/interface
        // reference type or native string" diagnostic), so `node->typeKey` reaching here as a
        // value type should never actually happen. But Bug A's own lesson was precisely that an
        // upstream invariant everyone was confident held ("owned parameters storage is always a
        // pointer") turned out not to, so this LLVM layer no longer assumes it silently here
        // either -- if a future MIR change ever let a value-typed cleanup node through, this
        // still degrades to a safe no-op instead of loading/storing an alloca as a pointer.
        if (!requiresOwnedDestruction(node->typeKey) && !node->typeKey.ends_with("[]"))
        {
            return true;
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, found->second, "raii.owner.cleanup");
        LLVMBasicBlockRef destroyBlock = appendBlock("raii.destroy");
        LLVMBasicBlockRef doneBlock = appendBlock("raii.destroy.done");
        // Bug B-006 fix -- see emitCleanupGuardBranch()'s comment. `__C_specific_handler` really does
        // run an enclosing cleanup while unwinding to a catch in the same frame (verified against a
        // clang-built C SEH reference program under the same Wine, which double-runs the analogous
        // __finally identically -- this is the personality's documented behaviour, not a defect
        // unique to this compiler), so a raii object whose scope encloses a try that catches gets
        // its cleanup entered twice: once from the funclet during that unwind, once from the normal
        // scope exit. The volatile flag below is what turns "the cleanup may be entered twice" into
        // "the destructor runs exactly once".
        emitCleanupGuardBranch(found->second, value, destroyBlock, doneBlock, "raii.owner");
        if (isDelegateReference(node->typeKey))
        {
            if (!emitDelegateDestroy(value, "raii.delegate", error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, doneBlock);
            LLVMPositionBuilderAtEnd(builder_, doneBlock);
            return true;
        }
        if (node->typeKey == "builtin:string" || node->typeKey.ends_with("[]"))
        {
            LLVMValueRef arguments[] = {value};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            LLVMBuildBr(builder_, doneBlock);
            LLVMPositionBuilderAtEnd(builder_, doneBlock);
            return true;
        }
        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
        {
            return fail(error, "Stage 49 RAII cleanup has no class-descriptor destruction ABI");
        }
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, value, "raii.descriptor");
        const unsigned destructionEntryIndex = node->stackPromotedObject ? 7U : 5U;
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, destructionEntryIndex,
            node->stackPromotedObject ? "raii.object.entry.addr" : "raii.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "raii.entry");
        LLVMValueRef arguments[] = {value};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return false;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return true;
    }

    bool emitOwnedParameterDestroy(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || node->kind != frontend::ProjectOopMirNodeKind::OwnedParameterCleanup ||
            !node->parameterOrdinal.has_value() || !node->cleanupRegionOrdinal.has_value())
        {
            return fail(error, "Stage 50.8 owned-parameter cleanup is missing parameter/region identity");
        }
        const auto found = parameterStorage_.find(*node->parameterOrdinal);
        if (found == parameterStorage_.end() || found->second == nullptr)
        {
            return fail(error, "Stage 50.8 owned-parameter cleanup cannot find parameter storage");
        }
        // Foreach-driven regression find (52_x_nativesharp_foreach.md): an `owned` PARAMETER,
        // unlike a `raii` LOCAL, is legitimately allowed to be a value type (int, bool, a plain
        // struct, ...) -- "owned" is a harmless no-op for those (Stage 52 fix 1/2/3), which is
        // exactly what lets e.g. `List<T>.Add(owned T value)` compile and run for `T = int`. But
        // this function used to load/store through `found->second` UNCONDITIONALLY as an 8-byte
        // pointer, with no check at all -- correct for the reference-typed/string case this was
        // originally written for, but for an `owned int` parameter `found->second` is a 4-byte
        // `alloca i32`: the unconditional 8-byte load/store below read and wrote past the end of
        // that allocation, corrupting whatever stack memory happened to sit next to it (in
        // practice: silently truncating a caller's `count` field to 1 after any `owned`
        // value-type parameter method that also makes a nested call, e.g. exactly
        // `List<T>.Add`'s `EnsureCapacity(count + 1)` -- the bug foreach-over-List<T> testing
        // caught). This was ALSO the exact root cause identified (but, per
        // WORKLOG_CURRENT_STATE_52.md/WINE_EXECUTION_FINDINGS_STAGE52.md, never actually fixed in
        // source) by the Stage 52 closure's Wine execution finding.
        //
        // A value type owns no separate heap allocation for `owned` to release, so the correct
        // fix is not "load fewer bytes" but "there is nothing to destroy here at all": skip the
        // entire null-check/load/store/destructor-call sequence below whenever the parameter's
        // type does not require real destruction. Centralized in requiresOwnedDestruction() (P1
        // item 6 of the closure checklist) so this exact check can no longer silently diverge
        // from emitRaiiDestroy's and lowerDeallocation's own copies of it.
        if (!requiresOwnedDestruction(node->typeKey))
        {
            return true;
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, found->second, "owned.param.cleanup");
        LLVMBasicBlockRef destroyBlock = appendBlock("owned.param.destroy");
        LLVMBasicBlockRef doneBlock = appendBlock("owned.param.destroy.done");
        // Bug B-006 fix -- identical guard, identical reason as emitRaiiDestroy above (an `owned`
        // parameter's cleanup region is reached from the very same exceptional cleanup chains a
        // `raii` local's is). Fixing only the `raii` side would leave `owned` parameters silently
        // double-destroyed in exactly the same shape.
        emitCleanupGuardBranch(found->second, value, destroyBlock, doneBlock, "owned.param");
        if (isDelegateReference(node->typeKey))
        {
            if (!emitDelegateDestroy(value, "owned.param.delegate", error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, doneBlock);
            LLVMPositionBuilderAtEnd(builder_, doneBlock);
            return true;
        }
        if (node->typeKey == "builtin:string")
        {
            LLVMValueRef arguments[] = {value};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            LLVMBuildBr(builder_, doneBlock);
            LLVMPositionBuilderAtEnd(builder_, doneBlock);
            return true;
        }
        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
        {
            return fail(error, "Stage 50.8 owned-parameter cleanup has no class-descriptor destruction ABI");
        }
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, value, "owned.param.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 5, "owned.param.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "owned.param.entry");
        LLVMValueRef arguments[] = {value};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return false;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return true;
    }

    bool emitCleanupAction(std::size_t ordinal, std::string& error)
    {
        const auto found = cleanupRegions_.find(ordinal);
        if (found == cleanupRegions_.end() || found->second == nullptr)
        {
            return fail(error, "Stage 45.10 cannot resolve MIR cleanup action");
        }
        const frontend::ProjectOopMirNode* cleanup = found->second;
        if (cleanup->kind == frontend::ProjectOopMirNodeKind::UsingRegion)
        {
            return emitUsingDispose(cleanup, error);
        }
        if ((cleanup->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
             cleanup->kind == frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow) &&
            cleanup->cleanupRegionOrdinal.has_value())
        {
            return emitRaiiDestroy(cleanup, error);
        }
        if (cleanup->kind == frontend::ProjectOopMirNodeKind::OwnedParameterCleanup &&
            cleanup->cleanupRegionOrdinal.has_value())
        {
            return emitOwnedParameterDestroy(cleanup, error);
        }
        if (cleanup->kind == frontend::ProjectOopMirNodeKind::FinallyRegion)
        {
            if (cleanup->children.empty() || cleanup->children.front() == nullptr)
            {
                return fail(error, "Stage 45.10 finally cleanup has no body");
            }
            return lowerStatement(cleanup->children.front().get(), error);
        }
        return fail(error, "Stage 50.8 cleanup ordinal does not refer to using/finally/RAII/owned-parameter MIR");
    }

    bool emitOneStructuredCleanup(std::size_t ordinal, std::string& error)
    {
        const std::optional<std::size_t> index = activeCleanupIndex(
            ordinal, activeCleanupRegions_);
        if (!index.has_value())
        {
            return fail(error, "Stage 45.10 structured exit references an inactive cleanup region");
        }
        activeCleanupRegions_.resize(*index);
        return emitCleanupAction(ordinal, error);
    }

    // CC1 EH-001 -- "does this structured exit leave the body of an enclosing `catch`?"
    //
    // The invariant being repaired: the NORMAL end of a catch clause falls through
    // `eh.catch.handled.N`, which calls __nativesharp_exception_clear and drops the thread-local
    // current exception. A `break`, `continue` or `return` out of that same clause branches
    // straight to its structured target and never passes through that block, so the same clause
    // could end with the TLS slot cleared or not cleared depending purely on WHICH legal exit the
    // program took. Clearing the current exception is a semantic obligation of leaving a catch,
    // not a property of falling into one particular LLVM block, so it is decided here.
    //
    // `return` leaves every clause it sits inside. `break`/`continue` target loopStack_.back();
    // that frame leaves the innermost clause exactly when it already existed when the clause body
    // began. Testing the innermost clause is enough: an exit that does not leave the innermost one
    // leaves none, and one call clears the single TLS slot however many it leaves.
    bool structuredExitLeavesCatchBody(bool isReturn) const noexcept
    {
        if (activeCatchExceptions_.empty()) return false;
        if (isReturn) return true;
        if (loopStack_.empty()) return false;
        return (loopStack_.size() - 1U) < activeCatchExceptions_.back().loopDepthAtEntry;
    }

    // The clear is emitted into a block of its own rather than inline, so that what the obligation
    // produced is NAMEABLE in the emitted IR and can be asserted by a test instead of reasoned
    // about. -O2 folds the block away; the call survives.
    void emitCatchBodyExceptionClear()
    {
        LLVMBasicBlockRef leaveBlock = appendBlock("eh.catch.leave");
        LLVMBuildBr(builder_, leaveBlock);
        LLVMPositionBuilderAtEnd(builder_, leaveBlock);
        buildNoUnwindCall(exceptionClearFunctionType_, exceptionClearFunction_, nullptr, 0, "");
    }

    bool lowerStructuredExit(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        const bool isReturn = node->kind == frontend::ProjectOopMirNodeKind::Return;
        const bool isBreak = node->kind == frontend::ProjectOopMirNodeKind::Break;
        const bool isContinue = node->kind == frontend::ProjectOopMirNodeKind::Continue;
        if (!isReturn && !isBreak && !isContinue)
            return fail(error, "Stage 45.10 structured-exit helper received a non-exit MIR node");

        std::size_t targetFuncletDepth = 0;
        if (isBreak || isContinue)
        {
            if (loopStack_.empty()) return fail(error, "Stage 45.10 loop exit has no LLVM loop target");
            // A switch frame carries its own break target but the ENCLOSING LOOP's continue
            // target, so the two sides can sit at different funclet depths. Picking the wrong one
            // here would make a break unwind funclets it must not, or a continue skip funclets it
            // must.
            targetFuncletDepth = isBreak
                ? loopStack_.back().funcletDepth
                : loopStack_.back().continueFuncletDepth;
            if (targetFuncletDepth > funcletStack_.size())
                return fail(error, "Stage 45.10 loop target has an invalid funclet depth");
            if (isContinue && loopStack_.back().continueTarget == nullptr)
                return fail(error, "continue reached the backend with no enclosing loop target");
        }

        LLVMValueRef returnValue = nullptr;
        if (isReturn && !node->children.empty())
        {
            returnValue = lowerExpression(node->children.front().get(), error);
            if (returnValue == nullptr) return false;
        }

        const bool complexExit = !node->cleanupRegionOrdinals.empty() ||
            funcletStack_.size() > targetFuncletDepth;
        if (isReturn && returnValue != nullptr && complexExit)
        {
            if (returnStorage_ == nullptr)
                return fail(error, "Stage 45.10 return through cleanup/catch has no return spill slot");
            LLVMBuildStore(builder_, returnValue, returnStorage_);
        }

        // CC1 EH-001. Before ANY of the exit shapes below -- simple branch, cleanup traversal,
        // funclet unwind -- and on both paths equally, because the divergence being closed is
        // precisely that the simple one skipped it. Emitted BEFORE the cleanups rather than after,
        // matching what eh.catch.handled.N already does (it clears, then branches into the
        // `finally`): a destructor running on the way out may itself throw and publish a new
        // current exception, and clearing afterwards would wipe that one instead of this one.
        if (structuredExitLeavesCatchBody(isReturn))
        {
            emitCatchBodyExceptionClear();
        }

        if (!complexExit)
        {
            if (isReturn)
            {
                releaseOwnedCaptureCells();
                if (returnValue == nullptr) LLVMBuildRetVoid(builder_);
                else emitAbiReturn(returnValue);
            }
            else if (isBreak) LLVMBuildBr(builder_, loopStack_.back().breakTarget);
            else LLVMBuildBr(builder_, loopStack_.back().continueTarget);
            return true;
        }

        const std::vector<std::size_t> savedActive = activeCleanupRegions_;
        const std::vector<FuncletContext> savedFunclets = funcletStack_;
        std::size_t pathIndex = 0;
        auto restoreState = [&]()
        {
            activeCleanupRegions_ = savedActive;
            funcletStack_ = savedFunclets;
        };

        while (funcletStack_.size() > targetFuncletDepth)
        {
            const FuncletContext current = funcletStack_.back();
            if (current.kind != FuncletKind::Catch)
            {
                restoreState();
                return fail(error,
                    "Stage 45.10 semantic invariant forbids structured exit across an active cleanup funclet");
            }

            while (pathIndex < node->cleanupRegionOrdinals.size())
            {
                const std::optional<std::size_t> index = activeCleanupIndex(
                    node->cleanupRegionOrdinals[pathIndex], activeCleanupRegions_);
                if (!index.has_value())
                {
                    restoreState();
                    return fail(error, "Stage 45.10 cleanup path lost lexical cleanup state");
                }
                if (*index < current.cleanupDepthAtEntry) break;
                if (!emitOneStructuredCleanup(node->cleanupRegionOrdinals[pathIndex], error))
                {
                    restoreState();
                    return false;
                }
                ++pathIndex;
                if (terminated())
                {
                    restoreState();
                    return true;
                }
            }

            // NOT REACHED TODAY, and left standing deliberately. Since the B-006/B-007 fix the
            // only push site for funcletStack_ builds FuncletKind::Cleanup entries, so the guard
            // above always takes the failure branch and no exit ever unwinds a Catch funclet here.
            // Verified by sweeping every push_back into funcletStack_, not assumed. It stays
            // because it is the correct code for a Catch entry should one ever be pushed again --
            // and if that happens the extra clear is harmless: __nativesharp_exception_clear only
            // stores null into one TLS slot, so the EH-001 clear emitted above and this one are
            // idempotent with respect to each other. That is what makes "clear twice" a non-issue
            // rather than a bug to guard against.
            LLVMBasicBlockRef catchExit = appendBlock("eh.catch.exit");
            LLVMBuildCatchRet(builder_, current.pad, catchExit);
            funcletStack_.pop_back();
            LLVMPositionBuilderAtEnd(builder_, catchExit);
            buildNoUnwindCall(
                exceptionClearFunctionType_, exceptionClearFunction_, nullptr, 0, "");
        }

        while (pathIndex < node->cleanupRegionOrdinals.size())
        {
            if (!emitOneStructuredCleanup(node->cleanupRegionOrdinals[pathIndex], error))
            {
                restoreState();
                return false;
            }
            ++pathIndex;
            if (terminated())
            {
                restoreState();
                return true;
            }
        }

        if (isReturn)
        {
            releaseOwnedCaptureCells();
            if (returnValue == nullptr)
            {
                LLVMBuildRetVoid(builder_);
            }
            else
            {
                LLVMTypeRef returnType = currentMirFunction_->referenceReturn
                    ? LLVMPointerTypeInContext(context_, 0)
                    : llvmType(currentMirFunction_->returnTypeKey, error);
                if (returnType == nullptr)
                {
                    restoreState();
                    return false;
                }
                LLVMValueRef loaded = LLVMBuildLoad2(
                    builder_, returnType, returnStorage_, "eh.return.value");
                emitAbiReturn(loaded);
            }
        }
        else if (isBreak)
        {
            LLVMBuildBr(builder_, loopStack_.back().breakTarget);
        }
        else
        {
            LLVMBuildBr(builder_, loopStack_.back().continueTarget);
        }
        restoreState();
        return true;
    }

    bool lowerUsingRegion(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (!node->cleanupRegionOrdinal.has_value() || !node->localOrdinal.has_value() ||
            node->children.size() < 2 || node->children[0] == nullptr || node->children[1] == nullptr)
        {
            return fail(error, "Stage 45.10 using MIR region is incomplete");
        }
        const auto localFound = localStorage_.find(*node->localOrdinal);
        if (localFound == localStorage_.end() || localFound->second == nullptr)
            return fail(error, "Stage 45.10 using MIR has no allocated resource local");
        if (!emitDebugLocal(node, localFound->second, error)) return false;

        LLVMValueRef initializer = lowerExpression(node->children[0].get(), error);
        if (initializer == nullptr) return false;
        LLVMBuildStore(builder_, initializer, localFound->second);

        activeCleanupRegions_.push_back(*node->cleanupRegionOrdinal);
        const bool bodyOk = lowerStatement(node->children[1].get(), error);
        activeCleanupRegions_.pop_back();
        if (!bodyOk) return false;
        if (!terminated())
        {
            if (!emitCleanupAction(*node->cleanupRegionOrdinal, error)) return false;
        }
        return true;
    }

    bool lowerExceptionalTransfer(
        LLVMValueRef exception,
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        LLVMBasicBlockRef finalTarget = nullptr;
        bool targetIsLocalToCurrentFunclet = false;
        if (node->exceptionTargetRegionOrdinal.has_value())
        {
            const auto found = exceptionDispatchBlocks_.find(*node->exceptionTargetRegionOrdinal);
            if (found == exceptionDispatchBlocks_.end())
                return fail(error, "Stage 45.10 exception transfer references an inactive MIR handler");
            finalTarget = found->second;
            for (auto handler = exceptionHandlerStack_.rbegin();
                 handler != exceptionHandlerStack_.rend(); ++handler)
            {
                if (handler->regionOrdinal == *node->exceptionTargetRegionOrdinal)
                {
                    targetIsLocalToCurrentFunclet =
                        handler->funcletDepthAtEntry == funcletStack_.size();
                    break;
                }
            }
        }

        std::vector<std::size_t> cleanupPath = node->cleanupRegionOrdinals;
        if (!funcletStack_.empty() && !targetIsLocalToCurrentFunclet)
        {
            const std::size_t boundaryDepth = funcletStack_.back().cleanupDepthAtEntry;
            cleanupPath.erase(
                std::remove_if(
                    cleanupPath.begin(), cleanupPath.end(),
                    [&](std::size_t ordinal)
                    {
                        const std::optional<std::size_t> index =
                            activeCleanupIndex(ordinal, activeCleanupRegions_);
                        return !index.has_value() || *index < boundaryDepth;
                    }),
                cleanupPath.end());
            finalTarget = funcletStack_.back().unwindDestination;
        }

        if (!node->exceptionTargetRegionOrdinal.has_value() && finalTarget == nullptr &&
            !ownedCaptureCells_.empty())
        {
            finalTarget = captureCellExceptionalCleanup(error);
            if (finalTarget == nullptr) return false;
        }

        LLVMBasicBlockRef unwindTarget = cleanupPath.empty()
            ? finalTarget
            : emitExceptionalCleanupChain(cleanupPath, finalTarget, error);
        if (!error.empty()) return false;

        LLVMValueRef arguments[] = {exception};
        if (unwindTarget != nullptr)
        {
            LLVMBasicBlockRef normalBlock = appendBlock("eh.throw.unreachable");
            LLVMValueRef invoke = buildInvokeInCurrentFunclet(
                nativeThrowFunctionType_, nativeThrowFunction_, arguments, 1,
                normalBlock, unwindTarget, "");
            if (invoke == nullptr) return fail(error, "Stage 45.10 failed to create exceptional invoke");
            LLVMPositionBuilderAtEnd(builder_, normalBlock);
            // This block is unreachable in principle -- __nativesharp_throw ends in RaiseException
            // and never returns -- but it must NOT be left as a bare `unreachable`, which emits
            // ZERO bytes. Bug found by real execution (see
            // tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp scenario 14):
            //
            // `__C_specific_handler` decides whether a `__finally` scope is being exited by testing
            // whether the unwind TargetIp lies inside that scope's [Begin, End) instruction range,
            // where End is "last protected instruction + 1". When the last protected instruction is
            // the throw call and this block emits nothing, the catch handler laid out next starts
            // at exactly that End-1 address -- inside the range -- so the personality concludes the
            // enclosing scope is NOT being left and silently skips its `finally`. At -O0 unrelated
            // instructions happened to sit in between and hid it; at -O2 they do not.
            //
            // Verified against clang: for the equivalent C SEH nesting clang always has real
            // normal-path code (the finally call plus the epilogue) between the protected range and
            // the handler, so its TargetIp lands outside the range and its finally runs. Emitting a
            // real trap here restores that property -- and, on the impossible edge where the throw
            // does return, fails loudly instead of falling into the handler.
            LLVMValueRef trapReason = LLVMConstInt(
                LLVMInt32TypeInContext(context_),
                static_cast<std::uint64_t>(LlvmTrapReason::ThrowReturned),
                0);
            LLVMValueRef trapArguments[] = {trapReason};
            buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
            LLVMBuildUnreachable(builder_);
            return true;
        }

        buildNoUnwindCall(
            nativeThrowFunctionType_, nativeThrowFunction_, arguments, 1, "");
        // Same reason as the invoke path above, and just as load-bearing. This is the branch taken
        // when the throw has no unwind destination of its own -- typically a small method whose
        // only statement is `throw`. LLVM's INLINER then turns this plain call into an `invoke` at
        // any call site that does have one, synthesising an empty `.noexc` continuation block; if
        // nothing follows the call here, that block emits zero bytes and the enclosing scope's
        // `finally` (or `owned` parameter cleanup) is silently skipped at -O2 by exactly the
        // TargetIp-inside-the-range rule described above. Emitting the trap on BOTH branches is
        // what makes the property hold after inlining, not just before it.
        LLVMValueRef trapReason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::ThrowReturned),
            0);
        LLVMValueRef trapArguments[] = {trapReason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
        return true;
    }

    bool lowerRethrow(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        // Multi-catch fix: catch bodies are ordinary parent code now, not funclet interiors, so
        // the innermost enclosing catch's exception object lives on activeCatchExceptions_ rather
        // than on a Catch entry in funcletStack_. The funcletStack_ walk is kept as a fallback so
        // any remaining path that still lowers a handler inside a funclet keeps working unchanged.
        LLVMValueRef exception =
            activeCatchExceptions_.empty() ? nullptr : activeCatchExceptions_.back().exception;
        for (auto found = funcletStack_.rbegin();
             exception == nullptr && found != funcletStack_.rend(); ++found)
        {
            if (found->kind == FuncletKind::Catch && found->caughtException != nullptr)
            {
                exception = found->caughtException;
                break;
            }
        }
        if (exception == nullptr)
            return fail(error, "Stage 45.10 rethrow has no active caught NativeSharp exception");
        return lowerExceptionalTransfer(exception, node, error);
    }

    bool lowerInlineAssembly(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || node->kind != frontend::ProjectOopMirNodeKind::InlineAssembly)
            return fail(error, "inline assembly lowering requires an assembly MIR operation");

        std::vector<const frontend::ProjectOopMirNode*> lines;
        std::vector<const frontend::ProjectOopMirNode*> inputs;
        std::vector<const frontend::ProjectOopMirNode*> outputs;
        std::vector<const frontend::ProjectOopMirNode*> clobbers;
        bool sawMemoryClobber = false;
        bool sawFlagsClobber = false;
        for (const auto& childOwner : node->children)
        {
            const frontend::ProjectOopMirNode* child = childOwner.get();
            if (child == nullptr) return fail(error, "inline assembly MIR contains a null record");
            switch (child->kind)
            {
                case frontend::ProjectOopMirNodeKind::InlineAssemblyLine: lines.push_back(child); break;
                case frontend::ProjectOopMirNodeKind::InlineAssemblyInput: inputs.push_back(child); break;
                case frontend::ProjectOopMirNodeKind::InlineAssemblyOutput: outputs.push_back(child); break;
                case frontend::ProjectOopMirNodeKind::InlineAssemblyClobber: clobbers.push_back(child); break;
                default: return fail(error, "inline assembly MIR contains a non-contract child");
            }
        }

        const auto byOrdinal = [](const auto* left, const auto* right)
        {
            return left->integerValue < right->integerValue;
        };
        std::stable_sort(inputs.begin(), inputs.end(), byOrdinal);
        std::stable_sort(outputs.begin(), outputs.end(), byOrdinal);
        std::stable_sort(clobbers.begin(), clobbers.end(), byOrdinal);
        const auto ordinalsAreDense = [](const auto& records)
        {
            for (std::size_t index = 0; index < records.size(); ++index)
                if (records[index]->integerValue != index) return false;
            return true;
        };
        if (!ordinalsAreDense(inputs) || !ordinalsAreDense(outputs) || !ordinalsAreDense(clobbers))
            return fail(error, "inline assembly MIR operand or clobber ordinals are not dense");
        if (!node->inlineAssemblyHasSideEffects)
            return fail(error, "inline assembly MIR lost its mandatory side-effect barrier");

        std::vector<std::string> constraintParts;
        std::vector<LLVMTypeRef> resultTypes;
        std::vector<LLVMValueRef> resultAddresses;
        std::vector<LLVMTypeRef> parameterTypes;
        std::vector<LLVMValueRef> arguments;
        std::vector<std::pair<std::size_t, LLVMTypeRef>> indirectElementTypes;
        std::vector<std::pair<LLVMValueRef, LLVMTypeRef>> readWriteMemoryInputs;
        constraintParts.reserve(outputs.size() + inputs.size() + clobbers.size());

        for (const frontend::ProjectOopMirNode* output : outputs)
        {
            if (output->children.size() != 1 || output->children.front() == nullptr)
                return fail(error, "inline assembly output has no storage address");
            LLVMValueRef address = lowerExpression(output->children.front().get(), error);
            LLVMTypeRef valueType = llvmType(output->typeKey, error);
            if (address == nullptr || valueType == nullptr) return false;

            std::string constraint = output->stringValue;
            const bool memory = constraint == "=m" || constraint == "+m";
            const bool readWrite = !constraint.empty() && constraint.front() == '+';
            if (memory)
            {
                constraint = "=*m";
                constraintParts.push_back(std::move(constraint));
                parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
                arguments.push_back(address);
                indirectElementTypes.emplace_back(arguments.size() - 1, valueType);
                if (readWrite) readWriteMemoryInputs.emplace_back(address, valueType);
            }
            else
            {
                constraintParts.push_back(std::move(constraint));
                resultTypes.push_back(valueType);
                resultAddresses.push_back(address);
                if (readWrite)
                {
                    parameterTypes.push_back(valueType);
                    arguments.push_back(LLVMBuildLoad2(
                        builder_, valueType, address, "asm.output.input"));
                }
            }
        }

        for (const auto& [address, elementType] : readWriteMemoryInputs)
        {
            constraintParts.push_back("*m");
            parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
            arguments.push_back(address);
            indirectElementTypes.emplace_back(arguments.size() - 1, elementType);
        }

        for (const frontend::ProjectOopMirNode* input : inputs)
        {
            if (input->children.size() != 1 || input->children.front() == nullptr)
                return fail(error, "inline assembly input has no lowered operand");
            LLVMValueRef value = lowerExpression(input->children.front().get(), error);
            if (value == nullptr) return false;
            std::string constraint = input->stringValue;
            const bool memory = constraint == "m";
            if (memory) constraint.insert(0, "*");
            LLVMTypeRef parameterType = memory
                ? LLVMPointerTypeInContext(context_, 0)
                : llvmType(input->typeKey, error);
            if (parameterType == nullptr) return false;
            constraintParts.push_back(std::move(constraint));
            parameterTypes.push_back(parameterType);
            arguments.push_back(value);
            if (memory)
            {
                LLVMTypeRef elementType = llvmType(input->typeKey, error);
                if (elementType == nullptr) return false;
                indirectElementTypes.emplace_back(arguments.size() - 1, elementType);
            }
        }

        for (const frontend::ProjectOopMirNode* clobber : clobbers)
        {
            sawFlagsClobber = sawFlagsClobber || clobber->elementTypeKey == "flags";
            sawMemoryClobber = sawMemoryClobber || clobber->elementTypeKey == "memory";
            const std::string name = clobber->elementTypeKey == "flags" ? "cc"
                : clobber->elementTypeKey == "memory" ? "memory" : clobber->stringValue;
            constraintParts.push_back("~{" + name + "}");
        }
        if (sawMemoryClobber != node->inlineAssemblyClobbersMemory ||
            sawFlagsClobber != node->inlineAssemblyClobbersFlags)
            return fail(error, "inline assembly MIR barrier summary disagrees with its clobbers");

        std::vector<std::size_t> inputConstraintOrdinals(inputs.size());
        for (std::size_t index = 0; index < inputs.size(); ++index)
            inputConstraintOrdinals[index] = outputs.size() + readWriteMemoryInputs.size() + index;

        const auto rewritePlaceholders = [&](std::string_view source, std::string& rewritten) -> bool
        {
            for (std::size_t index = 0; index < source.size();)
            {
                if (source[index] != '$')
                {
                    rewritten.push_back(source[index++]);
                    continue;
                }
                if (index + 1 < source.size() && source[index + 1] == '$')
                {
                    rewritten.append("$$");
                    index += 2;
                    continue;
                }

                const bool isOutput = source.substr(index + 1, 3) == "out";
                std::size_t digits = index + (isOutput ? 4 : 1);
                if (digits >= source.size() || !std::isdigit(static_cast<unsigned char>(source[digits])))
                    return fail(error, "inline assembly contains an invalid operand placeholder");
                std::size_t end = digits;
                std::size_t ordinal = 0;
                while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])))
                {
                    ordinal = ordinal * 10 + static_cast<std::size_t>(source[end] - '0');
                    ++end;
                }
                if ((isOutput && ordinal >= outputs.size()) || (!isOutput && ordinal >= inputs.size()))
                    return fail(error, "inline assembly operand placeholder is out of range");
                const std::size_t llvmOrdinal = isOutput ? ordinal : inputConstraintOrdinals[ordinal];
                rewritten.push_back('$');
                rewritten.append(std::to_string(llvmOrdinal));
                index = end;
            }
            return true;
        };

        std::string assembly;
        const auto appendAssemblyLine = [&](std::string_view text)
        {
            if (!assembly.empty()) assembly.push_back('\n');
            assembly.append(text);
        };
        for (const frontend::ProjectOopMirNode* line : lines)
        {
            std::string rewritten;
            if (!rewritePlaceholders(line->stringValue, rewritten)) return false;

            if (debugInformation_ != DebugInformationKind::None && sourceManager_ != nullptr &&
                line->syntax != nullptr && line->syntax->range().isValid())
            {
                const SourceLocation location = line->syntax->range().begin();
                const unsigned fileNumber = debugCodeViewFileNumber(location);
                if (fileNumber != 0)
                {
                    const source::SourcePosition position = sourceManager_->getPosition(location);
                    appendAssemblyLine(
                        ".cv_loc 0 " + std::to_string(fileNumber) + " " +
                        std::to_string(position.line) + " " + std::to_string(position.column));
                }
            }

            constexpr std::string_view labelPrefix = "label:";
            constexpr std::string_view branchPrefix = "branch:";
            if (line->elementTypeKey.starts_with(labelPrefix))
            {
                const std::string label = line->elementTypeKey.substr(labelPrefix.size());
                appendAssemblyLine(".Lnsc_" + label + "_${:uid}:");
            }
            else if (line->elementTypeKey.starts_with(branchPrefix))
            {
                const std::size_t separator = line->elementTypeKey.find(':', branchPrefix.size());
                if (separator == std::string::npos)
                    return fail(error, "inline assembly branch record is incomplete");
                const std::string mnemonic = line->elementTypeKey.substr(
                    branchPrefix.size(), separator - branchPrefix.size());
                const std::string label = line->elementTypeKey.substr(separator + 1);
                appendAssemblyLine(mnemonic + " .Lnsc_" + label + "_${:uid}");
            }
            else
            {
                appendAssemblyLine(rewritten);
            }
        }

        std::string constraints;
        for (std::size_t index = 0; index < constraintParts.size(); ++index)
        {
            if (index != 0) constraints.push_back(',');
            constraints.append(constraintParts[index]);
        }

        LLVMTypeRef returnType = LLVMVoidTypeInContext(context_);
        if (resultTypes.size() == 1) returnType = resultTypes.front();
        else if (resultTypes.size() > 1)
            returnType = LLVMStructTypeInContext(
                context_, resultTypes.data(), static_cast<unsigned>(resultTypes.size()), 0);
        LLVMTypeRef functionType = LLVMFunctionType(
            returnType,
            parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()),
            0);
        LLVMValueRef inlineAssembly = LLVMGetInlineAsm(
            functionType,
            assembly.data(), assembly.size(),
            constraints.data(), constraints.size(),
            node->inlineAssemblyHasSideEffects ? 1 : 0,
            0,
            LLVMInlineAsmDialectIntel,
            0);
        if (inlineAssembly == nullptr)
            return fail(error, "LLVM rejected the integrated assembly contract");
        LLVMValueRef call = LLVMBuildCall2(
            builder_, functionType, inlineAssembly,
            arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()),
            resultTypes.empty() ? "" : "asm.result");
        if (call == nullptr) return fail(error, "LLVM could not build the integrated assembly call");
        const unsigned elementTypeKind = LLVMGetEnumAttributeKindForName("elementtype", 11);
        for (const auto& [argumentIndex, elementType] : indirectElementTypes)
        {
            LLVMAddCallSiteAttribute(
                call,
                static_cast<LLVMAttributeIndex>(argumentIndex + 1),
                LLVMCreateTypeAttribute(context_, elementTypeKind, elementType));
        }

        for (std::size_t index = 0; index < resultAddresses.size(); ++index)
        {
            LLVMValueRef value = resultTypes.size() == 1
                ? call
                : LLVMBuildExtractValue(builder_, call, static_cast<unsigned>(index), "asm.output");
            LLVMBuildStore(builder_, value, resultAddresses[index]);
        }
        return true;
    }

    bool lowerStatement(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr) return true;
        ScopedDebugLocation scopedDebugLocation(builder_, debugLocation(node));
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::Block:
            {
                LLVMMetadataRef lexicalScope = nullptr;
                if (debugInformation_ == DebugInformationKind::Full &&
                    currentDebugScope_ != nullptr && node->syntax != nullptr &&
                    node->syntax->range().isValid())
                {
                    const SourceRange range = node->syntax->range();
                    LLVMMetadataRef file = debugFile(range.begin());
                    const source::SourcePosition position =
                        sourceManager_->getPosition(range.begin());
                    if (file != nullptr)
                    {
                        lexicalScope = LLVMDIBuilderCreateLexicalBlock(
                            debugBuilder_, currentDebugScope_, file,
                            position.line, position.column);
                    }
                }
                ScopedDebugScope scopedDebugScope(currentDebugScope_, lexicalScope);
                const std::size_t cleanupDepthAtEntry = activeCleanupRegions_.size();
                for (const auto& child : node->children)
                {
                    if (terminated()) break;
                    if (!lowerStatement(child.get(), error))
                    {
                        activeCleanupRegions_.resize(cleanupDepthAtEntry);
                        return false;
                    }
                    if (child != nullptr &&
                        (child->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
                         child->kind == frontend::ProjectOopMirNodeKind::OwnedParameterCleanup) &&
                        child->cleanupRegionOrdinal.has_value())
                    {
                        activeCleanupRegions_.push_back(*child->cleanupRegionOrdinal);
                    }
                }
                if (!terminated())
                {
                    while (activeCleanupRegions_.size() > cleanupDepthAtEntry)
                    {
                        const std::size_t ordinal = activeCleanupRegions_.back();
                        activeCleanupRegions_.pop_back();
                        if (!emitCleanupAction(ordinal, error))
                        {
                            activeCleanupRegions_.resize(cleanupDepthAtEntry);
                            return false;
                        }
                        if (terminated()) break;
                    }
                }
                activeCleanupRegions_.resize(cleanupDepthAtEntry);
                return true;
            }

            case frontend::ProjectOopMirNodeKind::OwnedParameterCleanup:
                return true;

            case frontend::ProjectOopMirNodeKind::LocalDeclaration:
                if (node->localOrdinal.has_value())
                {
                    const auto storage = localStorage_.find(*node->localOrdinal);
                    if (storage != localStorage_.end() &&
                        !emitDebugLocal(node, storage->second, error))
                        return false;
                }
                if (!node->children.empty() && node->children.front() != nullptr)
                {
                    LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                    LLVMValueRef address = node->localOrdinal.has_value() ? localStorage_[*node->localOrdinal] : nullptr;
                    if (value == nullptr || address == nullptr) return fail(error, "Stage 34 local initialization failed");
                    LLVMBuildStore(builder_, value, address);
                    return true;
                }
                return emitDefaultLocalStorage(node, error);

            case frontend::ProjectOopMirNodeKind::ExpressionStatement:
                if (!node->children.empty() && lowerExpression(node->children.front().get(), error) == nullptr && error.empty())
                    return fail(error, "Stage 34 expression statement produced no value");
                return error.empty();

            case frontend::ProjectOopMirNodeKind::InlineAssembly:
                return lowerInlineAssembly(node, error);

            case frontend::ProjectOopMirNodeKind::Return:
            case frontend::ProjectOopMirNodeKind::Break:
            case frontend::ProjectOopMirNodeKind::Continue:
                return lowerStructuredExit(node, error);

            case frontend::ProjectOopMirNodeKind::TryRegion:
                return lowerTryRegion(node, error);
            case frontend::ProjectOopMirNodeKind::CatchRegion:
                return fail(error, "Stage 45.10 catch-region must be lowered by its enclosing try-region");
            case frontend::ProjectOopMirNodeKind::FinallyRegion:
                return fail(error, "Stage 45.10 finally-region must be lowered by its owning try/cleanup path");
            case frontend::ProjectOopMirNodeKind::UsingRegion:
                return lowerUsingRegion(node, error);
            case frontend::ProjectOopMirNodeKind::CleanupContinuation:
                return true;
            case frontend::ProjectOopMirNodeKind::Throw:
                return lowerThrow(node, error);
            case frontend::ProjectOopMirNodeKind::Rethrow:
                return lowerRethrow(node, error);

            case frontend::ProjectOopMirNodeKind::If:
                return lowerIf(node, error);
            case frontend::ProjectOopMirNodeKind::While:
                return lowerWhile(node, error);
            case frontend::ProjectOopMirNodeKind::DoWhile:
                return lowerDoWhile(node, error);
            case frontend::ProjectOopMirNodeKind::For:
                return lowerFor(node, error);
            case frontend::ProjectOopMirNodeKind::Switch:
                return lowerSwitch(node, error);
            case frontend::ProjectOopMirNodeKind::SwitchSection:
                return fail(error, "a switch section must be lowered by its enclosing switch");

            default:
            {
                LLVMValueRef value = lowerExpression(node, error);
                return value != nullptr && error.empty();
            }
        }
    }

    // C7 "definite assignment", backend half. A local declared without an initializer used to be
    // an `alloca` and nothing else, so reading it returned whatever the stack happened to hold --
    // silently, at every optimization level, for every type. This gives that declaration a
    // meaning: the slot is zeroed at the point the declaration executes.
    //
    // Why at the declaration and not next to the entry-block alloca: a local declared inside a
    // loop body is a fresh variable on each iteration, and its default has to be re-established on
    // each one. The allocas are hoisted to the entry block (see collectLocalAllocas) purely so
    // mem2reg can promote them; the semantics live here.
    //
    // What this costs: nothing in a program the front-end analysis accepts. There, every read is
    // dominated by a real store, so the zeroing store is dead and SROA/mem2reg delete it. It is
    // paid for only where the program relies on the default -- an aggregate filled field by field,
    // which is exactly the C-style struct idiom the analysis deliberately permits.
    bool emitDefaultLocalStorage(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || !node->localOrdinal.has_value())
        {
            return true;
        }
        const auto slot = localStorage_.find(*node->localOrdinal);
        if (slot == localStorage_.end() || slot->second == nullptr)
        {
            return fail(error, "Stage 56 C7 default local storage has no alloca");
        }
        LLVMTypeRef type = llvmType(node->typeKey, error);
        if (type == nullptr)
        {
            return false;
        }
        // LLVMConstNull covers every shape this can be -- integers and floats zero, pointers
        // null, and an indirect-aggregate carrier `[N x i8]` becomes `zeroinitializer`, which the
        // backend expands into the memset it would have taken to write by hand.
        LLVMValueRef store = LLVMBuildStore(builder_, LLVMConstNull(type), slot->second);
        const unsigned structAlignment = structAlignmentFor(node->typeKey);
        if (structAlignment > 1U)
        {
            LLVMSetAlignment(store, structAlignment);
        }
        return true;
    }

    bool lowerTryRegion(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || node->children.empty() || node->children.front() == nullptr)
            return fail(error, "Stage 45.11 try MIR region is incomplete");

        std::vector<const frontend::ProjectOopMirNode*> catchNodes;
        const frontend::ProjectOopMirNode* finallyNode = nullptr;
        bool sawCatchAll = false;
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* child = node->children[index].get();
            if (child == nullptr) continue;
            if (child->kind == frontend::ProjectOopMirNodeKind::CatchRegion)
            {
                if (sawCatchAll)
                    return fail(error, "Stage 45.11 catch-all handler must remain terminal");
                if (child->children.size() != 1 || child->children.front() == nullptr)
                    return fail(error, "Stage 45.11 catch MIR handler body is incomplete");
                if (child->catchAll)
                {
                    if (child->exceptionTypeOrdinal.has_value())
                        return fail(error, "Stage 45.11 catch-all unexpectedly carries a typed RTTI ordinal");
                    sawCatchAll = true;
                }
                else
                {
                    if (!child->exceptionTypeOrdinal.has_value())
                        return fail(error, "Stage 45.11 typed catch has no exception type ordinal");
                    if (!typedCatchFilters_.contains(*child->exceptionTypeOrdinal))
                        return fail(error, "Stage 45.11 typed catch cannot resolve its RTTI filter");
                }
                catchNodes.push_back(child);
            }
            else if (child->kind == frontend::ProjectOopMirNodeKind::FinallyRegion)
            {
                if (finallyNode != nullptr)
                    return fail(error, "Stage 45.11 try MIR has duplicate finally regions");
                finallyNode = child;
            }
            else
            {
                return fail(error, "Stage 45.11 try MIR contains an unexpected child region");
            }
        }

        const bool hasFinally = node->cleanupRegionOrdinal.has_value();
        if (hasFinally != (finallyNode != nullptr))
            return fail(error, "Stage 45.11 try/finally MIR cleanup identity is inconsistent");
        if (catchNodes.empty() && finallyNode == nullptr)
            return fail(error, "Stage 45.11 try region has neither catch nor finally");
        if (!catchNodes.empty() && !node->mirRegionOrdinal.has_value())
            return fail(error, "Stage 45.11 catch-bearing try has no MIR region identity");

        LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);

        LLVMBasicBlockRef continueBlock = appendBlock("eh.try.cont");
        LLVMBasicBlockRef normalFinallyBlock = hasFinally
            ? appendBlock("eh.finally.normal")
            : nullptr;
        LLVMBasicBlockRef dispatchBlock = !catchNodes.empty()
            ? appendBlock("eh.catch.dispatch")
            : nullptr;
        std::vector<LLVMBasicBlockRef> catchBlocks;
        std::vector<LLVMBasicBlockRef> handledBlocks;
        catchBlocks.reserve(catchNodes.size());
        handledBlocks.reserve(catchNodes.size());
        for (std::size_t index = 0; index < catchNodes.size(); ++index)
        {
            catchBlocks.push_back(appendBlock("eh.catch." + std::to_string(index)));
            handledBlocks.push_back(appendBlock("eh.catch.handled." + std::to_string(index)));
        }

        if (hasFinally)
            activeCleanupRegions_.push_back(*node->cleanupRegionOrdinal);

        if (!catchNodes.empty())
        {
            exceptionDispatchBlocks_[*node->mirRegionOrdinal] = dispatchBlock;
            exceptionHandlerStack_.push_back(ExceptionHandlerContext{
                *node->mirRegionOrdinal,
                dispatchBlock,
                activeCleanupRegions_.size(),
                funcletStack_.size()});
        }

        const bool protectedOk = lowerStatement(node->children.front().get(), error);
        if (!catchNodes.empty())
        {
            exceptionHandlerStack_.pop_back();
            exceptionDispatchBlocks_.erase(*node->mirRegionOrdinal);
        }
        if (!protectedOk)
        {
            if (hasFinally) activeCleanupRegions_.pop_back();
            return false;
        }
        if (!terminated())
            LLVMBuildBr(builder_, hasFinally ? normalFinallyBlock : continueBlock);

        if (!catchNodes.empty())
        {
            LLVMPositionBuilderAtEnd(builder_, dispatchBlock);
            // NOTE: no buildCurrentUnwindDestination() call here any more. The catchswitch below
            // deliberately unwinds to caller, and the enclosing cleanups are run explicitly on the
            // "no clause matched" path instead -- see the long comment on the catchswitch for why.
            // Building the chain here as well would only emit unreachable cleanuppad blocks.
            // Bugs B-006/B-007 (silent premature destruction -- and therefore use-after-free -- of
            // any cleanup-owning local whose scope ENCLOSES a try whose catch actually catches; see
            // BUGS_ENCONTRADOS/bug_eh_raii_doble_destruccion_tras_catch_o0.md) AND the multi-catch
            // bug found while fixing them (a second `catch` clause never matched at all) share one
            // root and are fixed together here. Three facts drive this shape:
            //
            // 1. `__C_specific_handler` -- the Windows x64 personality this compiler targets -- runs
            //    an ENCLOSING cleanup while unwinding to a catch in the SAME frame. LLVM lowers a
            //    catchswitch's unwind edge to a __finally scope-table entry covering the TRY's
            //    instruction range, and the personality runs every such entry whose range contains
            //    the throw IP but not the unwind target IP; the catch handler always lands outside
            //    the try's range, so an enclosing cleanup is indistinguishable from one genuinely
            //    being exited. It therefore destroyed and freed objects that were still in scope.
            //    This is the personality's documented IP-range behaviour, NOT bad IR from this
            //    compiler: compiling the analogous C construct
            //    (`__try { __try {raise} __except {...} } __finally {cleanup}`) with clang and
            //    running it under this same Wine double-runs clang's own __finally identically. So
            //    the fix cannot be "shape the scope table better" -- it has to stop depending on
            //    that edge, which means the catchswitch unwinds TO CALLER.
            // 2. SEH honours exactly ONE handler per catchswitch. LLVM's SEH state numbering emits
            //    a scope-table entry for the first handler only -- verified on the emitted table.
            //    Everything past the first `catch` clause was therefore dead code, and a program
            //    whose matching clause was not the first one simply let the exception escape. That
            //    was a pre-existing P0 bug, independent of the two above, and it also rules out
            //    "just add a synthetic catch-all clause" as a way to solve point 1.
            // 3. A catch clause body does not need to live inside the funclet at all.
            //
            // Hence: ONE catchpad, whose filter is the generic "is this one of ours" filter, that
            // does nothing but `catchret` straight out to ordinary parent code. That parent code
            // re-tests the caught object against each clause's RTTI type IN SOURCE ORDER and jumps
            // to the matching clause body -- so every clause works, not just the first -- and when
            // no clause matches it runs the enclosing cleanups explicitly and re-raises. Because
            // the clause bodies are now ordinary parent blocks rather than funclet interiors, an
            // exception thrown inside a catch body is lowered by the normal machinery and unwinds
            // through the enclosing cleanups exactly like any other code in this scope, with none
            // of the funclet-parentage constraints that previously forced compromises here.
            LLVMValueRef catchSwitch = LLVMBuildCatchSwitch(
                builder_, activeFuncletPad(), nullptr, 1, "eh.catchswitch");
            if (catchSwitch == nullptr)
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return fail(error, "Stage 45.11 failed to create catchswitch");
            }
            LLVMBasicBlockRef padBlock = appendBlock("eh.catchpad.entry");
            LLVMAddHandler(catchSwitch, padBlock);
            LLVMBasicBlockRef selectBlock = appendBlock("eh.catch.select");

            LLVMPositionBuilderAtEnd(builder_, padBlock);
            // __nativesharp_eh_filter accepts any exception carrying this compiler's own exception
            // code -- exactly the set a NativeSharp `catch` could ever handle. A foreign SEH
            // exception (an access violation, say) is still declined here and propagates untouched.
            LLVMValueRef catchArguments[] = {ehFilterFunction_};
            LLVMValueRef catchPad = LLVMBuildCatchPad(
                builder_, catchSwitch, catchArguments, 1, "eh.catchpad");
            if (catchPad == nullptr)
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return fail(error, "Stage 45.11 failed to create catchpad");
            }
            LLVMBuildCatchRet(builder_, catchPad, selectBlock);

            LLVMPositionBuilderAtEnd(builder_, selectBlock);
            LLVMValueRef caughtException = buildNoUnwindCall(
                exceptionCurrentFunctionType_, exceptionCurrentFunction_, nullptr, 0,
                "eh.caught.exception");
            if (caughtException == nullptr)
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return fail(error, "Stage 45.11 could not read the caught exception object");
            }

            // Clause selection, in source order. A catch-all clause terminates the chain; anything
            // after it was already rejected up front ("catch-all handler must remain terminal").
            LLVMBasicBlockRef unmatchedBlock = appendBlock("eh.catch.unmatched");
            LLVMTypeRef i64Type = LLVMInt64TypeInContext(context_);
            for (std::size_t index = 0; index < catchNodes.size(); ++index)
            {
                const frontend::ProjectOopMirNode* catchNode = catchNodes[index];
                if (catchNode->catchAll)
                {
                    LLVMBuildBr(builder_, catchBlocks[index]);
                    break;
                }
                const auto typeIdFound = typedCatchTypeIds_.find(*catchNode->exceptionTypeOrdinal);
                if (typeIdFound == typedCatchTypeIds_.end())
                {
                    if (hasFinally) activeCleanupRegions_.pop_back();
                    return fail(error, "Stage 45.11 lost a typed catch RTTI type id during lowering");
                }
                LLVMValueRef rttiArguments[] = {
                    caughtException, LLVMConstInt(i64Type, typeIdFound->second, 0)};
                LLVMValueRef matches = buildNoUnwindCall(
                    rttiTestFunctionType_, rttiClassTestFunction_, rttiArguments, 2,
                    "eh.catch.matches");
                if (matches == nullptr)
                {
                    if (hasFinally) activeCleanupRegions_.pop_back();
                    return fail(error, "Stage 45.11 failed to emit a catch clause RTTI test");
                }
                const bool isLast = (index + 1 == catchNodes.size());
                LLVMBasicBlockRef nextTest = isLast
                    ? unmatchedBlock
                    : appendBlock("eh.catch.next." + std::to_string(index));
                LLVMBuildCondBr(builder_, matches, catchBlocks[index], nextTest);
                LLVMPositionBuilderAtEnd(builder_, nextTest);
            }
            // Only when the chain did NOT already end by falling into unmatchedBlock itself: the
            // last typed test uses unmatchedBlock as its own "no match" successor, which leaves the
            // builder sitting in that block, and branching from there to itself would put a second
            // terminator in it.
            if (LLVMGetInsertBlock(builder_) != unmatchedBlock && !terminated())
            {
                LLVMBuildBr(builder_, unmatchedBlock);
            }

            // No clause matched: this exception really is leaving the try, so the enclosing cleanup
            // regions must run -- innermost first, the order a real unwind would have used. They are
            // emitted as the same ordinary, guarded destroy sequences the normal scope-exit path
            // uses, so the exactly-once guard in emitRaiiDestroy/emitOwnedParameterDestroy covers
            // this path too. activeCleanupRegions_ still holds this try's own `finally` region here
            // (it is popped further below), which is correct: a `finally` must run before an
            // unmatched exception leaves its try.
            // Snapshot the path before lowering any cleanup. A finally body can contain another
            // cleanup region, and lowering that nested region grows activeCleanupRegions_. Walking
            // the live vector with reverse iterators would then retain an iterator across a
            // possible reallocation (debug MSVC diagnoses this as "can't decrement invalidated
            // vector iterator"; release builds have undefined behaviour).
            LLVMPositionBuilderAtEnd(builder_, unmatchedBlock);
            const std::vector<std::size_t> unmatchedCleanupPath = activeCleanupRegions_;
            for (auto ordinal = unmatchedCleanupPath.rbegin();
                 ordinal != unmatchedCleanupPath.rend(); ++ordinal)
            {
                if (!emitCleanupAction(*ordinal, error))
                {
                    if (hasFinally) activeCleanupRegions_.pop_back();
                    return false;
                }
            }
            // Re-raise the very same exception object, so outer frames' typed filters still see the
            // type they would have seen had this frame never intervened. A plain call, not an
            // invoke: every cleanup this frame owns has just run above, so there is nothing left
            // for an unwind edge here to do.
            LLVMValueRef rethrowArguments[] = {caughtException};
            buildNoUnwindCall(
                nativeThrowFunctionType_, nativeThrowFunction_, rethrowArguments, 1, "");
            LLVMBuildUnreachable(builder_);

            for (std::size_t index = 0; index < catchNodes.size(); ++index)
            {
                const frontend::ProjectOopMirNode* catchNode = catchNodes[index];
                LLVMPositionBuilderAtEnd(builder_, catchBlocks[index]);

                if (catchNode->localOrdinal.has_value())
                {
                    const auto localFound = localStorage_.find(*catchNode->localOrdinal);
                    if (localFound == localStorage_.end() || localFound->second == nullptr)
                    {
                        if (hasFinally) activeCleanupRegions_.pop_back();
                        return fail(error, "Stage 45.11 typed catch local has no LLVM storage");
                    }
                    if (!emitDebugLocal(catchNode, localFound->second, error))
                    {
                        if (hasFinally) activeCleanupRegions_.pop_back();
                        return false;
                    }
                    LLVMBuildStore(builder_, caughtException, localFound->second);
                }

                // `rethrow` inside this body used to find its exception by walking funcletStack_ for
                // an active catch funclet. The body is ordinary parent code now, so it looks here.
                // CC1 EH-001: loopStack_.size() is recorded HERE, before the body is lowered, so
                // a `break`/`continue` inside it can tell "leaves this catch" from "leaves a loop
                // or switch opened inside this catch".
                activeCatchExceptions_.push_back(
                    ActiveCatchBody{caughtException, loopStack_.size()});
                const bool handlerOk = lowerStatement(catchNode->children.front().get(), error);
                const bool handlerTerminated = terminated();
                activeCatchExceptions_.pop_back();
                if (!handlerOk)
                {
                    if (hasFinally) activeCleanupRegions_.pop_back();
                    return false;
                }
                if (!handlerTerminated) LLVMBuildBr(builder_, handledBlocks[index]);

                LLVMPositionBuilderAtEnd(builder_, handledBlocks[index]);
                // The O0 SEH legalization pass has to run mem2reg on this whole function (see
                // LlvmOptimizer.cpp).  A catch-local assignment can consequently disappear into
                // a PHI and leave the machine block with only this mandatory exception-clear
                // call.  Without an explicit source location on that surviving instruction,
                // CodeView inherited the enclosing `try` location and the catch body had no
                // stoppable line at all.  Attribute the handler-completion block to its first
                // source statement (or to `catch` for an empty body).  Instructions that survive
                // from the body keep their more precise nested locations; this is only the stable
                // anchor that mem2reg cannot erase.
                const frontend::ProjectOopMirNode* catchBody = catchNode->children.front().get();
                const frontend::ProjectOopMirNode* catchAnchor = catchNode;
                if (catchBody != nullptr &&
                    catchBody->kind == frontend::ProjectOopMirNodeKind::Block &&
                    !catchBody->children.empty() && catchBody->children.front() != nullptr)
                {
                    catchAnchor = catchBody->children.front().get();
                }
                ScopedDebugLocation catchCompletionLocation(
                    builder_, debugLocation(catchAnchor));
                buildNoUnwindCall(
                    exceptionClearFunctionType_, exceptionClearFunction_, nullptr, 0, "");
                LLVMBuildBr(builder_, hasFinally ? normalFinallyBlock : continueBlock);
            }
        }

        if (hasFinally)
        {
            activeCleanupRegions_.pop_back();
            LLVMPositionBuilderAtEnd(builder_, normalFinallyBlock);
            if (!emitCleanupAction(*node->cleanupRegionOrdinal, error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, continueBlock);
        }

        LLVMPositionBuilderAtEnd(builder_, continueBlock);
        return true;
    }

    bool lowerThrow(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 1 || node->children.front() == nullptr)
            return fail(error, "Stage 45.10 throw MIR requires one exception object value");
        LLVMValueRef exception = lowerExpression(node->children.front().get(), error);
        if (exception == nullptr) return false;
        return lowerExceptionalTransfer(exception, node, error);
    }

    bool lowerIf(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() < 2) return fail(error, "Stage 34 if MIR is incomplete");
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return false;
        LLVMBasicBlockRef thenBlock = appendBlock("if.then");
        LLVMBasicBlockRef elseBlock = node->children.size() >= 3 ? appendBlock("if.else") : nullptr;
        LLVMBasicBlockRef endBlock = appendBlock("if.end");
        LLVMBuildCondBr(builder_, condition, thenBlock, elseBlock != nullptr ? elseBlock : endBlock);

        LLVMPositionBuilderAtEnd(builder_, thenBlock);
        if (!lowerStatement(node->children[1].get(), error)) return false;
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        if (elseBlock != nullptr)
        {
            LLVMPositionBuilderAtEnd(builder_, elseBlock);
            if (!lowerStatement(node->children[2].get(), error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, endBlock);
        }
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

    bool lowerWhile(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 2) return fail(error, "Stage 34 while MIR is incomplete");
        LLVMBasicBlockRef condBlock = appendBlock("while.cond");
        LLVMBasicBlockRef bodyBlock = appendBlock("while.body");
        LLVMBasicBlockRef endBlock = appendBlock("while.end");
        LLVMBuildBr(builder_, condBlock);
        LLVMPositionBuilderAtEnd(builder_, condBlock);
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return false;
        LLVMBuildCondBr(builder_, condition, bodyBlock, endBlock);
        loopStack_.push_back({endBlock, condBlock, funcletStack_.size(), funcletStack_.size()});
        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        const bool ok = lowerStatement(node->children[1].get(), error);
        if (ok && !terminated()) LLVMBuildBr(builder_, condBlock);
        loopStack_.pop_back();
        if (!ok) return false;
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

    // `do { body } while (cond);` -- the same loop as lowerWhile with one difference: the entry
    // branch goes to the BODY instead of to the condition, so the body always runs once.
    //
    // `continue` still lands on the condition block, which is what both C and C# specify: it skips
    // the rest of the body and goes to the test, not back to the top of the body. That falls out
    // of pushing the same {end, cond} pair onto loopStack_ as a while does.
    bool lowerDoWhile(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 2) return fail(error, "do-while MIR is incomplete");
        LLVMBasicBlockRef bodyBlock = appendBlock("dowhile.body");
        LLVMBasicBlockRef condBlock = appendBlock("dowhile.cond");
        LLVMBasicBlockRef endBlock = appendBlock("dowhile.end");
        LLVMBuildBr(builder_, bodyBlock);
        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        loopStack_.push_back({endBlock, condBlock, funcletStack_.size(), funcletStack_.size()});
        const bool ok = lowerStatement(node->children[1].get(), error);
        if (ok && !terminated()) LLVMBuildBr(builder_, condBlock);
        loopStack_.pop_back();
        if (!ok) return false;
        LLVMPositionBuilderAtEnd(builder_, condBlock);
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return false;
        LLVMBuildCondBr(builder_, condition, bodyBlock, endBlock);
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

    // `switch (governing) { case a: ... }` lowers to ONE `LLVMBuildSwitch`, which is what the
    // whole construct is for: LLVM turns that into a jump table when the label values are dense
    // enough and into a balanced decision tree when they are not. The chain of `else if` it
    // replaces is n comparisons in the worst case no matter what the values look like, and a
    // WndProc is forty of them.
    //
    // NO FALL-THROUGH, so the block layout is flat: each case block ends by branching to
    // switch.end -- exactly as if the section had written `break` -- rather than falling into the
    // next section's block. `break` inside a section produces the same branch through the ordinary
    // structured-exit path, which is why `break` stays legal and means what it always meant.
    //
    // WHEN THERE IS NO `default`, the switch's default target is switch.end: an unmatched value
    // skips the whole statement.
    bool lowerSwitch(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.empty()) return fail(error, "switch MIR is incomplete");

        LLVMValueRef governing = lowerExpression(node->children[0].get(), error);
        if (governing == nullptr) return false;
        const std::string& governingTypeKey = node->children[0]->typeKey;
        LLVMTypeRef governingType = llvmType(governingTypeKey, error);
        if (governingType == nullptr) return false;
        // The frontend already rejected anything that is not integral or an enum, and llvmType maps
        // exactly those two to an integer carrier. Restating the rule here -- in type-key terms
        // rather than by probing the LLVM type -- keeps the backend honest if that ever changes,
        // and needs no new entry in LlvmCApi.h.
        if (!isEnumTypeKey(governingTypeKey) &&
            !frontend::ProjectTypeConversion::isIntegral(governingTypeKey))
        {
            return fail(error, "switch governing value is not integral or an enum: " + governingTypeKey);
        }
        // EL SIGNO DE LAS CONSTANTES DE `case` es el del tipo que gobierna: el subyacente si es un
        // enum, y el suyo propio en cualquier otro caso. Es la misma regla que usa la bajada de
        // campos `const`, asi que una constante de `case` y el valor con el que se compara no
        // pueden discrepar de signo. Un `switch` sobre un `enum : byte` con un miembro en 200
        // dependia de esto: con signo, la constante se emitia como -56.
        const frontend::builtins::IntegerTypeInfo* governingEnum = enumUnderlyingInfo(governingTypeKey);
        const LLVMBool signExtend =
            (governingEnum != nullptr ? governingEnum->isSigned : isSignedInteger(governingTypeKey))
                ? 1
                : 0;

        const std::size_t sectionCount = node->children.size() - 1;
        std::vector<LLVMBasicBlockRef> sectionBlocks;
        sectionBlocks.reserve(sectionCount);
        for (std::size_t index = 0; index < sectionCount; ++index)
        {
            sectionBlocks.push_back(appendBlock("switch.case"));
        }
        LLVMBasicBlockRef endBlock = appendBlock("switch.end");

        LLVMBasicBlockRef defaultBlock = endBlock;
        unsigned caseCount = 0;
        for (std::size_t index = 0; index < sectionCount; ++index)
        {
            const frontend::ProjectOopMirNode* section = node->children[index + 1].get();
            if (section == nullptr || section->kind != frontend::ProjectOopMirNodeKind::SwitchSection)
                return fail(error, "a switch MIR child is not a section");
            if (section->switchHasDefault) defaultBlock = sectionBlocks[index];
            caseCount += static_cast<unsigned>(section->switchLabels.size());
        }

        LLVMValueRef instruction = LLVMBuildSwitch(builder_, governing, defaultBlock, caseCount);
        for (std::size_t index = 0; index < sectionCount; ++index)
        {
            const frontend::ProjectOopMirNode* section = node->children[index + 1].get();
            for (const std::int64_t label : section->switchLabels)
            {
                LLVMAddCase(
                    instruction,
                    LLVMConstInt(governingType, static_cast<unsigned long long>(label), signExtend),
                    sectionBlocks[index]);
            }
        }

        // The frame that makes `break` mean "leave the switch". Its continue target is INHERITED
        // from the enclosing loop, if there is one, so `continue` inside a case continues that loop
        // -- the body model already proved there is one to continue.
        const LLVMBasicBlockRef enclosingContinue =
            loopStack_.empty() ? nullptr : loopStack_.back().continueTarget;
        const std::size_t enclosingContinueDepth =
            loopStack_.empty() ? 0 : loopStack_.back().continueFuncletDepth;
        loopStack_.push_back(
            {endBlock, enclosingContinue, funcletStack_.size(), enclosingContinueDepth});

        bool ok = true;
        for (std::size_t index = 0; index < sectionCount && ok; ++index)
        {
            const frontend::ProjectOopMirNode* section = node->children[index + 1].get();
            LLVMPositionBuilderAtEnd(builder_, sectionBlocks[index]);
            if (section->children.empty() || section->children.front() == nullptr)
            {
                // Labels with no statements. Legal, and it means "match these values and do
                // nothing" -- never "fall into the next section".
                LLVMBuildBr(builder_, endBlock);
                continue;
            }
            ok = lowerStatement(section->children.front().get(), error);
            if (ok && !terminated()) LLVMBuildBr(builder_, endBlock);
        }
        loopStack_.pop_back();
        if (!ok) return false;

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

    bool lowerFor(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 4) return fail(error, "Stage 34 for MIR is incomplete");
        const std::size_t cleanupDepthAtEntry = activeCleanupRegions_.size();
        if (node->children[0] != nullptr)
        {
            if (node->children[0]->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration)
            {
                if (!lowerStatement(node->children[0].get(), error)) return false;
                if (node->children[0]->cleanupRegionOrdinal.has_value())
                    activeCleanupRegions_.push_back(*node->children[0]->cleanupRegionOrdinal);
            }
            else if (lowerExpression(node->children[0].get(), error) == nullptr) return false;
        }
        LLVMBasicBlockRef condBlock = appendBlock("for.cond");
        LLVMBasicBlockRef bodyBlock = appendBlock("for.body");
        LLVMBasicBlockRef incBlock = appendBlock("for.inc");
        LLVMBasicBlockRef endBlock = appendBlock("for.end");
        LLVMBuildBr(builder_, condBlock);
        LLVMPositionBuilderAtEnd(builder_, condBlock);
        if (node->children[1] != nullptr)
        {
            LLVMValueRef condition = lowerExpression(node->children[1].get(), error);
            if (condition == nullptr) return false;
            LLVMBuildCondBr(builder_, condition, bodyBlock, endBlock);
        }
        else LLVMBuildBr(builder_, bodyBlock);

        loopStack_.push_back({endBlock, incBlock, funcletStack_.size(), funcletStack_.size()});
        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        const bool bodyOk = lowerStatement(node->children[3].get(), error);
        if (bodyOk && !terminated()) LLVMBuildBr(builder_, incBlock);
        loopStack_.pop_back();
        if (!bodyOk) return false;

        LLVMPositionBuilderAtEnd(builder_, incBlock);
        if (node->children[2] != nullptr && lowerExpression(node->children[2].get(), error) == nullptr) return false;
        if (!terminated()) LLVMBuildBr(builder_, condBlock);
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (!terminated())
        {
            while (activeCleanupRegions_.size() > cleanupDepthAtEntry)
            {
                const std::size_t ordinal = activeCleanupRegions_.back();
                activeCleanupRegions_.pop_back();
                if (!emitCleanupAction(ordinal, error))
                {
                    activeCleanupRegions_.resize(cleanupDepthAtEntry);
                    return false;
                }
            }
        }
        activeCleanupRegions_.resize(cleanupDepthAtEntry);
        return true;
    }

    LLVMValueRef lowerExpression(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr) return nullptr;
        ScopedDebugLocation scopedDebugLocation(builder_, debugLocation(node));
        switch (node->kind)
        {
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
                }
                return value;
            }
            case frontend::ProjectOopMirNodeKind::MoveArrayElement:
            {
                // Stage 52: move(items[i]) for generic collections (Queue<T>.Dequeue(),
                // Stack<T>.Pop()). The single child is the already-lowered element address
                // (ArrayElementAddress), bounds-checked exactly like a normal array read.
                if (node->children.size() != 1 || node->children.front() == nullptr)
                    return failValue(error, "Stage 52 move array element requires exactly one address operand");
                LLVMValueRef address = lowerExpression(node->children.front().get(), error);
                if (address == nullptr) return nullptr;
                LLVMTypeRef elementType = llvmType(node->typeKey, error);
                if (elementType == nullptr) return nullptr;
                LLVMValueRef value = LLVMBuildLoad2(builder_, elementType, address, "move.array.element.load");
                if (node->booleanValue)
                {
                    // Reference-like element (class/interface reference or native string):
                    // null out the slot so the container's own cleanup/Deallocation code
                    // never double-frees or double-uses the value that was just moved out.
                    // Value-typed elements (int, bool, a plain struct, ...) have no ownership
                    // to give up, so the slot is left untouched -- a harmless plain read.
                    LLVMBuildStore(builder_, LLVMConstNull(elementType), address);
                }
                return value;
            }
            case frontend::ProjectOopMirNodeKind::AddressValue:
            {
                // `&lvalue`: the wrapped child already computes the address; this node exists
                // so the EXPRESSION carries the pointer type (T*) for ABI decisions downstream.
                if (node->children.size() != 1 || node->children.front() == nullptr)
                    return failValue(error, "address-of requires exactly one addressable operand");
                return lowerExpression(node->children.front().get(), error);
            }
            case frontend::ProjectOopMirNodeKind::RawElementAddress:
            {
                // C2.1 "fixed arrays": base + index * elementSize, as a byte-wise GEP. The base is
                // already an address by the time it gets here -- a fixed array's own storage, or
                // the value of a raw pointer -- so there is nothing to load and nothing to skip:
                // unlike a managed array (ArrayElementAddress above) there is NO length header at
                // the front and therefore no bounds check either. That is the raw tier's whole
                // contract, and it is what makes `float color[4]` inside a C struct expressible:
                // the element sits exactly where the C compiler put it.
                if (node->children.size() != 2 || node->immediate == 0)
                    return failValue(error, "C2.1 raw element address requires a base, an index and an element size");
                LLVMValueRef base = lowerExpression(node->children[0].get(), error);
                if (base == nullptr) return nullptr;
                LLVMValueRef index = normalizeNativeIndex(node->children[1].get(), error);
                if (index == nullptr) return nullptr;
                LLVMValueRef offset = LLVMBuildMul(
                    builder_, index,
                    LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                    "raw.element.offset");
                LLVMValueRef indices[] = {offset};
                return LLVMBuildGEP2(
                    builder_, LLVMInt8TypeInContext(context_), base, indices, 1, "raw.element.addr");
            }
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
            {
                if (node->children.empty() || node->immediate == 0)
                    return failValue(error, "stack allocation requires extents and an element size");
                constexpr std::uint64_t kMaximumStackAllocationBytes = std::uint64_t{1} << 20;
                LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
                LLVMValueRef total = LLVMConstInt(i64, 1, 0);
                const std::uint64_t maximumElements =
                    kMaximumStackAllocationBytes / node->immediate;
                LLVMValueRef maximum = LLVMConstInt(i64, maximumElements, 0);
                for (const auto& extentChild : node->children)
                {
                    const frontend::ProjectOopMirNode* extentNode = extentChild.get();
                    LLVMValueRef rawExtent = lowerExpression(extentNode, error);
                    if (rawExtent == nullptr) return nullptr;
                    if (isSignedInteger(extentNode->typeKey))
                    {
                        LLVMTypeRef extentType = llvmType(extentNode->typeKey, error);
                        if (extentType == nullptr) return nullptr;
                        LLVMValueRef zero = LLVMConstNull(extentType);
                        LLVMValueRef nonNegative = LLVMBuildICmp(
                            builder_, LLVMIntSGE, rawExtent, zero, "stack.extent.nonnegative");
                        if (!emitGuard(
                                nonNegative,
                                LlvmTrapReason::AllocationSizeOverflow,
                                "stack.extent",
                                error))
                            return nullptr;
                    }
                    LLVMValueRef extent = convert(
                        rawExtent, extentNode->typeKey, "builtin:nuint", error);
                    if (extent == nullptr) return nullptr;

                    // Check before multiplying. `safeDivisor` keeps the guard itself defined for
                    // an already-zero product; once any extent is zero, the final byte count is
                    // zero and every remaining factor is harmless regardless of its magnitude.
                    LLVMValueRef totalIsZero = LLVMBuildICmp(
                        builder_, LLVMIntEQ, total, LLVMConstInt(i64, 0, 0),
                        "stack.total.zero");
                    LLVMValueRef safeDivisor = LLVMBuildSelect(
                        builder_, totalIsZero, LLVMConstInt(i64, 1, 0), total,
                        "stack.total.divisor");
                    LLVMValueRef remaining = LLVMBuildUDiv(
                        builder_, maximum, safeDivisor, "stack.extent.maximum");
                    LLVMValueRef extentFits = LLVMBuildICmp(
                        builder_, LLVMIntULE, extent, remaining, "stack.extent.fits");
                    LLVMValueRef fits = LLVMBuildOr(
                        builder_, totalIsZero, extentFits, "stack.extent.safe");
                    if (!emitGuard(
                            fits,
                            LlvmTrapReason::AllocationSizeOverflow,
                            "stack.size",
                            error))
                        return nullptr;
                    total = LLVMBuildMul(builder_, total, extent, "stack.element.count");
                }
                LLVMValueRef bytes = LLVMBuildMul(
                    builder_, total,
                    LLVMConstInt(i64, node->immediate, 0),
                    "stackalloc.bytes");
                LLVMValueRef storage = LLVMBuildArrayAlloca(
                    builder_, LLVMInt8TypeInContext(context_), bytes, "stackalloc.storage");
                if (storage == nullptr)
                    return failValue(error, "CC1 stackalloc failed to emit its array alloca");
                // Un alloca de i8 sale con alineacion 1. El elemento puede necesitar mas -- un
                // `double` la necesita -- y sin esto los accesos quedarian desalineados.
                LLVMSetAlignment(storage, static_cast<unsigned>(node->alignment == 0 ? 1 : node->alignment));
                return storage;
            }

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
            {
                if (node->children.size() != 2 || node->immediate == 0)
                    return failValue(error, "CC1 pointer arithmetic requires two operands and an element size");
                const frontend::ProjectOopMirNode* leftNode = node->children[0].get();
                const frontend::ProjectOopMirNode* rightNode = node->children[1].get();
                if (leftNode == nullptr || rightNode == nullptr)
                    return failValue(error, "CC1 pointer arithmetic has a null operand");

                LLVMTypeRef i64Type = LLVMInt64TypeInContext(context_);
                LLVMValueRef stride = LLVMConstInt(i64Type, node->immediate, 0);

                // DIFERENCIA. El frontend ya decidio que esto es `p - q` dandole al nodo el tipo
                // `builtin:nint` en vez de `T*`; aqui no se vuelve a decidir nada.
                if (!node->typeKey.ends_with("*"))
                {
                    LLVMValueRef left = lowerExpression(leftNode, error);
                    if (left == nullptr) return nullptr;
                    LLVMValueRef right = lowerExpression(rightNode, error);
                    if (right == nullptr) return nullptr;
                    LLVMValueRef leftInt = LLVMBuildPtrToInt(builder_, left, i64Type, "ptrdiff.left");
                    LLVMValueRef rightInt = LLVMBuildPtrToInt(builder_, right, i64Type, "ptrdiff.right");
                    LLVMValueRef bytes = LLVMBuildSub(builder_, leftInt, rightInt, "ptrdiff.bytes");
                    // Con signo: q - p puede ser negativo, y esa es la mitad del valor de la
                    // operacion. Una division sin signo daria un numero enorme en vez de -1.
                    return LLVMBuildSDiv(builder_, bytes, stride, "ptrdiff.elements");
                }

                // DESPLAZAMIENTO. Cual de los dos hijos es el puntero se deduce de su tipo, que ya
                // lo trae; `i + p` es legal y pone el puntero a la derecha.
                const bool leftIsPointer = leftNode->typeKey.ends_with("*");
                const frontend::ProjectOopMirNode* pointerNode = leftIsPointer ? leftNode : rightNode;
                const frontend::ProjectOopMirNode* offsetNode = leftIsPointer ? rightNode : leftNode;

                LLVMValueRef pointer = lowerExpression(pointerNode, error);
                if (pointer == nullptr) return nullptr;
                LLVMValueRef index = normalizeNativeIndex(offsetNode, error);
                if (index == nullptr) return nullptr;

                LLVMValueRef offset = LLVMBuildMul(builder_, index, stride, "pointer.offset");
                if (node->operation == lexer::TokenKind::Minus)
                {
                    // `p - i`. El frontend ya rechazo `i - p`, asi que restar siempre significa
                    // retroceder desde el puntero.
                    offset = LLVMBuildNeg(builder_, offset, "pointer.offset.neg");
                }
                LLVMValueRef indices[] = {offset};
                // GEP SIN `inbounds`, a proposito. `inbounds` promete que el resultado se queda
                // dentro de la misma asignacion, y este nivel del lenguaje no hace esa promesa --
                // no hay comprobacion de rango, la semantica es la de C, y el propio comentario de
                // RawElementAddress lo dice. Prometerlo aqui convertiria en comportamiento
                // indefinido codigo que el lenguaje declara legal.
                return LLVMBuildGEP2(
                    builder_, LLVMInt8TypeInContext(context_), pointer, indices, 1, "pointer.arith");
            }

            // IDs 79 y 81 -- `x op= e`, `++x`, `x++`. children[0] es la DIRECCION del destino,
            // bajada UNA sola vez; children[1] el operando derecho. Carga, opera, guarda.
            //
            // NADA DE ARITMETICA PROPIA: la operacion entera se delega en applyBinaryOperation,
            // el mismo codigo que emite un `Binary` corriente. Por eso `x /= y` con `uint` divide
            // sin signo y `x >>= n` con `int` desplaza aritmeticamente sin que este caso tenga que
            // saber nada de eso.
            case frontend::ProjectOopMirNodeKind::CompoundStore:
            {
                if (node->children.size() != 2 || node->children[0] == nullptr ||
                    node->children[1] == nullptr)
                    return failValue(error, "compound store requires a target address and a value");

                LLVMValueRef address = lowerExpression(node->children[0].get(), error);
                if (address == nullptr) return nullptr;
                LLVMTypeRef targetType = llvmType(node->typeKey, error);
                if (targetType == nullptr) return nullptr;
                const bool bitField =
                    node->children[0]->kind == frontend::ProjectOopMirNodeKind::BitFieldAddress;
                LLVMValueRef previousStorage = nullptr;
                LLVMValueRef previous = nullptr;
                if (bitField)
                {
                    LLVMTypeRef storageType = bitFieldStorageType(node->children[0].get(), error);
                    if (storageType == nullptr) return nullptr;
                    previousStorage = LLVMBuildLoad2(
                        builder_, storageType, address, "bitfield.compound.storage.load");
                    LLVMSetAlignment(
                        previousStorage, static_cast<unsigned>(node->children[0]->alignment));
                    previous = extractBitField(node->children[0].get(), previousStorage, error);
                }
                else
                {
                    previous = LLVMBuildLoad2(builder_, targetType, address, "compound.load");
                    if (node->children[0]->volatileAccess) LLVMSetVolatile(previous, 1);
                }
                if (previous == nullptr) return nullptr;

                // El operando derecho se baja UNA SOLA VEZ, en la rama que le toca. Bajarlo antes
                // del `if` y ademas dentro de normalizeNativeIndex lo evaluaria dos veces, que es
                // el mismo defecto que este nodo existe para evitar en el destino.
                LLVMValueRef updated = nullptr;
                if (node->immediate != 0)
                {
                    // DESTINO PUNTERO (`p += n`, `++p`). El paso es el del tipo apuntado, igual
                    // que en PointerArithmetic, y el GEP va SIN `inbounds` por la misma razon que
                    // alli: este nivel del lenguaje no promete quedarse dentro de una asignacion.
                    if (node->operation != lexer::TokenKind::Plus &&
                        node->operation != lexer::TokenKind::Minus)
                        return failValue(error, "a pointer target supports only '+' and '-' updates");
                    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context_);
                    LLVMValueRef stride = LLVMConstInt(i64Type, node->immediate, 0);
                    LLVMValueRef index = normalizeNativeIndex(node->children[1].get(), error);
                    if (index == nullptr) return nullptr;
                    LLVMValueRef offset = LLVMBuildMul(builder_, index, stride, "compound.offset");
                    if (node->operation == lexer::TokenKind::Minus)
                    {
                        offset = LLVMBuildNeg(builder_, offset, "compound.offset.neg");
                    }
                    LLVMValueRef indices[] = {offset};
                    updated = LLVMBuildGEP2(
                        builder_, LLVMInt8TypeInContext(context_), previous, indices, 1, "compound.arith");
                }
                else
                {
                    LLVMValueRef operand = lowerExpression(node->children[1].get(), error);
                    if (operand == nullptr) return nullptr;
                    if (node->operation == lexer::TokenKind::LessLess ||
                        node->operation == lexer::TokenKind::GreaterGreater)
                    {
                        operand = normalizeShiftCount(
                            operand, node->children[1]->typeKey, node->typeKey, error);
                        if (operand == nullptr) return nullptr;
                    }
                    updated = applyBinaryOperation(
                        node->operation, previous, operand, node->typeKey, error);
                }
                if (updated == nullptr) return nullptr;
                LLVMValueRef storedValue = updated;
                if (bitField)
                {
                    LLVMValueRef merged = insertBitField(
                        node->children[0].get(), previousStorage, updated, error);
                    if (merged == nullptr) return nullptr;
                    LLVMValueRef store = LLVMBuildStore(builder_, merged, address);
                    LLVMSetAlignment(store, static_cast<unsigned>(node->children[0]->alignment));
                    storedValue = extractBitField(node->children[0].get(), merged, error);
                }
                else
                {
                    LLVMValueRef store = LLVMBuildStore(builder_, updated, address);
                    if (node->children[0]->volatileAccess) LLVMSetVolatile(store, 1);
                }
                if (storedValue == nullptr) return nullptr;
                // `x++` VALE EL VALOR ANTIGUO; `++x` y `x += e` valen el nuevo. Un solo bit de
                // diferencia porque todo lo anterior es identico.
                return node->booleanValue ? previous : storedValue;
            }

            case frontend::ProjectOopMirNodeKind::PointerLoad:
            {
                // Interop Foundation: `*pointer` as a value -- a plain typed load through a raw
                // pointer. Struct pointees load their [N x i8] carrier (value copy semantics),
                // primitives load directly. Null/validity is deliberately unchecked: this is
                // the raw tier, matching C semantics for FFI shapes like `GetMessage(&msg)`.
                if (node->children.size() != 1 || node->children.front() == nullptr)
                    return failValue(error, "pointer load requires exactly one pointer operand");
                LLVMValueRef pointer = lowerExpression(node->children.front().get(), error);
                if (pointer == nullptr) return nullptr;
                LLVMTypeRef pointee = llvmType(node->typeKey, error);
                if (pointee == nullptr) return nullptr;
                return LLVMBuildLoad2(builder_, pointee, pointer, "pointer.load");
            }
            case frontend::ProjectOopMirNodeKind::MoveOwnedField:
            {
                // Bloque 1: move(holder.slot) / move(slot) -- extract the owner out of an `owned`
                // field. The single child is the field's already-lowered ADDRESS. Load the value,
                // then null the vacated slot for reference-like types so the container's own
                // discharge -- its destruction entry, or a later overwrite's release, both of
                // which are guarded by the slot's null state -- skips it instead of double-freeing
                // the value that just changed owners. Value-typed fields (a generic `owned T`
                // closed over int, a struct, ...) have no ownership to hand over, so the move
                // degrades to a plain read and the slot is left untouched, exactly like the
                // array-element and named-owner cases.
                if (node->children.size() != 1 || node->children.front() == nullptr)
                    return failValue(error, "Bloque 1 move owned field requires exactly one address operand");
                LLVMValueRef address = lowerExpression(node->children.front().get(), error);
                if (address == nullptr) return nullptr;
                LLVMTypeRef fieldType = llvmType(node->typeKey, error);
                if (fieldType == nullptr) return nullptr;
                LLVMValueRef value = LLVMBuildLoad2(builder_, fieldType, address, "move.owned.field.load");
                if (node->booleanValue)
                {
                    LLVMBuildStore(builder_, LLVMConstNull(fieldType), address);
                }
                return value;
            }
            case frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow:
            {
                if (node->children.size() != 1 || node->children.front() == nullptr ||
                    !node->localOrdinal.has_value() || !node->cleanupRegionOrdinal.has_value())
                {
                    return failValue(
                        error, "NSC-LIMIT-021 owned temporary borrow is missing value/storage/cleanup identity");
                }
                const auto storage = localStorage_.find(*node->localOrdinal);
                if (storage == localStorage_.end() || storage->second == nullptr)
                {
                    return failValue(error, "NSC-LIMIT-021 owned temporary borrow has no hidden owner slot");
                }
                // Evaluate before activating the cleanup: if construction itself throws, no owner
                // exists. Once stored, later arguments and the call see this region and unwind it.
                LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                if (value == nullptr) return nullptr;
                LLVMBuildStore(builder_, value, storage->second);
                activeCleanupRegions_.push_back(*node->cleanupRegionOrdinal);
                return value;
            }
            case frontend::ProjectOopMirNodeKind::FullExpressionCleanup:
            {
                if (node->children.size() != 1 || node->children.front() == nullptr)
                {
                    return failValue(error, "NSC-LIMIT-021 full expression requires exactly one value");
                }
                const std::size_t retainedDepth = activeCleanupRegions_.size();
                LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                if (value == nullptr) return nullptr;
                std::vector<std::size_t> temporaries(
                    activeCleanupRegions_.begin() + static_cast<std::ptrdiff_t>(retainedDepth),
                    activeCleanupRegions_.end());
                activeCleanupRegions_.resize(retainedDepth);
                for (auto it = temporaries.rbegin(); it != temporaries.rend(); ++it)
                {
                    if (!emitCleanupAction(*it, error)) return nullptr;
                }
                return value;
            }
            case frontend::ProjectOopMirNodeKind::Conditional:
                return lowerConditional(node, error);
            case frontend::ProjectOopMirNodeKind::BorrowValue:
                if (node->children.size() != 1 || node->children.front() == nullptr)
                    return failValue(error, "Stage 50.8 borrow value must wrap exactly one physical reference value");
                return lowerExpression(node->children.front().get(), error);
            case frontend::ProjectOopMirNodeKind::ThisValue:
                return constructionThisOverride_ != nullptr ? constructionThisOverride_ : thisValue_;
            case frontend::ProjectOopMirNodeKind::ParameterValue:
            {
                if (!node->parameterOrdinal.has_value()) return failValue(error, "parameter value missing ordinal");
                LLVMValueRef address = parameterStorage_[*node->parameterOrdinal];
                LLVMTypeRef type = llvmType(node->typeKey, error);
                return type != nullptr && address != nullptr ? LLVMBuildLoad2(builder_, type, address, "param.load") : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::LocalValue:
            {
                if (!node->localOrdinal.has_value()) return failValue(error, "local value missing ordinal");
                LLVMValueRef address = localStorage_[*node->localOrdinal];
                LLVMTypeRef type = llvmType(node->typeKey, error);
                return type != nullptr && address != nullptr ? LLVMBuildLoad2(builder_, type, address, "local.load") : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::FieldLoad:
            {
                LLVMValueRef address = fieldAddress(node, error);
                LLVMTypeRef type = llvmType(node->typeKey, error);
                if (address == nullptr || type == nullptr) return nullptr;
                LLVMValueRef load = LLVMBuildLoad2(builder_, type, address, "field.load");
                if (node->volatileAccess) LLVMSetVolatile(load, 1);
                return load;
            }
            case frontend::ProjectOopMirNodeKind::BitFieldLoad:
            {
                LLVMValueRef address = fieldAddress(node, error);
                LLVMTypeRef storageType = bitFieldStorageType(node, error);
                if (address == nullptr || storageType == nullptr) return nullptr;
                LLVMValueRef storage = LLVMBuildLoad2(
                    builder_, storageType, address, "bitfield.storage.load");
                LLVMSetAlignment(storage, static_cast<unsigned>(node->alignment));
                return extractBitField(node, storage, error);
            }
            case frontend::ProjectOopMirNodeKind::StaticFieldLoad:
            {
                LLVMValueRef address = staticFieldAddress(node, error);
                LLVMTypeRef type = llvmType(node->typeKey, error);
                if (address == nullptr || type == nullptr) return nullptr;
                LLVMValueRef load = LLVMBuildLoad2(builder_, type, address, "static.field.load");
                if (node->volatileAccess) LLVMSetVolatile(load, 1);
                return load;
            }
            case frontend::ProjectOopMirNodeKind::ParameterAddress:
                return node->parameterOrdinal.has_value() ? parameterStorage_[*node->parameterOrdinal] : nullptr;
            case frontend::ProjectOopMirNodeKind::LocalAddress:
                return node->localOrdinal.has_value() ? localStorage_[*node->localOrdinal] : nullptr;
            case frontend::ProjectOopMirNodeKind::FieldAddress:
                return fieldAddress(node, error);
            case frontend::ProjectOopMirNodeKind::BitFieldAddress:
                return fieldAddress(node, error);
            case frontend::ProjectOopMirNodeKind::StaticFieldAddress:
                return staticFieldAddress(node, error);

            case frontend::ProjectOopMirNodeKind::IntegerConstant:
            {
                LLVMTypeRef type = llvmType(node->typeKey, error);
                return type != nullptr ? LLVMConstInt(type, node->integerValue, isSignedInteger(node->typeKey) ? 1 : 0) : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::FloatingConstant:
            {
                LLVMTypeRef type = llvmType(node->typeKey, error);
                return type != nullptr ? LLVMConstReal(type, node->floatingValue) : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::BooleanConstant:
                return LLVMConstInt(LLVMInt1TypeInContext(context_), node->booleanValue ? 1U : 0U, 0);
            case frontend::ProjectOopMirNodeKind::NullConstant:
                return LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
            case frontend::ProjectOopMirNodeKind::DefaultValue:
            {
                // Stage 52: default(T) -- LLVMConstNull is uniformly correct for every closed T
                // here: an integer/float/bool/char primitive type gives numeric zero, a pointer
                // type (class, string, delegate) gives a null pointer, and a struct (value) type
                // gives an all-zero-fields aggregate -- exactly "the value every field of T would
                // have if never assigned", with no per-primitive special-casing needed.
                LLVMTypeRef type = llvmType(node->typeKey, error);
                return type != nullptr ? LLVMConstNull(type) : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::StringLiteral:
                return lowerStringLiteral(node, error);
            case frontend::ProjectOopMirNodeKind::ArrayAllocate:
                return lowerArrayAllocate(node, error);
            case frontend::ProjectOopMirNodeKind::ArrayLength:
                return lowerNativeLength(node, false, error);
            case frontend::ProjectOopMirNodeKind::ArrayElementAddress:
                return lowerArrayElementAddress(node, false, error);
            case frontend::ProjectOopMirNodeKind::ArrayElementLoad:
            {
                LLVMValueRef address = lowerArrayElementAddress(node, false, error);
                LLVMTypeRef type = llvmType(node->elementTypeKey, error);
                return address != nullptr && type != nullptr
                    ? LLVMBuildLoad2(builder_, type, address, "array.element.load")
                    : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::StringLength:
                return lowerNativeLength(node, true, error);
            case frontend::ProjectOopMirNodeKind::StringElementLoad:
            {
                LLVMValueRef address = lowerArrayElementAddress(node, true, error);
                return address != nullptr
                    ? LLVMBuildLoad2(builder_, LLVMInt8TypeInContext(context_), address, "string.element.load")
                    : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::EnumName:
                return lowerEnumName(node, error);
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
                return lowerArrayElementAddress(node, true, error);
            case frontend::ProjectOopMirNodeKind::StringSubstring:
                return lowerStringSubstring(node, error);
            case frontend::ProjectOopMirNodeKind::StringFromCharArray:
                return lowerStringFromCharArray(node, error);
            case frontend::ProjectOopMirNodeKind::CoreConsoleWriteLine:
            {
                if (node->children.size() != 1 || consoleWriteLineFunction_ == nullptr)
                    return failValue(error, "Console write requires one string argument and runtime hook");
                LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                if (value == nullptr) return nullptr;
                // SHR-022: `immediate` trae la mascara de variante desde el MIR. Es una CONSTANTE
                // de compilacion --el nombre del metodo la fijo-- asi que el descriptor y el salto
                // de linea se resuelven con dos `and` en el runtime y no hay ninguna rama que el
                // optimizador no pueda plegar.
                LLVMValueRef mode = LLVMConstInt(
                    LLVMInt32TypeInContext(context_), static_cast<unsigned long long>(node->immediate), 0);
                LLVMValueRef args[] = {value, mode};
                buildNoUnwindCall(consoleWriteLineFunctionType_, consoleWriteLineFunction_, args, 2, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 53.1 "Time": three sibling zero-argument, zero-branch calls. No argument
            // lowering (there are no children to lower), no result post-processing -- the raw
            // i64 the runtime hook returns IS the NativeSharp `long` value, unmodified.
            case frontend::ProjectOopMirNodeKind::CoreClockQueryCounter:
                if (clockQueryCounterFunction_ == nullptr)
                    return failValue(error, "Stage 53.1 NativeClock.QueryCounterRaw requires its runtime hook");
                return buildNoUnwindCall(clockQueryCounterFunctionType_, clockQueryCounterFunction_, nullptr, 0, "clock.counter");
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
                return buildNoUnwindCall(clockQueryFrequencyFunctionType_, clockQueryFrequencyFunction_, nullptr, 0, "clock.frequency");
            case frontend::ProjectOopMirNodeKind::CoreClockQuerySystemFileTime:
                if (clockQuerySystemFileTimeFunction_ == nullptr)
                    return failValue(error, "Stage 53.1 NativeClock.QuerySystemFileTimeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    clockQuerySystemFileTimeFunctionType_, clockQuerySystemFileTimeFunction_, nullptr, 0, "clock.filetime");

            // Stage 53.2 "Environment": TickCount64/ProcessorCount are zero-argument calls,
            // same as the NativeClock trio -- the raw i64/i32 the runtime hook returns IS the
            // NativeSharp long/int value, unmodified.
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentTickCount64:
                if (environmentTickCount64Function_ == nullptr)
                    return failValue(error, "Stage 53.2 Environment.TickCount64 requires its runtime hook");
                return buildNoUnwindCall(
                    environmentTickCount64FunctionType_, environmentTickCount64Function_, nullptr, 0, "environment.tickcount64");
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentProcessorCount:
                if (environmentProcessorCountFunction_ == nullptr)
                    return failValue(error, "Stage 53.2 Environment.ProcessorCount requires its runtime hook");
                return buildNoUnwindCall(
                    environmentProcessorCountFunctionType_, environmentProcessorCountFunction_, nullptr, 0, "environment.processorcount");
            // Exit: one argument, void return -- same shape as CoreConsoleWriteLine above, just
            // an i32 value instead of a string pointer, and there is nothing meaningful to
            // return afterward since ExitProcess never returns control (see buildEnvironmentExit
            // in WindowsRuntimeBuilder.cpp, which ends in "unreachable" rather than a ret).
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentExit:
            {
                if (node->children.size() != 1 || environmentExitFunction_ == nullptr)
                    return failValue(error, "Stage 53.2 Environment.Exit requires one int argument and runtime hook");
                LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {value};
                buildNoUnwindCall(environmentExitFunctionType_, environmentExitFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 53.2b "Environment (variables)": GetEnvironmentVariable takes one string
            // argument and returns the freshly-allocated string the runtime hook builds (same
            // "just get me the pointer values and call the hook" shape lowerStringConcat uses
            // for its own two operands -- all the length/allocation work happens on the runtime
            // side, in WindowsRuntimeBuilder.cpp's buildEnvironmentGetVariable, not here).
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetVariable:
            {
                if (node->children.size() != 1 || environmentGetVariableFunction_ == nullptr)
                    return failValue(error, "Stage 53.2b Environment.GetEnvironmentVariable requires one string argument and runtime hook");
                LLVMValueRef name = lowerExpression(node->children.front().get(), error);
                if (name == nullptr) return nullptr;
                LLVMValueRef args[] = {name};
                return buildNoUnwindCall(
                    environmentGetVariableFunctionType_, environmentGetVariableFunction_, args, 1, "environment.getvariable");
            }
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentSetVariable:
            {
                if (node->children.size() != 2 || environmentSetVariableFunction_ == nullptr)
                    return failValue(error, "Stage 53.2b Environment.SetEnvironmentVariable requires two string arguments and runtime hook");
                LLVMValueRef name = lowerExpression(node->children[0].get(), error);
                if (name == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {name, value};
                buildNoUnwindCall(environmentSetVariableFunctionType_, environmentSetVariableFunction_, args, 2, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 53.3 "Filesystem/Paths": File.Exists/Directory.Exists take one string
            // argument (a path) and return the runtime hook's i1 result directly -- no result
            // post-processing needed, the hook already returns exactly the NativeSharp
            // builtin:bool this HIR node claims as its type (see buildFileExists/
            // buildDirectoryExists in WindowsRuntimeBuilder.cpp).
            case frontend::ProjectOopMirNodeKind::CoreFileExists:
            {
                if (node->children.size() != 1 || fileExistsFunction_ == nullptr)
                    return failValue(error, "Stage 53.3 File.Exists requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(fileExistsFunctionType_, fileExistsFunction_, args, 1, "file.exists");
            }
            case frontend::ProjectOopMirNodeKind::CoreDirectoryExists:
            {
                if (node->children.size() != 1 || directoryExistsFunction_ == nullptr)
                    return failValue(error, "Stage 53.3 Directory.Exists requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(directoryExistsFunctionType_, directoryExistsFunction_, args, 1, "directory.exists");
            }
            // File.Delete/Directory.CreateDirectory: one string argument, void return -- same
            // "call the hook, discard nothing meaningful to return, hand back a dummy i1" shape
            // CoreEnvironmentSetVariable above uses, just with one argument instead of two.
            case frontend::ProjectOopMirNodeKind::CoreFileDelete:
            {
                if (node->children.size() != 1 || fileDeleteFunction_ == nullptr)
                    return failValue(error, "Stage 53.3 File.Delete requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                buildNoUnwindCall(fileDeleteFunctionType_, fileDeleteFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreDirectoryCreate:
            {
                if (node->children.size() != 1 || directoryCreateFunction_ == nullptr)
                    return failValue(error, "Stage 53.3 Directory.CreateDirectory requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                buildNoUnwindCall(directoryCreateFunctionType_, directoryCreateFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 53.3b "Filesystem/Paths (contenido de archivo)": ReadAllText takes one
            // string argument (a path) and returns the freshly-allocated string the runtime
            // hook builds -- same shape as CoreEnvironmentGetVariable above (all the size/
            // allocation work happens on the runtime side, in
            // WindowsRuntimeBuilder.cpp's buildFileReadAllText).
            case frontend::ProjectOopMirNodeKind::CoreFileReadAllText:
            {
                if (node->children.size() != 1 || fileReadAllTextFunction_ == nullptr)
                    return failValue(error, "Stage 53.3b File.ReadAllText requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(
                    fileReadAllTextFunctionType_, fileReadAllTextFunction_, args, 1, "file.readalltext");
            }
            // Bloque 4, fase A -- File.GetSize. Un operando (la ruta) y un i64 de vuelta. Toda
            // la logica --abrir, preguntar el tamano, cerrar, devolver -1 si algo falla-- vive en
            // el hook del runtime, igual que en CoreFileReadAllText.
            case frontend::ProjectOopMirNodeKind::CoreFileGetSize:
            {
                if (node->children.size() != 1 || fileGetSizeFunction_ == nullptr)
                    return failValue(error, "Bloque 4 File.GetSize requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(
                    fileGetSizeFunctionType_, fileGetSizeFunction_, args, 1, "file.getsize");
            }
            // Bloque 4, fase A -- ReadAllBytes: un operando, un puntero al arreglo recien
            // asignado de vuelta. Misma forma que CoreFileReadAllText, y el mismo hook salvo el
            // nombre: el layout de un byte[] y el de un string son el mismo.
            case frontend::ProjectOopMirNodeKind::CoreFileReadAllBytes:
            {
                if (node->children.size() != 1 || fileReadAllBytesFunction_ == nullptr)
                    return failValue(error, "Bloque 4 File.ReadAllBytes requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(
                    fileReadAllBytesFunctionType_, fileReadAllBytesFunction_, args, 1, "file.readallbytes");
            }
            // ReadBytes: la ruta y el arreglo del llamador; devuelve cuantos bytes se leyeron.
            case frontend::ProjectOopMirNodeKind::CoreFileReadBytes:
            {
                if (node->children.size() != 2 || fileReadBytesFunction_ == nullptr)
                    return failValue(error, "Bloque 4 File.ReadBytes requires a path and a byte[] argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children[0].get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef destination = lowerExpression(node->children[1].get(), error);
                if (destination == nullptr) return nullptr;
                LLVMValueRef args[] = {path, destination};
                return buildNoUnwindCall(
                    fileReadBytesFunctionType_, fileReadBytesFunction_, args, 2, "file.readbytes");
            }
            // WriteAllBytes: ruta, arreglo y cuantos bytes de el escribir. Devuelve i1.
            case frontend::ProjectOopMirNodeKind::CoreFileWriteAllBytes:
            {
                if (node->children.size() != 3 || fileWriteAllBytesFunction_ == nullptr)
                    return failValue(error, "Bloque 4 File.WriteAllBytes requires a path, a byte[] and a count, plus the runtime hook");
                LLVMValueRef path = lowerExpression(node->children[0].get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef data = lowerExpression(node->children[1].get(), error);
                if (data == nullptr) return nullptr;
                LLVMValueRef count = lowerExpression(node->children[2].get(), error);
                if (count == nullptr) return nullptr;
                LLVMValueRef args[] = {path, data, count};
                return buildNoUnwindCall(
                    fileWriteAllBytesFunctionType_, fileWriteAllBytesFunction_, args, 3, "file.writeallbytes");
            }
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
            {
                if (node->children.size() != 3 || processRunFunction_ == nullptr)
                    return failValue(error, "Bloque 4 Process.Run requires three string arguments and runtime hook");
                LLVMValueRef executable = lowerExpression(node->children[0].get(), error);
                if (executable == nullptr) return nullptr;
                LLVMValueRef commandLine = lowerExpression(node->children[1].get(), error);
                if (commandLine == nullptr) return nullptr;
                LLVMValueRef outputPath = lowerExpression(node->children[2].get(), error);
                if (outputPath == nullptr) return nullptr;
                LLVMValueRef args[] = {executable, commandLine, outputPath};
                return buildNoUnwindCall(
                    processRunFunctionType_, processRunFunction_, args, 3, "process.run");
            }
            // WriteAllText/AppendAllText: two string arguments (path, content), void return --
            // same "call the hook, dummy i1 result" shape CoreEnvironmentSetVariable uses.
            case frontend::ProjectOopMirNodeKind::CoreFileWriteAllText:
            {
                if (node->children.size() != 2 || fileWriteAllTextFunction_ == nullptr)
                    return failValue(error, "Stage 53.3b File.WriteAllText requires two string arguments and runtime hook");
                LLVMValueRef path = lowerExpression(node->children[0].get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef content = lowerExpression(node->children[1].get(), error);
                if (content == nullptr) return nullptr;
                LLVMValueRef args[] = {path, content};
                buildNoUnwindCall(fileWriteAllTextFunctionType_, fileWriteAllTextFunction_, args, 2, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreFileAppendAllText:
            {
                if (node->children.size() != 2 || fileAppendAllTextFunction_ == nullptr)
                    return failValue(error, "Stage 53.3b File.AppendAllText requires two string arguments and runtime hook");
                LLVMValueRef path = lowerExpression(node->children[0].get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef content = lowerExpression(node->children[1].get(), error);
                if (content == nullptr) return nullptr;
                LLVMValueRef args[] = {path, content};
                buildNoUnwindCall(fileAppendAllTextFunctionType_, fileAppendAllTextFunction_, args, 2, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 53.4 "Filesystem/Paths (cierre)": Directory.Delete takes one string argument
            // (a path), void return -- same "call the hook, dummy i1 result" shape CoreFileDelete/
            // CoreDirectoryCreate above use.
            case frontend::ProjectOopMirNodeKind::CoreDirectoryDelete:
            {
                if (node->children.size() != 1 || directoryDeleteFunction_ == nullptr)
                    return failValue(error, "Stage 53.4 Directory.Delete requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                buildNoUnwindCall(directoryDeleteFunctionType_, directoryDeleteFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            // Path.GetFullPath takes one string argument (a path) and returns the freshly-
            // allocated string the runtime hook builds -- same shape as CoreFileReadAllText/
            // CoreEnvironmentGetVariable above (all the size/allocation work happens on the
            // runtime side, in WindowsRuntimeBuilder.cpp's buildPathGetFullPath).
            case frontend::ProjectOopMirNodeKind::CorePathGetFullPath:
            {
                if (node->children.size() != 1 || pathGetFullPathFunction_ == nullptr)
                    return failValue(error, "Stage 53.4 Path.GetFullPath requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(
                    pathGetFullPathFunctionType_, pathGetFullPathFunction_, args, 1, "path.getfullpath");
            }

            // Stage 53.5 "Command-line arguments": GetCommandLineArgCount takes zero arguments
            // and returns the i32 the runtime hook computes directly -- unlike every void-
            // returning core operation above, there is no "dummy i1" here because the real
            // return type (builtin:int) already carries meaningful information the caller needs
            // (how many elements to allocate before calling GetCommandLineArgs).
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount:
            {
                if (node->children.size() != 0 || environmentGetCommandLineArgCountFunction_ == nullptr)
                    return failValue(error, "Stage 53.5 Environment.GetCommandLineArgCount takes no arguments and requires runtime hook");
                return buildNoUnwindCall(
                    environmentGetCommandLineArgCountFunctionType_, environmentGetCommandLineArgCountFunction_,
                    nullptr, 0, "environment.getcommandlineargcount");
            }
            // GetCommandLineArgs takes one operand -- a builtin:string[] expression, the
            // caller-provided buffer (see Environment.ns's "hueco de owned string[]" note) --
            // and returns the i32 the runtime hook computes (how many elements were actually
            // written). lowerExpression on an array-typed child already yields the array's
            // pointer, exactly as every other array-consuming MIR node in this lowerer expects,
            // so this is a direct pass-through, no special handling needed for the operand
            // itself being an array rather than a string.
            case frontend::ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs:
            {
                if (node->children.size() != 1 || environmentGetCommandLineArgsFunction_ == nullptr)
                    return failValue(error, "Stage 53.5 Environment.GetCommandLineArgs requires one string[] argument and runtime hook");
                LLVMValueRef destination = lowerExpression(node->children.front().get(), error);
                if (destination == nullptr) return nullptr;
                LLVMValueRef args[] = {destination};
                return buildNoUnwindCall(
                    environmentGetCommandLineArgsFunctionType_, environmentGetCommandLineArgsFunction_,
                    args, 1, "environment.getcommandlineargs");
            }

            // Stage 53.6 "Enumeracion de directorios": GetFileCount/GetDirectoryCount take one
            // string argument (the path) and return the i32 the runtime hook computes directly --
            // same "real int result, no dummy i1" shape as CoreEnvironmentGetCommandLineArgCount
            // above, just with a real argument this time.
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetFileCount:
            {
                if (node->children.size() != 1 || directoryGetFileCountFunction_ == nullptr)
                    return failValue(error, "Stage 53.6 Directory.GetFileCount requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(
                    directoryGetFileCountFunctionType_, directoryGetFileCountFunction_,
                    args, 1, "directory.getfilecount");
            }
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount:
            {
                if (node->children.size() != 1 || directoryGetDirectoryCountFunction_ == nullptr)
                    return failValue(error, "Stage 53.6 Directory.GetDirectoryCount requires one string argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children.front().get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef args[] = {path};
                return buildNoUnwindCall(
                    directoryGetDirectoryCountFunctionType_, directoryGetDirectoryCountFunction_,
                    args, 1, "directory.getdirectorycount");
            }
            // GetFiles/GetDirectories take two operands -- the path, then the caller-provided
            // builtin:string[] buffer (same "buffer provided by caller" pattern
            // CoreEnvironmentGetCommandLineArgs established) -- and return the i32 the runtime
            // hook computes (how many elements were actually written).
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetFiles:
            {
                if (node->children.size() != 2 || directoryGetFilesFunction_ == nullptr)
                    return failValue(error, "Stage 53.6 Directory.GetFiles requires a path and a string[] argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children[0].get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef destination = lowerExpression(node->children[1].get(), error);
                if (destination == nullptr) return nullptr;
                LLVMValueRef args[] = {path, destination};
                return buildNoUnwindCall(
                    directoryGetFilesFunctionType_, directoryGetFilesFunction_,
                    args, 2, "directory.getfiles");
            }
            case frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectories:
            {
                if (node->children.size() != 2 || directoryGetDirectoriesFunction_ == nullptr)
                    return failValue(error, "Stage 53.6 Directory.GetDirectories requires a path and a string[] argument and runtime hook");
                LLVMValueRef path = lowerExpression(node->children[0].get(), error);
                if (path == nullptr) return nullptr;
                LLVMValueRef destination = lowerExpression(node->children[1].get(), error);
                if (destination == nullptr) return nullptr;
                LLVMValueRef args[] = {path, destination};
                return buildNoUnwindCall(
                    directoryGetDirectoriesFunctionType_, directoryGetDirectoriesFunction_,
                    args, 2, "directory.getdirectories");
            }

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
            {
                if (node->children.size() != 1 || threadCreateFunction_ == nullptr)
                    return failValue(error, "Stage 54.1 Thread.ThreadCreate requires one delegate argument and runtime hook");
                LLVMValueRef delegateCell = lowerExpression(node->children.front().get(), error);
                if (delegateCell == nullptr) return nullptr;
                LLVMValueRef args[] = {delegateCell};
                return buildNoUnwindCall(
                    threadCreateFunctionType_, threadCreateFunction_, args, 1, "thread.create");
            }
            // ThreadJoin/ThreadDetach take one operand -- a builtin:nint (the HANDLE the caller
            // already holds, saved by Thread's constructor from ThreadCreate's result) -- and
            // return void at the NativeSharp level. Same "runtime function genuinely returns
            // void, this case block synthesizes its own harmless i1 dummy value after the call"
            // pattern CoreDirectoryDelete/CoreFileDelete above already use.
            case frontend::ProjectOopMirNodeKind::CoreThreadJoin:
            {
                if (node->children.size() != 1 || threadJoinFunction_ == nullptr)
                    return failValue(error, "Stage 54.1 Thread.ThreadJoin requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(threadJoinFunctionType_, threadJoinFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreThreadDetach:
            {
                if (node->children.size() != 1 || threadDetachFunction_ == nullptr)
                    return failValue(error, "Stage 54.1 Thread.ThreadDetach requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(threadDetachFunctionType_, threadDetachFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

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

            case frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt32:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 1)
                        return failValue(error, "ordered atomic Load requires one pointer argument");
                    LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                    if (location == nullptr) return nullptr;
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicLoadOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic Load carries an invalid MemoryOrder");
                    LLVMValueRef load = LLVMBuildLoad2(
                        builder_, LLVMInt32TypeInContext(context_), location, "atomic.load.ordered.i32");
                    LLVMSetOrdering(load, *ordering);
                    LLVMSetAlignment(load, 4U);
                    return load;
                }
                if (node->children.size() != 1 || atomicLoadInt32Function_ == nullptr)
                    return failValue(error, "atomic Load(int*) requires one pointer argument and runtime hook");
                LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef args[] = {location};
                return buildNoUnwindCall(
                    atomicLoadInt32FunctionType_, atomicLoadInt32Function_, args, 1, "atomic.load.i32");
            }
            // Entrega 228: Float32x4 conserva su carrier agregado publico, pero la operacion
            // interna se realiza sobre <4 x float>. Con punteros opacos basta un alloca del
            // carrier para reinterpretar sin cambiar firmas, layout, PDB ni metadata.
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Add:
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Subtract:
            case frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Multiply:
            {
                if (node->children.size() != 2)
                    return failValue(error, "Float32x4 SIMD operation requires two operands");
                LLVMValueRef left = lowerExpression(node->children[0].get(), error);
                if (left == nullptr) return nullptr;
                LLVMValueRef right = lowerExpression(node->children[1].get(), error);
                if (right == nullptr) return nullptr;
                LLVMTypeRef carrier = llvmType(node->typeKey, error);
                if (carrier == nullptr) return nullptr;
                LLVMTypeRef vectorType = LLVMVectorType(LLVMFloatTypeInContext(context_), 4U);
                LLVMValueRef leftSlot = LLVMBuildAlloca(builder_, carrier, "simd.left.addr");
                LLVMValueRef rightSlot = LLVMBuildAlloca(builder_, carrier, "simd.right.addr");
                LLVMBuildStore(builder_, left, leftSlot);
                LLVMBuildStore(builder_, right, rightSlot);
                LLVMValueRef leftVector = LLVMBuildLoad2(builder_, vectorType, leftSlot, "simd.left");
                LLVMValueRef rightVector = LLVMBuildLoad2(builder_, vectorType, rightSlot, "simd.right");
                LLVMValueRef vectorResult = nullptr;
                if (node->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Add)
                    vectorResult = LLVMBuildFAdd(builder_, leftVector, rightVector, "simd.add");
                else if (node->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Subtract)
                    vectorResult = LLVMBuildFSub(builder_, leftVector, rightVector, "simd.subtract");
                else
                    vectorResult = LLVMBuildFMul(builder_, leftVector, rightVector, "simd.multiply");
                LLVMValueRef resultSlot = LLVMBuildAlloca(builder_, carrier, "simd.result.addr");
                LLVMBuildStore(builder_, vectorResult, resultSlot);
                return LLVMBuildLoad2(builder_, carrier, resultSlot, "simd.result");
            }
            case frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt32:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 2)
                        return failValue(error, "ordered atomic Store requires pointer and value arguments");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicStoreOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic Store carries an invalid MemoryOrder");
                    LLVMValueRef store = LLVMBuildStore(builder_, value, location);
                    LLVMSetOrdering(store, *ordering);
                    LLVMSetAlignment(store, 4U);
                    return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
                }
                if (node->children.size() != 2 || atomicStoreInt32Function_ == nullptr)
                    return failValue(error, "atomic Store(int*, int) requires two arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value};
                buildNoUnwindCall(atomicStoreInt32FunctionType_, atomicStoreInt32Function_, args, 2, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt64:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 1)
                        return failValue(error, "ordered atomic Load requires one pointer argument");
                    LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                    if (location == nullptr) return nullptr;
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicLoadOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic Load carries an invalid MemoryOrder");
                    LLVMValueRef load = LLVMBuildLoad2(
                        builder_, LLVMInt64TypeInContext(context_), location, "atomic.load.ordered.i64");
                    LLVMSetOrdering(load, *ordering);
                    LLVMSetAlignment(load, 8U);
                    return load;
                }
                if (node->children.size() != 1 || atomicLoadInt64Function_ == nullptr)
                    return failValue(error, "atomic Load(long*) requires one pointer argument and runtime hook");
                LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef args[] = {location};
                return buildNoUnwindCall(
                    atomicLoadInt64FunctionType_, atomicLoadInt64Function_, args, 1, "atomic.load.i64");
            }
            case frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt64:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 2)
                        return failValue(error, "ordered atomic Store requires pointer and value arguments");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicStoreOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic Store carries an invalid MemoryOrder");
                    LLVMValueRef store = LLVMBuildStore(builder_, value, location);
                    LLVMSetOrdering(store, *ordering);
                    LLVMSetAlignment(store, 8U);
                    return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
                }
                if (node->children.size() != 2 || atomicStoreInt64Function_ == nullptr)
                    return failValue(error, "atomic Store(long*, long) requires two arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value};
                buildNoUnwindCall(atomicStoreInt64FunctionType_, atomicStoreInt64Function_, args, 2, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt32:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 1)
                        return failValue(error, "ordered atomic Increment requires one pointer argument");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMTypeRef type = LLVMInt32TypeInContext(context_);
                    LLVMValueRef delta = LLVMConstInt(type, 1U, 1);
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpAdd, location, delta, *ordering, 4U,
                        true, "atomic.increment.ordered.i32");
                }
                if (node->children.size() != 1 || interlockedIncrementInt32Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Increment(int*) requires one pointer argument and runtime hook");
                LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef args[] = {location};
                return buildNoUnwindCall(
                    interlockedIncrementInt32FunctionType_, interlockedIncrementInt32Function_, args, 1, "interlocked.increment.i32");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt32:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 1)
                        return failValue(error, "ordered atomic Decrement requires one pointer argument");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMTypeRef type = LLVMInt32TypeInContext(context_);
                    LLVMValueRef delta = LLVMConstInt(type, ~0ULL, 1);
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpAdd, location, delta, *ordering, 4U,
                        true, "atomic.decrement.ordered.i32");
                }
                if (node->children.size() != 1 || interlockedDecrementInt32Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Decrement(int*) requires one pointer argument and runtime hook");
                LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef args[] = {location};
                return buildNoUnwindCall(
                    interlockedDecrementInt32FunctionType_, interlockedDecrementInt32Function_, args, 1, "interlocked.decrement.i32");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt64:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 1)
                        return failValue(error, "ordered atomic Increment requires one pointer argument");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMTypeRef type = LLVMInt64TypeInContext(context_);
                    LLVMValueRef delta = LLVMConstInt(type, 1U, 1);
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpAdd, location, delta, *ordering, 8U,
                        true, "atomic.increment.ordered.i64");
                }
                if (node->children.size() != 1 || interlockedIncrementInt64Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Increment(long*) requires one pointer argument and runtime hook");
                LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef args[] = {location};
                return buildNoUnwindCall(
                    interlockedIncrementInt64FunctionType_, interlockedIncrementInt64Function_, args, 1, "interlocked.increment.i64");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt64:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 1)
                        return failValue(error, "ordered atomic Decrement requires one pointer argument");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMTypeRef type = LLVMInt64TypeInContext(context_);
                    LLVMValueRef delta = LLVMConstInt(type, ~0ULL, 1);
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpAdd, location, delta, *ordering, 8U,
                        true, "atomic.decrement.ordered.i64");
                }
                if (node->children.size() != 1 || interlockedDecrementInt64Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Decrement(long*) requires one pointer argument and runtime hook");
                LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef args[] = {location};
                return buildNoUnwindCall(
                    interlockedDecrementInt64FunctionType_, interlockedDecrementInt64Function_, args, 1, "interlocked.decrement.i64");
            }
            // Add/Exchange take two operands -- location, then the value -- and return a real
            // i32/i64 result (the new value for Add, the old value for Exchange -- the difference
            // is handled entirely on the runtime side, see WindowsRuntimeBuilder.cpp; this lowerer
            // only forwards arguments and returns whatever the runtime function computed).
            case frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt32:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 2)
                        return failValue(error, "ordered atomic Add requires pointer and value arguments");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpAdd, location, value, *ordering, 4U,
                        true, "atomic.add.ordered.i32");
                }
                if (node->children.size() != 2 || interlockedAddInt32Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Add(int*, int) requires two arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value};
                return buildNoUnwindCall(
                    interlockedAddInt32FunctionType_, interlockedAddInt32Function_, args, 2, "interlocked.add.i32");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt32:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 2)
                        return failValue(error, "ordered atomic Exchange requires pointer and value arguments");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpXchg, location, value, *ordering, 4U,
                        false, "atomic.exchange.ordered.i32");
                }
                if (node->children.size() != 2 || interlockedExchangeInt32Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Exchange(int*, int) requires two arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value};
                return buildNoUnwindCall(
                    interlockedExchangeInt32FunctionType_, interlockedExchangeInt32Function_, args, 2, "interlocked.exchange.i32");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt64:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 2)
                        return failValue(error, "ordered atomic Add requires pointer and value arguments");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpAdd, location, value, *ordering, 8U,
                        true, "atomic.add.ordered.i64");
                }
                if (node->children.size() != 2 || interlockedAddInt64Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Add(long*, long) requires two arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value};
                return buildNoUnwindCall(
                    interlockedAddInt64FunctionType_, interlockedAddInt64Function_, args, 2, "interlocked.add.i64");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt64:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 2)
                        return failValue(error, "ordered atomic Exchange requires pointer and value arguments");
                    const std::optional<LLVMAtomicOrdering> ordering =
                        orderedAtomicRmwOrdering(node->integerValue);
                    if (!ordering.has_value())
                        return failValue(error, "ordered atomic RMW carries an invalid MemoryOrder");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    return buildOrderedAtomicRmw(
                        builder_, LLVMAtomicRMWBinOpXchg, location, value, *ordering, 8U,
                        false, "atomic.exchange.ordered.i64");
                }
                if (node->children.size() != 2 || interlockedExchangeInt64Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.Exchange(long*, long) requires two arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value};
                return buildNoUnwindCall(
                    interlockedExchangeInt64FunctionType_, interlockedExchangeInt64Function_, args, 2, "interlocked.exchange.i64");
            }
            // CompareExchange takes three operands -- location, value, comparand -- same order as
            // Win32's InterlockedCompareExchange(Destination, Exchange, Comparand) and C#'s
            // Interlocked.CompareExchange(location, value, comparand). Returns the OLD value
            // regardless of whether the comparison matched, forwarded directly from Win32.
            case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 3)
                        return failValue(error, "ordered atomic CompareExchange requires pointer, value, and comparand arguments");
                    const std::optional<LLVMAtomicOrdering> successOrder =
                        orderedAtomicRmwOrdering(node->integerValue & 0xffU);
                    const std::optional<LLVMAtomicOrdering> failureOrder =
                        orderedAtomicLoadOrdering((node->integerValue >> 8U) & 0xffU);
                    if (!successOrder.has_value() || !failureOrder.has_value())
                        return failValue(error, "ordered atomic CompareExchange carries invalid MemoryOrder metadata");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    LLVMValueRef comparand = lowerExpression(node->children[2].get(), error);
                    if (comparand == nullptr) return nullptr;
                    return buildOrderedAtomicCompareExchange(
                        builder_, location, value, comparand, *successOrder, *failureOrder, 4U,
                        "atomic.compare_exchange.ordered.i32");
                }
                if (node->children.size() != 3 || interlockedCompareExchangeInt32Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.CompareExchange(int*, int, int) requires three arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef comparand = lowerExpression(node->children[2].get(), error);
                if (comparand == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value, comparand};
                return buildNoUnwindCall(
                    interlockedCompareExchangeInt32FunctionType_, interlockedCompareExchangeInt32Function_, args, 3, "interlocked.cmpxchg.i32");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64:
            {
                if (node->integerValue != 0U)
                {
                    if (node->children.size() != 3)
                        return failValue(error, "ordered atomic CompareExchange requires pointer, value, and comparand arguments");
                    const std::optional<LLVMAtomicOrdering> successOrder =
                        orderedAtomicRmwOrdering(node->integerValue & 0xffU);
                    const std::optional<LLVMAtomicOrdering> failureOrder =
                        orderedAtomicLoadOrdering((node->integerValue >> 8U) & 0xffU);
                    if (!successOrder.has_value() || !failureOrder.has_value())
                        return failValue(error, "ordered atomic CompareExchange carries invalid MemoryOrder metadata");
                    LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                    if (location == nullptr) return nullptr;
                    LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                    if (value == nullptr) return nullptr;
                    LLVMValueRef comparand = lowerExpression(node->children[2].get(), error);
                    if (comparand == nullptr) return nullptr;
                    return buildOrderedAtomicCompareExchange(
                        builder_, location, value, comparand, *successOrder, *failureOrder, 8U,
                        "atomic.compare_exchange.ordered.i64");
                }
                if (node->children.size() != 3 || interlockedCompareExchangeInt64Function_ == nullptr)
                    return failValue(error, "Stage 54.2 Interlocked.CompareExchange(long*, long, long) requires three arguments and runtime hook");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef comparand = lowerExpression(node->children[2].get(), error);
                if (comparand == nullptr) return nullptr;
                LLVMValueRef args[] = {location, value, comparand};
                return buildNoUnwindCall(
                    interlockedCompareExchangeInt64FunctionType_, interlockedCompareExchangeInt64Function_, args, 3, "interlocked.cmpxchg.i64");
            }

            case frontend::ProjectOopMirNodeKind::CoreAtomicLoadPointer:
            {
                if (node->children.size() != 1)
                    return failValue(error, "AtomicPointer.Load requires one pointer-location argument");
                LLVMValueRef location = lowerExpression(node->children.front().get(), error);
                if (location == nullptr) return nullptr;
                const LLVMAtomicOrdering ordering = node->integerValue == 0U
                    ? LLVMAtomicOrderingSequentiallyConsistent
                    : orderedAtomicLoadOrdering(node->integerValue).value_or(LLVMAtomicOrderingNotAtomic);
                if (ordering == LLVMAtomicOrderingNotAtomic)
                    return failValue(error, "AtomicPointer.Load carries an invalid MemoryOrder");
                LLVMTypeRef pointerType = llvmType("builtin:void*", error);
                if (pointerType == nullptr) return nullptr;
                LLVMValueRef load = LLVMBuildLoad2(builder_, pointerType, location, "atomic.load.pointer");
                LLVMSetOrdering(load, ordering);
                LLVMSetAlignment(load, 8U);
                return load;
            }
            case frontend::ProjectOopMirNodeKind::CoreAtomicStorePointer:
            {
                if (node->children.size() != 2)
                    return failValue(error, "AtomicPointer.Store requires pointer-location and value arguments");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                const LLVMAtomicOrdering ordering = node->integerValue == 0U
                    ? LLVMAtomicOrderingSequentiallyConsistent
                    : orderedAtomicStoreOrdering(node->integerValue).value_or(LLVMAtomicOrderingNotAtomic);
                if (ordering == LLVMAtomicOrderingNotAtomic)
                    return failValue(error, "AtomicPointer.Store carries an invalid MemoryOrder");
                LLVMValueRef store = LLVMBuildStore(builder_, value, location);
                LLVMSetOrdering(store, ordering);
                LLVMSetAlignment(store, 8U);
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedExchangePointer:
            {
                if (node->children.size() != 2)
                    return failValue(error, "AtomicPointer.Exchange requires pointer-location and value arguments");
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                const LLVMAtomicOrdering ordering = node->integerValue == 0U
                    ? LLVMAtomicOrderingSequentiallyConsistent
                    : orderedAtomicRmwOrdering(node->integerValue).value_or(LLVMAtomicOrderingNotAtomic);
                if (ordering == LLVMAtomicOrderingNotAtomic)
                    return failValue(error, "AtomicPointer.Exchange carries an invalid MemoryOrder");
                return buildOrderedAtomicRmw(
                    builder_, LLVMAtomicRMWBinOpXchg, location, value, ordering, 8U,
                    false, "atomic.exchange.pointer");
            }
            case frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer:
            {
                if (node->children.size() != 3)
                    return failValue(
                        error, "AtomicPointer.CompareExchange requires pointer-location, value, and comparand arguments");
                LLVMAtomicOrdering successOrder = LLVMAtomicOrderingSequentiallyConsistent;
                LLVMAtomicOrdering failureOrder = LLVMAtomicOrderingSequentiallyConsistent;
                if (node->integerValue != 0U)
                {
                    const std::optional<LLVMAtomicOrdering> orderedSuccess =
                        orderedAtomicRmwOrdering(node->integerValue & 0xffU);
                    const std::optional<LLVMAtomicOrdering> orderedFailure =
                        orderedAtomicLoadOrdering((node->integerValue >> 8U) & 0xffU);
                    if (!orderedSuccess.has_value() || !orderedFailure.has_value())
                        return failValue(
                            error, "AtomicPointer.CompareExchange carries invalid MemoryOrder metadata");
                    successOrder = *orderedSuccess;
                    failureOrder = *orderedFailure;
                }
                LLVMValueRef location = lowerExpression(node->children[0].get(), error);
                if (location == nullptr) return nullptr;
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef comparand = lowerExpression(node->children[2].get(), error);
                if (comparand == nullptr) return nullptr;
                return buildOrderedAtomicCompareExchange(
                    builder_, location, value, comparand, successOrder, failureOrder, 8U,
                    "atomic.compare_exchange.pointer");
            }

            // Stage 54.3 "Mutex". MutexCreate takes ZERO operands (the first zero-operand core
            // operation in the project) and returns the real, meaningful i64 HANDLE (as nint) --
            // no dummy-i1 wrapping, same discipline as CoreThreadCreate above.
            case frontend::ProjectOopMirNodeKind::CoreMutexCreate:
            {
                if (!node->children.empty() || mutexCreateFunction_ == nullptr)
                    return failValue(error, "Stage 54.3 Mutex.MutexCreate takes no arguments and requires its runtime hook");
                return buildNoUnwindCall(
                    mutexCreateFunctionType_, mutexCreateFunction_, nullptr, 0, "mutex.create");
            }
            // MutexLock/MutexUnlock/MutexClose take one operand -- a builtin:nint (the HANDLE the
            // caller already holds, saved by Mutex's constructor from MutexCreate's result) --
            // and return void at the NativeSharp level. Same "runtime function genuinely returns
            // void, this case block synthesizes its own harmless i1 dummy value after the call"
            // pattern CoreThreadJoin/CoreThreadDetach above already use.
            case frontend::ProjectOopMirNodeKind::CoreMutexLock:
            {
                if (node->children.size() != 1 || mutexLockFunction_ == nullptr)
                    return failValue(error, "Stage 54.3 Mutex.MutexLock requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(mutexLockFunctionType_, mutexLockFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreMutexUnlock:
            {
                if (node->children.size() != 1 || mutexUnlockFunction_ == nullptr)
                    return failValue(error, "Stage 54.3 Mutex.MutexUnlock requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(mutexUnlockFunctionType_, mutexUnlockFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreMutexClose:
            {
                if (node->children.size() != 1 || mutexCloseFunction_ == nullptr)
                    return failValue(error, "Stage 54.3 Mutex.MutexClose requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(mutexCloseFunctionType_, mutexCloseFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll: one FieldAddress child
            // (built in ProjectOopHirModel.cpp, resolved to a real pointer by `fieldAddress()`
            // already, INCLUDING its own null-receiver trap) -- exact same "lower the one child,
            // forward the pointer, dummy-i1 return" shape as CoreMutexLock/Unlock/Close above.
            case frontend::ProjectOopMirNodeKind::CoreMonitorEnter:
            {
                if (node->children.size() != 1 || monitorEnterFunction_ == nullptr)
                    return failValue(error, "Stage 54.4 Monitor.Enter requires one pointer argument and runtime hook");
                LLVMValueRef lockPointer = lowerExpression(node->children.front().get(), error);
                if (lockPointer == nullptr) return nullptr;
                LLVMValueRef args[] = {lockPointer};
                buildNoUnwindCall(monitorEnterFunctionType_, monitorEnterFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreMonitorExit:
            {
                if (node->children.size() != 1 || monitorExitFunction_ == nullptr)
                    return failValue(error, "Stage 54.4 Monitor.Exit requires one pointer argument and runtime hook");
                LLVMValueRef lockPointer = lowerExpression(node->children.front().get(), error);
                if (lockPointer == nullptr) return nullptr;
                LLVMValueRef args[] = {lockPointer};
                buildNoUnwindCall(monitorExitFunctionType_, monitorExitFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreMonitorPulse:
            {
                if (node->children.size() != 1 || monitorPulseFunction_ == nullptr)
                    return failValue(error, "Stage 54.4 Monitor.Pulse requires one pointer argument and runtime hook");
                LLVMValueRef cvPointer = lowerExpression(node->children.front().get(), error);
                if (cvPointer == nullptr) return nullptr;
                LLVMValueRef args[] = {cvPointer};
                buildNoUnwindCall(monitorPulseFunctionType_, monitorPulseFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreMonitorPulseAll:
            {
                if (node->children.size() != 1 || monitorPulseAllFunction_ == nullptr)
                    return failValue(error, "Stage 54.4 Monitor.PulseAll requires one pointer argument and runtime hook");
                LLVMValueRef cvPointer = lowerExpression(node->children.front().get(), error);
                if (cvPointer == nullptr) return nullptr;
                LLVMValueRef args[] = {cvPointer};
                buildNoUnwindCall(monitorPulseAllFunctionType_, monitorPulseAllFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            // CoreMonitorWait: the ONE case in this round that lowers its receiver child exactly
            // ONCE and then GEPs it TWICE (using `immediate`/`secondaryImmediate`, the cv_/lock_
            // byte offsets ProjectOopMirModel.cpp already resolved) -- see this node kind's own
            // header comment in ProjectOopHirModel.h/MethodCoreOperationKind::MonitorWait's
            // comment in ProjectMethodBodyModel.h for the full "why not two FieldAddress children"
            // rationale. checkedReceiver runs ONCE, shared by both GEPs -- Monitor's receiver is
            // never re-validated twice for the same call, unlike two independent FieldAddress
            // nodes would each do.
            case frontend::ProjectOopMirNodeKind::CoreMonitorWait:
            {
                if (node->children.size() != 2 || monitorWaitFunction_ == nullptr)
                    return failValue(error, "Stage 54.4 Monitor.Wait requires a receiver + timeout argument and runtime hook");
                LLVMValueRef receiver = lowerExpression(node->children[0].get(), error);
                if (receiver == nullptr) return nullptr;
                receiver = checkedReceiver(receiver, error);
                if (receiver == nullptr) return nullptr;
                LLVMValueRef cvOffset = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
                LLVMValueRef cvIndices[] = {cvOffset};
                LLVMValueRef cvPointer = LLVMBuildGEP2(
                    builder_, LLVMInt8TypeInContext(context_), receiver, cvIndices, 1, "monitor.cv.addr");
                LLVMValueRef lockOffset = LLVMConstInt(LLVMInt64TypeInContext(context_), node->secondaryImmediate, 0);
                LLVMValueRef lockIndices[] = {lockOffset};
                LLVMValueRef lockPointer = LLVMBuildGEP2(
                    builder_, LLVMInt8TypeInContext(context_), receiver, lockIndices, 1, "monitor.lock.addr");
                LLVMValueRef timeout = lowerExpression(node->children[1].get(), error);
                if (timeout == nullptr) return nullptr;
                LLVMValueRef args[] = {cvPointer, lockPointer, timeout};
                return buildNoUnwindCall(monitorWaitFunctionType_, monitorWaitFunction_, args, 3, "monitor.wait");
            }

            // Stage 54.5 "Otras primitivas de sincronizacion". SemaphoreCreate: two explicit i32
            // operands (initialCount, maximumCount), a REAL i64 HANDLE result -- no dummy-i1
            // wrapping, same discipline as CoreMutexCreate/CoreThreadCreate.
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreCreate:
            {
                if (node->children.size() != 2 || semaphoreCreateFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Semaphore.SemaphoreCreate requires two int arguments and runtime hook");
                LLVMValueRef initialCount = lowerExpression(node->children[0].get(), error);
                if (initialCount == nullptr) return nullptr;
                LLVMValueRef maximumCount = lowerExpression(node->children[1].get(), error);
                if (maximumCount == nullptr) return nullptr;
                LLVMValueRef args[] = {initialCount, maximumCount};
                return buildNoUnwindCall(
                    semaphoreCreateFunctionType_, semaphoreCreateFunction_, args, 2, "semaphore.create");
            }
            // SemaphoreWait/EventWait BOTH lower to the SAME shared runtime hook
            // (waitForHandleTimeoutFunction_) -- see buildWaitForHandleTimeout's header comment in
            // WindowsRuntimeBuilder.cpp for why sharing one wrapper across two
            // MethodCoreOperationKind values is safe here (the underlying Win32 operation is
            // byte-for-byte identical: WaitForSingleObject(handle, ms) collapsed to a bool).
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreWait:
            case frontend::ProjectOopMirNodeKind::CoreEventWait:
            {
                if (node->children.size() != 2 || waitForHandleTimeoutFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Semaphore/Event Wait requires a handle + timeout argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children[0].get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef timeout = lowerExpression(node->children[1].get(), error);
                if (timeout == nullptr) return nullptr;
                LLVMValueRef args[] = {handle, timeout};
                return buildNoUnwindCall(
                    waitForHandleTimeoutFunctionType_, waitForHandleTimeoutFunction_, args, 2, "wait.timeout");
            }
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreRelease:
            {
                if (node->children.size() != 1 || semaphoreReleaseFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Semaphore.SemaphoreRelease requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(semaphoreReleaseFunctionType_, semaphoreReleaseFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreSemaphoreClose:
            {
                if (node->children.size() != 1 || semaphoreCloseFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Semaphore.SemaphoreClose requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(semaphoreCloseFunctionType_, semaphoreCloseFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            // EventCreate: two explicit i1 operands (manualReset, initialState), a REAL i64
            // HANDLE result.
            case frontend::ProjectOopMirNodeKind::CoreEventCreate:
            {
                if (node->children.size() != 2 || eventCreateFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Event.EventCreate requires two bool arguments and runtime hook");
                LLVMValueRef manualReset = lowerExpression(node->children[0].get(), error);
                if (manualReset == nullptr) return nullptr;
                LLVMValueRef initialState = lowerExpression(node->children[1].get(), error);
                if (initialState == nullptr) return nullptr;
                LLVMValueRef args[] = {manualReset, initialState};
                return buildNoUnwindCall(eventCreateFunctionType_, eventCreateFunction_, args, 2, "event.create");
            }
            case frontend::ProjectOopMirNodeKind::CoreEventSet:
            {
                if (node->children.size() != 1 || eventSetFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Event.EventSet requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(eventSetFunctionType_, eventSetFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreEventReset:
            {
                if (node->children.size() != 1 || eventResetFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Event.EventReset requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(eventResetFunctionType_, eventResetFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreEventClose:
            {
                if (node->children.size() != 1 || eventCloseFunction_ == nullptr)
                    return failValue(error, "Stage 54.5 Event.EventClose requires one nint argument and runtime hook");
                LLVMValueRef handle = lowerExpression(node->children.front().get(), error);
                if (handle == nullptr) return nullptr;
                LLVMValueRef args[] = {handle};
                buildNoUnwindCall(eventCloseFunctionType_, eventCloseFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 55.1 "Process". Every getter is the simplest possible core-operation shape
            // (zero arguments, one call, return its result) -- same as CoreClockQueryCounter.
            case frontend::ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId:
                if (processGetCurrentProcessIdFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.GetCurrentProcessIdRaw requires its runtime hook");
                return buildNoUnwindCall(
                    processGetCurrentProcessIdFunctionType_, processGetCurrentProcessIdFunction_, nullptr, 0, "process.currentprocessid");
            case frontend::ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize:
                if (processGetWorkingSetSizeFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.GetWorkingSetSizeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    processGetWorkingSetSizeFunctionType_, processGetWorkingSetSizeFunction_, nullptr, 0, "process.workingsetsize");
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
                    processGetPriorityClassFunctionType_, processGetPriorityClassFunction_, nullptr, 0, "process.priorityclass");
            case frontend::ProjectOopMirNodeKind::CoreProcessSetPriorityClass:
            {
                if (node->children.size() != 1 || processSetPriorityClassFunction_ == nullptr)
                    return failValue(error, "Stage 55.1 Process.SetPriorityClassRaw requires one int argument and runtime hook");
                LLVMValueRef priorityClass = lowerExpression(node->children.front().get(), error);
                if (priorityClass == nullptr) return nullptr;
                LLVMValueRef args[] = {priorityClass};
                buildNoUnwindCall(processSetPriorityClassFunctionType_, processSetPriorityClassFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Stage 55.2 "Hardware / System Info". Same zero-argument shape as Process above.
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetPageSize:
                if (systemInfoGetPageSizeFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetPageSizeRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetPageSizeFunctionType_, systemInfoGetPageSizeFunction_, nullptr, 0, "systeminfo.pagesize");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture:
                if (systemInfoGetProcessorArchitectureFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetProcessorArchitectureRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetProcessorArchitectureFunctionType_, systemInfoGetProcessorArchitectureFunction_, nullptr, 0, "systeminfo.processorarchitecture");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity:
                if (systemInfoGetAllocationGranularityFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetAllocationGranularityRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetAllocationGranularityFunctionType_, systemInfoGetAllocationGranularityFunction_, nullptr, 0, "systeminfo.allocationgranularity");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes:
                if (systemInfoGetTotalPhysicalMemoryBytesFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetTotalPhysicalMemoryBytesRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetTotalPhysicalMemoryBytesFunctionType_, systemInfoGetTotalPhysicalMemoryBytesFunction_, nullptr, 0, "systeminfo.totalphysicalmemorybytes");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes:
                if (systemInfoGetAvailablePhysicalMemoryBytesFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetAvailablePhysicalMemoryBytesRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetAvailablePhysicalMemoryBytesFunctionType_, systemInfoGetAvailablePhysicalMemoryBytesFunction_, nullptr, 0, "systeminfo.availablephysicalmemorybytes");
            case frontend::ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent:
                if (systemInfoGetMemoryLoadPercentFunction_ == nullptr)
                    return failValue(error, "Stage 55.2 SystemInfo.GetMemoryLoadPercentRaw requires its runtime hook");
                return buildNoUnwindCall(
                    systemInfoGetMemoryLoadPercentFunctionType_, systemInfoGetMemoryLoadPercentFunction_, nullptr, 0, "systeminfo.memoryloadpercent");

            // Stage 55.3 "Diagnostics".
            case frontend::ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent:
                if (debugIsDebuggerPresentFunction_ == nullptr)
                    return failValue(error, "Stage 55.3 Debug.IsDebuggerPresentRaw requires its runtime hook");
                return buildNoUnwindCall(
                    debugIsDebuggerPresentFunctionType_, debugIsDebuggerPresentFunction_, nullptr, 0, "debug.isdebuggerpresent");
            case frontend::ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount:
                if (debugCaptureStackFrameCountFunction_ == nullptr)
                    return failValue(error, "Stage 55.3 Debug.CaptureStackFrameCountRaw requires its runtime hook");
                return buildNoUnwindCall(
                    debugCaptureStackFrameCountFunctionType_, debugCaptureStackFrameCountFunction_, nullptr, 0, "debug.capturestackframecount");
            case frontend::ProjectOopMirNodeKind::CoreDebugOutputString:
            {
                if (node->children.size() != 1 || debugOutputStringFunction_ == nullptr)
                    return failValue(error, "Stage 55.3 Debug.OutputStringRaw requires one string argument and runtime hook");
                LLVMValueRef message = lowerExpression(node->children.front().get(), error);
                if (message == nullptr) return nullptr;
                LLVMValueRef args[] = {message};
                buildNoUnwindCall(debugOutputStringFunctionType_, debugOutputStringFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }

            // Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29).
            case frontend::ProjectOopMirNodeKind::CoreThreadSleep:
            {
                if (node->children.size() != 1 || threadSleepFunction_ == nullptr)
                    return failValue(error, "Pre-Stage-56 Thread.SleepRaw requires one int argument and runtime hook");
                LLVMValueRef milliseconds = lowerExpression(node->children.front().get(), error);
                if (milliseconds == nullptr) return nullptr;
                LLVMValueRef args[] = {milliseconds};
                buildNoUnwindCall(threadSleepFunctionType_, threadSleepFunction_, args, 1, "");
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::CoreFileCopy:
            {
                if (node->children.size() != 3 || fileCopyFunction_ == nullptr)
                    return failValue(error, "Pre-Stage-56 File.CopyRaw requires source, dest, overwrite arguments and runtime hook");
                LLVMValueRef source = lowerExpression(node->children[0].get(), error);
                if (source == nullptr) return nullptr;
                LLVMValueRef dest = lowerExpression(node->children[1].get(), error);
                if (dest == nullptr) return nullptr;
                LLVMValueRef overwrite = lowerExpression(node->children[2].get(), error);
                if (overwrite == nullptr) return nullptr;
                LLVMValueRef args[] = {source, dest, overwrite};
                return buildNoUnwindCall(fileCopyFunctionType_, fileCopyFunction_, args, 3, "file.copy");
            }
            // File.MoveRaw and Directory.MoveRaw BOTH lower to the SAME shared runtime hook
            // (moveFileFunction_) -- see buildMoveFile's header comment in
            // WindowsRuntimeBuilder.cpp for why sharing one wrapper across two
            // MethodCoreOperationKind values is safe here (Win32's MoveFileA is byte-for-byte
            // identical for files and directories). Same pattern as CoreSemaphoreWait/CoreEventWait
            // above (Stage 54.5).
            case frontend::ProjectOopMirNodeKind::CoreFileMove:
            case frontend::ProjectOopMirNodeKind::CoreDirectoryMove:
            {
                if (node->children.size() != 2 || moveFileFunction_ == nullptr)
                    return failValue(error, "Pre-Stage-56 File/Directory MoveRaw requires source + dest arguments and runtime hook");
                LLVMValueRef source = lowerExpression(node->children[0].get(), error);
                if (source == nullptr) return nullptr;
                LLVMValueRef dest = lowerExpression(node->children[1].get(), error);
                if (dest == nullptr) return nullptr;
                LLVMValueRef args[] = {source, dest};
                return buildNoUnwindCall(moveFileFunctionType_, moveFileFunction_, args, 2, "move.file");
            }

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
                return lowerDelegateInvoke(node, error);
            case frontend::ProjectOopMirNodeKind::EventAdd:
                return lowerEventAdd(node, error);
            case frontend::ProjectOopMirNodeKind::EventRemove:
                return lowerEventRemove(node, error);
            case frontend::ProjectOopMirNodeKind::EventInvoke:
                return lowerEventInvoke(node, error);

            case frontend::ProjectOopMirNodeKind::Unary:
                return lowerUnary(node, error);
            case frontend::ProjectOopMirNodeKind::Binary:
                return lowerBinary(node, error);
            case frontend::ProjectOopMirNodeKind::Store:
            {
                if (node->children.size() != 2) return failValue(error, "store MIR requires target and value");
                LLVMValueRef address = lowerExpression(node->children[0].get(), error);
                if (address == nullptr) return nullptr;
                // NSC-LIMIT-015: evaluate the target address and the complete RHS before releasing
                // the owner displaced by the store. The old order made `text = Concat(text, value)`
                // pass freed memory to Concat. The materialized SSA value dominates the release and
                // ready blocks and is transferred by the final store.
                LLVMValueRef value = lowerExpression(node->children[1].get(), error);
                if (value == nullptr) return nullptr;
                if (node->children[0]->kind == frontend::ProjectOopMirNodeKind::BitFieldAddress)
                {
                    const frontend::ProjectOopMirNode* field = node->children[0].get();
                    LLVMTypeRef storageType = bitFieldStorageType(field, error);
                    if (storageType == nullptr) return nullptr;
                    LLVMValueRef previous = LLVMBuildLoad2(
                        builder_, storageType, address, "bitfield.store.storage.load");
                    LLVMSetAlignment(previous, static_cast<unsigned>(field->alignment));
                    LLVMValueRef merged = insertBitField(field, previous, value, error);
                    if (merged == nullptr) return nullptr;
                    LLVMValueRef store = LLVMBuildStore(builder_, merged, address);
                    LLVMSetAlignment(store, static_cast<unsigned>(field->alignment));
                    return extractBitField(field, merged, error);
                }
                if (node->booleanValue && node->localOrdinal.has_value())
                {
                    const auto cleanup = std::find_if(
                        cleanupRegions_.begin(), cleanupRegions_.end(),
                        [&](const auto& entry)
                        {
                            return entry.second != nullptr &&
                                entry.second->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration &&
                                entry.second->localOrdinal == node->localOrdinal;
                        });
                    if (cleanup == cleanupRegions_.end() || !emitRaiiDestroy(cleanup->second, error)) return nullptr;
                }
                else if (node->booleanValue && requiresOwnedDestruction(node->typeKey))
                {
                    // Bloque 1 item 1 (owning field storage) -- a write into a field that ALREADY
                    // owns something releases the displaced value first, or the overwrite leaks it.
                    // Flagged without a localOrdinal by the HIR (see the Assignment case there),
                    // because a field has no cleanup region to resolve through: the destroy works
                    // straight off the store address, guarded by its null state exactly like every
                    // other cleanup in this compiler.
                    //
                    // The requiresOwnedDestruction gate above is what keeps a generic `owned T`
                    // field closed over a value type a no-op here -- the HIR flags every owned
                    // field store type-agnostically and this is the single point that decides, the
                    // same way the owned-parameter and raii-local cleanups already decide.
                    //
                    // Native array fields deliberately stay out of this path. Core containers use
                    // the explicit low-level contract `delete[] field; field = replacement;`.
                    // Dispatching the dangling old array as though its length header were a class
                    // descriptor double-frees on the first capacity growth. `raii T[]` locals are
                    // handled by emitRaiiDestroy and do not need field replacement semantics here.
                    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context_, 0);
                    LLVMValueRef previous = LLVMBuildLoad2(
                        builder_, ptrType, address, "owned.field.previous");
                    LLVMValueRef previousIsNull = LLVMBuildICmp(
                        builder_, LLVMIntEQ, previous, LLVMConstNull(ptrType),
                        "owned.field.previous.isnull");
                    LLVMBasicBlockRef releaseBlock = appendBlock("owned.field.replace");
                    LLVMBasicBlockRef readyBlock = appendBlock("owned.field.ready");
                    LLVMBuildCondBr(builder_, previousIsNull, readyBlock, releaseBlock);

                    LLVMPositionBuilderAtEnd(builder_, releaseBlock);
                    LLVMBuildStore(builder_, LLVMConstNull(ptrType), address);
                    if (node->typeKey == "builtin:string")
                    {
                        // Audit finding (Bloque 1, P0/P1 "owned string overwrite", MEASURED as a
                        // crash: exit 5 under Wine at -O0 and -O2): this branch used to dispatch
                        // the displaced value through its class descriptor unconditionally, but a
                        // native string is a bare heap allocation with no descriptor to read --
                        // interpreting its first eight bytes as a descriptor pointer walked into
                        // character data. The final-destruction traversal already made exactly
                        // this distinction (OwnedField::isNativeString); the overwrite path is the
                        // same obligation at a different time and must use the same fork. One slot
                        // type, one destruction semantics -- never one rule at container exit and
                        // another at replacement.
                        if (deallocationFunction_ == nullptr)
                        {
                            return failValue(error, "Bloque 1 owned string overwrite has no deallocation ABI");
                        }
                        LLVMValueRef stringArguments[] = {previous};
                        buildNoUnwindCall(
                            deallocationFunctionType_, deallocationFunction_, stringArguments, 1, "");
                    }
                    else
                    {
                        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
                        {
                            return failValue(error, "Bloque 1 owned field overwrite has no destruction ABI");
                        }
                        LLVMValueRef previousDescriptor = LLVMBuildLoad2(
                            builder_, ptrType, previous, "owned.field.previous.descriptor");
                        LLVMValueRef previousEntryAddress = LLVMBuildStructGEP2(
                            builder_, descriptorType_, previousDescriptor, 5,
                            "owned.field.previous.entry.addr");
                        LLVMValueRef previousEntry = LLVMBuildLoad2(
                            builder_, ptrType, previousEntryAddress, "owned.field.previous.entry");
                        LLVMValueRef releaseArguments[] = {previous};
                        buildNoUnwindCall(
                            destructionEntryFunctionType_, previousEntry, releaseArguments, 1, "");
                    }
                    LLVMBuildBr(builder_, readyBlock);
                    LLVMPositionBuilderAtEnd(builder_, readyBlock);
                }
                LLVMValueRef store = LLVMBuildStore(builder_, value, address);
                if (node->children[0]->volatileAccess) LLVMSetVolatile(store, 1);
                return value;
            }
            case frontend::ProjectOopMirNodeKind::Convert:
            {
                if (node->children.size() != 1) return failValue(error, "convert MIR requires one operand");
                LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                if (value == nullptr) return nullptr;
                return convert(value, node->children.front()->typeKey, node->typeKey, error);
            }
            case frontend::ProjectOopMirNodeKind::RuntimeTypeTest:
                return lowerRuntimeTypeTest(node, error);
            case frontend::ProjectOopMirNodeKind::SafeReferenceCast:
                return lowerSafeReferenceCast(node, error);
            case frontend::ProjectOopMirNodeKind::CheckedReferenceCast:
                return lowerCheckedReferenceCast(node, error);
            case frontend::ProjectOopMirNodeKind::DirectInstanceCall:
                return lowerCall(node, error);
            case frontend::ProjectOopMirNodeKind::DirectStaticCall:
                return lowerStaticCall(node, error);
            case frontend::ProjectOopMirNodeKind::VirtualInstanceCall:
                return lowerVirtualCall(node, error);
            case frontend::ProjectOopMirNodeKind::InterfaceInstanceCall:
                return lowerInterfaceCall(node, error);
            case frontend::ProjectOopMirNodeKind::TemporaryAddress:
            {
                // C6 "struct members": give a struct temporary a home, and hand back its address.
                //
                // One entry-block slot, the value stored into it, the slot returned. Entry block,
                // not here, so a loop does not grow the stack.
                //
                // MEASURED, and worth writing down because the assumption is easy to make the
                // other way round: an INDIRECT aggregate is NOT already a pointer at this level.
                // A call that returns one writes into an sret slot and then LOADS it
                // (`abi.sret.load`), so what an expression of that type hands over is the loaded
                // `[N x i8]` value, exactly like a direct aggregate hands over its integer. The
                // first version of this case short-circuited on isIndirectStructTypeKey and
                // returned that value as if it were an address; the verifier caught it
                // immediately ("GEP base pointer is not a vector", GEPing into a loaded
                // [12 x i8]). So both shapes take the same path, which is also the simpler one.
                if (node->children.size() != 1)
                    return failValue(error, "C6 temporary address requires one operand");
                LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                if (value == nullptr) return nullptr;
                LLVMTypeRef carrier = llvmType(node->typeKey, error);
                if (carrier == nullptr) return nullptr;
                LLVMValueRef slot = entryBlockAlloca(
                    carrier, structAlignmentFor(node->typeKey), "struct.temp");
                if (slot == nullptr)
                    return failValue(error, "C6 could not reserve storage for a struct temporary");
                LLVMBuildStore(builder_, value, slot);
                return slot;
            }
            case frontend::ProjectOopMirNodeKind::ValueConstruct:
                return lowerValueConstruction(node, error);
            case frontend::ProjectOopMirNodeKind::PlacementConstruct:
                return lowerPlacementConstruction(node, error);
            case frontend::ProjectOopMirNodeKind::AggregateInitialize:
                return lowerAggregateInitializer(node, error);
            case frontend::ProjectOopMirNodeKind::AggregateElementStore:
                return failValue(
                    error, "C7 aggregate element store must be lowered by its owning initializer");
            case frontend::ProjectOopMirNodeKind::AllocateAndConstruct:
                return lowerConstruction(node, error);
            case frontend::ProjectOopMirNodeKind::Deallocate:
                return lowerDeallocation(node, error);
            case frontend::ProjectOopMirNodeKind::ObjectDestroy:
                return lowerObjectDestruction(node, error);

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
            case frontend::ProjectOopMirNodeKind::Throw:
            case frontend::ProjectOopMirNodeKind::Rethrow:
            case frontend::ProjectOopMirNodeKind::CleanupContinuation:
            case frontend::ProjectOopMirNodeKind::OwnedParameterCleanup:
            case frontend::ProjectOopMirNodeKind::InlineAssembly:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyLine:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyInput:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyOutput:
            case frontend::ProjectOopMirNodeKind::InlineAssemblyClobber:
                return failValue(error, "non-expression Stage 34 MIR node reached expression lowering");
        }
    }

    bool emitGuard(LLVMValueRef valid, LlvmTrapReason reason, std::string_view stem, std::string& error)
    {
        if (valid == nullptr || trapFunction_ == nullptr || currentFunction_ == nullptr)
        {
            return fail(error, "Stage 41 cannot emit runtime guard");
        }
        LLVMBasicBlockRef okBlock = appendBlock(std::string(stem) + ".ok");
        LLVMBasicBlockRef trapBlock = appendBlock(std::string(stem) + ".trap");
        LLVMBuildCondBr(builder_, valid, okBlock, trapBlock);
        LLVMPositionBuilderAtEnd(builder_, trapBlock);
        LLVMValueRef reasonValue = LLVMConstInt(
            LLVMInt32TypeInContext(context_), static_cast<std::uint64_t>(reason), 0);
        LLVMValueRef args[] = {reasonValue};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, args, 1, "");
        LLVMBuildUnreachable(builder_);
        LLVMPositionBuilderAtEnd(builder_, okBlock);
        return true;
    }

    LLVMValueRef byteOffset(LLVMValueRef base, std::uint64_t offset, const char* name)
    {
        LLVMValueRef index = LLVMConstInt(LLVMInt64TypeInContext(context_), offset, 0);
        LLVMValueRef indices[] = {index};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), base, indices, 1, name);
    }

    std::pair<std::uint64_t, std::uint64_t> captureValueLayout(
        std::string_view typeKey, std::string& error) const
    {
        if (const frontend::ProjectOopMirStructLayout* layout = structLayout(typeKey); layout != nullptr)
            return {layout->size, layout->alignment};
        if (const frontend::builtins::IntegerTypeInfo* info =
                frontend::builtins::findIntegerType(typeKey); info != nullptr)
        {
            const std::uint64_t bytes = std::max<std::uint64_t>(1, info->width / 8U);
            return {bytes, bytes};
        }
        if (const frontend::builtins::IntegerTypeInfo* info = enumUnderlyingInfo(typeKey); info != nullptr)
        {
            const std::uint64_t bytes = std::max<std::uint64_t>(1, info->width / 8U);
            return {bytes, bytes};
        }
        if (typeKey == "builtin:bool" || typeKey == "builtin:char") return {1, 1};
        if (typeKey == "builtin:float") return {4, 4};
        if (typeKey == "builtin:double") return {8, 8};
        if (typeKey == "builtin:string" || frontend::ProjectTypeConversion::isNativeArray(typeKey) ||
            isPointerLike(typeKey) || isClassReference(typeKey) || isDelegateReference(typeKey))
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

    bool ensureCaptureCellRuntime(std::string& error)
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

    LLVMValueRef allocateCaptureCell(
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

    void retainCaptureCell(LLVMValueRef cell)
    {
        LLVMValueRef arguments[] = {cell};
        buildNoUnwindCall(
            captureCellFunctionType_, captureCellRetainFunction_, arguments, 1, "");
    }

    void releaseCaptureCell(LLVMValueRef cell)
    {
        LLVMValueRef arguments[] = {cell};
        buildNoUnwindCall(
            captureCellFunctionType_, captureCellReleaseFunction_, arguments, 1, "");
    }

    void releaseOwnedCaptureCells()
    {
        for (LLVMValueRef cell : ownedCaptureCells_) releaseCaptureCell(cell);
    }

    LLVMBasicBlockRef captureCellExceptionalCleanup(std::string& error)
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
        LLVMPositionBuilderAtEnd(builder_, captureCellExceptionalCleanupBlock_);
        LLVMValueRef cleanupPad = LLVMBuildCleanupPad(
            builder_, nullptr, nullptr, 0, "eh.capture.cells.pad");
        if (cleanupPad == nullptr)
        {
            captureCellExceptionalCleanupBlock_ = nullptr;
            LLVMPositionBuilderAtEnd(builder_, sourceBlock);
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
        LLVMPositionBuilderAtEnd(builder_, sourceBlock);
        return captureCellExceptionalCleanupBlock_;
    }

    LLVMValueRef closureContextDestroy(
        std::string_view calleeName,
        std::size_t captureCount,
        std::string& error)
    {
        const auto existing = closureContextDestroyFunctions_.find(std::string(calleeName));
        if (existing != closureContextDestroyFunctions_.end()) return existing->second;
        if (!ensureCaptureCellRuntime(error)) return nullptr;

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef parameters[] = {ptr};
        LLVMTypeRef functionType = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameters, 1, 0);
        const std::string name = "__nsc_closure_context_destroy_" + std::string(calleeName);
        LLVMValueRef function = LLVMAddFunction(module_, name.c_str(), functionType);
        if (function == nullptr)
            return failValue(error, "E11 could not create closure-context destroy helper");

        LLVMMetadataRef subprogram = nullptr;
        SourceRange debugRange;
        const frontend::ProjectOopMirFunction* origin = nullptr;
        for (const frontend::ProjectOopMirFunction& candidate : mirModel_.functions())
        {
            if (candidate.mangledName == calleeName)
            {
                origin = &candidate;
                break;
            }
        }
        if (origin != nullptr && memberModel_ != nullptr &&
            origin->memberOrdinal < memberModel_->members().size() &&
            origin->declaringTypeOrdinal < nameLookup_.namedTypes().size())
        {
            const frontend::ClassMemberSymbol& member = memberModel_->members()[origin->memberOrdinal];
            debugRange = member.range;
            if (origin->body != nullptr && origin->body->syntax != nullptr &&
                origin->body->syntax->range().isValid())
                debugRange = origin->body->syntax->range();
            std::string displayName =
                nameLookup_.namedTypes()[origin->declaringTypeOrdinal].identity.fullyQualifiedName() +
                "." + member.name;
            if (origin->isLambda)
                displayName += ".lambda#" + std::to_string(origin->lambdaOrdinal.value_or(0));
            displayName += " [generated: closure context destroy]";
            const std::vector<std::string> debugParameters = {"builtin:void*"};
            subprogram = createSynthesizedDebugSubprogram(
                debugRange, displayName, name, "builtin:void",
                debugParameters, true, false);
        }
        if (debugBuilder_ != nullptr && subprogram == nullptr)
            return failValue(error, "failed to create debug subprogram for closure-context destroy helper " + name);
        if (subprogram != nullptr) LLVMSetSubprogram(function, subprogram);

        LLVMBuilderRef savedBuilder = builder_;
        LLVMBuilderRef helper = LLVMCreateBuilderInContext(context_);
        builder_ = helper;
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context_, function, "entry");
        LLVMPositionBuilderAtEnd(builder_, entry);
        {
            ScopedDebugLocation generatedLocation(
                builder_, debugSubprogramLocation(debugRange, subprogram));
            LLVMValueRef environment = LLVMGetParam(function, 0);
            LLVMSetValueName2(environment, "context", 7);
            for (std::size_t index = 0; index < captureCount; ++index)
            {
                LLVMValueRef slot = byteOffset(
                    environment, static_cast<std::uint64_t>(index) * 8U, "closure.cell.slot");
                LLVMValueRef cell = LLVMBuildLoad2(builder_, ptr, slot, "closure.cell");
                releaseCaptureCell(cell);
            }
            LLVMValueRef freeArguments[] = {environment};
            buildNoUnwindCall(
                deallocationFunctionType_, deallocationFunction_, freeArguments, 1, "");
            LLVMBuildRetVoid(builder_);
        }
        builder_ = savedBuilder;
        LLVMDisposeBuilder(helper);
        closureContextDestroyFunctions_[std::string(calleeName)] = function;
        return function;
    }

    LLVMValueRef checkedStage42Allocation(LLVMValueRef memory, std::string_view stem, std::string& error)
    {
        if (memory == nullptr) return nullptr;
        LLVMValueRef notNull = LLVMBuildICmp(
            builder_, LLVMIntNE, memory, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)),
            (std::string(stem) + ".nonnull").c_str());
        return emitGuard(notNull, LlvmTrapReason::AllocationFailure, stem, error) ? memory : nullptr;
    }

    LLVMValueRef allocateDelegateCell(
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

    bool emitDelegateDestroy(LLVMValueRef cell, std::string_view stem, std::string& error)
    {
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMBasicBlockRef destroyBlock = appendBlock(std::string(stem) + ".destroy");
        LLVMBasicBlockRef doneBlock = appendBlock(std::string(stem) + ".done");
        LLVMValueRef isNull = LLVMBuildICmp(builder_, LLVMIntEQ, cell, LLVMConstNull(ptr),
                                            (std::string(stem) + ".isnull").c_str());
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);
        LLVMPositionBuilderAtEnd(builder_, destroyBlock);

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

        LLVMPositionBuilderAtEnd(builder_, environmentBlock);
        LLVMValueRef contextArguments[] = {context};
        buildNoUnwindCall(
            deallocationFunctionType_, destroyContext, contextArguments, 1, "");
        LLVMBuildBr(builder_, freeCellBlock);

        LLVMPositionBuilderAtEnd(builder_, freeCellBlock);
        LLVMValueRef freeCell = LLVMBuildLoad2(
            builder_, ptr, byteOffset(cell, 24, "delegate.destroy.cell.callback.addr"),
            "delegate.destroy.cell.callback");
        LLVMValueRef callbackPresent = LLVMBuildICmp(
            builder_, LLVMIntNE, freeCell, LLVMConstNull(ptr), "delegate.destroy.cell.callback.present");
        LLVMBasicBlockRef callbackBlock = appendBlock(std::string(stem) + ".cell.callback");
        LLVMBuildCondBr(builder_, callbackPresent, callbackBlock, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, callbackBlock);
        LLVMValueRef cellArguments[] = {cell};
        buildNoUnwindCall(deallocationFunctionType_, freeCell, cellArguments, 1, "");
        LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return error.empty();
    }

    LLVMValueRef closureCaptureCell(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (currentMirFunction_ == nullptr || !currentMirFunction_->isLambda ||
            !currentMirFunction_->hasClosureEnvironment || closureEnvironment_ == nullptr)
        {
            return failValue(error, "E11 closure capture has no environment receiver");
        }
        const std::size_t capture = static_cast<std::size_t>(node->immediate);
        if (capture >= currentMirFunction_->captureTypeKeys.size())
            return failValue(error, "E11 closure capture ordinal is invalid");
        LLVMValueRef slot = byteOffset(
            closureEnvironment_, static_cast<std::uint64_t>(capture) * 8U,
            "closure.capture.cell.addr");
        return LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0), slot, "closure.capture.cell");
    }

    LLVMValueRef closureCaptureAddress(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        LLVMValueRef cell = closureCaptureCell(node, error);
        if (cell == nullptr) return nullptr;
        const auto [valueSize, valueAlignment] = captureValueLayout(node->typeKey, error);
        if (valueSize == 0 || valueAlignment == 0) return nullptr;
        const std::uint64_t valueOffset =
            ((8U + valueAlignment - 1U) / valueAlignment) * valueAlignment;
        LLVMValueRef address = byteOffset(cell, valueOffset, "closure.capture.value.addr");
        const std::size_t capture = static_cast<std::size_t>(node->immediate);
        if (debugInformation_ == DebugInformationKind::Full &&
            currentDebugSubprogram_ != nullptr &&
            !debugCaptureVariables_.contains(capture))
        {
            const std::string name = capture < currentMirFunction_->captureNames.size() &&
                    !currentMirFunction_->captureNames[capture].empty()
                ? currentMirFunction_->captureNames[capture]
                : "capture#" + std::to_string(capture);
            LLVMMetadataRef type = debugType(currentMirFunction_->captureTypeKeys[capture]);
            SourceRange range;
            if (node->syntax != nullptr) range = node->syntax->range();
            if (!range.isValid() && currentMirFunction_->body != nullptr &&
                currentMirFunction_->body->syntax != nullptr)
                range = currentMirFunction_->body->syntax->range();
            if (type != nullptr && range.isValid())
            {
                LLVMMetadataRef file = debugFile(range.begin());
                const source::SourcePosition position = sourceManager_->getPosition(range.begin());
                LLVMMetadataRef variable = LLVMDIBuilderCreateAutoVariable(
                    debugBuilder_, currentDebugSubprogram_, name.data(), name.size(), file,
                    position.line, type, 1, LLVMDIFlagZero, 0);
                LLVMMetadataRef expression =
                    LLVMDIBuilderCreateExpression(debugBuilder_, nullptr, 0);
                LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
                    context_, position.line, position.column, currentDebugSubprogram_, nullptr);
                if (variable == nullptr || expression == nullptr || location == nullptr ||
                    LLVMDIBuilderInsertDeclareRecordAtEnd(
                        debugBuilder_, address, variable, expression, location,
                        LLVMGetInsertBlock(builder_)) == nullptr)
                    return failValue(error, "failed to emit debug storage for captured variable '" +
                        name + "'");
                debugCaptureVariables_.emplace(capture, variable);
            }
        }
        return address;
    }

    LLVMValueRef lowerClosureCaptureLoad(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        LLVMValueRef address = closureCaptureAddress(node, error);
        if (address == nullptr) return nullptr;
        const std::size_t capture = static_cast<std::size_t>(node->immediate);
        LLVMTypeRef type = llvmType(currentMirFunction_->captureTypeKeys[capture], error);
        if (type == nullptr || type == LLVMVoidTypeInContext(context_))
            return failValue(error, "E11 closure capture has invalid LLVM type");
        LLVMValueRef value = LLVMBuildLoad2(builder_, type, address, "closure.capture.load");
        const auto debugVariable = debugCaptureVariables_.find(capture);
        if (debugVariable != debugCaptureVariables_.end())
        {
            LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder_, nullptr, 0);
            LLVMMetadataRef location = LLVMGetCurrentDebugLocation2(builder_);
            if (expression == nullptr || location == nullptr ||
                LLVMDIBuilderInsertDbgValueRecordAtEnd(
                    debugBuilder_, value, debugVariable->second, expression, location,
                    LLVMGetInsertBlock(builder_)) == nullptr)
                return failValue(error, "failed to emit the loaded value of a captured variable");
        }
        return value;
    }

    LLVMValueRef lowerCaptureCellReference(const frontend::ProjectOopMirNode* node, std::string& error)
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

    LLVMValueRef lowerLambdaCreate(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end() || found->second.function == nullptr)
            return failValue(error, "Stage 43 lambda generated callable is unavailable: " + node->calleeName);

        if (isExternCDelegateType(node->typeKey))
        {
            if (!node->children.empty())
                return failValue(error, "E11 extern C lambda reached LLVM with captured state");
            return callbackGuardThunk(node->calleeName, error);
        }

        LLVMValueRef receiver = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        if (!node->children.empty())
        {
            const std::uint64_t bytesCount = static_cast<std::uint64_t>(node->children.size()) * 8U;
            LLVMValueRef bytes = LLVMConstInt(LLVMInt64TypeInContext(context_), bytesCount, 0);
            LLVMValueRef args[] = {bytes};
            receiver = buildNoUnwindCall(
                allocationFunctionType_, allocationFunction_, args, 1, "closure.env");
            receiver = checkedStage42Allocation(receiver, "closure.env.alloc", error);
            if (receiver == nullptr) return nullptr;

            for (std::size_t index = 0; index < node->children.size(); ++index)
            {
                LLVMValueRef value = lowerExpression(node->children[index].get(), error);
                if (value == nullptr) return nullptr;
                retainCaptureCell(value);
                LLVMValueRef address = byteOffset(
                    receiver, static_cast<std::uint64_t>(index) * 8U, "closure.capture.store.addr");
                LLVMBuildStore(builder_, value, address);
            }
        }
        LLVMValueRef contextDestroy = node->children.empty()
            ? LLVMConstNull(LLVMPointerTypeInContext(context_, 0))
            : closureContextDestroy(node->calleeName, node->children.size(), error);
        if (contextDestroy == nullptr && !node->children.empty()) return nullptr;
        return allocateDelegateCell(
            found->second.function, receiver, contextDestroy, "lambda.delegate", error);
    }

    LLVMValueRef lowerDelegateCreate(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end() || found->second.function == nullptr)
            return failValue(error, "Stage 42 delegate target function is unavailable: " + node->calleeName);
        if (node->children.size() > 1)
            return failValue(error, "Stage 42 delegate-create accepts at most one receiver");

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
            // C1.3 "callback exception boundary": what C receives is the GUARD THUNK's address,
            // not the target's. The thunk has the target's exact signature -- which is already the
            // Win64 C ABI -- so nothing about the call changes for the host; what changes is that
            // a NativeSharp exception reaching the outer edge of the callback traps there instead
            // of unwinding into foreign frames. defineCallbackGuardThunks emitted it before any
            // body was lowered, and it is cached per target, so two conversions of the same method
            // still yield the same pointer.
            return callbackGuardThunk(node->calleeName, error);
        }

        LLVMValueRef receiver = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        if (!node->children.empty())
        {
            receiver = lowerExpression(node->children.front().get(), error);
            if (receiver == nullptr) return nullptr;
            receiver = checkedReceiver(receiver, error);
            if (receiver == nullptr) return nullptr;
        }

        return allocateDelegateCell(
            found->second.function, receiver,
            LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), "delegate", error);
    }

    LLVMValueRef emitDelegateCall(
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

        // Win64 aggregate ABI: the reconstructed target types must match declareFunctions --
        // pointer-typed indirect parameters (the caller already wrapped the values), and an
        // indirect return through an sret slot: leading for a static target, after the
        // receiver for an instance target. Both branches store into the SAME slot, so the
        // merge reads the slot instead of phi-ing an aggregate.
        const bool sretReturn = isIndirectStructTypeKey(returnTypeKey);
        LLVMValueRef sretSlot = nullptr;
        if (sretReturn)
        {
            LLVMTypeRef carrier = llvmType(returnTypeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(returnTypeKey), "abi.sret.slot");
        }
        LLVMTypeRef returnType = sretReturn
            ? LLVMVoidTypeInContext(context_)
            : llvmType(returnTypeKey, error);
        if (returnType == nullptr) return nullptr;
        std::vector<LLVMTypeRef> staticTypes;
        staticTypes.reserve(argumentTypeKeys.size() + 1);
        if (sretReturn) staticTypes.push_back(ptr);
        for (const std::string& typeKey : argumentTypeKeys)
        {
            LLVMTypeRef type = abiParameterType(typeKey, error);
            if (type == nullptr || type == LLVMVoidTypeInContext(context_))
                return failValue(error, "Stage 42 delegate has invalid parameter type '" + typeKey + "'");
            staticTypes.push_back(type);
        }
        std::vector<LLVMTypeRef> instanceTypes;
        instanceTypes.reserve(staticTypes.size() + 1);
        instanceTypes.push_back(ptr);
        instanceTypes.insert(instanceTypes.end(), staticTypes.begin(), staticTypes.end());
        LLVMTypeRef staticFunctionType = LLVMFunctionType(
            returnType, staticTypes.empty() ? nullptr : staticTypes.data(),
            static_cast<unsigned>(staticTypes.size()), 0);
        LLVMTypeRef instanceFunctionType = LLVMFunctionType(
            returnType, instanceTypes.data(), static_cast<unsigned>(instanceTypes.size()), 0);

        LLVMBasicBlockRef instanceBlock = appendBlock("delegate.instance");
        LLVMBasicBlockRef staticBlock = appendBlock("delegate.static");
        LLVMBasicBlockRef mergeBlock = appendBlock("delegate.merge");
        LLVMBuildCondBr(builder_, hasReceiver, instanceBlock, staticBlock);

        LLVMPositionBuilderAtEnd(builder_, staticBlock);
        std::vector<LLVMValueRef> staticArguments;
        staticArguments.reserve(argumentValues.size() + 1);
        if (sretSlot != nullptr) staticArguments.push_back(sretSlot);
        staticArguments.insert(staticArguments.end(), argumentValues.begin(), argumentValues.end());
        LLVMValueRef staticResult = buildPotentiallyThrowingCall(
            staticFunctionType, code,
            staticArguments.empty() ? nullptr : staticArguments.data(),
            static_cast<unsigned>(staticArguments.size()),
            returnType == LLVMVoidTypeInContext(context_) ? "" : "delegate.static.result", error);
        if (staticResult == nullptr) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(staticResult, attributeIndex, attribute); },
                0U, returnTypeKey);
        }
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef staticIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, instanceBlock);
        std::vector<LLVMValueRef> instanceArguments;
        instanceArguments.reserve(argumentValues.size() + 2);
        instanceArguments.push_back(receiver);
        if (sretSlot != nullptr) instanceArguments.push_back(sretSlot);
        instanceArguments.insert(instanceArguments.end(), argumentValues.begin(), argumentValues.end());
        LLVMValueRef instanceResult = buildPotentiallyThrowingCall(
            instanceFunctionType, code, instanceArguments.data(),
            static_cast<unsigned>(instanceArguments.size()),
            returnType == LLVMVoidTypeInContext(context_) ? "" : "delegate.instance.result", error);
        if (instanceResult == nullptr) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(instanceResult, attributeIndex, attribute); },
                1U, returnTypeKey);
        }
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef instanceIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, mergeBlock);
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

    LLVMValueRef emitExternCDelegateCall(
        LLVMValueRef functionPointer,
        const std::vector<LLVMValueRef>& argumentValues,
        const std::vector<std::string>& argumentTypeKeys,
        std::string_view returnTypeKey,
        std::string& error)
    {
        if (argumentValues.size() != argumentTypeKeys.size())
            return failValue(error, "Stage 47 extern C delegate invocation lost signature argument types");
        functionPointer = checkedReceiver(functionPointer, error);
        if (functionPointer == nullptr) return nullptr;

        // Win64 aggregate ABI at the C-function-pointer boundary: identical rules to every
        // other call -- indirect parameters as pointers (already wrapped by the invoker),
        // indirect return through a leading sret slot. This is what makes an extern C delegate
        // interchangeable with a function pointer an MSVC-compiled host produced.
        const bool sretReturn = isIndirectStructTypeKey(returnTypeKey);
        LLVMValueRef sretSlot = nullptr;
        if (sretReturn)
        {
            LLVMTypeRef carrier = llvmType(returnTypeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(returnTypeKey), "abi.sret.slot");
        }
        LLVMTypeRef returnType = sretReturn
            ? LLVMVoidTypeInContext(context_)
            : llvmType(returnTypeKey, error);
        if (returnType == nullptr) return nullptr;
        std::vector<LLVMTypeRef> parameterTypes;
        parameterTypes.reserve(argumentTypeKeys.size() + 1);
        if (sretReturn) parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
        for (const std::string& typeKey : argumentTypeKeys)
        {
            LLVMTypeRef type = abiParameterType(typeKey, error);
            if (type == nullptr || type == LLVMVoidTypeInContext(context_))
                return failValue(error, "Stage 47 extern C delegate has invalid parameter type '" + typeKey + "'");
            parameterTypes.push_back(type);
        }
        LLVMTypeRef functionType = LLVMFunctionType(
            returnType, parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()), 0);
        std::vector<LLVMValueRef> callArguments;
        callArguments.reserve(argumentValues.size() + 1);
        if (sretSlot != nullptr) callArguments.push_back(sretSlot);
        callArguments.insert(callArguments.end(), argumentValues.begin(), argumentValues.end());
        LLVMValueRef call = LLVMBuildCall2(
            builder_, functionType, functionPointer,
            callArguments.empty() ? nullptr : callArguments.data(),
            static_cast<unsigned>(callArguments.size()),
            returnType == LLVMVoidTypeInContext(context_) ? "" : "cabi.callback.result");
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
        return call;
    }

    LLVMValueRef lowerDelegateInvoke(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.empty() || node->children.size() != node->argumentCount + 1)
            return failValue(error, "Stage 42 delegate invocation has invalid MIR children");
        LLVMValueRef delegateValue = lowerExpression(node->children.front().get(), error);
        if (delegateValue == nullptr) return nullptr;
        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->argumentCount);
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            // Win64 aggregate ABI: indirect struct arguments cross ANY call boundary --
            // delegate targets included -- as pointers to per-call copies. Wrapped once here
            // so the static/instance/extern-C emitters all see ABI-ready values. Measured
            // before this existed: a delegate taking a 64-byte struct read garbage (1 at -O0,
            // 5 at -O2, for a fixture whose answer is 4).
            if (index - 1 < node->argumentTypeKeys.size())
            {
                value = wrapIndirectArgument(value, node->argumentTypeKeys[index - 1], error);
                if (value == nullptr) return nullptr;
            }
            arguments.push_back(value);
        }
        if (isExternCDelegateType(node->elementTypeKey))
        {
            return emitExternCDelegateCall(
                delegateValue, arguments, node->argumentTypeKeys, node->typeKey, error);
        }
        return emitDelegateCall(delegateValue, arguments, node->argumentTypeKeys, node->typeKey, error);
    }

    LLVMValueRef delegateCellsEqual(LLVMValueRef left, LLVMValueRef right)
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

    LLVMValueRef lowerEventAdd(const frontend::ProjectOopMirNode* node, std::string& error)
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
        LLVMValueRef head = LLVMBuildLoad2(builder_, ptr, backingAddress, "event.head");
        LLVMBuildStore(builder_, handler, eventNode);
        LLVMValueRef nextAddress = byteOffset(eventNode, 8, "event.next.addr");
        LLVMBuildStore(builder_, head, nextAddress);
        LLVMBuildStore(builder_, eventNode, backingAddress);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

    LLVMValueRef lowerEventRemove(const frontend::ProjectOopMirNode* node, std::string& error)
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

        LLVMPositionBuilderAtEnd(builder_, loopBlock);
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

        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        LLVMValueRef currentHandler = LLVMBuildLoad2(builder_, ptr, currentPhi, "event.remove.handler");
        LLVMValueRef equal = delegateCellsEqual(currentHandler, target);
        LLVMBuildCondBr(builder_, equal, removeBlock, advanceBlock);

        LLVMPositionBuilderAtEnd(builder_, removeBlock);
        LLVMValueRef removeNextAddress = byteOffset(currentPhi, 8, "event.remove.next.addr");
        LLVMValueRef removeNext = LLVMBuildLoad2(builder_, ptr, removeNextAddress, "event.remove.next");
        LLVMBuildStore(builder_, removeNext, linkPhi);
        LLVMValueRef freeArgs[] = {currentPhi};
        buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, freeArgs, 1, "");
        LLVMBuildBr(builder_, doneBlock);

        LLVMPositionBuilderAtEnd(builder_, advanceBlock);
        LLVMValueRef nextAddress = byteOffset(currentPhi, 8, "event.advance.next.addr");
        LLVMValueRef next = LLVMBuildLoad2(builder_, ptr, nextAddress, "event.advance.next");
        LLVMBuildBr(builder_, loopBlock);
        LLVMValueRef nextValues[] = {next};
        LLVMBasicBlockRef advanceIncoming[] = {advanceBlock};
        LLVMAddIncoming(currentPhi, nextValues, advanceIncoming, 1);
        LLVMValueRef linkValues[] = {nextAddress};
        LLVMAddIncoming(linkPhi, linkValues, advanceIncoming, 1);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

    LLVMValueRef lowerEventInvoke(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.empty() || node->children.size() != node->argumentCount + 1 ||
            node->typeKey != "builtin:void")
            return failValue(error, "Stage 42 event invocation requires void delegate semantics");
        LLVMValueRef head = lowerExpression(node->children.front().get(), error);
        if (head == nullptr) return nullptr;
        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->argumentCount);
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            arguments.push_back(value);
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef loopBlock = appendBlock("event.invoke.loop");
        LLVMBasicBlockRef bodyBlock = appendBlock("event.invoke.body");
        LLVMBasicBlockRef doneBlock = appendBlock("event.invoke.done");
        LLVMBuildBr(builder_, loopBlock);
        LLVMPositionBuilderAtEnd(builder_, loopBlock);
        LLVMValueRef currentPhi = LLVMBuildPhi(builder_, ptr, "event.invoke.current");
        LLVMValueRef initialValues[] = {head};
        LLVMBasicBlockRef initialBlocks[] = {preheader};
        LLVMAddIncoming(currentPhi, initialValues, initialBlocks, 1);
        LLVMValueRef hasCurrent = LLVMBuildICmp(
            builder_, LLVMIntNE, currentPhi, LLVMConstNull(ptr), "event.invoke.has.current");
        LLVMBuildCondBr(builder_, hasCurrent, bodyBlock, doneBlock);

        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
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

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

    // C8 "type root", struct half. Both members below read the struct's SCALAR LEAVES -- the flat
    // list of primitive fields with absolute byte offsets that the HIR pass computed (see
    // ProjectOopHirModel::appendAggregateLeaves). Padding is not in that list, deliberately.
    //
    // The one shared step: an operand of struct type arrives as a VALUE here (a carrier integer or
    // a loaded [N x i8]), and leaves are reached by address, so the value gets a home first.
    LLVMValueRef spillAggregate(const frontend::ProjectOopMirNode* node, std::size_t index, std::string& error)
    {
        LLVMValueRef value = lowerExpression(node->children[index].get(), error);
        if (value == nullptr) return nullptr;
        LLVMTypeRef carrier = llvmType(node->elementTypeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "struct.root.operand");
        if (slot == nullptr) return failValue(error, "C8 could not reserve struct operand storage");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        if (node->alignment > 1U) LLVMSetAlignment(store, static_cast<unsigned>(node->alignment));
        return slot;
    }

    // Loads one leaf as raw BITS, widened to i64. Nothing here asks what TYPE the leaf is, only
    // how many bytes it occupies (the MIR pass resolved that from the same value layout everything
    // else in this compiler reads): memory is untyped, so a float leaf is loaded as an i32 and a
    // reference leaf as an i64 without a single cast instruction.
    //
    // Reading bits rather than values is the whole contract. It is what makes the synthesized
    // Equals and GetHashCode agree by construction -- a "compare floats with fcmp, hash their
    // bits" pairing would not, since +0.0 and -0.0 compare equal but hash apart, and a NaN would
    // never equal itself while hashing identically.
    LLVMValueRef loadLeafBits(
        LLVMValueRef base,
        const frontend::ProjectOopMirNode* leaf,
        std::string_view stem,
        std::string& error)
    {
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef loadType = nullptr;
        switch (leaf->secondaryImmediate)
        {
            case 1: loadType = LLVMInt8TypeInContext(context_); break;
            case 2: loadType = LLVMInt16TypeInContext(context_); break;
            case 4: loadType = LLVMInt32TypeInContext(context_); break;
            case 8: loadType = i64; break;
            default: break;
        }
        if (loadType == nullptr)
        {
            return failValue(
                error,
                "C8 struct leaf '" + leaf->typeKey + "' has no scalar width the root members can read");
        }
        const std::string stemText(stem);
        LLVMValueRef address = byteOffset(base, leaf->immediate, (stemText + ".addr").c_str());
        LLVMValueRef value = LLVMBuildLoad2(builder_, loadType, address, stemText.c_str());
        // ALINEACION 1, Y NO EL ANCHO DEL CAMPO.
        //
        // Esta linea decia `LLVMSetAlignment(value, leaf->secondaryImmediate)` -- la alineacion
        // NATURAL del escalar-- y eso es una PROMESA, no una descripcion. En un struct `pack=1`
        // un `int` puede vivir en el offset 1, de modo que el modulo afirmaba `load i32 align 4`
        // sobre una direccion impar: comportamiento indefinido, y de la clase peor, porque el
        // codigo sale bien en O0 y el optimizador tiene derecho a romperlo en O2.
        //
        // La alineacion REAL de una hoja depende de dos cosas que aqui no se conocen: la del
        // agregado --que `pack(N)` puede bajar hasta 1-- y el offset. Sin las dos, el unico valor
        // que nunca miente es 1.
        //
        // Y no cuesta nada donde importa: en x86-64 una carga escalar desalineada es la misma
        // instruccion `mov`, asi que lo unico que se pierde es que el optimizador funda varias
        // cargas en una vectorial -- en una funcion de igualdad de cuatro hojas, nada. Prometer
        // de menos es gratis; prometer de mas es un miscompile.
        LLVMSetAlignment(value, 1);
        if (leaf->secondaryImmediate == 8) return value;
        return LLVMBuildZExt(builder_, value, i64, (stemText + ".bits").c_str());
    }

    LLVMValueRef lowerAggregateEquals(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->argumentCount != 2 || node->children.size() <= 2 || node->alignment == 0)
            return failValue(error, "C8 struct Equals is malformed");
        LLVMValueRef left = spillAggregate(node, 0, error);
        if (left == nullptr) return nullptr;
        LLVMValueRef right = spillAggregate(node, 1, error);
        if (right == nullptr) return nullptr;

        LLVMValueRef equal = nullptr;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* leaf = node->children[index].get();
            if (leaf == nullptr || leaf->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore)
                return failValue(error, "C8 struct Equals has a malformed leaf");
            LLVMValueRef leftBits = loadLeafBits(left, leaf, "struct.eq.left", error);
            if (leftBits == nullptr) return nullptr;
            LLVMValueRef rightBits = loadLeafBits(right, leaf, "struct.eq.right", error);
            if (rightBits == nullptr) return nullptr;
            LLVMValueRef same = LLVMBuildICmp(
                builder_, LLVMIntEQ, leftBits, rightBits, "struct.eq.leaf");
            equal = equal == nullptr ? same : LLVMBuildAnd(builder_, equal, same, "struct.eq.and");
        }
        // A straight-line AND rather than an early-out chain: the leaf loads have no side effects
        // and cannot fault (both operands are live stack slots), so short-circuiting would only
        // add branches for the optimizer to undo.
        return equal;
    }

    LLVMValueRef lowerAggregateHash(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->argumentCount != 1 || node->children.size() <= 1 || node->alignment == 0)
            return failValue(error, "C8 struct GetHashCode is malformed");
        LLVMValueRef value = spillAggregate(node, 0, error);
        if (value == nullptr) return nullptr;

        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        // FNV-1a's offset basis and prime, over 64 bits, applied one leaf at a time. Chosen for
        // the same reason it is chosen everywhere: it is two instructions per leaf and it spreads
        // low-entropy inputs (small integers, mostly-zero structs) across the whole word, which a
        // plain XOR or sum would not.
        LLVMValueRef hash = LLVMConstInt(i64, 0xCBF29CE484222325ULL, 0);
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* leaf = node->children[index].get();
            if (leaf == nullptr || leaf->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore)
                return failValue(error, "C8 struct GetHashCode has a malformed leaf");
            LLVMValueRef bits = loadLeafBits(value, leaf, "struct.hash.leaf", error);
            if (bits == nullptr) return nullptr;
            hash = LLVMBuildXor(builder_, hash, bits, "struct.hash.xor");
            hash = LLVMBuildMul(
                builder_, hash, LLVMConstInt(i64, 0x100000001B3ULL, 0), "struct.hash.mul");
        }
        LLVMValueRef folded = LLVMBuildXor(
            builder_, hash, LLVMBuildLShr(builder_, hash, LLVMConstInt(i64, 32, 0), "struct.hash.shift"),
            "struct.hash.fold");
        return LLVMBuildTrunc(builder_, folded, LLVMInt32TypeInContext(context_), "struct.hash");
    }

    // C8 "type root": materializes one type-name constant with the exact layout a NativeSharp
    // string has -- {i64 length, [N+1 x i8] bytes}. Deliberately the same shape lowerStringLiteral
    // builds, because the value ends up flowing through the language as an ordinary `string`.
    LLVMValueRef internTypeNameConstant(const std::string& globalName, const std::string& text)
    {
        const auto found = typeNameGlobals_.find(globalName);
        if (found != typeNameGlobals_.end()) return found->second;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef bytesType = LLVMArrayType2(i8, text.size() + 1U);
        LLVMTypeRef literalType = LLVMStructCreateNamed(
            context_, (globalName + "_type").c_str());
        LLVMTypeRef fields[] = {i64, bytesType};
        LLVMStructSetBody(literalType, fields, 2, 0);
        LLVMValueRef values[] = {
            LLVMConstInt(i64, static_cast<std::uint64_t>(text.size()), 0),
            LLVMConstStringInContext2(context_, text.data(), text.size(), 0)};
        LLVMValueRef initializer = LLVMConstNamedStruct(literalType, values, 2);
        LLVMValueRef global = LLVMAddGlobal(module_, literalType, globalName.c_str());
        if (global == nullptr || initializer == nullptr) return nullptr;
        LLVMSetInitializer(global, initializer);
        LLVMSetGlobalConstant(global, 1);
        typeNameGlobals_.emplace(globalName, global);
        return global;
    }

    // C8 "type root": `RuntimeHelpers.TypeName(value)` -- the name stored in the object's class
    // descriptor, handed back as an ordinary `string`. The descriptor sits at offset 0 of every
    // object and its field 6 already points at a constant with the exact layout a NativeSharp
    // string has, so this is two loads and no allocation. A null reference answers a null string
    // rather than faulting: every string operation in this compiler is already null-tolerant (see
    // nullableStringLength), and a helper that traps would be worse than one that tells the truth.
    LLVMValueRef lowerObjectTypeName(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 1 || descriptorType_ == nullptr)
            return failValue(error, "C8 RuntimeHelpers.TypeName requires one reference operand");
        LLVMValueRef value = lowerExpression(node->children.front().get(), error);
        if (value == nullptr) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);

        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, value, LLVMConstNull(ptr), "typename.isnull");
        LLVMBasicBlockRef nullBlock = appendBlock("typename.null");
        LLVMBasicBlockRef valueBlock = appendBlock("typename.value");
        LLVMBasicBlockRef doneBlock = appendBlock("typename.done");
        LLVMBuildCondBr(builder_, isNull, nullBlock, valueBlock);

        LLVMPositionBuilderAtEnd(builder_, nullBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, value, "typename.descriptor");
        LLVMValueRef nameAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 6, "descriptor.type_name.addr");
        LLVMValueRef name = LLVMBuildLoad2(builder_, ptr, nameAddress, "descriptor.type_name");
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef valueIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, ptr, "typename.phi");
        LLVMValueRef incomingValues[] = {LLVMConstNull(ptr), name};
        LLVMBasicBlockRef incomingBlocks[] = {nullIncoming, valueIncoming};
        LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
        return phi;
    }

    // C8 "type root": `RuntimeHelpers.ReferenceHash(value)` -- the reference's own address, mixed
    // down to an int. The mix is the final avalanche of SplitMix64, chosen because it maps zero to
    // zero (so a null reference hashes to 0 without a branch) and because a raw address makes a
    // poor hash on its own: allocations are aligned, so the low bits a bucket index reads are
    // always the same. Not stable across runs -- ASLR sees to that -- exactly like real C#'s
    // RuntimeHelpers.GetHashCode.
    LLVMValueRef lowerObjectReferenceHash(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 1)
            return failValue(error, "C8 RuntimeHelpers.ReferenceHash requires one reference operand");
        LLVMValueRef value = lowerExpression(node->children.front().get(), error);
        if (value == nullptr) return nullptr;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMValueRef bits = LLVMBuildPtrToInt(builder_, value, i64, "refhash.bits");
        LLVMValueRef shifted = LLVMBuildLShr(
            builder_, bits, LLVMConstInt(i64, 33, 0), "refhash.shift0");
        LLVMValueRef mixed = LLVMBuildXor(builder_, bits, shifted, "refhash.mix0");
        mixed = LLVMBuildMul(
            builder_, mixed, LLVMConstInt(i64, 0xFF51AFD7ED558CCDULL, 0), "refhash.mul0");
        shifted = LLVMBuildLShr(builder_, mixed, LLVMConstInt(i64, 29, 0), "refhash.shift1");
        mixed = LLVMBuildXor(builder_, mixed, shifted, "refhash.mix1");
        return LLVMBuildTrunc(builder_, mixed, i32, "refhash");
    }

    // ID 129. `ToString()` de un enum -- el NOMBRE del enumerador.
    //
    // FORMA DEL NODO: children[0] el valor, children[1] la cadena de reserva (el nombre del tipo),
    // y de ahi en adelante PARES (constante, nombre) en orden de declaracion.
    //
    // SE BAJA A UN `switch` CON UN `phi`. La clave del switch es el PATRON DE BITS promovido sin
    // signo a i64, no el entero estrecho original:
    //
    //     %bits = zext i8 <valor> to i64
    //     switch i64 %bits, label %reserva [ i64 K1, label %n1  i64 K2, label %n2 ... ]
    //     %resultado = phi ptr [ S1, %n1 ], [ S2, %n2 ], ..., [ <reserva>, %reserva ]
    //
    // POR QUE NO ES UNA CADENA DE `select`, QUE ES LO QUE ERA HASTA EL 2026-09-04. Lo era, y la
    // cadena que emitia es CORRECTA: `icmp eq i8 %v, -3` seguido de `select` sobre punteros a
    // globales constantes. LLVM 18 la compila bien a -O2 --comprobado pasandole el IR de c38 por
    // `opt -O2`, que pliega la funcion entera hasta `ret i32 42`--. Con LLVM 23, que es contra el
    // que enlaza el compilador que se entrega, a -O2 devuelve la RESERVA.
    //
    // LO MEDIDO, y es lo unico sobre lo que se decidio (corrida del 2026-09-04):
    //
    //   * `c38` codigo 39 y `b9` codigo 61: `ConSigno.MenosTres.ToString()` --`enum : sbyte`,
    //     valor -3-- devuelve "ConSigno", el nombre del tipo. Con receptor CONSTANTE y con
    //     receptor opaco, o sea que no depende de que el valor cruce una llamada.
    //   * la capa 1 de `b9` pasa: el valor de 8 bits SIGUE siendo -3 al llegar a ToString().
    //   * `c38` codigo 26 PASA: `ClasificaConSigno`, que es un `switch` del lenguaje sobre el
    //     MISMO enum con el MISMO -3, acierta a -O2. En el IR eso es
    //     `switch i8 %v, [ i8 -3, ... ]`.
    //   * `c38` codigos 37, 38 y 50-53 pasan: un `enum : byte` con 200 y 128 --que en i8 son -56
    //     y -128-- acierta con la cadena de `select`, y los alias tambien.
    //
    // La primera correccion intento conservar `i8 -3` y cambiar solamente `select` por `switch`.
    // b9 demostro que no bastaba: LLVMRunPasses de LLVM 23 plegaba TAMBIEN ese switch al bloque de
    // reserva, aunque el IR impreso era correcto; el mismo IR serializado y pasado a opt.exe se
    // plegaba bien. Normalizar ambos lados al patron i64 elimina de la decision del optimizador la
    // unica forma conflictiva (una constante negativa estrecha) sin cambiar el valor observable
    // ni el ABI del enum. La igualdad de enums depende de sus bits, y esos bits son precisamente
    // los que ya se usan abajo para deduplicar aliases.
    //
    // LO QUE SE GANA DE PASO, y esto si es una mejora por si misma: EL ALIAS DEJA DE DEPENDER DEL
    // ORDEN DEL BUCLE. Un `switch` de LLVM no admite dos casos con el mismo valor --el
    // verificador lo rechaza-- asi que los pares se deduplican aqui y gana EL PRIMERO DECLARADO,
    // que es la regla que documenta el frontend y que fija la fixture c34 (`Solo = 1` declarado
    // despues de `Lectura = 1` sale "Lectura"). Antes esa regla se conseguia recorriendo los
    // pares al reves para que el primero quedara en el `select` de mas afuera: correcto, pero
    // invertible por accidente con un cambio de bucle -- y ya se invirtio una vez. Ahora esta
    // escrita como lo que es, una eleccion entre iguales, y no como un efecto del anidamiento.
    //
    // EL RECEPTOR SE SIGUE BAJANDO UNA SOLA VEZ, que es la propiedad por la que este nodo existe:
    // `Dame().ToString()` no puede llamar a `Dame()` una vez por enumerador.
    LLVMValueRef lowerEnumName(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() < 2U || (node->children.size() % 2U) != 0U)
        {
            return failValue(error, "ID 129 enum name node needs a value, a fallback and pairs");
        }
        LLVMValueRef value = lowerExpression(node->children[0].get(), error);
        if (value == nullptr) return nullptr;

        // EL ANCHO DEL TIPO QUE GOBIERNA, para deduplicar por el PATRON DE BITS y no por el numero
        // escrito. Es el mismo ancho con el que `IntegerConstant` construye cada constante
        // (LLVMConstInt trunca al ancho del tipo), asi que dos enumeradores que aqui salgan
        // iguales son exactamente los que LLVM veria como el mismo caso. Deduplicar por el valor
        // de 64 bits sin enmascarar dejaria pasar un par que colisiona al truncar, y eso no seria
        // un nombre equivocado: seria un `switch` con dos casos iguales, o sea IR invalido.
        const std::string& receiverTypeKey = node->children[0]->typeKey;
        if (llvmType(receiverTypeKey, error) == nullptr) return nullptr;
        const frontend::builtins::IntegerTypeInfo* receiverEnum =
            enumUnderlyingInfo(receiverTypeKey);
        const unsigned governingWidth =
            receiverEnum != nullptr ? receiverEnum->width : integerWidth(receiverTypeKey);
        if (governingWidth == 0 || governingWidth > 64)
        {
            return failValue(
                error, "ID 129 enum name receiver is not an integer carrier: " + receiverTypeKey);
        }
        const std::uint64_t mask = governingWidth >= 64
            ? ~std::uint64_t{0}
            : ((std::uint64_t{1} << governingWidth) - 1U);

        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef governingBits = value;
        if (governingWidth < 64)
        {
            governingBits = LLVMBuildZExt(builder_, value, i64, "enum.name.bits");
        }

        // LOS PARES QUE SOBREVIVEN A LA DEDUPLICACION, en orden de declaracion. Se guarda el
        // indice del par y no el valor bajado porque las constantes y los nombres se bajan mas
        // abajo, ya dentro del bloque que les toca.
        std::vector<std::pair<std::uint64_t, std::size_t>> cases;
        cases.reserve((node->children.size() - 2U) / 2U);
        for (std::size_t index = 2U; index + 1U < node->children.size(); index += 2U)
        {
            const std::uint64_t bits = node->children[index]->integerValue & mask;
            bool alias = false;
            for (const auto& existing : cases)
            {
                if (existing.first != bits) continue;
                alias = true;
                break;
            }
            if (!alias) cases.emplace_back(bits, index);
        }

        LLVMTypeRef stringType = LLVMPointerTypeInContext(context_, 0);
        LLVMBasicBlockRef fallbackBlock = appendBlock("enum.name.fallback");
        std::vector<LLVMBasicBlockRef> caseBlocks;
        caseBlocks.reserve(cases.size());
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            caseBlocks.push_back(appendBlock("enum.name.case"));
        }
        LLVMBasicBlockRef joinBlock = appendBlock("enum.name.join");

        LLVMValueRef instruction =
            LLVMBuildSwitch(
                builder_, governingBits, fallbackBlock, static_cast<unsigned>(cases.size()));
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            LLVMValueRef constant = LLVMConstInt(i64, cases[index].first, 0);
            LLVMAddCase(instruction, constant, caseBlocks[index]);
        }

        std::vector<LLVMValueRef> incomingValues;
        std::vector<LLVMBasicBlockRef> incomingBlocks;
        incomingValues.reserve(cases.size() + 1U);
        incomingBlocks.reserve(cases.size() + 1U);

        LLVMPositionBuilderAtEnd(builder_, fallbackBlock);
        LLVMValueRef fallback = lowerExpression(node->children[1].get(), error);
        if (fallback == nullptr) return nullptr;
        incomingValues.push_back(fallback);
        incomingBlocks.push_back(LLVMGetInsertBlock(builder_));
        LLVMBuildBr(builder_, joinBlock);

        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            LLVMPositionBuilderAtEnd(builder_, caseBlocks[index]);
            LLVMValueRef name =
                lowerExpression(node->children[cases[index].second + 1U].get(), error);
            if (name == nullptr) return nullptr;
            incomingValues.push_back(name);
            // El bloque que se anota es el que el constructor tiene ABIERTO, no `caseBlocks[i]`:
            // bajar el nombre podria haber abierto bloques nuevos. Hoy no lo hace --un literal de
            // cadena es un global, no una instruccion-- pero anotar el bloque de partida seria un
            // `phi` que nombra un predecesor que no lo es, y eso es IR invalido en cuanto deje de
            // ser cierto. Es el mismo cuidado que ya toma lowerStringEquals.
            incomingBlocks.push_back(LLVMGetInsertBlock(builder_));
            LLVMBuildBr(builder_, joinBlock);
        }

        LLVMPositionBuilderAtEnd(builder_, joinBlock);
        LLVMValueRef result = LLVMBuildPhi(builder_, stringType, "enum.name");
        LLVMAddIncoming(
            result,
            incomingValues.data(),
            incomingBlocks.data(),
            static_cast<unsigned>(incomingValues.size()));
        return result;
    }

    LLVMValueRef internStringLiteral(std::string_view value, std::string& error)
    {
        const auto found = stringGlobals_.find(std::string(value));
        if (found != stringGlobals_.end()) return found->second;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef bytesType = LLVMArrayType2(i8, value.size() + 1U);
        const std::string typeName = "__nsc_string_literal_type_" + std::to_string(stringSequence_);
        LLVMTypeRef literalType = LLVMStructCreateNamed(context_, typeName.c_str());
        LLVMTypeRef fields[] = {i64, bytesType};
        LLVMStructSetBody(literalType, fields, 2, 0);
        LLVMValueRef values[] = {
            LLVMConstInt(i64, static_cast<std::uint64_t>(value.size()), 0),
            LLVMConstStringInContext2(context_, value.data(), value.size(), 0)};
        LLVMValueRef initializer = LLVMConstNamedStruct(literalType, values, 2);
        const std::string name = "__nsc_str_" + std::to_string(stringSequence_++);
        LLVMValueRef global = LLVMAddGlobal(module_, literalType, name.c_str());
        if (global == nullptr || initializer == nullptr)
            return failValue(error, "Stage 41 failed to materialize native string literal");
        LLVMSetInitializer(global, initializer);
        LLVMSetGlobalConstant(global, 1);
        // A literal belongs to this LLVM module, not to the NativeSharp public
        // symbol contract. Positional external names such as __nsc_str_0 collide
        // as soon as an executable links a static library that also contains a
        // literal. Private linkage lets each COFF object retain its own interned
        // bytes without publishing a process-wide definition.
        LLVMSetLinkage(global, LLVMPrivateLinkage);
        stringGlobals_.emplace(value, global);
        return global;
    }

    LLVMValueRef lowerStringLiteral(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        return internStringLiteral(node->stringValue, error);
    }

    LLVMValueRef nullableStringLength(LLVMValueRef value, std::string_view stem)
    {
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, value, LLVMConstNull(ptr),
            (std::string(stem) + ".isnull").c_str());
        LLVMBasicBlockRef nullBlock = appendBlock(std::string(stem) + ".null");
        LLVMBasicBlockRef valueBlock = appendBlock(std::string(stem) + ".value");
        LLVMBasicBlockRef doneBlock = appendBlock(std::string(stem) + ".done");
        LLVMBuildCondBr(builder_, isNull, nullBlock, valueBlock);

        LLVMPositionBuilderAtEnd(builder_, nullBlock);
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        LLVMValueRef length = LLVMBuildLoad2(
            builder_, i64, value, (std::string(stem) + ".length").c_str());
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef valueIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, i64, (std::string(stem) + ".length.phi").c_str());
        LLVMValueRef incomingValues[] = {zero, length};
        LLVMBasicBlockRef incomingBlocks[] = {nullIncoming, valueIncoming};
        LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
        return phi;
    }

    LLVMValueRef dynamicByteOffset(
        LLVMValueRef base,
        LLVMValueRef offset,
        const char* name)
    {
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), base, indices, 1, name);
    }

    bool emitMemcpyBytes(
        LLVMValueRef destination,
        LLVMValueRef source,
        LLVMValueRef length,
        std::string_view stem,
        std::string& error)
    {
        if (destination == nullptr || source == nullptr || length == nullptr || memcpyFunction_ == nullptr)
            return fail(error, "Stage 51 cannot emit native string byte copy");
        LLVMValueRef hasBytes = LLVMBuildICmp(
            builder_, LLVMIntNE, length,
            LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0),
            (std::string(stem) + ".hasbytes").c_str());
        LLVMBasicBlockRef copyBlock = appendBlock(std::string(stem) + ".copy");
        LLVMBasicBlockRef doneBlock = appendBlock(std::string(stem) + ".done");
        LLVMBuildCondBr(builder_, hasBytes, copyBlock, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, copyBlock);
        LLVMValueRef args[] = {
            destination,
            source,
            length,
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memcpyFunctionType_, memcpyFunction_, args, 4, "");
        LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return true;
    }

    LLVMValueRef allocateNativeString(
        LLVMValueRef length,
        std::string_view stem,
        std::string& error)
    {
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMValueRef fits = LLVMBuildICmp(
            builder_, LLVMIntULE, length, LLVMConstInt(i64, 0x7fffffffULL, 0),
            (std::string(stem) + ".length.fits").c_str());
        if (!emitGuard(fits, LlvmTrapReason::AllocationSizeOverflow,
                std::string(stem) + ".length", error))
        {
            return nullptr;
        }
        LLVMValueRef bytes = LLVMBuildAdd(
            builder_, length, LLVMConstInt(i64, 9, 0),
            (std::string(stem) + ".bytes").c_str());
        LLVMValueRef args[] = {bytes};
        LLVMValueRef memory = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, args, 1,
            (std::string(stem) + ".memory").c_str());
        memory = checkedStage42Allocation(memory, std::string(stem) + ".alloc", error);
        if (memory == nullptr) return nullptr;
        LLVMBuildStore(builder_, length, memory);
        LLVMValueRef data = byteOffset(memory, 8, (std::string(stem) + ".data").c_str());
        LLVMValueRef terminator = dynamicByteOffset(
            data, length, (std::string(stem) + ".nul.addr").c_str());
        LLVMBuildStore(builder_, LLVMConstInt(i8, 0, 0), terminator);
        return memory;
    }

    LLVMValueRef lowerStringConcat(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 2)
            return failValue(error, "Stage 51 string concat requires two operands");
        LLVMValueRef left = lowerExpression(node->children[0].get(), error);
        LLVMValueRef right = lowerExpression(node->children[1].get(), error);
        if (left == nullptr || right == nullptr) return nullptr;
        LLVMValueRef leftLength = nullableStringLength(left, "string.concat.left");
        LLVMValueRef rightLength = nullableStringLength(right, "string.concat.right");
        LLVMValueRef total = LLVMBuildAdd(
            builder_, leftLength, rightLength, "string.concat.length");
        LLVMValueRef noOverflow = LLVMBuildICmp(
            builder_, LLVMIntUGE, total, leftLength, "string.concat.nooverflow");
        if (!emitGuard(noOverflow, LlvmTrapReason::AllocationSizeOverflow, "string.concat.overflow", error))
            return nullptr;
        LLVMValueRef result = allocateNativeString(total, "string.concat", error);
        if (result == nullptr) return nullptr;
        LLVMValueRef resultData = byteOffset(result, 8, "string.concat.result.data");
        LLVMValueRef leftData = byteOffset(left, 8, "string.concat.left.data");
        if (!emitMemcpyBytes(resultData, leftData, leftLength, "string.concat.copy.left", error))
            return nullptr;
        LLVMValueRef rightDestination = dynamicByteOffset(
            resultData, leftLength, "string.concat.right.dest");
        LLVMValueRef rightData = byteOffset(right, 8, "string.concat.right.data");
        if (!emitMemcpyBytes(rightDestination, rightData, rightLength, "string.concat.copy.right", error))
            return nullptr;
        return result;
    }

    LLVMValueRef lowerStringEquals(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 2)
            return failValue(error, "Stage 51 string equals requires two operands");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMValueRef left = lowerExpression(node->children[0].get(), error);
        LLVMValueRef right = lowerExpression(node->children[1].get(), error);
        if (left == nullptr || right == nullptr) return nullptr;

        LLVMValueRef samePointer = LLVMBuildICmp(builder_, LLVMIntEQ, left, right, "string.eq.sameptr");
        LLVMBasicBlockRef sameBlock = appendBlock("string.eq.same");
        LLVMBasicBlockRef distinctBlock = appendBlock("string.eq.distinct");
        LLVMBasicBlockRef doneBlock = appendBlock("string.eq.done");
        LLVMBuildCondBr(builder_, samePointer, sameBlock, distinctBlock);

        LLVMPositionBuilderAtEnd(builder_, sameBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef sameIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, distinctBlock);
        LLVMValueRef leftNull = LLVMBuildICmp(builder_, LLVMIntEQ, left, LLVMConstNull(ptr), "string.eq.leftnull");
        LLVMValueRef rightNull = LLVMBuildICmp(builder_, LLVMIntEQ, right, LLVMConstNull(ptr), "string.eq.rightnull");
        LLVMValueRef anyNull = LLVMBuildOr(builder_, leftNull, rightNull, "string.eq.anynull");
        LLVMBasicBlockRef nullBlock = appendBlock("string.eq.null");
        LLVMBasicBlockRef lengthBlock = appendBlock("string.eq.length");
        LLVMBuildCondBr(builder_, anyNull, nullBlock, lengthBlock);

        LLVMPositionBuilderAtEnd(builder_, nullBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, lengthBlock);
        LLVMValueRef leftLength = LLVMBuildLoad2(builder_, i64, left, "string.eq.left.length");
        LLVMValueRef rightLength = LLVMBuildLoad2(builder_, i64, right, "string.eq.right.length");
        LLVMValueRef sameLength = LLVMBuildICmp(builder_, LLVMIntEQ, leftLength, rightLength, "string.eq.samelen");
        LLVMBasicBlockRef mismatchLengthBlock = appendBlock("string.eq.length.mismatch");
        LLVMBasicBlockRef loopEntryBlock = appendBlock("string.eq.loop.entry");
        LLVMBuildCondBr(builder_, sameLength, loopEntryBlock, mismatchLengthBlock);

        LLVMPositionBuilderAtEnd(builder_, mismatchLengthBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef lengthMismatchIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, loopEntryBlock);
        LLVMBasicBlockRef loopBlock = appendBlock("string.eq.loop");
        LLVMBasicBlockRef compareBlock = appendBlock("string.eq.compare");
        LLVMBasicBlockRef matchBlock = appendBlock("string.eq.match");
        LLVMBasicBlockRef mismatchByteBlock = appendBlock("string.eq.byte.mismatch");
        LLVMBasicBlockRef equalBlock = appendBlock("string.eq.equal");
        LLVMBuildBr(builder_, loopBlock);
        LLVMBasicBlockRef loopEntryIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, loopBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "string.eq.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {loopEntryIncoming};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef finished = LLVMBuildICmp(builder_, LLVMIntEQ, index, leftLength, "string.eq.finished");
        LLVMBuildCondBr(builder_, finished, equalBlock, compareBlock);

        LLVMPositionBuilderAtEnd(builder_, compareBlock);
        LLVMValueRef leftData = byteOffset(left, 8, "string.eq.left.data");
        LLVMValueRef rightData = byteOffset(right, 8, "string.eq.right.data");
        LLVMValueRef leftAddress = dynamicByteOffset(leftData, index, "string.eq.left.addr");
        LLVMValueRef rightAddress = dynamicByteOffset(rightData, index, "string.eq.right.addr");
        LLVMValueRef leftByte = LLVMBuildLoad2(builder_, i8, leftAddress, "string.eq.left.byte");
        LLVMValueRef rightByte = LLVMBuildLoad2(builder_, i8, rightAddress, "string.eq.right.byte");
        LLVMValueRef byteEqual = LLVMBuildICmp(builder_, LLVMIntEQ, leftByte, rightByte, "string.eq.byte.equal");
        LLVMBuildCondBr(builder_, byteEqual, matchBlock, mismatchByteBlock);

        LLVMPositionBuilderAtEnd(builder_, mismatchByteBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef byteMismatchIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, matchBlock);
        LLVMValueRef next = LLVMBuildAdd(builder_, index, LLVMConstInt(i64, 1, 0), "string.eq.next");
        LLVMBuildBr(builder_, loopBlock);
        LLVMBasicBlockRef matchIncoming = LLVMGetInsertBlock(builder_);
        LLVMValueRef nextValues[] = {next};
        LLVMBasicBlockRef nextBlocks[] = {matchIncoming};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        LLVMPositionBuilderAtEnd(builder_, equalBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef equalIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        LLVMValueRef result = LLVMBuildPhi(builder_, LLVMInt1TypeInContext(context_), "string.eq.result");
        LLVMValueRef trueValue = LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0);
        LLVMValueRef falseValue = LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        LLVMValueRef values[] = {trueValue, falseValue, falseValue, falseValue, trueValue};
        LLVMBasicBlockRef blocks[] = {
            sameIncoming, nullIncoming, lengthMismatchIncoming, byteMismatchIncoming, equalIncoming};
        LLVMAddIncoming(result, values, blocks, 5);
        if (node->operation == lexer::TokenKind::BangEqual)
            return LLVMBuildNot(builder_, result, "string.ne.result");
        return result;
    }

    LLVMValueRef lowerStringSubstring(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 3)
            return failValue(error, "Stage 51 Substring requires value, startIndex and length");
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef value = lowerExpression(node->children[0].get(), error);
        if (value == nullptr) return nullptr;
        value = checkedReceiver(value, error);
        if (value == nullptr) return nullptr;
        LLVMValueRef start = normalizeNativeIndex(node->children[1].get(), error);
        LLVMValueRef count = normalizeNativeIndex(node->children[2].get(), error);
        if (start == nullptr || count == nullptr) return nullptr;
        if (isSignedInteger(node->children[1]->typeKey))
        {
            LLVMValueRef valid = LLVMBuildICmp(builder_, LLVMIntSGE, start, LLVMConstInt(i64, 0, 0), "string.substring.start.nonnegative");
            if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "string.substring.start", error)) return nullptr;
        }
        if (isSignedInteger(node->children[2]->typeKey))
        {
            LLVMValueRef valid = LLVMBuildICmp(builder_, LLVMIntSGE, count, LLVMConstInt(i64, 0, 0), "string.substring.count.nonnegative");
            if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "string.substring.count", error)) return nullptr;
        }
        LLVMValueRef sourceLength = LLVMBuildLoad2(builder_, i64, value, "string.substring.source.length");
        LLVMValueRef startFits = LLVMBuildICmp(builder_, LLVMIntULE, start, sourceLength, "string.substring.start.fits");
        if (!emitGuard(startFits, LlvmTrapReason::BoundsViolation, "string.substring.start.bounds", error)) return nullptr;
        LLVMValueRef remaining = LLVMBuildSub(builder_, sourceLength, start, "string.substring.remaining");
        LLVMValueRef countFits = LLVMBuildICmp(builder_, LLVMIntULE, count, remaining, "string.substring.count.fits");
        if (!emitGuard(countFits, LlvmTrapReason::BoundsViolation, "string.substring.count.bounds", error)) return nullptr;
        LLVMValueRef result = allocateNativeString(count, "string.substring", error);
        if (result == nullptr) return nullptr;
        LLVMValueRef sourceData = byteOffset(value, 8, "string.substring.source.data");
        sourceData = dynamicByteOffset(sourceData, start, "string.substring.source.start");
        LLVMValueRef resultData = byteOffset(result, 8, "string.substring.result.data");
        if (!emitMemcpyBytes(resultData, sourceData, count, "string.substring.copy", error)) return nullptr;
        return result;
    }

    LLVMValueRef lowerStringFromCharArray(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 1)
            return failValue(error, "Stage 51 FromCharArray requires one char[] operand");
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef array = lowerExpression(node->children.front().get(), error);
        if (array == nullptr) return nullptr;
        array = checkedReceiver(array, error);
        if (array == nullptr) return nullptr;
        LLVMValueRef length = LLVMBuildLoad2(builder_, i64, array, "string.fromchars.length");
        LLVMValueRef result = allocateNativeString(length, "string.fromchars", error);
        if (result == nullptr) return nullptr;
        LLVMValueRef sourceData = byteOffset(array, 8, "string.fromchars.source.data");
        LLVMValueRef resultData = byteOffset(result, 8, "string.fromchars.result.data");
        if (!emitMemcpyBytes(resultData, sourceData, length, "string.fromchars.copy", error)) return nullptr;
        return result;
    }

    LLVMValueRef normalizeNativeIndex(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        LLVMValueRef value = lowerExpression(node, error);
        if (value == nullptr) return nullptr;
        return convert(value, node->typeKey, "builtin:nuint", error);
    }

    LLVMValueRef lowerNativeLength(
        const frontend::ProjectOopMirNode* node,
        bool stringValue,
        std::string& error)
    {
        (void)stringValue;
        if (node->children.size() != 1)
            return failValue(error, "Stage 41 native Length requires one receiver");
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;
        object = checkedReceiver(object, error);
        if (object == nullptr) return nullptr;
        LLVMValueRef length64 = LLVMBuildLoad2(builder_, LLVMInt64TypeInContext(context_), object, "native.length64");
        return LLVMBuildTrunc(builder_, length64, LLVMInt32TypeInContext(context_), "native.length");
    }

    LLVMValueRef lowerArrayElementAddress(
        const frontend::ProjectOopMirNode* node,
        bool stringValue,
        std::string& error)
    {
        if (node->children.size() != 2)
            return failValue(error, "Stage 41 array/string index requires receiver and index");
        LLVMValueRef object = lowerExpression(node->children[0].get(), error);
        if (object == nullptr) return nullptr;
        object = checkedReceiver(object, error);
        if (object == nullptr) return nullptr;
        LLVMValueRef index = normalizeNativeIndex(node->children[1].get(), error);
        if (index == nullptr) return nullptr;
        LLVMValueRef length = LLVMBuildLoad2(builder_, LLVMInt64TypeInContext(context_), object, "native.index.length");
        LLVMValueRef inRange = LLVMBuildICmp(builder_, LLVMIntULT, index, length, "native.index.inrange");
        if (isSignedInteger(node->children[1]->typeKey))
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            LLVMValueRef nonNegative = LLVMBuildICmp(builder_, LLVMIntSGE, index, zero, "native.index.nonnegative");
            inRange = LLVMBuildAnd(builder_, nonNegative, inRange, "native.index.valid");
        }
        if (!emitGuard(inRange, LlvmTrapReason::BoundsViolation, "native.index", error)) return nullptr;
        const std::uint64_t elementSize = stringValue ? 1U : node->immediate;
        const std::uint64_t elementAlignment = stringValue ? 1U : node->alignment;
        const std::uint64_t dataOffset = (8U + elementAlignment - 1U) / elementAlignment * elementAlignment;
        LLVMValueRef scaled = LLVMBuildMul(
            builder_, index, LLVMConstInt(LLVMInt64TypeInContext(context_), elementSize, 0), "native.index.scaled");
        LLVMValueRef offset = LLVMBuildAdd(
            builder_, scaled, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0), "native.index.offset");
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(builder_, LLVMInt8TypeInContext(context_), object, indices, 1, "native.element.addr");
    }

    LLVMValueRef lowerArrayAllocate(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.empty() || node->immediate == 0 || node->alignment == 0)
            return failValue(error, "Stage 41 array allocation has invalid MIR layout/count");
        if (!node->booleanValue && node->children.size() != 1)
            return failValue(error, "Stage 41 counted array allocation cannot carry initializer elements");
        LLVMValueRef count = normalizeNativeIndex(node->children.front().get(), error);
        if (count == nullptr) return nullptr;
        if (isSignedInteger(node->children.front()->typeKey))
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            LLVMValueRef nonNegative = LLVMBuildICmp(builder_, LLVMIntSGE, count, zero, "array.count.nonnegative");
            if (!emitGuard(nonNegative, LlvmTrapReason::AllocationSizeOverflow, "array.count", error)) return nullptr;
        }
        const std::uint64_t dataOffset = (8U + node->alignment - 1U) / node->alignment * node->alignment;
        const std::uint64_t storageMaxCount = (UINT64_MAX - dataOffset) / node->immediate;
        const std::uint64_t maxCount = std::min<std::uint64_t>(storageMaxCount, 0x7fffffffULL);
        LLVMValueRef fits = LLVMBuildICmp(
            builder_, LLVMIntULE, count,
            LLVMConstInt(LLVMInt64TypeInContext(context_), maxCount, 0), "array.count.fits");
        if (!emitGuard(fits, LlvmTrapReason::AllocationSizeOverflow, "array.size", error)) return nullptr;
        LLVMValueRef bytes = LLVMBuildMul(
            builder_, count, LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0), "array.data.bytes");
        LLVMValueRef total = LLVMBuildAdd(
            builder_, bytes, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0), "array.total.bytes");
        LLVMValueRef args[] = {total};
        LLVMValueRef memory = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, args, 1, "array.memory");
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, memory, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), "array.alloc.null");
        LLVMBasicBlockRef nullBlock = appendBlock("array.alloc.null");
        LLVMBasicBlockRef initBlock = appendBlock("array.alloc.init");
        LLVMBasicBlockRef mergeBlock = appendBlock("array.alloc.merge");
        LLVMBuildCondBr(builder_, isNull, nullBlock, initBlock);
        LLVMPositionBuilderAtEnd(builder_, nullBlock);
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);
        LLVMPositionBuilderAtEnd(builder_, initBlock);
        LLVMValueRef memsetArgs[] = {
            memory,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            total,
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArgs, 4, "");
        LLVMBuildStore(builder_, count, memory);
        // NSC-LIMIT-017. The first child is the inferred count; the remaining children are
        // already converted element values. Initialize directly in the successful-allocation
        // block, in source order, so every expression is evaluated once and a null allocation is
        // never dereferenced.
        LLVMTypeRef elementType = llvmType(node->elementTypeKey, error);
        if (elementType == nullptr) return nullptr;
        std::vector<LLVMValueRef> sourceArguments;
        std::vector<LLVMTypeRef> sourceParameterTypes;
        sourceArguments.reserve(node->children.size() - 1);
        sourceParameterTypes.reserve(node->children.size() - 1);
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            const std::uint64_t bytePosition = dataOffset + (index - 1U) * node->immediate;
            LLVMValueRef address = byteOffset(memory, bytePosition, "array.init.element.addr");
            LLVMBuildStore(builder_, value, address);
        }
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef initIncoming = LLVMGetInsertBlock(builder_);
        LLVMPositionBuilderAtEnd(builder_, mergeBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, LLVMPointerTypeInContext(context_, 0), "array.result");
        LLVMValueRef incomingValues[] = {LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), memory};
        LLVMBasicBlockRef incomingBlocks[] = {nullIncoming, initIncoming};
        LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
        return phi;
    }

    LLVMValueRef emitRuntimeTypeTest(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        std::string& error)
    {
        if (rttiTestFunctionType_ == nullptr || rttiClassTestFunction_ == nullptr ||
            rttiInterfaceTestFunction_ == nullptr)
        {
            return failValue(error, "Stage 39 RTTI helper functions are unavailable");
        }
        LLVMValueRef targetId = LLVMConstInt(
            LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef arguments[] = {object, targetId};
        LLVMValueRef function = node->targetIsInterface
            ? rttiInterfaceTestFunction_
            : rttiClassTestFunction_;
        return buildNoUnwindCall(
            rttiTestFunctionType_, function, arguments, 2,
            node->targetIsInterface ? "is.interface" : "is.class");
    }

    LLVMValueRef lowerRuntimeTypeTest(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 1)
        {
            return failValue(error, "Stage 39 runtime-type-test MIR requires one operand");
        }
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        return object != nullptr ? emitRuntimeTypeTest(node, object, error) : nullptr;
    }

    LLVMValueRef lowerSafeReferenceCast(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 1)
        {
            return failValue(error, "Stage 39 safe-reference-cast MIR requires one operand");
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;
        LLVMValueRef compatible = emitRuntimeTypeTest(node, object, error);
        if (compatible == nullptr) return nullptr;

        LLVMBasicBlockRef successBlock = appendBlock("cast.safe.success");
        LLVMBasicBlockRef failureBlock = appendBlock("cast.safe.failure");
        LLVMBasicBlockRef doneBlock = appendBlock("cast.safe.done");
        LLVMBuildCondBr(builder_, compatible, successBlock, failureBlock);

        LLVMPositionBuilderAtEnd(builder_, successBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, failureBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        LLVMValueRef result = LLVMBuildPhi(builder_, ptr, "cast.safe.result");
        LLVMValueRef incomingValues[] = {object, LLVMConstNull(ptr)};
        LLVMBasicBlockRef incomingBlocks[] = {successBlock, failureBlock};
        LLVMAddIncoming(result, incomingValues, incomingBlocks, 2);
        return result;
    }

    LLVMValueRef lowerCheckedReferenceCast(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 1 || trapFunction_ == nullptr || trapFunctionType_ == nullptr)
        {
            return failValue(error, "Stage 39 checked-reference-cast MIR is incomplete");
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;

        LLVMBasicBlockRef testBlock = appendBlock("cast.checked.test");
        LLVMBasicBlockRef okBlock = appendBlock("cast.checked.ok");
        LLVMBasicBlockRef trapBlock = appendBlock("cast.checked.invalid");
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, object, LLVMConstNull(ptr), "cast.checked.null");
        LLVMBuildCondBr(builder_, isNull, okBlock, testBlock);

        LLVMPositionBuilderAtEnd(builder_, testBlock);
        LLVMValueRef compatible = emitRuntimeTypeTest(node, object, error);
        if (compatible == nullptr) return nullptr;
        LLVMBuildCondBr(builder_, compatible, okBlock, trapBlock);

        LLVMPositionBuilderAtEnd(builder_, trapBlock);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::InvalidCast),
            0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        LLVMPositionBuilderAtEnd(builder_, okBlock);
        return object;
    }

    LLVMValueRef lowerUnary(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 1) return failValue(error, "unary MIR requires one operand");
        LLVMValueRef value = lowerExpression(node->children.front().get(), error);
        if (value == nullptr) return nullptr;
        if (node->operation == lexer::TokenKind::Plus) return value;
        if (node->operation == lexer::TokenKind::Bang) return LLVMBuildNot(builder_, value, "not");
        if (node->operation == lexer::TokenKind::Tilde) return LLVMBuildNot(builder_, value, "bitnot");
        if (node->operation == lexer::TokenKind::Minus)
            return isFloating(node->typeKey) ? LLVMBuildFNeg(builder_, value, "fneg") : LLVMBuildNeg(builder_, value, "neg");
        return failValue(error, "unsupported unary operation in Stage 34 LLVM lowering");
    }

    LLVMValueRef lowerBinary(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 2) return failValue(error, "binary MIR requires two operands");
        if (node->operation == lexer::TokenKind::AmpAmp || node->operation == lexer::TokenKind::PipePipe)
            return lowerShortCircuit(node, error);
        LLVMValueRef left = lowerExpression(node->children[0].get(), error);
        LLVMValueRef right = left != nullptr ? lowerExpression(node->children[1].get(), error) : nullptr;
        if (left == nullptr || right == nullptr) return nullptr;
        // NSC-LUNAR-007. LLVM requires both operands of shl/lshr/ashr to have exactly the same
        // integer type. NativeSharp keeps the counter's semantic type, then validates and converts
        // it immediately before emission. Thus `ulong << int` is valid without producing
        // `shl i64 ..., i32 ...` or exposing a promotion in the language.
        if (node->operation == lexer::TokenKind::LessLess ||
            node->operation == lexer::TokenKind::GreaterGreater)
        {
            right = normalizeShiftCount(
                right, node->children[1]->typeKey, node->children[0]->typeKey, error);
            if (right == nullptr) return nullptr;
        }
        return applyBinaryOperation(node->operation, left, right, node->children[0]->typeKey, error);
    }

    LLVMValueRef normalizeShiftCount(
        LLVMValueRef count,
        const std::string& countTypeKey,
        const std::string& valueTypeKey,
        std::string& error)
    {
        const unsigned valueWidth = integerWidth(valueTypeKey);
        LLVMTypeRef countType = llvmType(countTypeKey, error);
        if (valueWidth == 0 || countType == nullptr)
        {
            return failValue(error, "shift operands must have concrete integer widths");
        }

        LLVMValueRef belowWidth = LLVMBuildICmp(
            builder_,
            LLVMIntULT,
            count,
            LLVMConstInt(countType, valueWidth, 0),
            "shift.below_width");
        LLVMValueRef validCount = belowWidth;
        if (isSignedInteger(countTypeKey))
        {
            LLVMValueRef nonnegative = LLVMBuildICmp(
                builder_,
                LLVMIntSGE,
                count,
                LLVMConstInt(countType, 0, 0),
                "shift.nonnegative");
            validCount = LLVMBuildAnd(builder_, nonnegative, belowWidth, "shift.valid");
        }
        if (!emitGuard(validCount, LlvmTrapReason::InvalidShiftCount, "shift.range", error))
        {
            return nullptr;
        }
        return convert(count, countTypeKey, valueTypeKey, error);
    }

    // IDs 79/81. La aritmetica de DOS VALORES YA BAJADOS, sacada de lowerBinary para que la
    // asignacion compuesta y `++`/`--` la reusen en vez de traer una segunda copia de las mismas
    // reglas. Las reglas que estan aqui y en ningun otro sitio: coma flotante contra entero,
    // division y resto con signo contra sin signo, y desplazamiento a la derecha aritmetico
    // contra logico. Duplicarlas es exactamente el defecto que el milestone CC1 existe para
    // quitar -- una regla del lenguaje definida por dos capas que pueden discrepar.
    LLVMValueRef applyBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error)
    {
        // NSC-LIMIT-004. DEFENSA, no arreglo. Un agregado no puede llegar aqui: `icmp eq [24 x i8]`
        // no es una instruccion legal y el verificador rechaza el modulo entero, con un mensaje
        // que habla de vectores y no menciona structs por ningun lado. La correccion esta en el
        // frontend --`==` sobre structs baja a AggregateEquals-- y esto es la red por si alguna
        // vez vuelve a colarse: dice el nombre del tipo y de donde viene el problema.
        if (isEnumTypeKey(operandType) == false && operandType.starts_with("type:") &&
            structLayout(operandType) != nullptr)
        {
            return failValue(
                error,
                "aggregate comparison reached scalar binary lowering for '" + operandType +
                    "': struct equality must lower through AggregateEquals");
        }
        const bool floating = isFloating(operandType);
        if (isComparison(operation))
        {
            return floating
                ? LLVMBuildFCmp(builder_, floatingPredicate(operation), left, right, "fcmp")
                : LLVMBuildICmp(builder_, integerPredicate(operation, isSignedInteger(operandType)), left, right, "icmp");
        }
        if (floating)
        {
            switch (operation)
            {
                case lexer::TokenKind::Plus: return LLVMBuildFAdd(builder_, left, right, "fadd");
                case lexer::TokenKind::Minus: return LLVMBuildFSub(builder_, left, right, "fsub");
                case lexer::TokenKind::Star: return LLVMBuildFMul(builder_, left, right, "fmul");
                case lexer::TokenKind::Slash: return LLVMBuildFDiv(builder_, left, right, "fdiv");
                case lexer::TokenKind::Percent: return LLVMBuildFRem(builder_, left, right, "frem");
                default: return failValue(error, "unsupported floating binary operation in Stage 34");
            }
        }
        switch (operation)
        {
            case lexer::TokenKind::Plus: return LLVMBuildAdd(builder_, left, right, "add");
            case lexer::TokenKind::Minus: return LLVMBuildSub(builder_, left, right, "sub");
            case lexer::TokenKind::Star: return LLVMBuildMul(builder_, left, right, "mul");
            case lexer::TokenKind::Slash:
            case lexer::TokenKind::Percent:
                return applyIntegerDivision(operation, left, right, operandType, error);
            case lexer::TokenKind::Amp: return LLVMBuildAnd(builder_, left, right, "and");
            case lexer::TokenKind::Pipe: return LLVMBuildOr(builder_, left, right, "or");
            case lexer::TokenKind::Caret: return LLVMBuildXor(builder_, left, right, "xor");
            case lexer::TokenKind::LessLess: return LLVMBuildShl(builder_, left, right, "shl");
            case lexer::TokenKind::GreaterGreater:
                return isSignedInteger(operandType) ? LLVMBuildAShr(builder_, left, right, "ashr") : LLVMBuildLShr(builder_, left, right, "lshr");
            default: return failValue(error, "unsupported integer binary operation in Stage 34");
        }
    }

    // LA DIVISION ENTERA, CON SUS DOS CASOS DEGENERADOS. Sacada de applyBinaryOperation porque
    // ya no cabe en una linea, y porque los dos casos son decisiones del contrato (doc 110 §2.5)
    // y no detalles de emision:
    //
    //   * DIVISION POR CERO -> trampa explicita. Antes se dejaba al hardware, y eso NO cumple el
    //     contrato: en -O0 el `idiv` falla con 0xC0000094, pero en -O2 `sdiv x, 0` es POISON en
    //     el IR y el optimizador pliega la funcion entera a lo que le convenga. El contrato dice
    //     "trampa en -O0 y en -O2", asi que la trampa tiene que estar EN EL IR. Ademas ahora
    //     llega con su razon (LlvmTrapReason::DivisionByZero) en vez de como fallo de hardware.
    //
    //   * MinValue / -1 -> ENVUELVE, dividiendo por 1. La regla vive en
    //     src/LLVM/SignedDivisionCorrection.h, compartida con el bajador de fichero unico, y
    //     alli esta escrito el por que con la medicion de la fixture c30.
    //
    // Lo sin signo no necesita nada mas que la guarda del cero: `udiv`/`urem` no tienen caso
    // degenerado de desbordamiento.
    LLVMValueRef applyIntegerDivision(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error)
    {
        LLVMTypeRef integerType = llvmType(operandType, error);
        if (integerType == nullptr) return nullptr;

        const bool remainder = operation == lexer::TokenKind::Percent;
        LLVMValueRef zero = LLVMConstInt(integerType, 0, 0);
        LLVMValueRef nonZero = LLVMBuildICmp(builder_, LLVMIntNE, right, zero, "div.nonzero");
        if (!emitGuard(nonZero, LlvmTrapReason::DivisionByZero, "div.zero", error))
        {
            return nullptr;
        }

        if (!isSignedInteger(operandType))
        {
            return remainder
                ? LLVMBuildURem(builder_, left, right, "urem")
                : LLVMBuildUDiv(builder_, left, right, "udiv");
        }

        LLVMValueRef divisor = correctedSignedDivisor(
            builder_, integerType, left, right, integerWidth(operandType), "div");
        return remainder
            ? LLVMBuildSRem(builder_, left, divisor, "srem")
            : LLVMBuildSDiv(builder_, left, divisor, "sdiv");
    }

    // `c ? a : b`. Both branches are LAZILY evaluated -- only the taken one runs -- so this is
    // control flow plus a phi, not an LLVM `select`, which would evaluate both. That matters for
    // correctness and not just for speed: `p != null ? p.Value : 0` must not read through a null
    // pointer to build the value it then discards. Same shape as the `&&`/`||` lowering below.
    //
    // The two branches were already given a single result type by the front end, so the incoming
    // values agree and the phi needs no conversion of its own.
    LLVMValueRef lowerConditional(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 3)
            return failValue(error, "conditional expression MIR is incomplete");
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return nullptr;
        LLVMBasicBlockRef trueBlock = appendBlock("cond.true");
        LLVMBasicBlockRef falseBlock = appendBlock("cond.false");
        LLVMBasicBlockRef endBlock = appendBlock("cond.end");
        LLVMBuildCondBr(builder_, condition, trueBlock, falseBlock);

        LLVMPositionBuilderAtEnd(builder_, trueBlock);
        LLVMValueRef whenTrue = lowerExpression(node->children[1].get(), error);
        if (whenTrue == nullptr) return nullptr;
        LLVMBasicBlockRef trueExit = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, falseBlock);
        LLVMValueRef whenFalse = lowerExpression(node->children[2].get(), error);
        if (whenFalse == nullptr) return nullptr;
        LLVMBasicBlockRef falseExit = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return nullptr;
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "cond");
        LLVMValueRef values[] = {whenTrue, whenFalse};
        LLVMBasicBlockRef blocks[] = {trueExit, falseExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

    LLVMValueRef lowerShortCircuit(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        LLVMValueRef left = lowerExpression(node->children[0].get(), error);
        if (left == nullptr) return nullptr;
        LLVMBasicBlockRef leftBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef rhsBlock = appendBlock("logic.rhs");
        LLVMBasicBlockRef endBlock = appendBlock("logic.end");
        const bool isAnd = node->operation == lexer::TokenKind::AmpAmp;
        LLVMBuildCondBr(builder_, left, isAnd ? rhsBlock : endBlock, isAnd ? endBlock : rhsBlock);
        LLVMPositionBuilderAtEnd(builder_, rhsBlock);
        LLVMValueRef right = lowerExpression(node->children[1].get(), error);
        if (right == nullptr) return nullptr;
        LLVMBasicBlockRef rightBlock = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, LLVMInt1TypeInContext(context_), "logic");
        LLVMValueRef shortValue = LLVMConstInt(LLVMInt1TypeInContext(context_), isAnd ? 0U : 1U, 0);
        LLVMValueRef values[] = {shortValue, right};
        LLVMBasicBlockRef blocks[] = {leftBlock, rightBlock};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

    bool callArgumentOrder(
        const frontend::ProjectOopMirNode* node,
        std::size_t sourceCount,
        std::vector<std::size_t>& parameterToSource,
        std::string& error)
    {
        parameterToSource.resize(sourceCount);
        if (node->argumentParameterOrdinals.empty())
        {
            for (std::size_t index = 0; index < sourceCount; ++index) parameterToSource[index] = index;
            return true;
        }
        if (node->argumentParameterOrdinals.size() != sourceCount)
        {
            return fail(error, "named call argument map does not match the evaluated argument count");
        }
        std::vector<bool> assigned(sourceCount, false);
        for (std::size_t source = 0; source < sourceCount; ++source)
        {
            const std::size_t parameter = node->argumentParameterOrdinals[source];
            if (parameter >= sourceCount || assigned[parameter])
            {
                return fail(error, "named call argument map is not a parameter permutation");
            }
            assigned[parameter] = true;
            parameterToSource[parameter] = source;
        }
        return true;
    }

    LLVMValueRef lowerCall(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
        {
            return failValue(error, "Macro A call target was not declared: " + node->calleeName);
        }
        if (node->children.empty())
        {
            return failValue(error, "Macro A instance call is missing its explicit receiver operand");
        }

        LLVMValueRef receiver = lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        const bool structReceiver = node->children.front() != nullptr &&
            isStructValue(node->children.front()->typeKey);
        if (!structReceiver)
        {
            receiver = checkedReceiver(receiver, error);
            if (receiver == nullptr)
            {
                return nullptr;
            }
        }

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->children.size() + 1);
        arguments.push_back(receiver);
        // Win64 aggregate ABI: an indirect struct return travels through an sret slot placed
        // right after the receiver; indirect struct arguments travel as pointers to per-call
        // copies. Mirrors the signature built in declareFunctions -- and clang's own output.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        std::vector<LLVMValueRef> sourceArguments;
        sourceArguments.reserve(node->children.size() - 1);
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr)
            {
                return nullptr;
            }
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            sourceArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource) arguments.push_back(sourceArguments[source]);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, found->second.function,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "call", error);
        if (call == nullptr) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        return call;
    }

    bool ensureTypeInitialized(std::size_t typeOrdinal, std::string& error)
    {
        const auto found = staticInitFunctions_.find(typeOrdinal);
        if (found == staticInitFunctions_.end())
        {
            return true;
        }
        if (currentFunction_ == nullptr)
        {
            return fail(error, "Stage 37 cannot emit a type-initialization guard without an active function");
        }
        if (buildPotentiallyThrowingCall(
                found->second.functionType, found->second.function,
                nullptr, 0, "", error) == nullptr && !error.empty())
        {
            return false;
        }
        return true;
    }

    LLVMValueRef staticFieldAddress(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node == nullptr || !node->memberOrdinal.has_value())
        {
            return failValue(error, "Stage 37 static field MIR is missing its member ordinal");
        }
        const auto globalFound = staticFieldGlobals_.find(*node->memberOrdinal);
        const auto infoFound = staticFieldInfo_.find(*node->memberOrdinal);
        if (infoFound == staticFieldInfo_.end() || infoFound->second == nullptr)
        {
            return failValue(error, "Stage 37 static field has no declared MIR information");
        }
        const frontend::ProjectOopMirStaticField& field = *infoFound->second;
        if (field.isImported && !field.isConst)
        {
            const auto accessorFound = staticFieldAccessorFunctions_.find(*node->memberOrdinal);
            if (accessorFound == staticFieldAccessorFunctions_.end())
                return failValue(error, "Stage 37 imported static field has no declared accessor");
            LLVMValueRef address = buildPotentiallyThrowingCall(
                accessorFound->second.functionType, accessorFound->second.function,
                nullptr, 0, "static.address", error);
            if (address == nullptr && !error.empty()) return nullptr;
            return address;
        }
        if (globalFound == staticFieldGlobals_.end())
            return failValue(error, "Stage 37 static field has no declared LLVM global");
        if (!field.isConst && !ensureTypeInitialized(field.declaringTypeOrdinal, error))
        {
            return nullptr;
        }
        return globalFound->second;
    }

    LLVMValueRef lowerStaticCall(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
        {
            return failValue(error, "Stage 37 static call target was not declared: " + node->calleeName);
        }

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->children.size() + 1);
        // Win64 aggregate ABI -- see lowerCall; for a static callee the sret slot leads.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        std::vector<LLVMValueRef> sourceArguments;
        sourceArguments.reserve(node->children.size());
        for (const auto& child : node->children)
        {
            LLVMValueRef value = lowerExpression(child.get(), error);
            if (value == nullptr)
            {
                return nullptr;
            }
            value = wrapIndirectArgument(value, child->typeKey, error);
            if (value == nullptr) return nullptr;
            sourceArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource) arguments.push_back(sourceArguments[source]);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, found->second.function,
            arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "static.call", error);
        if (call == nullptr) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                0U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        return call;
    }

    LLVMValueRef lowerVirtualCall(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (!node->virtualSlot.has_value())
        {
            return failValue(error, "Macro A virtual call is missing its vtable slot");
        }
        if (node->children.empty())
        {
            return failValue(error, "Macro A virtual call is missing its receiver operand");
        }
        if (descriptorType_ == nullptr)
        {
            return failValue(error, "Macro A virtual call has no class-descriptor LLVM type");
        }

        LLVMValueRef receiver = lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr)
        {
            return nullptr;
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "class.descriptor");
        LLVMValueRef vtableAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 1, "vtable.addr");
        LLVMValueRef vtable = LLVMBuildLoad2(builder_, ptr, vtableAddress, "vtable");
        LLVMValueRef slotIndex = LLVMConstInt(
            LLVMInt64TypeInContext(context_), *node->virtualSlot, 0);
        LLVMValueRef indices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, vtable, indices, 1, "vslot.addr");
        LLVMValueRef target = LLVMBuildLoad2(builder_, ptr, slotAddress, "vtarget");

        std::vector<LLVMValueRef> arguments;
        std::vector<LLVMTypeRef> parameterTypes;
        arguments.reserve(node->children.size() + 1);
        parameterTypes.reserve(node->children.size() + 1);
        arguments.push_back(receiver);
        parameterTypes.push_back(ptr);
        // Win64 aggregate ABI -- the reconstructed callee type must match what
        // declareFunctions gave every override: sret slot after the receiver, pointer-typed
        // indirect struct parameters.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
            parameterTypes.push_back(ptr);
        }
        std::vector<LLVMValueRef> sourceArguments;
        std::vector<LLVMTypeRef> sourceParameterTypes;
        sourceArguments.reserve(node->children.size() - 1);
        sourceParameterTypes.reserve(node->children.size() - 1);
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            LLVMTypeRef parameterType = abiParameterType(node->children[index]->typeKey, error);
            if (value == nullptr || parameterType == nullptr)
            {
                return nullptr;
            }
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            sourceArguments.push_back(value);
            sourceParameterTypes.push_back(parameterType);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource)
        {
            arguments.push_back(sourceArguments[source]);
            parameterTypes.push_back(sourceParameterTypes[source]);
        }
        LLVMTypeRef returnType = node->referenceResult
            ? ptr
            : sretSlot != nullptr
                ? LLVMVoidTypeInContext(context_)
                : llvmType(node->typeKey, error);
        if (returnType == nullptr)
        {
            return nullptr;
        }
        LLVMTypeRef virtualFunctionType = LLVMFunctionType(
            returnType,
            parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()),
            0);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            virtualFunctionType, target,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "vcall", error);
        if (call == nullptr) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        return call;
    }

    LLVMValueRef lowerInterfaceCall(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (!node->interfaceSlot.has_value() || !node->targetTypeOrdinal.has_value())
        {
            return failValue(error, "Stage 36 interface call is missing interface identity or method slot");
        }
        if (node->children.empty())
        {
            return failValue(error, "Stage 36 interface call is missing its receiver operand");
        }
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr || interfaceMapEntryType_ == nullptr)
        {
            return failValue(error, "Stage 36 interface call has no interface-map LLVM ABI types");
        }

        LLVMValueRef receiver = lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr)
        {
            return nullptr;
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "interface.descriptor");
        LLVMValueRef mapAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 4, "interface.map.addr");
        LLVMValueRef map = LLVMBuildLoad2(builder_, ptr, mapAddress, "interface.map");

        LLVMBasicBlockRef mapBlock = appendBlock("interface.map.ok");
        LLVMBasicBlockRef searchBlock = appendBlock("interface.search");
        LLVMBasicBlockRef probeBlock = appendBlock("interface.probe");
        LLVMBasicBlockRef nextBlock = appendBlock("interface.next");
        LLVMBasicBlockRef foundBlock = appendBlock("interface.found");
        LLVMBasicBlockRef missingBlock = appendBlock("interface.missing");

        LLVMValueRef hasMap = LLVMBuildICmp(
            builder_, LLVMIntNE, map, LLVMConstNull(ptr), "interface.map.nonnull");
        LLVMBuildCondBr(builder_, hasMap, mapBlock, missingBlock);

        LLVMPositionBuilderAtEnd(builder_, mapBlock);
        LLVMValueRef countAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 0, "interface.map.count.addr");
        LLVMValueRef count = LLVMBuildLoad2(builder_, i64, countAddress, "interface.map.count");
        LLVMValueRef entriesAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 1, "interface.map.entries.addr");
        LLVMValueRef entries = LLVMBuildLoad2(builder_, ptr, entriesAddress, "interface.map.entries");
        LLVMBuildBr(builder_, searchBlock);

        LLVMPositionBuilderAtEnd(builder_, searchBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "interface.map.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {mapBlock};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef inRange = LLVMBuildICmp(builder_, LLVMIntULT, index, count, "interface.map.inrange");
        LLVMBuildCondBr(builder_, inRange, probeBlock, missingBlock);

        LLVMPositionBuilderAtEnd(builder_, probeBlock);
        LLVMValueRef entryIndices[] = {index};
        LLVMValueRef entry = LLVMBuildGEP2(
            builder_, interfaceMapEntryType_, entries, entryIndices, 1, "interface.entry");
        LLVMValueRef idAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 0, "interface.entry.id.addr");
        LLVMValueRef id = LLVMBuildLoad2(builder_, i64, idAddress, "interface.entry.id");
        LLVMValueRef wantedId = LLVMConstInt(i64, node->immediate, 0);
        LLVMValueRef matches = LLVMBuildICmp(builder_, LLVMIntEQ, id, wantedId, "interface.entry.matches");
        LLVMBuildCondBr(builder_, matches, foundBlock, nextBlock);

        LLVMPositionBuilderAtEnd(builder_, nextBlock);
        LLVMValueRef one = LLVMConstInt(i64, 1, 0);
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, index, one, "interface.map.next");
        LLVMBuildBr(builder_, searchBlock);
        LLVMValueRef nextValues[] = {nextIndex};
        LLVMBasicBlockRef nextBlocks[] = {nextBlock};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        LLVMPositionBuilderAtEnd(builder_, missingBlock);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::InterfaceDispatchFailure),
            0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        LLVMPositionBuilderAtEnd(builder_, foundBlock);
        LLVMValueRef tableAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 1, "interface.entry.table.addr");
        LLVMValueRef methodTable = LLVMBuildLoad2(builder_, ptr, tableAddress, "interface.method.table");
        LLVMValueRef slotIndex = LLVMConstInt(i64, *node->interfaceSlot, 0);
        LLVMValueRef slotIndices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, methodTable, slotIndices, 1, "interface.slot.addr");
        LLVMValueRef target = LLVMBuildLoad2(builder_, ptr, slotAddress, "interface.target");

        std::vector<LLVMValueRef> arguments;
        std::vector<LLVMTypeRef> parameterTypes;
        arguments.reserve(node->children.size() + 1);
        parameterTypes.reserve(node->children.size() + 1);
        arguments.push_back(receiver);
        parameterTypes.push_back(ptr);
        // Win64 aggregate ABI -- same reconstruction rule as lowerVirtualCall: the callee type
        // must match what declareFunctions gave every implementation.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
            parameterTypes.push_back(ptr);
        }
        std::vector<LLVMValueRef> sourceArguments;
        std::vector<LLVMTypeRef> sourceParameterTypes;
        sourceArguments.reserve(node->children.size() - 1);
        sourceParameterTypes.reserve(node->children.size() - 1);
        for (std::size_t childIndex = 1; childIndex < node->children.size(); ++childIndex)
        {
            LLVMValueRef value = lowerExpression(node->children[childIndex].get(), error);
            LLVMTypeRef parameterType = abiParameterType(node->children[childIndex]->typeKey, error);
            if (value == nullptr || parameterType == nullptr)
            {
                return nullptr;
            }
            value = wrapIndirectArgument(value, node->children[childIndex]->typeKey, error);
            if (value == nullptr) return nullptr;
            sourceArguments.push_back(value);
            sourceParameterTypes.push_back(parameterType);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource)
        {
            arguments.push_back(sourceArguments[source]);
            parameterTypes.push_back(sourceParameterTypes[source]);
        }
        LLVMTypeRef returnType = node->referenceResult
            ? ptr
            : sretSlot != nullptr
                ? LLVMVoidTypeInContext(context_)
                : llvmType(node->typeKey, error);
        if (returnType == nullptr)
        {
            return nullptr;
        }
        LLVMTypeRef interfaceFunctionType = LLVMFunctionType(
            returnType,
            parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()),
            0);
        LLVMValueRef interfaceCall = buildPotentiallyThrowingCall(
            interfaceFunctionType, target,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "icall", error);
        if (interfaceCall == nullptr) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(interfaceCall, attributeIndex, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        return interfaceCall;
    }

    // C6 "struct members": `new Vec2(1, 2)` on a struct.
    //
    // Everything lowerConstruction below does that is about being on the HEAP is absent here, and
    // that absence is the point: no allocator call, no null-check branch, no class descriptor
    // stored at offset 0, nothing to free. What remains is what construction actually means --
    // storage, zeroed, with the constructor run on it:
    //
    //     %slot = alloca <carrier>            (hoisted to the entry block, like every other
    //                                          temporary, so a loop does not grow the stack)
    //     memset(%slot, 0, size)              every field starts at its zero value
    //     call void @ctor(ptr %slot, args)    `this` is the slot
    //
    // The VALUE of the expression then follows the same rule every other struct-typed expression
    // in this backend follows, which is what lets `new Vec2(1,2)` appear anywhere a Vec2 may:
    // an aggregate of 1/2/4/8 bytes travels directly, so the value is a load of that width; any
    // other size is indirect, and the value IS the address of the storage.
    LLVMValueRef lowerValueConstruction(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->immediate == 0 || node->alignment == 0)
        {
            return failValue(error, "C6 struct construction has no value layout");
        }
        if (node->argumentCount > node->children.size())
        {
            return failValue(error, "C6 struct construction has an invalid source-argument count");
        }
        std::vector<LLVMValueRef> evaluatedArguments;
        evaluatedArguments.reserve(node->argumentCount);
        for (std::size_t index = 0; index < node->argumentCount; ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            // Win64 aggregate ABI: a struct whose size is not 1/2/4/8 travels as a POINTER to a
            // caller-owned per-call copy, never by value. The constructor's SIGNATURE already
            // carries that mapping -- declareFunctions builds every parameter through
            // abiParameterType -- so the call site has to agree, exactly like the four ordinary
            // call lowerers already do.
            //
            // Missing this was the bug: the call site loaded `[16 x i8]` and passed it by value
            // while the constructor declared `ptr`, and the LLVM verifier rejected the whole
            // module with "Call parameter type does not match function signature!". Measured
            // scope was every aggregate that does not fit in a register -- 12, 16, 20 and 64
            // bytes fail, 8 passes -- and it was SPECIFIC to constructors, because a constructor
            // is the one call shape that did not go through the mapping. It deformed the public
            // API of every type naturally built from a Matrix or a Vector4.
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            evaluatedArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, evaluatedArguments.size(), parameterToSource, error)) return nullptr;
        std::vector<LLVMValueRef> orderedArguments;
        orderedArguments.reserve(evaluatedArguments.size());
        for (const std::size_t source : parameterToSource)
            orderedArguments.push_back(evaluatedArguments[source]);

        LLVMTypeRef carrier = llvmType(node->typeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "struct.new");
        if (slot == nullptr) return failValue(error, "C6 struct construction could not reserve storage");

        if (node->zeroInitialize)
        {
            LLVMValueRef args[] = {
                slot,
                LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }

        if (!node->calleeName.empty())
        {
            const auto target = functions_.find(node->calleeName);
            if (target == functions_.end())
                return failValue(error, "C6 struct constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> ctorArgs;
            ctorArgs.reserve(1 + orderedArguments.size());
            ctorArgs.push_back(slot);
            ctorArgs.insert(ctorArgs.end(), orderedArguments.begin(), orderedArguments.end());
            if (buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function, ctorArgs.data(),
                    static_cast<unsigned>(ctorArgs.size()), "", error) == nullptr && !error.empty())
            {
                return nullptr;
            }
        }

        // Field initializers synthesized for an implicit default constructor run against the slot,
        // exactly as they run against the freshly allocated object on the class path.
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = slot;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            if (lowerExpression(node->children[index].get(), error) == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return nullptr;
            }
        }
        constructionThisOverride_ = previousOverride;

        // BUG FOUND BY C7, FIXED HERE. This used to short-circuit -- `if
        // (isIndirectStructTypeKey(...)) return slot;` -- handing back the ADDRESS of the freshly
        // constructed struct for any aggregate that travels indirectly (anything not 1/2/4/8
        // bytes). But an expression of aggregate type hands over the LOADED carrier at this
        // level, never a pointer: a call returning one writes into an sret slot and then loads it
        // (`abi.sret.load`), and the TemporaryAddress case above already carries the same lesson
        // in writing. The mismatch was silent rather than fatal wherever the consumer stored the
        // result: `Big g = new Big(1.0f);` on a twenty-byte struct emitted
        // `store ptr %struct.new, ptr %local0.addr` -- the local ended up holding a POINTER in
        // its first eight bytes, and every field read afterwards read pointer bits as floats.
        // Wrong values, no diagnostic, no verifier complaint. One shape, one path.
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "struct.new.value");
        if (node->alignment > 1U) LLVMSetAlignment(loaded, static_cast<unsigned>(node->alignment));
        return loaded;
    }

    // C7 "aggregate initializer": `V v = {1, 2};`. Same shape as lowerValueConstruction above --
    // reserve the storage, give it a defined starting state, write into it, then hand back the
    // value the way this type's ABI hands values back. The only difference is what does the
    // writing: element stores at fixed byte offsets instead of a constructor call.
    //
    // The zeroing is not redundant with the one emitDefaultLocalStorage does: this builds a
    // TEMPORARY, and a partial initializer leaves the elements it does not mention untouched, so
    // this is what makes `{1}` on a four-element array mean "1, 0, 0, 0" rather than "1, garbage".
    LLVMValueRef lowerAggregateInitializer(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->immediate == 0 || node->alignment == 0)
        {
            return failValue(error, "C7 aggregate initializer has no value layout");
        }
        LLVMTypeRef carrier = llvmType(node->typeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "aggregate.init");
        if (slot == nullptr) return failValue(error, "C7 aggregate initializer could not reserve storage");

        LLVMValueRef memsetArgs[] = {
            slot,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArgs, 4, "");

        for (const auto& child : node->children)
        {
            if (child == nullptr ||
                child->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore ||
                child->children.size() != 1)
            {
                return failValue(error, "C7 aggregate initializer has a malformed element");
            }
            LLVMValueRef value = lowerExpression(child->children.front().get(), error);
            if (value == nullptr) return nullptr;
            LLVMValueRef address = byteOffset(slot, child->immediate, "aggregate.element.addr");
            if (child->bitWidth.has_value())
            {
                LLVMTypeRef storageType = bitFieldStorageType(child.get(), error);
                if (storageType == nullptr) return nullptr;
                LLVMValueRef previous = LLVMBuildLoad2(
                    builder_, storageType, address, "aggregate.bitfield.storage.load");
                LLVMSetAlignment(previous, static_cast<unsigned>(child->alignment));
                LLVMValueRef merged = insertBitField(child.get(), previous, value, error);
                if (merged == nullptr) return nullptr;
                LLVMValueRef store = LLVMBuildStore(builder_, merged, address);
                LLVMSetAlignment(store, static_cast<unsigned>(child->alignment));
                continue;
            }
            LLVMValueRef store = LLVMBuildStore(builder_, value, address);
            // An element is written through its own typed address, so it needs its own natural
            // alignment -- the carrier's `[N x i8]` shape would otherwise imply align 1.
            const unsigned elementAlignment = structAlignmentFor(child->typeKey);
            if (elementAlignment > 1U) LLVMSetAlignment(store, elementAlignment);
        }

        // Always the loaded carrier, never the slot pointer -- see the note in
        // lowerValueConstruction for the bug that short-circuit causes.
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "aggregate.init.value");
        if (node->alignment > 1U) LLVMSetAlignment(loaded, static_cast<unsigned>(node->alignment));
        return loaded;
    }

    LLVMValueRef lowerConstruction(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (!node->stackPromotedObject && !node->allocationFailureReturnsNull)
        {
            return failValue(
                error,
                "Stage 34 LLVM lowering requires the frozen Stage 29 allocation-failure -> null contract");
        }

        // Source arguments are evaluated left-to-right before object allocation. Their SSA values
        // therefore dominate both the allocation-success branch and the constructor call. This
        // prevents argument side effects from disappearing merely because allocation returns null.
        if (node->argumentCount > node->children.size())
        {
            return failValue(error, "Stage 37 construction MIR has an invalid source-argument count");
        }
        std::vector<LLVMValueRef> evaluatedArguments;
        evaluatedArguments.reserve(node->argumentCount);
        for (std::size_t index = 0; index < node->argumentCount; ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            // Win64 aggregate ABI: a struct whose size is not 1/2/4/8 travels as a POINTER to a
            // caller-owned per-call copy, never by value. The constructor's SIGNATURE already
            // carries that mapping -- declareFunctions builds every parameter through
            // abiParameterType -- so the call site has to agree, exactly like the four ordinary
            // call lowerers already do.
            //
            // Missing this was the bug: the call site loaded `[16 x i8]` and passed it by value
            // while the constructor declared `ptr`, and the LLVM verifier rejected the whole
            // module with "Call parameter type does not match function signature!". Measured
            // scope was every aggregate that does not fit in a register -- 12, 16, 20 and 64
            // bytes fail, 8 passes -- and it was SPECIFIC to constructors, because a constructor
            // is the one call shape that did not go through the mapping. It deformed the public
            // API of every type naturally built from a Matrix or a Vector4.
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            evaluatedArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, evaluatedArguments.size(), parameterToSource, error)) return nullptr;
        std::vector<LLVMValueRef> orderedArguments;
        orderedArguments.reserve(evaluatedArguments.size());
        for (const std::size_t source : parameterToSource)
            orderedArguments.push_back(evaluatedArguments[source]);

        LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef object = nullptr;
        LLVMBasicBlockRef endBlock = nullptr;
        if (node->stackPromotedObject)
        {
            if (!node->localOrdinal.has_value())
                return failValue(error, "item 323 stack construction has no local identity");
            const auto storage = stackObjectStorage_.find(*node->localOrdinal);
            if (storage == stackObjectStorage_.end() || storage->second == nullptr)
                return failValue(error, "item 323 stack construction cannot find its frame storage");
            object = storage->second;
        }
        else
        {
            LLVMValueRef allocArgs[] = {size};
            object = buildNoUnwindCall(
                allocationFunctionType_, allocationFunction_, allocArgs, 1, "object");
            LLVMValueRef nullValue = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
            LLVMValueRef isNull = LLVMBuildICmp(
                builder_, LLVMIntEQ, object, nullValue, "alloc.failed");
            LLVMBasicBlockRef initBlock = appendBlock("object.init");
            endBlock = appendBlock("object.end");
            LLVMBuildCondBr(builder_, isNull, endBlock, initBlock);
            LLVMPositionBuilderAtEnd(builder_, initBlock);
        }
        if (node->zeroInitialize)
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0);
            LLVMValueRef isVolatile = LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            LLVMValueRef args[] = {object, zero, size, isVolatile};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }
        if (!node->targetTypeOrdinal.has_value())
        {
            return failValue(error, "Macro A allocation is missing its target class ordinal");
        }
        LLVMValueRef descriptorValue = nullptr;
        const auto descriptorAccessor = descriptorAccessorFunctions_.find(*node->targetTypeOrdinal);
        if (descriptorAccessor != descriptorAccessorFunctions_.end() &&
            runtimeDescriptorAccessorTypes_.contains(*node->targetTypeOrdinal))
        {
            descriptorValue = buildNoUnwindCall(
                descriptorAccessor->second.functionType,
                descriptorAccessor->second.function,
                nullptr,
                0,
                "classdesc");
        }
        else
        {
            const auto descriptor = descriptorGlobals_.find(*node->targetTypeOrdinal);
            if (descriptor != descriptorGlobals_.end()) descriptorValue = descriptor->second;
        }
        if (descriptorValue == nullptr)
        {
            return failValue(error, "Macro A allocation cannot resolve its class descriptor");
        }
        LLVMBuildStore(builder_, descriptorValue, object);

        if (!node->calleeName.empty())
        {
            const auto target = functions_.find(node->calleeName);
            if (target == functions_.end()) return failValue(error, "Stage 34 constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> ctorArgs;
            ctorArgs.reserve(1 + orderedArguments.size());
            ctorArgs.push_back(object);
            ctorArgs.insert(ctorArgs.end(), orderedArguments.begin(), orderedArguments.end());
            if (buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function,
                    ctorArgs.data(), static_cast<unsigned>(ctorArgs.size()), "", error) == nullptr &&
                !error.empty())
            {
                return nullptr;
            }
        }

        // A construction node may carry synthesized instance-initializer work after its source
        // arguments.  These expressions conceptually execute against the just-allocated object;
        // they are not source arguments and therefore must not be evaluated before allocation.
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = object;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            LLVMValueRef ignored = lowerExpression(node->children[index].get(), error);
            if (ignored == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return nullptr;
            }
        }
        constructionThisOverride_ = previousOverride;
        if (!node->stackPromotedObject)
        {
            if (!terminated()) LLVMBuildBr(builder_, endBlock);
            LLVMPositionBuilderAtEnd(builder_, endBlock);
        }
        return object;
    }

    LLVMValueRef lowerPlacementConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.empty() || node->children.front() == nullptr)
            return failValue(error, "item 318 placement construction requires destination storage");
        if (node->immediate == 0 || node->alignment == 0)
            return failValue(error, "item 318 placement construction has no target layout");
        if (node->argumentCount + 1 > node->children.size())
            return failValue(error, "item 318 placement construction has an invalid argument count");

        // The destination is evaluated exactly once and before every constructor argument. Its
        // semantic type is exact T*, so no conversion or hidden temporary can substitute another
        // location after this point.
        LLVMValueRef storage = lowerExpression(node->children.front().get(), error);
        if (storage == nullptr) return nullptr;

        std::vector<LLVMValueRef> evaluatedArguments;
        evaluatedArguments.reserve(node->argumentCount);
        for (std::size_t index = 0; index < node->argumentCount; ++index)
        {
            const auto& child = node->children[index + 1];
            LLVMValueRef value = lowerExpression(child.get(), error);
            if (value == nullptr) return nullptr;
            value = wrapIndirectArgument(value, child->typeKey, error);
            if (value == nullptr) return nullptr;
            evaluatedArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, evaluatedArguments.size(), parameterToSource, error))
            return nullptr;
        std::vector<LLVMValueRef> orderedArguments;
        orderedArguments.reserve(evaluatedArguments.size());
        for (const std::size_t source : parameterToSource)
            orderedArguments.push_back(evaluatedArguments[source]);

        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef notNull = LLVMBuildICmp(
            builder_, LLVMIntNE, storage, LLVMConstNull(pointerType), "placement.notnull");
        if (!emitGuard(notNull, LlvmTrapReason::NullReference, "placement.storage", error))
            return nullptr;

        if (node->zeroInitialize)
        {
            LLVMValueRef args[] = {
                storage,
                LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }

        if (!node->targetTypeOrdinal.has_value() ||
            *node->targetTypeOrdinal >= nameLookup_.namedTypes().size())
        {
            return failValue(error, "item 318 placement construction has no target type ordinal");
        }
        const frontend::NamedTypeDeclaration& targetType =
            nameLookup_.namedTypes()[*node->targetTypeOrdinal];
        if (targetType.kind == frontend::NamedTypeDeclarationKind::Class)
        {
            LLVMValueRef descriptorValue = nullptr;
            const auto descriptorAccessor =
                descriptorAccessorFunctions_.find(*node->targetTypeOrdinal);
            if (descriptorAccessor != descriptorAccessorFunctions_.end() &&
                runtimeDescriptorAccessorTypes_.contains(*node->targetTypeOrdinal))
            {
                descriptorValue = buildNoUnwindCall(
                    descriptorAccessor->second.functionType,
                    descriptorAccessor->second.function,
                    nullptr,
                    0,
                    "placement.classdesc");
            }
            else
            {
                const auto descriptor = descriptorGlobals_.find(*node->targetTypeOrdinal);
                if (descriptor != descriptorGlobals_.end()) descriptorValue = descriptor->second;
            }
            if (descriptorValue == nullptr)
                return failValue(error, "item 318 placement construction cannot resolve its class descriptor");
            LLVMValueRef descriptorStore = LLVMBuildStore(builder_, descriptorValue, storage);
            if (node->alignment > 1U)
                LLVMSetAlignment(descriptorStore, static_cast<unsigned>(node->alignment));
        }

        if (!node->calleeName.empty())
        {
            const auto constructor = functions_.find(node->calleeName);
            if (constructor == functions_.end())
                return failValue(error, "item 318 placement constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> constructorArguments;
            constructorArguments.reserve(1 + orderedArguments.size());
            constructorArguments.push_back(storage);
            constructorArguments.insert(
                constructorArguments.end(), orderedArguments.begin(), orderedArguments.end());
            if (buildPotentiallyThrowingCall(
                    constructor->second.functionType,
                    constructor->second.function,
                    constructorArguments.data(),
                    static_cast<unsigned>(constructorArguments.size()),
                    "",
                    error) == nullptr && !error.empty())
            {
                return nullptr;
            }
        }

        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = storage;
        for (std::size_t index = node->argumentCount + 1; index < node->children.size(); ++index)
        {
            if (lowerExpression(node->children[index].get(), error) == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return nullptr;
            }
        }
        constructionThisOverride_ = previousOverride;
        return storage;
    }

    LLVMValueRef lowerObjectDestruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
    {
        if (node->children.size() != 1)
            return failValue(error, "item 319 object destruction requires one operand");

        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;

        // Structs currently have no destructor or owned fields. Evaluating the pointer ends the
        // language lifetime contract and requires no machine operation; importantly, it still
        // never becomes a call to the allocator's free hook.
        if (!node->dynamicDestruction)
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);

        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
            return failValue(error, "item 319 object destruction has no descriptor ABI");

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, object, LLVMConstNull(ptr), "destroy.object.isnull");
        LLVMBasicBlockRef destroyBlock = appendBlock("destroy.object");
        LLVMBasicBlockRef doneBlock = appendBlock("destroy.object.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);

        LLVMPositionBuilderAtEnd(builder_, destroyBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(
            builder_, ptr, object, "destroy.object.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 7, "destroy.object.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "destroy.object.entry");
        LLVMValueRef arguments[] = {object};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return nullptr;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

    LLVMValueRef lowerDeallocation(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 1)
        {
            return failValue(error, "Stage 35 deallocation MIR requires one operand");
        }
        LLVMValueRef loweredOperand = lowerExpression(node->children.front().get(), error);
        if (loweredOperand == nullptr)
        {
            return nullptr;
        }

        // Stage 52: `delete <value-typed expression>;` (e.g. an int-valued generic collection
        // element) was already evaluated above for its side effects (array-index bounds checks
        // in particular), but its result is not a heap pointer at all -- there is nothing to
        // free. Discard it rather than passing a non-pointer bit pattern to the runtime
        // allocator, which would misbehave or fail LLVM verification.
        if (node->valueTypeNoOpDeletion)
        {
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        LLVMValueRef pointer = loweredOperand;
        if (node->booleanValue)
        {
            LLVMTypeRef storedType = llvmType(node->elementTypeKey, error);
            if (storedType == nullptr) return nullptr;
            pointer = LLVMBuildLoad2(
                builder_, storedType, loweredOperand, "delete.element.load");
            // Clear before calling user destruction.  If a destructor transfers control through
            // EH, every cleanup path already observes an empty owner slot.
            LLVMBuildStore(builder_, LLVMConstNull(storedType), loweredOperand);
        }

        if (node->children.front() != nullptr && isDelegateReference(node->children.front()->typeKey))
        {
            if (!emitDelegateDestroy(pointer, "delete.delegate", error)) return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        if (!node->dynamicDestruction)
        {
            LLVMValueRef arguments[] = {pointer};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
        {
            return failValue(error, "Stage 35 dynamic delete has no class-descriptor destruction ABI");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, pointer, LLVMConstNull(ptr), "delete.isnull");
        LLVMBasicBlockRef destroyBlock = appendBlock("delete.destroy");
        LLVMBasicBlockRef doneBlock = appendBlock("delete.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);

        LLVMPositionBuilderAtEnd(builder_, destroyBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, pointer, "delete.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 5, "delete.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "delete.entry");
        LLVMValueRef arguments[] = {pointer};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return nullptr;
        }
        if (!terminated())
        {
            LLVMBuildBr(builder_, doneBlock);
        }

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

    LLVMTypeRef bitFieldStorageType(
        const frontend::ProjectOopMirNode* node,
        std::string& error) const
    {
        if (node == nullptr || !node->bitOffset.has_value() || !node->bitWidth.has_value() ||
            node->bitStorageSize == 0 || node->bitStorageSize > 8)
        {
            error = "item 329 bit-field access has incomplete allocation-unit metadata";
            return nullptr;
        }
        const std::uint64_t storageBits = node->bitStorageSize * 8U;
        if (*node->bitWidth == 0 || *node->bitOffset >= storageBits ||
            *node->bitWidth > storageBits - *node->bitOffset)
        {
            error = "item 329 bit-field coordinates exceed their allocation unit";
            return nullptr;
        }
        switch (storageBits)
        {
            case 8U: return LLVMInt8TypeInContext(context_);
            case 16U: return LLVMInt16TypeInContext(context_);
            case 32U: return LLVMInt32TypeInContext(context_);
            case 64U: return LLVMInt64TypeInContext(context_);
            default:
                error = "item 329 bit-field allocation unit has an unsupported integer width";
                return nullptr;
        }
    }

    LLVMValueRef extractBitField(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error)
    {
        LLVMTypeRef storageType = bitFieldStorageType(node, error);
        if (storageType == nullptr || storage == nullptr) return nullptr;

        const std::uint64_t storageBits = node->bitStorageSize * 8U;
        const std::uint64_t bitOffset = *node->bitOffset;
        const std::uint64_t bitWidth = *node->bitWidth;
        const std::uint64_t lowMask = bitWidth == 64U
            ? ~std::uint64_t{0}
            : (std::uint64_t{1} << bitWidth) - 1U;
        LLVMValueRef extracted = storage;
        if (bitOffset != 0)
        {
            extracted = LLVMBuildLShr(
                builder_, extracted, LLVMConstInt(storageType, bitOffset, 0), "bitfield.shift");
        }
        if (bitWidth != storageBits)
        {
            extracted = LLVMBuildAnd(
                builder_, extracted, LLVMConstInt(storageType, lowMask, 0), "bitfield.mask");
        }
        if (node->typeKey == "builtin:bool")
        {
            return LLVMBuildICmp(
                builder_, LLVMIntNE, extracted, LLVMConstNull(storageType), "bitfield.bool");
        }

        const frontend::builtins::IntegerTypeInfo* enumInfo = enumUnderlyingInfo(node->typeKey);
        const unsigned logicalWidth = enumInfo != nullptr
            ? enumInfo->width : integerWidth(node->typeKey);
        if (logicalWidth != storageBits)
        {
            return failValue(error, "item 329 bit-field logical type does not match its allocation unit");
        }
        const bool signedField = enumInfo != nullptr ? enumInfo->isSigned : isSignedInteger(node->typeKey);
        if (signedField && bitWidth < storageBits)
        {
            const std::uint64_t extensionShift = storageBits - bitWidth;
            LLVMValueRef shift = LLVMConstInt(storageType, extensionShift, 0);
            extracted = LLVMBuildShl(builder_, extracted, shift, "bitfield.sign.left");
            extracted = LLVMBuildAShr(builder_, extracted, shift, "bitfield.sign.extend");
        }
        return extracted;
    }

    LLVMValueRef insertBitField(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef previousStorage,
        LLVMValueRef value,
        std::string& error)
    {
        LLVMTypeRef storageType = bitFieldStorageType(node, error);
        if (storageType == nullptr || previousStorage == nullptr || value == nullptr) return nullptr;

        const std::uint64_t storageBits = node->bitStorageSize * 8U;
        const std::uint64_t bitOffset = *node->bitOffset;
        const std::uint64_t bitWidth = *node->bitWidth;
        const std::uint64_t lowMask = bitWidth == 64U
            ? ~std::uint64_t{0}
            : (std::uint64_t{1} << bitWidth) - 1U;
        const std::uint64_t fieldMask = lowMask << bitOffset;

        LLVMValueRef widened = value;
        if (node->typeKey == "builtin:bool")
        {
            widened = LLVMBuildZExt(builder_, value, storageType, "bitfield.bool.widen");
        }
        else
        {
            const frontend::builtins::IntegerTypeInfo* enumInfo = enumUnderlyingInfo(node->typeKey);
            const unsigned logicalWidth = enumInfo != nullptr
                ? enumInfo->width : integerWidth(node->typeKey);
            if (logicalWidth != storageBits)
            {
                return failValue(error, "item 329 bit-field store type does not match its allocation unit");
            }
        }
        LLVMValueRef bits = LLVMBuildAnd(
            builder_, widened, LLVMConstInt(storageType, lowMask, 0), "bitfield.value.mask");
        if (bitOffset != 0)
        {
            bits = LLVMBuildShl(
                builder_, bits, LLVMConstInt(storageType, bitOffset, 0), "bitfield.value.shift");
        }
        LLVMValueRef preserved = LLVMBuildAnd(
            builder_, previousStorage, LLVMConstInt(storageType, ~fieldMask, 0),
            "bitfield.storage.preserve");
        return LLVMBuildOr(builder_, preserved, bits, "bitfield.storage.merge");
    }

    LLVMValueRef fieldAddress(const frontend::ProjectOopMirNode* node, std::string& error)
    {
        if (node->children.size() != 1 || node->children.front() == nullptr)
        {
            return failValue(error, "Macro A field access requires exactly one explicit receiver operand");
        }
        LLVMValueRef receiver = lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        const bool structReceiver = node->children.front() != nullptr &&
            isStructValue(node->children.front()->typeKey);
        if (!structReceiver)
        {
            receiver = checkedReceiver(receiver, error);
            if (receiver == nullptr)
            {
                return nullptr;
            }
        }
        LLVMValueRef offset = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), receiver, indices, 1, "field.addr");
    }

    LLVMValueRef checkedReceiver(LLVMValueRef receiver, std::string& error)
    {
        if (receiver == nullptr || trapFunction_ == nullptr || trapFunctionType_ == nullptr ||
            currentFunction_ == nullptr)
        {
            return failValue(error, "Macro A cannot emit null-reference receiver guard");
        }

        LLVMValueRef nullValue = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        LLVMValueRef valid = LLVMBuildICmp(builder_, LLVMIntNE, receiver, nullValue, "receiver.nonnull");
        LLVMBasicBlockRef okBlock = appendBlock("receiver.ok");
        LLVMBasicBlockRef trapBlock = appendBlock("receiver.null");
        LLVMBuildCondBr(builder_, valid, okBlock, trapBlock);

        LLVMPositionBuilderAtEnd(builder_, trapBlock);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::NullReference),
            0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        LLVMPositionBuilderAtEnd(builder_, okBlock);
        return receiver;
    }

    LLVMValueRef convert(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        std::string& error)
    {
        if (source == target || source == "builtin:null") return value;
        LLVMTypeRef destination = llvmType(target, error);
        if (destination == nullptr) return nullptr;
        // UN ENUM SE CONVIERTE COMO SU TIPO SUBYACENTE. `integerWidth()` e `isSignedInteger()` solo
        // saben de claves "builtin:", asi que la clave de un enum tiene que traer su ancho y su
        // signo de la declaracion. Antes esto decia "32 bits y con signo" sin preguntar, y con un
        // `enum : byte` emitia `icmp ne i8 %v, i32 200` -- el verificador de LLVM lo rechazaba,
        // que es la forma AFORTUNADA del defecto; en `enum : sbyte` habria sido peor, porque
        // extender sin signo un -1 da 255 y eso compila y responde mal.
        const frontend::builtins::IntegerTypeInfo* sourceEnum = enumUnderlyingInfo(source);
        const frontend::builtins::IntegerTypeInfo* targetEnum = enumUnderlyingInfo(target);
        const unsigned sourceWidth = sourceEnum != nullptr ? sourceEnum->width : integerWidth(source);
        const unsigned targetWidth = targetEnum != nullptr ? targetEnum->width : integerWidth(target);
        const bool sourceSigned =
            sourceEnum != nullptr ? sourceEnum->isSigned : isSignedInteger(source);
        const bool targetSigned =
            targetEnum != nullptr ? targetEnum->isSigned : isSignedInteger(target);
        if (sourceWidth != 0 && targetWidth != 0)
        {
            if (sourceWidth == targetWidth) return value;
            if (sourceWidth > targetWidth) return LLVMBuildTrunc(builder_, value, destination, "trunc");
            return sourceSigned
                ? LLVMBuildSExt(builder_, value, destination, "sext")
                : LLVMBuildZExt(builder_, value, destination, "zext");
        }
        if (sourceWidth != 0 && isFloating(target))
            return sourceSigned
                ? LLVMBuildSIToFP(builder_, value, destination, "sitofp")
                : LLVMBuildUIToFP(builder_, value, destination, "uitofp");
        if (isFloating(source) && targetWidth != 0)
            return targetSigned
                ? LLVMBuildFPToSI(builder_, value, destination, "fptosi")
                : LLVMBuildFPToUI(builder_, value, destination, "fptoui");
        if (source == "builtin:float" && target == "builtin:double") return LLVMBuildFPExt(builder_, value, destination, "fpext");
        if (source == "builtin:double" && target == "builtin:float") return LLVMBuildFPTrunc(builder_, value, destination, "fptrunc");
        if ((isPointerLike(source) || isClassReference(source)) && (isPointerLike(target) || isClassReference(target))) return value;

        // C1.1 "function-pointer round trip". Everything above is the numeric/reference world;
        // these three lines are the address world. An extern C delegate, a raw `T*`/`void*` and a
        // pointer-sized integral are the same 64 bits in three spellings, so the round trip is
        // exactly one LLVM instruction in each direction -- and NONE between two pointer-shaped
        // ends, since opaque pointers make that a no-op. The frontend has already refused every
        // combination that is not part of the trip (see ProjectMethodBodyModel's CastExpression
        // case), so reaching here means the conversion is one of the sanctioned three.
        const bool sourceIsAddress =
            isPointerLike(source) || isClassReference(source) || isExternCDelegateType(source);
        const bool targetIsAddress =
            isPointerLike(target) || isClassReference(target) || isExternCDelegateType(target);
        if (sourceIsAddress && targetIsAddress) return value;
        if (sourceIsAddress && targetWidth != 0)
            return LLVMBuildPtrToInt(builder_, value, destination, "ptrtoint");
        if (sourceWidth != 0 && targetIsAddress)
            return LLVMBuildIntToPtr(builder_, value, destination, "inttoptr");
        return failValue(error, "unsupported Stage 34 LLVM conversion from '" + std::string(source) + "' to '" + std::string(target) + "'");
    }

    // EL TIPO ENTERO SUBYACENTE DE UN ENUM, buscado por su clave de tipo. Nulo si la clave no
    // nombra un enum -- ni una struct ni una referencia de clase, que comparten el prefijo
    // "type:".
    //
    // ES EL UNICO SITIO DE ESTE BACKEND que traduce una clave de tipo a la regla de
    // include/nativesharp/Frontend/EnumUnderlyingType.h. El tipo LLVM, el ancho de las
    // conversiones, el signo de las extensiones y el signo de las constantes de `case` salen los
    // cuatro de aqui, y por eso no pueden discrepar: hasta el 2026-09-03 eran cuatro copias de
    // "un enum mide 32 bits y tiene signo" escritas por separado, y un `enum : byte` las
    // contradecia a las cuatro a la vez.
    const frontend::builtins::IntegerTypeInfo* enumUnderlyingInfo(std::string_view typeKey) const
    {
        if (!typeKey.starts_with("type:")) return nullptr;
        const std::string_view key = typeKey.substr(5);
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() != key ||
                type.kind != frontend::NamedTypeDeclarationKind::Enum)
                continue;
            return frontend::builtins::findIntegerType(frontend::enumUnderlyingTypeKey(type.syntax));
        }
        return nullptr;
    }

    // Un enum SIEMPRE tiene tipo subyacente --lo escrito, o `int` por omision, y las dos formas
    // salen de la tabla unica de enteros-- asi que "hay tipo subyacente" y "es un enum" son la
    // misma pregunta.
    bool isEnumTypeKey(std::string_view typeKey) const
    {
        return enumUnderlyingInfo(typeKey) != nullptr;
    }

    LLVMTypeRef llvmType(std::string_view typeKey, std::string& error) const
    {
        if (typeKey == "builtin:void") return LLVMVoidTypeInContext(context_);
        if (typeKey == "builtin:bool") return LLVMInt1TypeInContext(context_);
        if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte" ||
            typeKey == "builtin:char") return LLVMInt8TypeInContext(context_);
        if (typeKey == "builtin:short" || typeKey == "builtin:ushort") return LLVMInt16TypeInContext(context_);
        if (typeKey == "builtin:int" || typeKey == "builtin:uint") return LLVMInt32TypeInContext(context_);
        if (typeKey == "builtin:long" || typeKey == "builtin:ulong" || typeKey == "builtin:nint" || typeKey == "builtin:nuint") return LLVMInt64TypeInContext(context_);
        if (typeKey == "builtin:float") return LLVMFloatTypeInContext(context_);
        if (typeKey == "builtin:double") return LLVMDoubleTypeInContext(context_);
        if (typeKey == "builtin:null" || typeKey == "string-literal" || typeKey == "builtin:string" ||
            frontend::ProjectTypeConversion::isNativeArray(typeKey) || isPointerLike(typeKey) ||
            isClassReference(typeKey) || isDelegateReference(typeKey))
            return LLVMPointerTypeInContext(context_, 0);
        // C2.1 "fixed arrays": `T[N]` is INLINE storage -- N elements laid out end to end, with no
        // length header and no indirection -- so its LLVM carrier is exactly `[N x T]`. That is
        // what makes a fixed-array LOCAL work (`float[4] a;` is one alloca of the whole thing) and
        // what makes a fixed-array FIELD occupy its real bytes inside the struct. It is
        // deliberately NOT the managed native-array shape (`T[]`), which is a pointer to a
        // length-prefixed heap block.
        if (const std::optional<std::string> element =
                frontend::ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
            element.has_value())
        {
            const std::optional<std::uint64_t> count =
                frontend::ProjectMemberModel::fixedArrayCount(typeKey);
            LLVMTypeRef elementType = llvmType(*element, error);
            if (elementType == nullptr || !count.has_value() || *count == 0)
            {
                if (error.empty())
                    error = "C2.1 cannot map fixed-array type key to LLVM type: " + std::string(typeKey);
                return nullptr;
            }
            return LLVMArrayType2(elementType, static_cast<unsigned>(*count));
        }
        if (typeKey.starts_with("type:"))
        {
            // El ancho del enum es el de su TIPO SUBYACENTE, no siempre 32. La regla esta en
            // include/nativesharp/Frontend/EnumUnderlyingType.h y la comparten el modelo de layout
            // y este backend: si discreparan, una struct con un campo de enum tendria un tamano en
            // el frontend y otro en el IR.
            //
            // Los cuatro anchos que la tabla de enteros admite se escriben uno a uno en vez de con
            // LLVMIntTypeInContext porque la API C que este proyecto declara a mano
            // (src/LLVM/LlvmCApi.h) no incluye esa funcion, y anadir un simbolo a esa frontera es
            // una decision aparte -- la lista de 146 esta congelada a proposito (SHR-090).
            if (const frontend::builtins::IntegerTypeInfo* enumInfo = enumUnderlyingInfo(typeKey);
                enumInfo != nullptr)
            {
                switch (enumInfo->width)
                {
                    case 8: return LLVMInt8TypeInContext(context_);
                    case 16: return LLVMInt16TypeInContext(context_);
                    case 64: return LLVMInt64TypeInContext(context_);
                    default: return LLVMInt32TypeInContext(context_);
                }
            }
            if (const frontend::ProjectOopMirStructLayout* layout = structLayout(typeKey); layout != nullptr)
            {
                switch (layout->size)
                {
                    // Win64 rule, confirmed against clang for x86_64-pc-windows-msvc: an
                    // aggregate of size 1, 2, 4 or 8 travels DIRECTLY, as an integer of that
                    // width -- {float,float} goes in a GPR as i64, never split into XMM.
                    case 1: return LLVMInt8TypeInContext(context_);
                    case 2: return LLVMInt16TypeInContext(context_);
                    case 4: return LLVMInt32TypeInContext(context_);
                    case 8: return LLVMInt64TypeInContext(context_);
                    default:
                        // Every OTHER size -- including 3 bytes, not just "big" -- is an
                        // INDIRECT aggregate: `[N x i8]` is its in-memory value carrier (local
                        // slots, whole-value copies), and the CALL BOUNDARY never sees this type
                        // at all -- parameters travel as a pointer to a caller-owned per-call
                        // copy and returns travel through a leading sret pointer. That mapping
                        // is applied where signatures and calls are built, NOT here, so every
                        // in-memory use (field GEPs, array elements, load/store of the whole
                        // value) keeps working off the natural byte layout.
                        //
                        // HISTORY: this used to be a hard error ("direct struct value ABI
                        // supports only 1, 2, 4 or 8-byte aggregates"), which made any struct
                        // over 8 bytes -- Mat4 (64), a MSG-shaped 48-byte record, even a 12-byte
                        // Vec3 -- unusable as a local, parameter or return anywhere in the
                        // language. Fields and array elements always worked, because they were
                        // already address-based; the value paths were the only gap.
                        return LLVMArrayType2(LLVMInt8TypeInContext(context_), layout->size);
                }
            }
            error = "Stage 40 cannot map named value type to LLVM carrier: " + std::string(typeKey);
            return nullptr;
        }
        error = "Stage 34 cannot map type key to LLVM type: " + std::string(typeKey);
        return nullptr;
    }

    const frontend::ProjectOopMirStructLayout* structLayout(std::string_view typeKey) const noexcept
    {
        for (const frontend::ProjectOopMirStructLayout& layout : mirModel_.structLayouts())
        {
            if (layout.typeKey == typeKey) return &layout;
        }
        return nullptr;
    }

    const frontend::ProjectOopMirClassLayout* classLayout(std::string_view typeKey) const noexcept
    {
        for (const frontend::ProjectOopMirClassLayout& layout : mirModel_.classLayouts())
        {
            if (layout.typeKey == typeKey) return &layout;
        }
        return nullptr;
    }

    bool isStructValue(std::string_view typeKey) const noexcept
    {
        return structLayout(typeKey) != nullptr;
    }

    bool isDelegateReference(std::string_view typeKey) const noexcept
    {
        if (!typeKey.starts_with("type:")) return false;
        const std::string_view key = typeKey.substr(5);
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() == key) return type.kind == frontend::NamedTypeDeclarationKind::Delegate;
        }
        return false;
    }

    bool isClassReference(std::string_view typeKey) const noexcept
    {
        if (!typeKey.starts_with("type:")) return false;
        const std::string_view key = typeKey.substr(5);
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() == key) return type.kind == frontend::NamedTypeDeclarationKind::Class ||
                type.kind == frontend::NamedTypeDeclarationKind::Interface;
        }
        return false;
    }

    // Stage 52 closure hardening (closure checklist P1 item 6: "requiresOwnedDestruction(typeKey)
    // ... reduce la posibilidad de que futuros stages repitan decisiones divergentes"). This is
    // the single, centralized answer to "does a closed NativeSharp type actually own separate
    // heap-allocated storage that a cleanup step must release" -- the exact classification that
    // emitOwnedParameterDestroy, emitRaiiDestroy, and lowerDeallocation each need at their own
    // "am I looking at a real owning pointer, or just a scalar bit pattern that happens to sit in
    // an `owned`-qualified slot" decision point.
    //
    // Before this helper existed, each of those three call sites spelled out its own copy of
    // this same reference-like/string/value-type split independently -- three chances to
    // diverge, which is exactly how "Bug A" (the Stage 50.8 finding: emitOwnedParameterDestroy
    // treated an `owned int` parameter's 4-byte alloca as an 8-byte pointer, corrupting adjacent
    // stack memory) went unnoticed for a full closure cycle: emitRaiiDestroy and
    // lowerDeallocation had each independently grown their own value-type handling by the time
    // emitOwnedParameterDestroy needed one too, and nobody had a single place to add it.
    //
    // A closed type requires real destruction when it is:
    //   - a class or interface reference (isClassReference) -- released through the class
    //     descriptor's virtual destruction entry (descriptorType_/destructionEntryFunctionType_);
    //   - `builtin:string` -- released through the native string deallocation path
    //     (deallocationFunction_/kDeallocationFunctionName);
    // and requires NO destruction (a harmless no-op cleanup) for every other closed type -- a
    // numeric/bool scalar, an enum (represented as its underlying scalar), or a struct value --
    // none of which separately own heap memory of their own, regardless of which generic T they
    // came from monomorphizing. This mirrors (without yet sharing translation-unit-level code
    // with) the equivalent HIR-layer classification `isReferenceType`/`nativeString` in
    // ProjectOopHirModel.cpp computes for `delete` (see its `node->valueTypeNoOpDeletion` flag)
    // -- unifying the HIR and LLVM layers into one shared classification function is a further,
    // still-open deduplication opportunity, out of scope for this pass, which only centralizes
    // the three call sites that already live in this file.
    bool requiresOwnedDestruction(std::string_view typeKey) const noexcept
    {
        return typeKey == "builtin:string" || isClassReference(typeKey) || isDelegateReference(typeKey);
    }

    // Win64 aggregate ABI (ground truth: clang for x86_64-pc-windows-msvc). True for a struct
    // whose size is not 1/2/4/8 -- the sizes that travel directly as integers. Everything else
    // (3 bytes included) is passed as a pointer to a caller-owned per-call copy and returned
    // through a leading sret pointer.
    bool isIndirectStructTypeKey(std::string_view typeKey) const noexcept
    {
        if (!typeKey.starts_with("type:") || isEnumTypeKey(typeKey)) return false;
        const frontend::ProjectOopMirStructLayout* layout = structLayout(typeKey);
        if (layout == nullptr) return false;
        switch (layout->size)
        {
            case 1: case 2: case 4: case 8: return false;
            default: return true;
        }
    }

    unsigned structAlignmentFor(std::string_view typeKey) const noexcept
    {
        const frontend::ProjectOopMirStructLayout* layout = structLayout(typeKey);
        return layout != nullptr ? static_cast<unsigned>(layout->alignment) : 1U;
    }

    // Allocates a stack slot in the current function's ENTRY block regardless of where the
    // builder currently sits, and stamps the struct layout's real alignment on it. Used for the
    // caller-side per-call argument copies and sret result slots the indirect ABI needs: emitted
    // at the call site itself an alloca would re-allocate on every loop iteration and grow the
    // stack without bound, which is why clang hoists the same temporaries into the entry block.
    LLVMValueRef entryBlockAlloca(LLVMTypeRef type, unsigned alignment, const char* name)
    {
        LLVMBasicBlockRef saved = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(currentFunction_);
        LLVMValueRef first = LLVMGetFirstInstruction(entry);
        if (first != nullptr) LLVMPositionBuilderBefore(builder_, first);
        else LLVMPositionBuilderAtEnd(builder_, entry);
        LLVMValueRef slot = LLVMBuildAlloca(builder_, type, name);
        if (alignment > 1U) LLVMSetAlignment(slot, alignment);
        LLVMPositionBuilderAtEnd(builder_, saved);
        return slot;
    }

    // The parameter type a signature actually carries for `typeKey`: indirect aggregates travel
    // as an untyped pointer (clang emits a PLAIN `ptr` here -- no byval on Win64), everything
    // else as its direct carrier.
    LLVMTypeRef abiParameterType(std::string_view typeKey, std::string& error) const
    {
        if (isIndirectStructTypeKey(typeKey)) return LLVMPointerTypeInContext(context_, 0);
        return llvmType(typeKey, error);
    }

    // Wraps one already-lowered argument VALUE for the call boundary: an indirect aggregate is
    // spilled into a fresh entry-block copy owned by THIS call (Win64 contract: the callee may
    // scribble on it; re-passing the same struct farther down makes another copy -- measured on
    // clang's own output), and the pointer is what travels. Direct values pass through untouched.
    LLVMValueRef wrapIndirectArgument(LLVMValueRef value, std::string_view typeKey, std::string& error)
    {
        if (value == nullptr || !isIndirectStructTypeKey(typeKey)) return value;
        LLVMTypeRef carrier = llvmType(typeKey, error);
        if (carrier == nullptr) return nullptr;
        const unsigned alignment = structAlignmentFor(typeKey);
        LLVMValueRef copy = entryBlockAlloca(carrier, alignment, "abi.arg.copy");
        LLVMValueRef store = LLVMBuildStore(builder_, value, copy);
        if (alignment > 1U) LLVMSetAlignment(store, alignment);
        return copy;
    }

    // Attaches `sret(<carrier>) noalias align <n>` to parameter `parameterPosition` (0-based) of
    // a function or call -- the exact attribute set clang stamps on the hidden return slot.
    template <typename AddAttribute>
    void applySretAttributes(
        AddAttribute&& addAttribute, unsigned parameterPosition, std::string_view typeKey)
    {
        LLVMTypeRef carrier = nullptr;
        {
            std::string ignored;
            carrier = llvmType(typeKey, ignored);
        }
        if (carrier == nullptr) return;
        const LLVMAttributeIndex index = parameterPosition + 1U;
        const unsigned sretKind = LLVMGetEnumAttributeKindForName("sret", 4);
        const unsigned noaliasKind = LLVMGetEnumAttributeKindForName("noalias", 7);
        const unsigned alignKind = LLVMGetEnumAttributeKindForName("align", 5);
        addAttribute(index, LLVMCreateTypeAttribute(context_, sretKind, carrier));
        addAttribute(index, LLVMCreateEnumAttribute(context_, noaliasKind, 0));
        addAttribute(index, LLVMCreateEnumAttribute(context_, alignKind, structAlignmentFor(typeKey)));
    }

    // The one way a value leaves a function: through RAX for direct types, through the sret
    // slot for indirect aggregates. Both return sites (the plain exit and the EH spill-slot
    // path) go through here so the two can never disagree about the ABI.
    void emitAbiReturn(LLVMValueRef value)
    {
        if (sretStorage_ != nullptr)
        {
            LLVMBuildStore(builder_, value, sretStorage_);
            LLVMBuildRetVoid(builder_);
            return;
        }
        LLVMBuildRet(builder_, value);
    }

    static bool isPointerLike(std::string_view typeKey) noexcept
    {
        return (!typeKey.empty() && typeKey.back() == '*') ||
            typeKey == "string-literal" ||
            frontend::ProjectTypeConversion::isNativeCoreReference(typeKey);
    }

    LLVMBasicBlockRef appendBlock(const std::string& stem)
    {
        const std::string name = stem + "." + std::to_string(blockCounter_++);
        return LLVMAppendBasicBlockInContext(context_, currentFunction_, name.c_str());
    }

    // Every real toolchain (clang included) marks EVERY defined function `uwtable` on a Windows
    // x64 target, unconditionally -- not just the ones that use exceptions. Before this call
    // existed, nothing in this file ever set a single LLVM function attribute, so none of our
    // emitted functions carried it. Setting it is correct, matches what every other Windows x64
    // toolchain does, and is worth keeping on those grounds alone.
    //
    // ACCURACY NOTE (external audit item "comentarios contradictorios sobre la causa raíz",
    // 2026-08-29): this comment used to assert that a missing `uwtable` was what corrupted
    // BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md's binary at runtime -- i.e. that
    // this call was the fix for bug B-001. That claim was never checked by running anything, and it
    // is FALSE: a controlled Wine re-run proved the attribute was already present in the built
    // binary (`attributes #N = { uwtable(sync) }` on the very function that crashed) while the
    // access violation still reproduced identically. B-001's actual fix is the per-function
    // mem2reg legalization in src/LLVM/LlvmOptimizer.cpp. See
    // claude/30_bloque0_correctness_gate_b001_b003.md for the full bisection. Treat this call as
    // correct Windows x64 hygiene, NOT as the fix for any known bug.
    // Adds a simple enum function attribute by name. Null-tolerant; a name LLVM does not know is
    // ignored rather than fatal, so a future LLVM renaming degrades to "attribute not set".
    void applyFunctionAttribute(LLVMValueRef function, const char* attributeName)
    {
        if (function == nullptr || context_ == nullptr || attributeName == nullptr) return;
        const unsigned kindId = LLVMGetEnumAttributeKindForName(
            attributeName, std::strlen(attributeName));
        if (kindId == 0) return;
        LLVMAttributeRef attribute = LLVMCreateEnumAttribute(context_, kindId, 0);
        if (attribute == nullptr) return;
        LLVMAddAttributeAtIndex(function, static_cast<LLVMAttributeIndex>(~0U), attribute);
    }

    // ID 130, ARREGLADO EL 2026-09-03. `uwtable` NO es un atributo booleano: su valor es un
    // UWTableKind, y el 0 que habia aqui significa None -- es decir, "esta funcion NO tiene tabla
    // de desenrollado", que es exactamente lo contrario de lo que este metodo dice hacer.
    //
    // MEDIDO contra libLLVM-18 con la propia src/LLVM/LlvmCApi.h de este proyecto y SIN el shim
    // del contenedor (que reescribia 0 -> 2 y por tanto tapaba el sintoma). kindId de `uwtable`
    // es 87, y lo que imprime cada valor:
    //
    //     valor 0  ->  NADA. Ni linea `attributes #0`, ni `#0` en el `define`. El modulo impreso
    //                  sale degradado: LLVMPrintModuleToString devuelve un texto en el que no
    //                  aparece ni la definicion de la funcion.
    //     valor 1  ->  attributes #0 = { uwtable(sync) }
    //     valor 2  ->  attributes #0 = { uwtable }          <- Async, la forma por omision
    //     valor 3  ->  attributes #0 = { uwtable(async) }
    //
    // SE ELIGE 2, que es Async y se imprime como `uwtable` a secas. Es lo que clang-cl emite en
    // Windows x64, donde las tablas de desenrollado asincronas son el comportamiento por omision.
    //
    // UNA ADVERTENCIA SOBRE LA MEDICION, porque importa: el contenedor usa LLVM 18 y el `nsc` de
    // Windows enlaza LLVM-C.lib de C:\clang-llvm, que es LLVM 23. La correspondencia
    // valor->UWTableKind es del formato del IR y no cambia entre versiones, pero como cada una
    // trata el 0 a su manera, el efecto OBSERVABLE puede diferir -- y eso explica que en el
    // contenedor el modulo saliera roto y en Windows el binario construido pareciera correcto.
    // Por eso el arreglo se hace igual y por eso la prueba tiene que mirar el IR EN WINDOWS
    // ademas de ejecutar EH.
    //
    // Y NO se toca applyFunctionAttribute: ese solo se usa para `nounwind`, `noreturn` y
    // `nounwind` de los thunks de carga, que son atributos SIN valor, donde el 0 es lo correcto.
    void applyUwtableAttribute(LLVMValueRef function)
    {
        if (function == nullptr || context_ == nullptr) return;
        static const unsigned kindId = LLVMGetEnumAttributeKindForName("uwtable", 7);
        if (kindId == 0) return;
        // 2 = UWTableKind::Async. Ver la medicion de arriba.
        LLVMAttributeRef attribute = LLVMCreateEnumAttribute(context_, kindId, 2);
        if (attribute == nullptr) return;
        LLVMAddAttributeAtIndex(function, static_cast<LLVMAttributeIndex>(~0U), attribute);
    }

    bool terminated() const noexcept
    {
        LLVMBasicBlockRef block = LLVMGetInsertBlock(builder_);
        return block != nullptr && LLVMGetBasicBlockTerminator(block) != nullptr;
    }

    static bool fail(std::string& error, std::string message)
    {
        error = std::move(message);
        return false;
    }

    static LLVMValueRef failValue(std::string& error, std::string message)
    {
        error = std::move(message);
        return nullptr;
    }

    const frontend::ProjectNameLookup& nameLookup_;
    const frontend::ProjectMemberModel* memberModel_ = nullptr;
    const frontend::ProjectOopMirModel& mirModel_;
    const source::SourceManager* sourceManager_ = nullptr;
    DebugInformationKind debugInformation_ = DebugInformationKind::None;
    LlvmOptimizationLevel optimizationLevel_ = LlvmOptimizationLevel::O0;
    std::vector<DebugPrefixMap> debugPrefixMaps_;
    bool partitioned_ = false;
    bool ownsModuleGlobals_ = true;
    std::vector<bool> functionDefinitions_;
    std::string moduleName_;
    std::optional<std::size_t> invalidFunctionOrdinal_;
    LLVMContextRef context_ = nullptr;
    LLVMModuleRef module_ = nullptr;
    LLVMBuilderRef builder_ = nullptr;
    LLVMDIBuilderRef debugBuilder_ = nullptr;
    LLVMMetadataRef debugCompileUnit_ = nullptr;
    std::unordered_map<std::uint32_t, LLVMMetadataRef> debugFiles_;
    std::unordered_map<std::uint32_t, unsigned> debugCodeViewFileNumbers_;
    std::unordered_map<std::string, LLVMMetadataRef> debugTypes_;
    std::unordered_map<std::string, LLVMMetadataRef> debugPointeeTypes_;
    std::unordered_set<std::size_t> debugDeclaredLocals_;
    std::unordered_map<std::size_t, LLVMMetadataRef> debugCaptureVariables_;
    LLVMValueRef trapFunction_ = nullptr;
    LLVMTypeRef trapFunctionType_ = nullptr;
    LLVMValueRef rttiClassTestFunction_ = nullptr;
    LLVMValueRef rttiInterfaceTestFunction_ = nullptr;
    LLVMTypeRef rttiTestFunctionType_ = nullptr;
    LLVMValueRef allocationFunction_ = nullptr;
    LLVMTypeRef allocationFunctionType_ = nullptr;
    LLVMValueRef deallocationFunction_ = nullptr;
    LLVMTypeRef deallocationFunctionType_ = nullptr;
    LLVMValueRef consoleWriteLineFunction_ = nullptr;
    LLVMTypeRef consoleWriteLineFunctionType_ = nullptr;
    LLVMValueRef clockQueryCounterFunction_ = nullptr;
    LLVMTypeRef clockQueryCounterFunctionType_ = nullptr;
    LLVMValueRef clockQueryFrequencyFunction_ = nullptr;
    LLVMTypeRef clockQueryFrequencyFunctionType_ = nullptr;
    LLVMValueRef clockQuerySystemFileTimeFunction_ = nullptr;
    LLVMTypeRef clockQuerySystemFileTimeFunctionType_ = nullptr;
    LLVMValueRef environmentTickCount64Function_ = nullptr;
    LLVMTypeRef environmentTickCount64FunctionType_ = nullptr;
    LLVMValueRef environmentProcessorCountFunction_ = nullptr;
    LLVMTypeRef environmentProcessorCountFunctionType_ = nullptr;
    LLVMValueRef environmentExitFunction_ = nullptr;
    LLVMTypeRef environmentExitFunctionType_ = nullptr;
    LLVMValueRef environmentGetVariableFunction_ = nullptr;
    LLVMTypeRef environmentGetVariableFunctionType_ = nullptr;
    LLVMValueRef environmentSetVariableFunction_ = nullptr;
    LLVMTypeRef environmentSetVariableFunctionType_ = nullptr;
    LLVMValueRef fileExistsFunction_ = nullptr;
    LLVMTypeRef fileExistsFunctionType_ = nullptr;
    LLVMValueRef fileDeleteFunction_ = nullptr;
    LLVMTypeRef fileDeleteFunctionType_ = nullptr;
    LLVMValueRef directoryExistsFunction_ = nullptr;
    LLVMTypeRef directoryExistsFunctionType_ = nullptr;
    LLVMValueRef directoryCreateFunction_ = nullptr;
    LLVMTypeRef directoryCreateFunctionType_ = nullptr;
    LLVMValueRef fileReadAllTextFunction_ = nullptr;
    LLVMTypeRef fileGetSizeFunctionType_ = nullptr;
    LLVMValueRef fileGetSizeFunction_ = nullptr;
    LLVMTypeRef fileReadAllBytesFunctionType_ = nullptr;
    LLVMValueRef fileReadAllBytesFunction_ = nullptr;
    LLVMTypeRef fileReadBytesFunctionType_ = nullptr;
    LLVMValueRef fileReadBytesFunction_ = nullptr;
    LLVMTypeRef fileWriteAllBytesFunctionType_ = nullptr;
    LLVMValueRef fileWriteAllBytesFunction_ = nullptr;
    LLVMTypeRef environmentCurrentDirectoryFunctionType_ = nullptr;
    LLVMValueRef environmentCurrentDirectoryFunction_ = nullptr;
    LLVMTypeRef processRunFunctionType_ = nullptr;
    LLVMValueRef processRunFunction_ = nullptr;
    LLVMTypeRef fileReadAllTextFunctionType_ = nullptr;
    LLVMValueRef fileWriteAllTextFunction_ = nullptr;
    LLVMTypeRef fileWriteAllTextFunctionType_ = nullptr;
    LLVMValueRef fileAppendAllTextFunction_ = nullptr;
    LLVMTypeRef fileAppendAllTextFunctionType_ = nullptr;
    LLVMValueRef directoryDeleteFunction_ = nullptr;
    LLVMTypeRef directoryDeleteFunctionType_ = nullptr;
    LLVMValueRef pathGetFullPathFunction_ = nullptr;
    LLVMTypeRef pathGetFullPathFunctionType_ = nullptr;
    LLVMValueRef environmentGetCommandLineArgCountFunction_ = nullptr;
    LLVMTypeRef environmentGetCommandLineArgCountFunctionType_ = nullptr;
    LLVMValueRef environmentGetCommandLineArgsFunction_ = nullptr;
    LLVMTypeRef environmentGetCommandLineArgsFunctionType_ = nullptr;
    LLVMValueRef directoryGetFileCountFunction_ = nullptr;
    LLVMTypeRef directoryGetFileCountFunctionType_ = nullptr;
    LLVMValueRef directoryGetFilesFunction_ = nullptr;
    LLVMTypeRef directoryGetFilesFunctionType_ = nullptr;
    LLVMValueRef directoryGetDirectoryCountFunction_ = nullptr;
    LLVMTypeRef directoryGetDirectoryCountFunctionType_ = nullptr;
    LLVMValueRef directoryGetDirectoriesFunction_ = nullptr;
    LLVMTypeRef directoryGetDirectoriesFunctionType_ = nullptr;
    LLVMValueRef threadCreateFunction_ = nullptr;
    LLVMTypeRef threadCreateFunctionType_ = nullptr;
    LLVMValueRef threadJoinFunction_ = nullptr;
    LLVMTypeRef threadJoinFunctionType_ = nullptr;
    LLVMValueRef threadDetachFunction_ = nullptr;
    LLVMTypeRef threadDetachFunctionType_ = nullptr;
    LLVMValueRef atomicLoadInt32Function_ = nullptr;
    LLVMTypeRef atomicLoadInt32FunctionType_ = nullptr;
    LLVMValueRef atomicStoreInt32Function_ = nullptr;
    LLVMTypeRef atomicStoreInt32FunctionType_ = nullptr;
    LLVMValueRef atomicLoadInt64Function_ = nullptr;
    LLVMTypeRef atomicLoadInt64FunctionType_ = nullptr;
    LLVMValueRef atomicStoreInt64Function_ = nullptr;
    LLVMTypeRef atomicStoreInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedIncrementInt32Function_ = nullptr;
    LLVMTypeRef interlockedIncrementInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedDecrementInt32Function_ = nullptr;
    LLVMTypeRef interlockedDecrementInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedAddInt32Function_ = nullptr;
    LLVMTypeRef interlockedAddInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedExchangeInt32Function_ = nullptr;
    LLVMTypeRef interlockedExchangeInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedCompareExchangeInt32Function_ = nullptr;
    LLVMTypeRef interlockedCompareExchangeInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedIncrementInt64Function_ = nullptr;
    LLVMTypeRef interlockedIncrementInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedDecrementInt64Function_ = nullptr;
    LLVMTypeRef interlockedDecrementInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedAddInt64Function_ = nullptr;
    LLVMTypeRef interlockedAddInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedExchangeInt64Function_ = nullptr;
    LLVMTypeRef interlockedExchangeInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedCompareExchangeInt64Function_ = nullptr;
    LLVMTypeRef interlockedCompareExchangeInt64FunctionType_ = nullptr;
    LLVMValueRef mutexCreateFunction_ = nullptr;
    LLVMTypeRef mutexCreateFunctionType_ = nullptr;
    LLVMValueRef mutexLockFunction_ = nullptr;
    LLVMTypeRef mutexLockFunctionType_ = nullptr;
    LLVMValueRef mutexUnlockFunction_ = nullptr;
    LLVMTypeRef mutexUnlockFunctionType_ = nullptr;
    LLVMValueRef mutexCloseFunction_ = nullptr;
    LLVMTypeRef mutexCloseFunctionType_ = nullptr;
    LLVMValueRef getCurrentThreadIdFunction_ = nullptr;
    LLVMTypeRef getCurrentThreadIdFunctionType_ = nullptr;
    LLVMValueRef monitorEnterFunction_ = nullptr;
    LLVMTypeRef monitorEnterFunctionType_ = nullptr;
    LLVMValueRef monitorExitFunction_ = nullptr;
    LLVMTypeRef monitorExitFunctionType_ = nullptr;
    LLVMValueRef monitorPulseFunction_ = nullptr;
    LLVMTypeRef monitorPulseFunctionType_ = nullptr;
    LLVMValueRef monitorPulseAllFunction_ = nullptr;
    LLVMTypeRef monitorPulseAllFunctionType_ = nullptr;
    LLVMValueRef monitorWaitFunction_ = nullptr;
    LLVMTypeRef monitorWaitFunctionType_ = nullptr;
    LLVMValueRef semaphoreCreateFunction_ = nullptr;
    LLVMTypeRef semaphoreCreateFunctionType_ = nullptr;
    LLVMValueRef waitForHandleTimeoutFunction_ = nullptr;
    LLVMTypeRef waitForHandleTimeoutFunctionType_ = nullptr;
    LLVMValueRef semaphoreReleaseFunction_ = nullptr;
    LLVMTypeRef semaphoreReleaseFunctionType_ = nullptr;
    LLVMValueRef semaphoreCloseFunction_ = nullptr;
    LLVMTypeRef semaphoreCloseFunctionType_ = nullptr;
    LLVMValueRef eventCreateFunction_ = nullptr;
    LLVMTypeRef eventCreateFunctionType_ = nullptr;
    LLVMValueRef eventSetFunction_ = nullptr;
    LLVMTypeRef eventSetFunctionType_ = nullptr;
    LLVMValueRef eventResetFunction_ = nullptr;
    LLVMTypeRef eventResetFunctionType_ = nullptr;
    LLVMValueRef eventCloseFunction_ = nullptr;
    LLVMTypeRef eventCloseFunctionType_ = nullptr;
    // Stage 55.1 "Process".
    LLVMValueRef processGetCurrentProcessIdFunction_ = nullptr;
    LLVMTypeRef processGetCurrentProcessIdFunctionType_ = nullptr;
    LLVMValueRef processGetWorkingSetSizeFunction_ = nullptr;
    LLVMTypeRef processGetWorkingSetSizeFunctionType_ = nullptr;
    LLVMValueRef processGetKernelTimeFunction_ = nullptr;
    LLVMTypeRef processGetKernelTimeFunctionType_ = nullptr;
    LLVMValueRef processGetUserTimeFunction_ = nullptr;
    LLVMTypeRef processGetUserTimeFunctionType_ = nullptr;
    LLVMValueRef processGetPriorityClassFunction_ = nullptr;
    LLVMTypeRef processGetPriorityClassFunctionType_ = nullptr;
    LLVMValueRef processSetPriorityClassFunction_ = nullptr;
    LLVMTypeRef processSetPriorityClassFunctionType_ = nullptr;
    // Stage 55.2 "Hardware / System Info".
    LLVMValueRef systemInfoGetPageSizeFunction_ = nullptr;
    LLVMTypeRef systemInfoGetPageSizeFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetProcessorArchitectureFunction_ = nullptr;
    LLVMTypeRef systemInfoGetProcessorArchitectureFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetAllocationGranularityFunction_ = nullptr;
    LLVMTypeRef systemInfoGetAllocationGranularityFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetTotalPhysicalMemoryBytesFunction_ = nullptr;
    LLVMTypeRef systemInfoGetTotalPhysicalMemoryBytesFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetAvailablePhysicalMemoryBytesFunction_ = nullptr;
    LLVMTypeRef systemInfoGetAvailablePhysicalMemoryBytesFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetMemoryLoadPercentFunction_ = nullptr;
    LLVMTypeRef systemInfoGetMemoryLoadPercentFunctionType_ = nullptr;
    // Stage 55.3 "Diagnostics".
    LLVMValueRef debugIsDebuggerPresentFunction_ = nullptr;
    LLVMTypeRef debugIsDebuggerPresentFunctionType_ = nullptr;
    LLVMValueRef debugCaptureStackFrameCountFunction_ = nullptr;
    LLVMTypeRef debugCaptureStackFrameCountFunctionType_ = nullptr;
    LLVMValueRef debugOutputStringFunction_ = nullptr;
    LLVMTypeRef debugOutputStringFunctionType_ = nullptr;
    // Pre-Stage-56 "utilidades basicas" round.
    LLVMValueRef threadSleepFunction_ = nullptr;
    LLVMTypeRef threadSleepFunctionType_ = nullptr;
    LLVMValueRef fileCopyFunction_ = nullptr;
    LLVMTypeRef fileCopyFunctionType_ = nullptr;
    LLVMValueRef moveFileFunction_ = nullptr;
    LLVMTypeRef moveFileFunctionType_ = nullptr;
    LLVMValueRef nativeThrowFunction_ = nullptr;
    LLVMTypeRef nativeThrowFunctionType_ = nullptr;
    LLVMValueRef exceptionCurrentFunction_ = nullptr;
    LLVMTypeRef exceptionCurrentFunctionType_ = nullptr;
    LLVMValueRef exceptionClearFunction_ = nullptr;
    LLVMTypeRef exceptionClearFunctionType_ = nullptr;
    LLVMValueRef ehFilterFunction_ = nullptr;
    LLVMTypeRef ehFilterFunctionType_ = nullptr;
    std::unordered_map<std::size_t, LLVMValueRef> typedCatchFilters_;
    // Multi-catch fix -- exception type ordinal -> the stable RTTI type id its catch clause tests
    // against on the parent-side dispatch path. See lowerTryRegion's catchswitch comment.
    std::unordered_map<std::size_t, std::uint64_t> typedCatchTypeIds_;
    // Multi-catch fix -- innermost-last stack of the ENCLOSING catch clause bodies currently being
    // lowered. Catch bodies are no longer lowered inside a funclet, so `rethrow` can no longer find
    // its exception by walking funcletStack_; this is where it looks instead.
    //
    // CC1 EH-001 added the second field. `loopDepthAtEntry` is loopStack_.size() at the moment the
    // clause body started lowering, and it is what tells a `break`/`continue` whether it LEAVES this
    // catch body or merely leaves a loop/switch that was opened INSIDE it: a structured exit whose
    // target loop frame already existed when the body began lands outside the body; one whose frame
    // was pushed inside it does not. See structuredExitLeavesCatchBody().
    std::vector<ActiveCatchBody> activeCatchExceptions_;
    LLVMValueRef sehPersonalityFunction_ = nullptr;
    LLVMTypeRef sehPersonalityFunctionType_ = nullptr;
    LLVMValueRef memsetFunction_ = nullptr;
    LLVMTypeRef memsetFunctionType_ = nullptr;
    LLVMValueRef memcpyFunction_ = nullptr;
    LLVMTypeRef memcpyFunctionType_ = nullptr;
    LLVMTypeRef descriptorType_ = nullptr;
    LLVMTypeRef descriptorAccessorFunctionType_ = nullptr;
    LLVMTypeRef interfaceMapEntryType_ = nullptr;
    LLVMTypeRef interfaceMapType_ = nullptr;
    LLVMTypeRef destructionEntryFunctionType_ = nullptr;
    std::unordered_map<std::size_t, LLVMValueRef> descriptorGlobals_;
    std::unordered_map<std::size_t, FunctionInfo> descriptorAccessorFunctions_;
    // Imported descriptors are only promised through their PE-safe accessor. Local descendants
    // of those descriptors also use their accessors so the complete base chain is initialized
    // before an object publishes its most-derived descriptor pointer.
    std::unordered_set<std::size_t> runtimeDescriptorAccessorTypes_;
    std::unordered_map<std::size_t, LLVMValueRef> interfaceMapGlobals_;
    std::unordered_map<std::size_t, FunctionInfo> destructionFunctions_;
    std::unordered_map<std::size_t, FunctionInfo> objectDestructionFunctions_;
    std::unordered_map<std::size_t, LLVMValueRef> vtableGlobals_;
    std::unordered_map<std::size_t, LLVMValueRef> staticFieldGlobals_;
    std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*> staticFieldInfo_;
    std::unordered_map<std::size_t, FunctionInfo> staticFieldAccessorFunctions_;
    std::unordered_map<std::size_t, LLVMValueRef> staticInitStateGlobals_;
    // Stage 54.3 "Mutex" (fix) -- see defineTypeInitializers's own comment for the full guard
    // design this owner-thread-id global enables (distinguishing a genuine cross-thread race from
    // a same-thread self-referencing static field initializer recursing into its own cctor).
    std::unordered_map<std::size_t, LLVMValueRef> staticInitOwnerGlobals_;
    std::unordered_map<std::size_t, FunctionInfo> staticInitFunctions_;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::unordered_map<std::string, LLVMValueRef> stringGlobals_;
    // C8 "type root": one name constant per class, keyed by its global's name.
    std::unordered_map<std::string, LLVMValueRef> typeNameGlobals_;
    std::size_t stringSequence_ = 0;
    // C1.2 "import identity": plain NUL-terminated C strings (library names, external symbol
    // names) and the two Win32 entry points a runtime-bound import needs. All three stay null in
    // a project with no ambiguous import name, which is the normal case.
    std::unordered_set<std::string> definedRuntimeBoundImports_;
    std::unordered_map<std::string, LLVMValueRef> cStringGlobals_;
    std::size_t cStringSequence_ = 0;
    LLVMValueRef loadLibraryFunction_ = nullptr;
    LLVMTypeRef loadLibraryFunctionType_ = nullptr;
    LLVMValueRef getProcAddressFunction_ = nullptr;
    LLVMTypeRef getProcAddressFunctionType_ = nullptr;
    // C1.3 "callback exception boundary": one guard thunk per target function, cached by the
    // target's mangled name -- two `extern C delegate` conversions of the same method must yield
    // the SAME address, or delegate equality and host-side identity checks would break.
    std::unordered_map<std::string, FunctionInfo> callbackGuardThunks_;
    LLVMValueRef currentFunction_ = nullptr;
    const frontend::ProjectOopMirFunction* currentMirFunction_ = nullptr;
    LLVMMetadataRef currentDebugSubprogram_ = nullptr;
    LLVMMetadataRef currentDebugScope_ = nullptr;
    std::uint32_t currentDebugFileKey_ = 0;
    LLVMValueRef thisValue_ = nullptr;
    LLVMValueRef closureEnvironment_ = nullptr;
    LLVMValueRef constructionThisOverride_ = nullptr;
    std::unordered_map<std::size_t, LLVMValueRef> parameterStorage_;
    std::unordered_map<std::size_t, LLVMValueRef> localStorage_;
    // Item 323. Backing bytes for promoted class objects, separate from each local's pointer slot.
    std::unordered_map<std::size_t, LLVMValueRef> stackObjectStorage_;
    std::unordered_map<std::size_t, LLVMValueRef> parameterCaptureCells_;
    std::unordered_map<std::size_t, LLVMValueRef> localCaptureCells_;
    LLVMValueRef thisCaptureCell_ = nullptr;
    std::vector<LLVMValueRef> ownedCaptureCells_;
    LLVMBasicBlockRef captureCellExceptionalCleanupBlock_ = nullptr;
    LLVMTypeRef captureCellFunctionType_ = nullptr;
    LLVMValueRef captureCellRetainFunction_ = nullptr;
    LLVMValueRef captureCellReleaseFunction_ = nullptr;
    std::unordered_map<std::string, LLVMValueRef> closureContextDestroyFunctions_;
    std::unordered_map<std::size_t, LLVMBasicBlockRef> exceptionDispatchBlocks_;
    std::unordered_map<std::size_t, const frontend::ProjectOopMirNode*> cleanupRegions_;
    std::unordered_map<std::size_t, CachedCleanup> cleanupChainCache_;
    std::vector<std::size_t> activeCleanupRegions_;
    std::vector<ExceptionHandlerContext> exceptionHandlerStack_;
    std::vector<FuncletContext> funcletStack_;
    std::vector<LoopTargets> loopStack_;
    LLVMValueRef returnStorage_ = nullptr;
    // Win64 aggregate ABI: the incoming sret pointer when the CURRENT function returns an
    // indirect struct -- returns store through it and `ret void`. Null otherwise.
    LLVMValueRef sretStorage_ = nullptr;
    std::size_t blockCounter_ = 0;
};

std::unique_ptr<LlvmModule> ProjectOopLlvmLowerer::lower(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectOopMirModel& mirModel)
{
    error_.clear();
    if (!verifyLoadedLlvmVersion(error_))
    {
        return nullptr;
    }

    if (!nameLookup.isBuilt() || !mirModel.isBuilt())
    {
        error_ = "Macro A LLVM lowering requires built project name lookup and physical OOP MIR";
        return nullptr;
    }
    // The compatibility overload intentionally preserves the historical no-debug path used by
    // lowerer unit tests and internal callers that do not own a SourceManager.
    OopLlvmImplementation implementation(
        nameLookup, nullptr, mirModel,
        nullptr, DebugInformationKind::None, LlvmOptimizationLevel::O0);
    if (!implementation.lower(error_))
    {
        return nullptr;
    }
    return std::unique_ptr<LlvmModule>(
        new LlvmModule(implementation.releaseContext(), implementation.releaseModule()));
}

std::unique_ptr<LlvmModule> ProjectOopLlvmLowerer::lower(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const DebugPrefixMap> debugPrefixMaps)
{
    error_.clear();
    if (!verifyLoadedLlvmVersion(error_)) return nullptr;
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !mirModel.isBuilt())
    {
        error_ = "debug OOP LLVM lowering requires built project names, members and physical MIR";
        return nullptr;
    }
    OopLlvmImplementation implementation(
        nameLookup, &memberModel, mirModel, &sourceManager, debugInformation, optimizationLevel,
        debugPrefixMaps);
    if (!implementation.lower(error_)) return nullptr;
    return std::unique_ptr<LlvmModule>(
        new LlvmModule(implementation.releaseContext(), implementation.releaseModule()));
}

std::unique_ptr<LlvmModule> ProjectOopLlvmLowerer::lowerPartition(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const std::size_t> functionOrdinals,
    bool ownsModuleGlobals,
    std::string_view moduleName,
    std::span<const DebugPrefixMap> debugPrefixMaps)
{
    error_.clear();
    if (!verifyLoadedLlvmVersion(error_)) return nullptr;
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !mirModel.isBuilt())
    {
        error_ = "partitioned LLVM lowering requires built project names, members and physical MIR";
        return nullptr;
    }
    if (moduleName.empty())
    {
        error_ = "partitioned LLVM lowering requires a stable module name";
        return nullptr;
    }
    OopLlvmImplementation implementation(
        nameLookup, &memberModel, mirModel, &sourceManager, debugInformation, optimizationLevel,
        debugPrefixMaps, true, ownsModuleGlobals, functionOrdinals, moduleName);
    if (!implementation.lower(error_)) return nullptr;
    return std::unique_ptr<LlvmModule>(
        new LlvmModule(implementation.releaseContext(), implementation.releaseModule()));
}

const std::string& ProjectOopLlvmLowerer::error() const noexcept
{
    return error_;
}
}
