# EN: Runs an integration script that creates its own --work-dir, after removing the previous
#     run's directory so the test can be repeated. SCRIPT_ARGUMENTS is a '|'-separated list.
# ES: Ejecuta un script de integracion que crea su propio --work-dir, tras borrar el directorio
#     de la ejecucion anterior para que la prueba pueda repetirse. SCRIPT_ARGUMENTS es una lista
#     separada por '|'.
if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SCRIPT OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "PYTHON_EXECUTABLE, SCRIPT and WORK_DIR are required")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
string(REPLACE "|" ";" arguments "${SCRIPT_ARGUMENTS}")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SCRIPT}" ${arguments} --work-dir "${WORK_DIR}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${SCRIPT} failed with ${result}")
endif()
