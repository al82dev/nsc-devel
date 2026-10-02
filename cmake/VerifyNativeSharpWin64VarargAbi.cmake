if(NOT DEFINED NSC_EXECUTABLE OR NOT EXISTS "${NSC_EXECUTABLE}")
    message(FATAL_ERROR "NSC_EXECUTABLE must name the compiler under test")
endif()
if(NOT DEFINED CLANG_C OR NOT EXISTS "${CLANG_C}")
    message(FATAL_ERROR "CLANG_C must name the independent C host compiler")
endif()
if(NOT DEFINED CLANG_CXX OR NOT EXISTS "${CLANG_CXX}")
    message(FATAL_ERROR "CLANG_CXX must name the independent C++ host compiler")
endif()
if(NOT DEFINED NSLINK_EXECUTABLE OR NOT EXISTS "${NSLINK_EXECUTABLE}")
    message(FATAL_ERROR "NSLINK_EXECUTABLE must name NSLink")
endif()
if(NOT DEFINED VARARG_ABI_SOURCE_DIR OR
   NOT EXISTS "${VARARG_ABI_SOURCE_DIR}/host.c" OR
   NOT EXISTS "${VARARG_ABI_SOURCE_DIR}/host.cpp" OR
   NOT EXISTS "${VARARG_ABI_SOURCE_DIR}/Consumer.ns.in" OR
   NOT EXISTS "${VARARG_ABI_SOURCE_DIR}/Consumer.nsproj.in")
    message(FATAL_ERROR "VARARG_ABI_SOURCE_DIR is incomplete")
endif()
if(NOT DEFINED VARARG_ABI_OUTPUT_DIR)
    message(FATAL_ERROR "VARARG_ABI_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${VARARG_ABI_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${VARARG_ABI_OUTPUT_DIR}")

foreach(language IN ITEMS c cpp)
    set(case_dir "${VARARG_ABI_OUTPUT_DIR}/${language}")
    file(MAKE_DIRECTORY "${case_dir}")
    if(language STREQUAL "c")
        set(compiler "${CLANG_C}")
        set(source "${VARARG_ABI_SOURCE_DIR}/host.c")
        set(language_flags -std=c11)
        set(HOST_DLL "Item333HostC.dll")
        set(PROJECT_NAME "Item333ConsumerC")
    else()
        set(compiler "${CLANG_CXX}")
        set(source "${VARARG_ABI_SOURCE_DIR}/host.cpp")
        set(language_flags -std=c++20 -fno-exceptions -fno-rtti)
        set(HOST_DLL "Item333HostCpp.dll")
        set(PROJECT_NAME "Item333ConsumerCpp")
    endif()

    set(host_object "${case_dir}/host.obj")
    set(host_dll "${case_dir}/${HOST_DLL}")
    set(host_library "${case_dir}/Item333Host.lib")
    execute_process(
        COMMAND "${compiler}" --target=x86_64-pc-windows-msvc -ffreestanding
            -fno-stack-protector -Wall -Wextra -Werror ${language_flags}
            -c "${source}" -o "${host_object}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_output
        ERROR_VARIABLE compile_error)
    if(NOT compile_result EQUAL 0 OR NOT EXISTS "${host_object}")
        message(FATAL_ERROR
            "${language} variadic host compilation failed (${compile_result})\n"
            "stdout:\n${compile_output}\nstderr:\n${compile_error}")
    endif()

    execute_process(
        COMMAND "${NSLINK_EXECUTABLE}" /dll /noentry /nodefaultlib /machine:x64
            "/out:${host_dll}" "/implib:${host_library}" /export:NSVarargProbe "${host_object}"
        RESULT_VARIABLE link_result
        OUTPUT_VARIABLE link_output
        ERROR_VARIABLE link_error)
    if(NOT link_result EQUAL 0 OR NOT EXISTS "${host_dll}" OR NOT EXISTS "${host_library}")
        message(FATAL_ERROR
            "${language} variadic host linkage failed (${link_result})\n"
            "stdout:\n${link_output}\nstderr:\n${link_error}")
    endif()

    configure_file("${VARARG_ABI_SOURCE_DIR}/Consumer.ns.in" "${case_dir}/Consumer.ns" @ONLY)
    configure_file(
        "${VARARG_ABI_SOURCE_DIR}/Consumer.nsproj.in"
        "${case_dir}/${PROJECT_NAME}.nsproj" @ONLY)

    foreach(optimization IN ITEMS O0 O2)
        set(executable "${case_dir}/${PROJECT_NAME}_${optimization}.exe")
        execute_process(
            COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
                "${case_dir}/${PROJECT_NAME}.nsproj" "-${optimization}" -o "${executable}"
            RESULT_VARIABLE producer_result
            OUTPUT_VARIABLE producer_output
            ERROR_VARIABLE producer_error)
        if(NOT producer_result EQUAL 0 OR NOT EXISTS "${executable}")
            message(FATAL_ERROR
                "NativeSharp ${language} variadic consumer ${optimization} failed (${producer_result})\n"
                "stdout:\n${producer_output}\nstderr:\n${producer_error}")
        endif()

        execute_process(
            COMMAND "${executable}"
            WORKING_DIRECTORY "${case_dir}"
            RESULT_VARIABLE run_result
            OUTPUT_VARIABLE run_output
            ERROR_VARIABLE run_error)
        if(NOT run_result EQUAL 42)
            message(FATAL_ERROR
                "NativeSharp -> ${language} Win64 vararg execution returned ${run_result}, expected 42\n"
                "stdout:\n${run_output}\nstderr:\n${run_error}")
        endif()
    endforeach()
endforeach()

message(STATUS "NativeSharp Win64 variadic ABI verified against independent C11 and C++20 hosts")
