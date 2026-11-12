file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(opt IN ITEMS 0 2)
    set(program "${OUTPUT_DIR}/StructImpl-O${opt}.exe")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
        "NSC_LINKER=${NSLINK_EXECUTABLE}"
        "${NSC_EXECUTABLE}" --emit-project-oop-exe "${FIXTURE_DIR}/Static.nsproj"
        -O${opt} --no-artifact-cache --no-error-log -o "${program}"
        RESULT_VARIABLE compile_result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT compile_result EQUAL 0)
        message(FATAL_ERROR "Struct impl O${opt} failed to compile: ${out}\n${err}")
    endif()
    execute_process(COMMAND "${program}" RESULT_VARIABLE run_result)
    if(NOT run_result EQUAL 42)
        message(FATAL_ERROR "Struct impl O${opt}: expected 42, got ${run_result}")
    endif()
endforeach()
execute_process(COMMAND "${NSC_EXECUTABLE}" --dump-project-oop-mir "${FIXTURE_DIR}/Static.nsproj"
    RESULT_VARIABLE mir_result OUTPUT_VARIABLE mir ERROR_VARIABLE mir_error)
if(NOT mir_result EQUAL 0 OR NOT mir MATCHES "direct-instance-call.*Number__Value" OR
   mir MATCHES "allocate-and-construct" OR mir MATCHES "interface-instance-call" OR
   mir MATCHES "virtual-instance-call")
    message(FATAL_ERROR "Struct implementation did not use direct non-boxing dispatch: ${mir_error}\n${mir}")
endif()
execute_process(COMMAND "${NSC_EXECUTABLE}" --check-project-method-bodies "${FIXTURE_DIR}/Dynamic.nsproj"
    RESULT_VARIABLE dynamic_result OUTPUT_VARIABLE dynamic_out ERROR_VARIABLE dynamic_err)
if(dynamic_result EQUAL 0 OR NOT "${dynamic_out}${dynamic_err}" MATCHES "cannot initialize local 'erased'")
    message(FATAL_ERROR "Dynamic struct conversion must remain rejected: ${dynamic_out}\n${dynamic_err}")
endif()
