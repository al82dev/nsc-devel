if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR
   NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "bootstrap profile verification requires nsc, Python, source root and output dir")
endif()

set(profile "${SOURCE_ROOT}/selfhost/bootstrap/bootstrap-profile.json")
set(positive_root "${SOURCE_ROOT}/selfhost/tests/profile/positive")
set(project "${positive_root}/compiler_skeleton/compiler_skeleton.nsproj")
set(executable "${OUTPUT_DIR}/bootstrap-profile-positive.exe")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(REMOVE "${executable}" "${executable}.pdb")

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B
        "${SOURCE_ROOT}/selfhost/bootstrap/verify_profile.py"
        --profile "${profile}"
        --source-root "${SOURCE_ROOT}/selfhost/nsc"
        --source-root "${positive_root}"
        --quiet
    RESULT_VARIABLE profile_result
    OUTPUT_VARIABLE profile_stdout
    ERROR_VARIABLE profile_stderr)
if(NOT profile_result EQUAL 0)
    message(FATAL_ERROR "positive Bootstrap Profile fixture was rejected:\n${profile_stdout}${profile_stderr}")
endif()

execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${project}" -O0 -o "${executable}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "nsc0 did not compile the positive Bootstrap Profile fixture:\n${compile_stdout}${compile_stderr}")
endif()

execute_process(COMMAND "${executable}" RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 42)
    message(FATAL_ERROR "positive Bootstrap Profile fixture returned ${run_result}; expected 42")
endif()
