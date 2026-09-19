if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED LINKER_EXECUTABLE)
    message(FATAL_ERROR "SH02 workload requires nsc, source root, output dir and linker")
endif()

set(project "${SOURCE_ROOT}/selfhost/tests/host/CompilerHostWorkload/CompilerHostWorkload.nsproj")
set(corpus "${SOURCE_ROOT}/selfhost/tests/host/corpus")
set(executable "${OUTPUT_DIR}/compiler-host-workload.exe")
set(active_linker "${LINKER_EXECUTABLE}")
if(DEFINED ENV{NSC_LLD_LINK} AND EXISTS "$ENV{NSC_LLD_LINK}")
    set(active_linker "$ENV{NSC_LLD_LINK}")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${project}" -O0 -o "${executable}"
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_err)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "SH02 workload compilation failed:\n${compile_out}${compile_err}")
endif()

foreach(iteration RANGE 1 2)
    set(linker_output "${OUTPUT_DIR}/linker-${iteration}.txt")
    set(metrics_output "${OUTPUT_DIR}/metrics-${iteration}.txt")
    execute_process(
        COMMAND "${executable}" "${corpus}" "${active_linker}" "${linker_output}" "${metrics_output}"
        RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err)
    if(NOT run_result EQUAL 42)
        message(FATAL_ERROR "SH02 workload run ${iteration} returned ${run_result}:\n${run_out}${run_err}")
    endif()
    if(NOT iteration EQUAL 1)
        if(NOT run_out STREQUAL first_out OR NOT run_err STREQUAL first_err)
            message(FATAL_ERROR "SH02 workload output is not deterministic")
        endif()
    else()
        set(first_out "${run_out}")
        set(first_err "${run_err}")
    endif()
    if(NOT EXISTS "${metrics_output}")
        message(FATAL_ERROR "SH02 workload run ${iteration} did not write its metrics")
    endif()
    file(READ "${metrics_output}" run_metrics)
    string(STRIP "${run_metrics}" run_metrics)
    if(NOT run_metrics MATCHES "^elapsed_ms=[0-9]+([.][0-9]+)?$")
        message(FATAL_ERROR "SH02 workload run ${iteration} wrote invalid metrics: ${run_metrics}")
    endif()
endforeach()

if(NOT first_out MATCHES
   "SH02 files=3 tokens=[0-9]+ allocations=[0-9]+ arena_peak_bytes=[0-9]+")
    message(FATAL_ERROR "SH02 workload did not emit its deterministic summary")
endif()
if(NOT first_err MATCHES "NSH0001")
    message(FATAL_ERROR "SH02 workload did not emit its structured diagnostic")
endif()

string(REGEX MATCH "tokens=([0-9]+)" ignored "${first_out}")
set(token_count "${CMAKE_MATCH_1}")
string(REGEX MATCH "allocations=([0-9]+)" ignored "${first_out}")
set(allocation_count "${CMAKE_MATCH_1}")
string(REGEX MATCH "arena_peak_bytes=([0-9]+)" ignored "${first_out}")
set(arena_peak_bytes "${CMAKE_MATCH_1}")
string(REGEX MATCH "elapsed_ms=([0-9]+([.][0-9]+)?)" ignored "${run_metrics}")
set(elapsed_ms "${CMAKE_MATCH_1}")
file(WRITE "${OUTPUT_DIR}/workload-report.json"
    "{\n"
    "  \"schema\": \"nativesharp-selfhost-host-workload\",\n"
    "  \"version\": 1,\n"
    "  \"status\": \"passed\",\n"
    "  \"files\": 3,\n"
    "  \"tokens\": ${token_count},\n"
    "  \"allocations\": ${allocation_count},\n"
    "  \"arena_peak_bytes\": ${arena_peak_bytes},\n"
    "  \"elapsed_ms\": ${elapsed_ms},\n"
    "  \"maximum_elapsed_ms\": 30000,\n"
    "  \"runs\": 2,\n"
    "  \"deterministic_output\": true,\n"
    "  \"exit_code\": 42\n"
    "}\n")
