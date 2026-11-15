# EN: Runs an integration script from the source root. SCRIPT_ARGUMENTS is a '|'-separated list in
#     which @WORK@ names WORK_DIR, which is removed first (and created again when PRECREATE is ON),
#     because most scripts refuse an output directory that already exists.
# ES: Ejecuta un script de integracion desde la raiz de fuentes. SCRIPT_ARGUMENTS es una lista
#     separada por '|' donde @WORK@ nombra WORK_DIR, que se borra antes (y se vuelve a crear si
#     PRECREATE es ON), porque la mayoria de los scripts rechaza un directorio de salida existente.
if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SCRIPT OR NOT DEFINED WORK_DIR OR NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "PYTHON_EXECUTABLE, SCRIPT, WORK_DIR and SOURCE_ROOT are required")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
if(PRECREATE)
    file(MAKE_DIRECTORY "${WORK_DIR}")
endif()
string(REPLACE "@WORK@" "${WORK_DIR}" arguments "${SCRIPT_ARGUMENTS}")
string(REPLACE "|" ";" arguments "${arguments}")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SCRIPT}" ${arguments}
    WORKING_DIRECTORY "${SOURCE_ROOT}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${SCRIPT} failed with ${result}")
endif()
