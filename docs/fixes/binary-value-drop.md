# Destrucción de valores desde bibliotecas binarias

La revisión base `152654e5a0e2d84d4c2f245ea7590488f0497430` compiló un
productor con un `struct` con destructor y un consumidor con dos locales, pero
el consumidor terminó con código 13: la traza no contenía las destrucciones.
La reproducción está en `A:/nsc-fixes-evidencia/70-binary-drop-base`; el
compilador de base informó el hash `14717501b74375ce`.

La metadata del productor contiene una entrada de limpieza para el valor,
pero el destructor privado no figura en la lista de miembros consumibles. El
consumidor reconstruía la declaración sin una marca de destrucción y no
programaba la llamada a la limpieza del productor. Se reconstruye una marca
sintética solo para structs cuya metadata declara esa entrada y no contiene
ya una declaración de destructor. La limpieza ejecutada sigue siendo la del
productor; no se cambia el formato de metadata.

La regresión `nsc.sdk.binary-value-drop` genera una biblioteca y su
consumidor por separado. Verifica en O0 y O2 la destrucción de dos locales en
orden inverso para un destructor propio y un campo `owned` privado, y comprueba
que un struct trivial puede copiarse mientras los valores que necesitan
limpieza no pueden copiarse implícitamente. La ejecución directa está en
`A:/nsc-fixes-evidencia/binary-value-drop-standalone.log`. Se probó además la
misma ruta con construcción fallida y un consumidor con caché; ver
`A:/nsc-fixes-evidencia/70-binary-drop-throw-tests.log`. Tras registrar la
regresión, CTest la ejecutó individualmente y obtuvo 1/1
(`A:/nsc-fixes-evidencia/binary-value-drop-ctest.log`).

La validación utilizó el compilador aislado construido desde un árbol que
también contiene cambios locales sin commit; no certifica una revisión limpia
ni todos los tipos, objetivos y combinaciones binarias. La regresión dirigida
de las cuatro familias de punteros pasó con el Core binario aislado existente,
sin regenerar ese SDK (`70-binary-drop-pointer-regression.log`). No se probó
interoperabilidad C/C++ ni se cambió el formato de metadata.
