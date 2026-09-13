if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED CLANG_C OR NOT EXISTS "${CLANG_C}")
    message(FATAL_ERROR "CLANG_C must name the independent C host compiler")
endif()
if(NOT DEFINED CLANG_CXX OR NOT EXISTS "${CLANG_CXX}")
    message(FATAL_ERROR "CLANG_CXX must name the independent C++ host compiler")
endif()
if(NOT DEFINED LLD_LINK OR NOT EXISTS "${LLD_LINK}")
    message(FATAL_ERROR "LLD_LINK must name the PE linker")
endif()
if(NOT DEFINED ASSEMBLY_ABI_SOURCE_DIR OR
   NOT EXISTS "${ASSEMBLY_ABI_SOURCE_DIR}/AssemblyAbi.nsproj" OR
   NOT EXISTS "${ASSEMBLY_ABI_SOURCE_DIR}/host.c" OR
   NOT EXISTS "${ASSEMBLY_ABI_SOURCE_DIR}/host.cpp")
    message(FATAL_ERROR "ASSEMBLY_ABI_SOURCE_DIR is incomplete")
endif()
if(NOT DEFINED ASSEMBLY_ABI_OUTPUT_DIR)
    message(FATAL_ERROR "ASSEMBLY_ABI_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${ASSEMBLY_ABI_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${ASSEMBLY_ABI_OUTPUT_DIR}")
set(dll "${ASSEMBLY_ABI_OUTPUT_DIR}/AssemblyAbi.dll")
set(import_library "${ASSEMBLY_ABI_OUTPUT_DIR}/AssemblyAbi.lib")
set(header "${ASSEMBLY_ABI_OUTPUT_DIR}/AssemblyAbi.h")

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-dll
        "${ASSEMBLY_ABI_SOURCE_DIR}/AssemblyAbi.nsproj" -O0 -o "${dll}"
    RESULT_VARIABLE producer_result
    OUTPUT_VARIABLE producer_output
    ERROR_VARIABLE producer_error)
if(NOT producer_result EQUAL 0 OR
   NOT EXISTS "${dll}" OR NOT EXISTS "${import_library}" OR NOT EXISTS "${header}")
    message(FATAL_ERROR
        "O0 NativeSharp assembly ABI producer failed (${producer_result})\n"
        "stdout:\n${producer_output}\nstderr:\n${producer_error}")
endif()

foreach(language IN ITEMS c cpp)
    if(language STREQUAL "c")
        set(compiler "${CLANG_C}")
        set(source "${ASSEMBLY_ABI_SOURCE_DIR}/host.c")
        set(language_flags -std=c11)
    else()
        set(compiler "${CLANG_CXX}")
        set(source "${ASSEMBLY_ABI_SOURCE_DIR}/host.cpp")
        set(language_flags -std=c++20 -fno-exceptions -fno-rtti)
    endif()
    set(object "${ASSEMBLY_ABI_OUTPUT_DIR}/host_${language}.obj")
    set(executable "${ASSEMBLY_ABI_OUTPUT_DIR}/host_${language}.exe")
    execute_process(
        COMMAND "${compiler}" --target=x86_64-pc-windows-msvc -ffreestanding
            -fno-stack-protector -Wall -Wextra -Werror ${language_flags}
            -I "${ASSEMBLY_ABI_OUTPUT_DIR}" -c "${source}" -o "${object}"
        RESULT_VARIABLE host_compile_result
        OUTPUT_VARIABLE host_compile_output
        ERROR_VARIABLE host_compile_error)
    if(NOT host_compile_result EQUAL 0 OR NOT EXISTS "${object}")
        message(FATAL_ERROR
            "${language} host compilation failed (${host_compile_result})\n"
            "stdout:\n${host_compile_output}\nstderr:\n${host_compile_error}")
    endif()

    execute_process(
        COMMAND "${LLD_LINK}" /entry:main /subsystem:console /nodefaultlib /machine:x64
            "/out:${executable}" "${object}" "${import_library}"
        RESULT_VARIABLE host_link_result
        OUTPUT_VARIABLE host_link_output
        ERROR_VARIABLE host_link_error)
    if(NOT host_link_result EQUAL 0 OR NOT EXISTS "${executable}")
        message(FATAL_ERROR
            "${language} host linkage failed (${host_link_result})\n"
            "stdout:\n${host_link_output}\nstderr:\n${host_link_error}")
    endif()

    execute_process(
        COMMAND "${executable}"
        WORKING_DIRECTORY "${ASSEMBLY_ABI_OUTPUT_DIR}"
        RESULT_VARIABLE host_result
        OUTPUT_VARIABLE host_output
        ERROR_VARIABLE host_error)
    if(NOT host_result EQUAL 42)
        message(FATAL_ERROR
            "${language} host returned ${host_result}, expected 42\n"
            "stdout:\n${host_output}\nstderr:\n${host_error}")
    endif()
endforeach()

message(STATUS "NativeSharp O0 integrated-assembly C/C++ ABI verified")
