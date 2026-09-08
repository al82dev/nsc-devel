set(_nsc_installed_compiler "${CMAKE_INSTALL_PREFIX}/bin/nsc.exe")
set(_nsc_installation_file_list
    "${CMAKE_INSTALL_PREFIX}/share/nativesharp/toolchain/NativeSharpInstallation.files")
set(_nsc_installation_manifest
    "${CMAKE_INSTALL_PREFIX}/share/nativesharp/toolchain/NativeSharpInstallation.manifest")

if(NOT EXISTS "${_nsc_installed_compiler}")
    message(FATAL_ERROR
        "NativeSharp install verification could not find ${_nsc_installed_compiler}")
endif()

execute_process(
    COMMAND "${_nsc_installed_compiler}" --verify-installation "${CMAKE_INSTALL_PREFIX}"
    RESULT_VARIABLE _nsc_verify_result
    OUTPUT_VARIABLE _nsc_verify_output
    ERROR_VARIABLE _nsc_verify_error)

if(NOT _nsc_verify_result EQUAL 0)
    message(FATAL_ERROR
        "NativeSharp installed layout verification failed (${_nsc_verify_result}):\n"
        "${_nsc_verify_output}${_nsc_verify_error}")
endif()

string(STRIP "${_nsc_verify_output}" _nsc_verify_output)
message(STATUS "${_nsc_verify_output}")

execute_process(
    COMMAND "${_nsc_installed_compiler}" --write-installation-manifest
        "${CMAKE_INSTALL_PREFIX}"
        "${_nsc_installation_file_list}"
        "${_nsc_installation_manifest}"
    RESULT_VARIABLE _nsc_manifest_write_result
    OUTPUT_VARIABLE _nsc_manifest_write_output
    ERROR_VARIABLE _nsc_manifest_write_error)

if(NOT _nsc_manifest_write_result EQUAL 0)
    message(FATAL_ERROR
        "NativeSharp installation manifest generation failed (${_nsc_manifest_write_result}):\n"
        "${_nsc_manifest_write_output}${_nsc_manifest_write_error}")
endif()

execute_process(
    COMMAND "${_nsc_installed_compiler}" --verify-installation-manifest
        "${CMAKE_INSTALL_PREFIX}"
        "${_nsc_installation_manifest}"
    RESULT_VARIABLE _nsc_manifest_verify_result
    OUTPUT_VARIABLE _nsc_manifest_verify_output
    ERROR_VARIABLE _nsc_manifest_verify_error)

if(NOT _nsc_manifest_verify_result EQUAL 0)
    message(FATAL_ERROR
        "NativeSharp installation manifest verification failed (${_nsc_manifest_verify_result}):\n"
        "${_nsc_manifest_verify_output}${_nsc_manifest_verify_error}")
endif()

string(STRIP "${_nsc_manifest_write_output}" _nsc_manifest_write_output)
string(STRIP "${_nsc_manifest_verify_output}" _nsc_manifest_verify_output)
message(STATUS "${_nsc_manifest_write_output}")
message(STATUS "${_nsc_manifest_verify_output}")
