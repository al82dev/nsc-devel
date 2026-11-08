# Enlace de la prueba de arreglos y cadenas

El objetivo `nsc_stage41_arrays_strings_core_tests` no enlazaba porque
`ProjectCompilationSnapshots.cpp` usa `PrecompiledSemanticSnapshotCodec`,
implementado en `nsc_precompiled`, que no figuraba entre sus dependencias.
Se agrega esa biblioteca solo al objetivo de prueba existente.

La compilación dirigida y `nsc.stage41.arrays-strings-core` pasan en la copia
aislada. Los registros son `A:/nsc-fixes-evidencia/stage41-link-fix.log` y
`A:/nsc-fixes-evidencia/stage41-link-test.log`. La documentación histórica de
la revisión base ya registra ese mismo símbolo sin resolver en otras pruebas;
no se ha demostrado aquí que todas ellas queden corregidas por este cambio.
