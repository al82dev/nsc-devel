cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS NSC_SOURCE NSC_BUILD_A NSC_BUILD_B NSC_PREFIX_A NSC_PREFIX_B)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required")
    endif()
endforeach()

foreach(_fresh IN ITEMS NSC_BUILD_A NSC_BUILD_B NSC_PREFIX_A NSC_PREFIX_B)
    if(EXISTS "${${_fresh}}")
        file(GLOB _contents "${${_fresh}}/*")
        if(_contents)
            message(FATAL_ERROR
                "${_fresh} must name a new or empty directory: ${${_fresh}}")
        endif()
    endif()
endforeach()

if(NOT DEFINED NSC_GENERATOR OR "${NSC_GENERATOR}" STREQUAL "")
    set(NSC_GENERATOR "Ninja")
endif()

function(_nsc_run _label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "${_label} failed (${_result}):\n${_output}${_error}")
    endif()
    string(STRIP "${_output}" _output)
    if(NOT "${_output}" STREQUAL "")
        message(STATUS "${_label}: ${_output}")
    endif()
endfunction()

set(_nsc_configure_arguments -DNSC_BUILD_ID_OVERRIDE=e9e10e9e10e9e10e)
if(NSC_FORCE_MONOLITHIC_LLVM)
    list(APPEND _nsc_configure_arguments -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE)
endif()
if(DEFINED NSC_LLVM_LIBRARY AND NOT "${NSC_LLVM_LIBRARY}" STREQUAL "")
    list(APPEND _nsc_configure_arguments
        "-DNSC_LLVM_MONOLITHIC_LIBRARY=${NSC_LLVM_LIBRARY}")
endif()
if(DEFINED NSC_CXX_COMPILER AND NOT "${NSC_CXX_COMPILER}" STREQUAL "")
    list(APPEND _nsc_configure_arguments
        "-DCMAKE_CXX_COMPILER=${NSC_CXX_COMPILER}")
endif()

foreach(_side IN ITEMS A B)
    _nsc_run(
        "configure independent build ${_side}"
        "${CMAKE_COMMAND}" -S "${NSC_SOURCE}" -B "${NSC_BUILD_${_side}}"
        -G "${NSC_GENERATOR}"
        -DCMAKE_BUILD_TYPE=Release
        -DNSC_BUILD_TESTS=OFF
        -DNSC_WARNINGS_AS_ERRORS=ON
        ${_nsc_configure_arguments})
    _nsc_run(
        "compile independent build ${_side}"
        "${CMAKE_COMMAND}" --build "${NSC_BUILD_${_side}}" --target nsc)
    _nsc_run(
        "install independent build ${_side}"
        "${CMAKE_COMMAND}" --install "${NSC_BUILD_${_side}}"
        --prefix "${NSC_PREFIX_${_side}}")
endforeach()

set(_manifest_a
    "${NSC_PREFIX_A}/share/nativesharp/toolchain/NativeSharpInstallation.manifest")
set(_manifest_b
    "${NSC_PREFIX_B}/share/nativesharp/toolchain/NativeSharpInstallation.manifest")
execute_process(
    COMMAND "${NSC_PREFIX_A}/bin/nsc.exe" --compare-installation-manifests
        "${_manifest_a}" "${_manifest_b}"
    RESULT_VARIABLE _compare_result
    OUTPUT_VARIABLE _compare_output
    ERROR_VARIABLE _compare_error)
if(NOT _compare_result EQUAL 0)
    message(FATAL_ERROR
        "independent NativeSharp packages differ (${_compare_result}):\n"
        "${_compare_output}${_compare_error}")
endif()
string(STRIP "${_compare_output}" _compare_output)
file(SHA256 "${NSC_PREFIX_A}/bin/nsc.exe" _nsc_executable_a_sha256)
file(SHA256 "${NSC_PREFIX_B}/bin/nsc.exe" _nsc_executable_b_sha256)
if(NOT _nsc_executable_a_sha256 STREQUAL _nsc_executable_b_sha256)
    message(FATAL_ERROR
        "independent nsc.exe binaries differ after fixing only their build identity:\n"
        "A=${_nsc_executable_a_sha256}\nB=${_nsc_executable_b_sha256}")
endif()
string(APPEND _compare_output
    "\nstrict_compiler_binary=identical"
    "\ncompiler_sha256=${_nsc_executable_a_sha256}")
if(DEFINED NSC_REPORT AND NOT "${NSC_REPORT}" STREQUAL "")
    get_filename_component(_report_parent "${NSC_REPORT}" DIRECTORY)
    file(MAKE_DIRECTORY "${_report_parent}")
    file(WRITE "${NSC_REPORT}" "${_compare_output}\n")
endif()
message(STATUS "independent NativeSharp packages: ${_compare_output}")
