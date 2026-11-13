file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(case IN ITEMS Safe Overflow)
    set(case_dir "${OUTPUT_DIR}/${case}")
    file(MAKE_DIRECTORY "${case_dir}")
    configure_file("${FIXTURE_DIR}/${case}.ns" "${case_dir}/Program.ns" COPYONLY)
    file(WRITE "${case_dir}/Program.nsproj" "<Project><PropertyGroup><Name>CheckedI128Neg</Name></PropertyGroup><ItemGroup><Compile Include=\"Program.ns\" /></ItemGroup></Project>")
    foreach(opt IN ITEMS 0 2)
        set(program "${case_dir}/O${opt}.exe")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env
            "NSC_WINDOWS_RUNTIME_LIBRARY=${RUNTIME_LIBRARY}"
            "NSC_LINKER=${NSLINK_EXECUTABLE}"
            "${NSC_EXECUTABLE}" --emit-project-oop-exe "${case_dir}/Program.nsproj"
            -O${opt} --no-artifact-cache --no-error-log -o "${program}"
            RESULT_VARIABLE compile_result OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT compile_result EQUAL 0)
            message(FATAL_ERROR "${case} O${opt} failed to compile: ${out}\n${err}")
        endif()
        execute_process(COMMAND "${program}" RESULT_VARIABLE run_result)
        if(case STREQUAL "Safe")
            set(expected 0)
        else()
            set(expected 147)
        endif()
        if(NOT run_result EQUAL expected)
            message(FATAL_ERROR "${case} O${opt}: expected ${expected}, got ${run_result}")
        endif()
    endforeach()
endforeach()
