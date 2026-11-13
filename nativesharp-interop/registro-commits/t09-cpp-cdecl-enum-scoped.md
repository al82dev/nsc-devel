# T09: enum class C++ en llamada variádica cdecl

## Cambio

Clang para Windows x64 pasa `enum class Mode : unsigned short` como `i16` en la muestra variádica. NSC emitía `i32`, aunque la ejecución de ese ejemplo parecía correcta. Se conserva la propiedad `scoped` observada por Clang en el tipo importado y restaurado desde el contrato vigente; la promoción variádica cdecl preserva el tipo del enum class. Para plantillas precompiladas se obtiene la dependencia del enum calificado desde el inventario semántico cuando aún no está materializado en la búsqueda. No se introduce versionamiento histórico de NSMeta.

## Validación

- Compilación incremental de `nsc` y tres pruebas internas: `A:/temp/t09-cpp-record-replay-build.log`, código 0.
- Función directa y callback, controles C++ y NativeSharp en estática y DLL O0–O3, con lectura real por `va_arg(Mode)` y verificación del argumento siguiente. Las pruebas comprueban `i16` en el IR de NSC: `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-scoped-t30bygib` y `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-scoped-indirecta-9whc1tny`.
- Consumidores precompilados con y sin encabezado en las mismas ocho combinaciones: 16 ejecuciones por ruta, `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-scoped-precompilado-5u7qy733` y `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-scoped-indirecta-precompilado-rzxb1c_v`.
- Regresiones precompiladas de enum no scoped global y en namespace: 16 ejecuciones cada una, `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-precompilado-b98qd8v4` y `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-namespace-precompilado-yh60hd9g`.
- `nsc_cpp_abi_probe_tests`, `nsc_cpp_record_value_observation_tests` y `nsc_cpp_regcall_record_plan_tests`: código 0 con recursos Clang y directorio de trabajo explícitos; `A:/temp/t09-scoped-enum-unit-tests.log`.

## Límites

Evidencia puntual para Windows x64, cdecl y enum class de base `unsigned short`; no certifica otras bases, convenciones, plataformas ni todos los usos de enums. La ruta precompilada comprueba ejecución, mientras la aserción de IR corresponde a la ruta viva. T09-003, T10-002 y T10-003 permanecen abiertos.
