# PROPUESTA PARA EL INTEGRADOR: no aplicada ni validada.
# Insertar junto a los registros ASan, dentro del ámbito de pruebas pertinente.
if(WIN32)
    add_test(NAME nsc.driver-address-sanitizer-realloc-preserve
        COMMAND python -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/asan_heap_realloc_preserve.py"
            --compiler "${NSC_TEST_COMPILER}"
            --clang "$ENV{NSC_TEST_ASAN_CLANG}"
            --asan-runtime "$ENV{NSC_TEST_ASAN_RUNTIME}"
            --temp-root "$ENV{NSC_TEST_TEMP_ROOT}")
    set_tests_properties(nsc.driver-address-sanitizer-realloc-preserve PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
