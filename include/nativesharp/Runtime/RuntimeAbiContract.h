#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::runtime
{
// Runtime ABI and symbol-set versions live with the contract they version.  The
// metadata codec aliases these values; it does not own a second copy.
inline constexpr std::uint16_t kRuntimeAbiMajor = 1;
inline constexpr std::uint16_t kRuntimeAbiMinor = 0;

// Version 2 completes the NativeSharp-prefixed Windows x64 symbol inventory and
// records the exact opaque-pointer LLVM type of every entry.
inline constexpr std::uint16_t kRuntimeSymbolSetVersion = 2;

enum class RuntimeAbiSymbolKind : std::uint8_t
{
    Function,
    Data
};

enum class RuntimeAbiImageKind : std::uint8_t
{
    Executable,
    SupportLibrary
};

struct RuntimeAbiSymbol
{
    std::string_view name;
    RuntimeAbiSymbolKind kind;
    // Canonical opaque-pointer LLVM type. Parameter names and implementation
    // details are deliberately excluded: this is the binary boundary.
    std::string_view llvmType;
};

inline constexpr std::array kRuntimeAbiSymbols = {
    RuntimeAbiSymbol{"__nativesharp_alloc", RuntimeAbiSymbolKind::Function, "ptr(i64)"},
    RuntimeAbiSymbol{"__nativesharp_atomic_load_int32", RuntimeAbiSymbolKind::Function, "i32(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_atomic_load_int64", RuntimeAbiSymbolKind::Function, "i64(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_atomic_store_int32", RuntimeAbiSymbolKind::Function, "void(ptr,i32)"},
    RuntimeAbiSymbol{"__nativesharp_atomic_store_int64", RuntimeAbiSymbolKind::Function, "void(ptr,i64)"},
    RuntimeAbiSymbol{"__nativesharp_clock_query_counter", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_clock_query_frequency", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_clock_query_system_filetime", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_console_write", RuntimeAbiSymbolKind::Function, "void(ptr,i32)"},
    RuntimeAbiSymbol{"__nativesharp_debug_capture_stack_frame_count", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_debug_is_debugger_present", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_debug_output_string", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_directory_create", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_directory_delete", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_directory_exists", RuntimeAbiSymbolKind::Function, "i1(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_directory_get_directories", RuntimeAbiSymbolKind::Function, "i32(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_directory_get_directory_count", RuntimeAbiSymbolKind::Function, "i32(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_directory_get_file_count", RuntimeAbiSymbolKind::Function, "i32(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_directory_get_files", RuntimeAbiSymbolKind::Function, "i32(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_eh_filter", RuntimeAbiSymbolKind::Function, "i32(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_eh_init", RuntimeAbiSymbolKind::Function, "i32()"},
    RuntimeAbiSymbol{"__nativesharp_environment_current_directory", RuntimeAbiSymbolKind::Function, "ptr()"},
    RuntimeAbiSymbol{"__nativesharp_environment_exit", RuntimeAbiSymbolKind::Function, "void(i32)"},
    RuntimeAbiSymbol{"__nativesharp_environment_get_command_line_arg_count", RuntimeAbiSymbolKind::Function, "i32()"},
    RuntimeAbiSymbol{"__nativesharp_environment_get_command_line_args", RuntimeAbiSymbolKind::Function, "i32(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_environment_get_variable", RuntimeAbiSymbolKind::Function, "ptr(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_environment_processor_count", RuntimeAbiSymbolKind::Function, "i32()"},
    RuntimeAbiSymbol{"__nativesharp_environment_set_variable", RuntimeAbiSymbolKind::Function, "void(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_environment_tick_count64", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_event_close", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_event_create", RuntimeAbiSymbolKind::Function, "i64(i1,i1)"},
    RuntimeAbiSymbol{"__nativesharp_event_create_named", RuntimeAbiSymbolKind::Function, "i64(i1,i1,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_event_reset", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_event_set", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_exception_clear", RuntimeAbiSymbolKind::Function, "void()"},
    RuntimeAbiSymbol{"__nativesharp_exception_current", RuntimeAbiSymbolKind::Function, "ptr()"},
    RuntimeAbiSymbol{"__nativesharp_exception_tls_index", RuntimeAbiSymbolKind::Data, "i32"},
    RuntimeAbiSymbol{"__nativesharp_file_append_all_text", RuntimeAbiSymbolKind::Function, "void(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_file_copy", RuntimeAbiSymbolKind::Function, "i1(ptr,ptr,i1)"},
    RuntimeAbiSymbol{"__nativesharp_file_delete", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_file_exists", RuntimeAbiSymbolKind::Function, "i1(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_file_get_size", RuntimeAbiSymbolKind::Function, "i64(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_file_read_all_bytes", RuntimeAbiSymbolKind::Function, "ptr(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_file_read_all_text", RuntimeAbiSymbolKind::Function, "ptr(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_file_read_bytes", RuntimeAbiSymbolKind::Function, "i32(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_file_write_all_bytes", RuntimeAbiSymbolKind::Function, "i1(ptr,ptr,i32)"},
    RuntimeAbiSymbol{"__nativesharp_file_write_all_text", RuntimeAbiSymbolKind::Function, "void(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_free", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_get_current_thread_id", RuntimeAbiSymbolKind::Function, "i32()"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_add_int32", RuntimeAbiSymbolKind::Function, "i32(ptr,i32)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_add_int64", RuntimeAbiSymbolKind::Function, "i64(ptr,i64)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_compare_exchange_int32", RuntimeAbiSymbolKind::Function, "i32(ptr,i32,i32)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_compare_exchange_int64", RuntimeAbiSymbolKind::Function, "i64(ptr,i64,i64)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_decrement_int32", RuntimeAbiSymbolKind::Function, "i32(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_decrement_int64", RuntimeAbiSymbolKind::Function, "i64(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_exchange_int32", RuntimeAbiSymbolKind::Function, "i32(ptr,i32)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_exchange_int64", RuntimeAbiSymbolKind::Function, "i64(ptr,i64)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_increment_int32", RuntimeAbiSymbolKind::Function, "i32(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_interlocked_increment_int64", RuntimeAbiSymbolKind::Function, "i64(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_monitor_enter", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_monitor_exit", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_monitor_pulse", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_monitor_pulse_all", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_monitor_wait", RuntimeAbiSymbolKind::Function, "i1(ptr,ptr,i32)"},
    RuntimeAbiSymbol{"__nativesharp_move_file", RuntimeAbiSymbolKind::Function, "i1(ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_mutex_close", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_mutex_create", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_mutex_create_named", RuntimeAbiSymbolKind::Function, "i64(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_mutex_lock", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_mutex_unlock", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_path_get_full_path", RuntimeAbiSymbolKind::Function, "ptr(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_process_get_current_process_id", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_process_get_kernel_time", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_process_get_priority_class", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_process_get_user_time", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_process_get_working_set_size", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_process_run", RuntimeAbiSymbolKind::Function, "i32(ptr,ptr,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_process_set_priority_class", RuntimeAbiSymbolKind::Function, "void(i32)"},
    RuntimeAbiSymbol{"__nativesharp_semaphore_close", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_semaphore_create", RuntimeAbiSymbolKind::Function, "i64(i32,i32)"},
    RuntimeAbiSymbol{"__nativesharp_semaphore_create_named", RuntimeAbiSymbolKind::Function, "i64(i32,i32,ptr)"},
    RuntimeAbiSymbol{"__nativesharp_semaphore_release", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_start", RuntimeAbiSymbolKind::Function, "void()"},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_allocation_granularity", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_available_physical_memory_bytes", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_memory_load_percent", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_page_size", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_processor_architecture", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_total_physical_memory_bytes", RuntimeAbiSymbolKind::Function, "i64()"},
    RuntimeAbiSymbol{"__nativesharp_thread_create", RuntimeAbiSymbolKind::Function, "i64(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_thread_detach", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_thread_join", RuntimeAbiSymbolKind::Function, "void(i64)"},
    RuntimeAbiSymbol{"__nativesharp_thread_sleep", RuntimeAbiSymbolKind::Function, "void(i32)"},
    RuntimeAbiSymbol{"__nativesharp_thread_trampoline", RuntimeAbiSymbolKind::Function, "i32(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_throw", RuntimeAbiSymbolKind::Function, "void(ptr)"},
    RuntimeAbiSymbol{"__nativesharp_trap", RuntimeAbiSymbolKind::Function, "void(i32)"},
    RuntimeAbiSymbol{"__nativesharp_wait_for_handle_timeout", RuntimeAbiSymbolKind::Function, "i1(i64,i32)"},
};

// Stable, line-oriented representation consumed by release tooling and by the
// independent runtime packaging introduced after this contract.
std::string runtimeAbiContractManifest();
std::uint64_t runtimeAbiContractFingerprint();
std::string runtimeAbiContractFingerprintHex();
bool writeRuntimeAbiContract(const std::filesystem::path& destination, std::string& error);
bool verifyRuntimeAbiContractFile(const std::filesystem::path& source, std::string& error);

// Verifies both directions: every contracted symbol has the exact type and no
// uncontracted NativeSharp-prefixed definition leaks from the runtime image.
bool verifyRuntimeAbiContract(
    std::string_view llvmIr,
    RuntimeAbiImageKind imageKind,
    std::string& error);
}
