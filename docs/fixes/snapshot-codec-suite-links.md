# Dependencia del codec en los ejecutables de prueba

En la compilación de 388 objetivos de prueba candidatos de la rama
`development-fixes-limitaciones`, 70 ejecutables no enlazaron porque
`ProjectCompilationSnapshots.cpp` llama a
`PrecompiledSemanticSnapshotCodec::decode`, definido por `nsc_precompiled`.
La lista y el log completos están en
`A:/nsc-fixes-evidencia/full-suite-run/snapshot-targets.txt` y `build3.log`.

El cambio añade `nsc_precompiled` únicamente a los 70 objetivos que mostraron
ese símbolo sin resolver. No cambia el enlazado del compilador ni habilita
pruebas de interoperabilidad C/C++. Los ajustes anteriores de la misma causa
se registraron en `stage41-test-link.md`. La reconstrucción incremental de los
388 objetivos terminó con código 0 (`full-suite-run/build5.log`). El enlace
correcto no implica que todas las pruebas pasen; sus resultados se registran
por separado en el estado de campaña.
