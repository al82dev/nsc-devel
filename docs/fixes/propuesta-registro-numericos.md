# Propuesta para el integrador: pruebas numéricas

Este carril no modifica `CMakeLists.txt`, reservado al integrador. Una vez incorporados
sus commits, el siguiente registro dentro del bloque existente de tests permite descubrir
los ejecutores dirigidos, usando variables ya utilizadas por el proyecto:

```cmake
add_test(NAME nsc.compiler.decimal-saturating-inexact
    COMMAND "${CMAKE_COMMAND}"
        "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
        "-DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/decimal_saturating_inexact"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/decimal-saturating-inexact"
        "-DRUNTIME_LIBRARY=${NSC_WINDOWS_RUNTIME_LIBRARY}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyDecimalSaturatingInexact.cmake")
set_tests_properties(nsc.compiler.decimal-saturating-inexact PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME nsc.compiler.decimal-multiply-divide
    COMMAND "${CMAKE_COMMAND}"
        "-DNSC_EXECUTABLE=${NSC_TEST_COMPILER}"
        "-DNSLINK_EXECUTABLE=nslink.exe"
        "-DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/decimal_mul_div_observe"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/decimal-multiply-divide"
        "-DRUNTIME_LIBRARY=${NSC_WINDOWS_RUNTIME_LIBRARY}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyDecimalMulDivObserve.cmake")
set_tests_properties(nsc.compiler.decimal-multiply-divide PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
```

La propuesta no está aplicada ni se afirma validada dentro de CTest. El integrador debe
comprobar que `NSC_TEST_COMPILER`, runtime e imports correspondan a su build: en el build
de este carril el valor predeterminado de `NSC_TEST_COMPILER` apuntaba a la instalación
en C:, por lo que se invocaron los runners directamente con el compilador aislado de A:.
`VerifyDecimalMultiply.cmake` permanece disponible para una ejecución más pequeña y no
necesita duplicar los siete casos en el registro anterior.

Los tres commits del carril cambian `src/LLVM/Lowering/OopLlvmOperators.cpp`; el integrador
debe comparar ese archivo y `CMakeLists.txt` contra las otras ramas antes de integrar.
Este clon no contiene una referencia a `development-cpp-clang`, por lo que no se ha medido
el solapamiento real con esa rama. Tampoco se modificaron `OopLlvmImplementation.h` ni los
documentos compartidos `campaign-status.md` y `decisiones-pendientes.md`.

El fixture de matemática consume Core fuente de esta copia. Su resultado no acredita el
SDK binario: ese gate pertenece al carril que lo produce. La ejecución bajo ese gate y la
integración de registros requieren artefactos propios de la revisión integrada.
