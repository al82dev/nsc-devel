# EN: Builds the self-hosted compiler (selfhost/nsc) with the compiler under test and runs one
#     integration script against it. The script receives the self-hosted compiler first and then
#     SCRIPT_ARGUMENTS ('|'-separated). LLD_LINK_DIRECTORY is put on PATH for scripts that link.
# ES: Construye el compilador autoalojado (selfhost/nsc) con el compilador bajo prueba y ejecuta un
#     script de integracion contra el. El script recibe primero el compilador autoalojado y despues
#     SCRIPT_ARGUMENTS (separados por '|'). LLD_LINK_DIRECTORY se agrega a PATH para los que enlazan.
if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT_DIR OR NOT DEFINED SCRIPT)
    message(FATAL_ERROR "NSC_EXECUTABLE, PYTHON_EXECUTABLE, SOURCE_ROOT, OUTPUT_DIR and SCRIPT are required")
endif()
file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(next "${OUTPUT_DIR}/nsc-next.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj"
        -o "${next}" --no-artifact-cache --no-error-log
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE built OUTPUT_VARIABLE built_output ERROR_VARIABLE built_error)
if(NOT built EQUAL 0)
    message(FATAL_ERROR "self-hosted compiler build failed: ${built_output}${built_error}")
endif()
if(DEFINED LLD_LINK_DIRECTORY)
    set(ENV{PATH} "${LLD_LINK_DIRECTORY};$ENV{PATH}")
endif()
string(REPLACE "|" ";" arguments "${SCRIPT_ARGUMENTS}")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${SCRIPT}" "${next}" ${arguments} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${SCRIPT} failed with ${result}")
endif()
