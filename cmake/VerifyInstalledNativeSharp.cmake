set(_nsc_installed_compiler "${CMAKE_INSTALL_PREFIX}/bin/nsc.exe")
set(_nsc_installation_file_list
    "${CMAKE_INSTALL_PREFIX}/share/nativesharp/toolchain/NativeSharpInstallation.files")
set(_nsc_installation_manifest
    "${CMAKE_INSTALL_PREFIX}/share/nativesharp/toolchain/NativeSharpInstallation.manifest")
set(_nsc_previous_manifest
    "${CMAKE_INSTALL_PREFIX}/.nativesharp-previous.manifest")
set(_nsc_update_plan
    "${CMAKE_INSTALL_PREFIX}/.nativesharp-update.plan")
set(_nsc_had_previous_manifest FALSE)

# Preserve the old ownership proof before the new compiler rewrites the manifest. The temporary
# files live outside every managed tree and are deleted on the successful path. They are not used
# as a source of truth: the update-plan command hashes the preserved manifest and the apply command
# checks that binding again before it removes a single path.
if(EXISTS "${_nsc_installation_manifest}")
    file(COPY_FILE
        "${_nsc_installation_manifest}"
        "${_nsc_previous_manifest}"
        ONLY_IF_DIFFERENT)
    set(_nsc_had_previous_manifest TRUE)
endif()

if(NOT EXISTS "${_nsc_installed_compiler}")
    file(REMOVE "${_nsc_previous_manifest}" "${_nsc_update_plan}")
    message(FATAL_ERROR
        "NativeSharp install verification could not find ${_nsc_installed_compiler}")
endif()

execute_process(
    COMMAND "${_nsc_installed_compiler}" --write-installation-manifest
        "${CMAKE_INSTALL_PREFIX}"
        "${_nsc_installation_file_list}"
        "${_nsc_installation_manifest}"
    RESULT_VARIABLE _nsc_manifest_write_result
    OUTPUT_VARIABLE _nsc_manifest_write_output
    ERROR_VARIABLE _nsc_manifest_write_error)

if(NOT _nsc_manifest_write_result EQUAL 0)
    file(REMOVE "${_nsc_previous_manifest}" "${_nsc_update_plan}")
    message(FATAL_ERROR
        "NativeSharp installation manifest generation failed (${_nsc_manifest_write_result}):\n"
        "${_nsc_manifest_write_output}${_nsc_manifest_write_error}")
endif()

if(_nsc_had_previous_manifest)
    execute_process(
        COMMAND "${_nsc_installed_compiler}" --write-installation-update-plan
            "${_nsc_previous_manifest}"
            "${_nsc_installation_manifest}"
            "${_nsc_update_plan}"
        RESULT_VARIABLE _nsc_update_plan_result
        OUTPUT_VARIABLE _nsc_update_plan_output
        ERROR_VARIABLE _nsc_update_plan_error)

    if(NOT _nsc_update_plan_result EQUAL 0)
        file(REMOVE "${_nsc_previous_manifest}" "${_nsc_update_plan}")
        message(FATAL_ERROR
            "NativeSharp update planning failed (${_nsc_update_plan_result}):\n"
            "${_nsc_update_plan_output}${_nsc_update_plan_error}")
    endif()

    execute_process(
        COMMAND "${_nsc_installed_compiler}" --apply-installation-removal-plan
            "${CMAKE_INSTALL_PREFIX}"
            "${_nsc_previous_manifest}"
            "${_nsc_installation_manifest}"
            "${_nsc_update_plan}"
        RESULT_VARIABLE _nsc_update_apply_result
        OUTPUT_VARIABLE _nsc_update_apply_output
        ERROR_VARIABLE _nsc_update_apply_error)

    if(NOT _nsc_update_apply_result EQUAL 0)
        file(REMOVE "${_nsc_previous_manifest}" "${_nsc_update_plan}")
        message(FATAL_ERROR
            "NativeSharp update cleanup failed (${_nsc_update_apply_result}):\n"
            "${_nsc_update_apply_output}${_nsc_update_apply_error}")
    endif()

    string(STRIP "${_nsc_update_plan_output}" _nsc_update_plan_output)
    string(STRIP "${_nsc_update_apply_output}" _nsc_update_apply_output)
    message(STATUS "${_nsc_update_plan_output}")
    message(STATUS "${_nsc_update_apply_output}")
    file(REMOVE "${_nsc_previous_manifest}" "${_nsc_update_plan}")
endif()

# Validate the final tree only after files retired by the prior owned inventory are gone. Running
# this check earlier would misclassify a legitimate old payload as an unexpected foreign file and
# make a clean in-place upgrade impossible.
execute_process(
    COMMAND "${_nsc_installed_compiler}" --verify-installation "${CMAKE_INSTALL_PREFIX}"
    RESULT_VARIABLE _nsc_verify_result
    OUTPUT_VARIABLE _nsc_verify_output
    ERROR_VARIABLE _nsc_verify_error)

if(NOT _nsc_verify_result EQUAL 0)
    file(REMOVE "${_nsc_previous_manifest}" "${_nsc_update_plan}")
    message(FATAL_ERROR
        "NativeSharp installed layout verification failed (${_nsc_verify_result}):\n"
        "${_nsc_verify_output}${_nsc_verify_error}")
endif()

string(STRIP "${_nsc_verify_output}" _nsc_verify_output)
message(STATUS "${_nsc_verify_output}")

execute_process(
    COMMAND "${_nsc_installed_compiler}" --verify-installation-manifest
        "${CMAKE_INSTALL_PREFIX}"
        "${_nsc_installation_manifest}"
    RESULT_VARIABLE _nsc_manifest_verify_result
    OUTPUT_VARIABLE _nsc_manifest_verify_output
    ERROR_VARIABLE _nsc_manifest_verify_error)

if(NOT _nsc_manifest_verify_result EQUAL 0)
    file(REMOVE "${_nsc_previous_manifest}" "${_nsc_update_plan}")
    message(FATAL_ERROR
        "NativeSharp installation manifest verification failed (${_nsc_manifest_verify_result}):\n"
        "${_nsc_manifest_verify_output}${_nsc_manifest_verify_error}")
endif()

string(STRIP "${_nsc_manifest_write_output}" _nsc_manifest_write_output)
string(STRIP "${_nsc_manifest_verify_output}" _nsc_manifest_verify_output)
message(STATUS "${_nsc_manifest_write_output}")
message(STATUS "${_nsc_manifest_verify_output}")
