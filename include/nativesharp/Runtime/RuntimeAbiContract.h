#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace nativesharp::runtime
{
// Version 1 is the complete symbol set emitted by the Windows x64 runtime in the
// NativeSharp 1.0 ABI line. Signatures remain governed by runtime_abi; this
// independent version makes additions/removals visible to package and debugger
// tooling instead of leaving the set implicit in LLVM construction code.
inline constexpr std::uint16_t kRuntimeSymbolSetVersion = 1;

enum class RuntimeAbiSymbolKind : std::uint8_t
{
    Function,
    Data
};

struct RuntimeAbiSymbol
{
    std::string_view name;
    RuntimeAbiSymbolKind kind;
};

inline constexpr std::array kRuntimeAbiSymbols = {
    RuntimeAbiSymbol{"__nativesharp_alloc", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_atomic_load_int32", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_atomic_load_int64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_atomic_store_int32", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_atomic_store_int64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_clock_query_counter", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_clock_query_frequency", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_clock_query_system_filetime", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_console_write", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_debug_capture_stack_frame_count", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_debug_is_debugger_present", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_debug_output_string", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_directory_create", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_directory_delete", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_directory_exists", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_directory_get_directories", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_directory_get_directory_count", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_directory_get_file_count", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_directory_get_files", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_eh_filter", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_eh_init", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_current_directory", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_exit", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_get_command_line_arg_count", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_get_command_line_args", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_get_variable", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_processor_count", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_set_variable", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_environment_tick_count64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_event_close", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_event_create", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_event_reset", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_event_set", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_exception_clear", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_exception_current", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_exception_tls_index", RuntimeAbiSymbolKind::Data},
    RuntimeAbiSymbol{"__nativesharp_file_append_all_text", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_copy", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_delete", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_exists", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_get_size", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_read_all_bytes", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_read_all_text", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_read_bytes", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_write_all_bytes", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_file_write_all_text", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_free", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_get_current_thread_id", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_add_int32", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_add_int64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_compare_exchange_int32", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_compare_exchange_int64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_decrement_int32", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_decrement_int64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_exchange_int32", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_exchange_int64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_increment_int32", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_interlocked_increment_int64", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_monitor_enter", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_monitor_exit", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_monitor_pulse", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_monitor_pulse_all", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_monitor_wait", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_move_file", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_mutex_close", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_mutex_create", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_mutex_lock", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_mutex_unlock", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_path_get_full_path", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_process_get_current_process_id", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_process_get_kernel_time", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_process_get_priority_class", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_process_get_user_time", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_process_get_working_set_size", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_process_run", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_process_set_priority_class", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_semaphore_close", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_semaphore_create", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_semaphore_release", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_start", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_allocation_granularity", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_available_physical_memory_bytes", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_memory_load_percent", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_page_size", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_processor_architecture", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_system_info_get_total_physical_memory_bytes", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_thread_create", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_thread_detach", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_thread_join", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_thread_sleep", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_thread_trampoline", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_throw", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_trap", RuntimeAbiSymbolKind::Function},
    RuntimeAbiSymbol{"__nativesharp_wait_for_handle_timeout", RuntimeAbiSymbolKind::Function},
};
}
