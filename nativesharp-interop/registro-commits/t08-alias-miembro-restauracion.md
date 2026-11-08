# Restauración tras la regresión de alias

Tras `a4841ee89`, la construcción incremental de `nsc` terminó con código 0
(`92f186f84f538e64`; tres advertencias de inicializadores en
`CppSemanticRecord.cpp`). `pruebas-tanda-08-anonimos-c/validar.cmd` volvió a
terminar con código 0 de O0 a O3, incluidos consumo compartido y metadata.

La publicación de `NAlias` del control nuevo quedó retirada junto con el
cambio que causaba el conflicto. No se considera resuelto el caso de alias y
no se ejecutó la validación completa de la tanda 8.
