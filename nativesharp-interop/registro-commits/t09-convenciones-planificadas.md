# T09-003: swiftcall, swiftasynccall e intel_ocl_bicc por plan de Clang

## Defecto

La matriz de convenciones de Clang 23.1 en Windows x64 registraba un rechazo para tres
convenciones que Clang emite. El diagnóstico era `C/C++ calling convention '...' is not yet
supported in HIR` y afectaba a:

- C `swiftcall` (`swiftcc`);
- C `swiftasynccall` (`swifttailcc`);
- C `intel_ocl_bicc` (`intel_ocl_bicc`);
- C++ `swiftcall` y `swiftasynccall`.

Clang no puede decorar `intel_ocl_bicc` en C++.

Causa en el código:

- La puerta HIR solo conocía `cdecl`, `vectorcall`, `regcall`, `sysv_abi` y `preserve_*`.
- Ni la función C ni la C++ recibían un plan de Clang para estas convenciones.
- El emisor no tenía cómo poner su convención LLVM.

## Cambio

- `CppRegCallAbiValidation.h`:
  - El validador escalar de `preserve_none` pasa a `isCppScalarPlanForConvention`,
    parametrizado por convención. `isPreserveNoneCppScalarPlan` lo llama con la misma
    semántica que antes.
  - `isCppPlannedScalarConventionPlan` lo aplica a `swiftcall` (16), `swiftasynccall` (20) e
    `intel_ocl_bicc` (77). Solo acepta escalares y punteros cuyo tipo físico coincide con el
    declarado, sin extensiones salvo `zeroext` de `bool`.
  - `cppPlannedScalarConventionOfPlan` devuelve la convención LLVM de ese plan.
- `CppSemanticInventory.cpp` (C) y `ClangSession.cpp` (C++): se pide el plan de Clang de la
  declaración exacta y se conserva solo si el validador lo acepta.
- `HirBuildCoordinator.cpp`: estas convenciones se aceptan únicamente con ese plan validado,
  sin records por valor ni variádicas. El enlace en ejecución de DLL con alias TLS sigue
  rechazado, como para las demás convenciones no `cdecl`.
- `OopLlvmDeclarations.cpp` y `OopLlvmCalls.cpp`: la declaración y la llamada toman la
  convención LLVM del plan validado. La bajada escalar no cambia.
- `CppCRecordMetadataCodec.cpp`: el plan viaja en el campo de plan ya existente de la función
  dependiente y se revalida al decodificar. El contrato NSMeta vigente no cambia.

No se modificó Clang ni NSLink.

## Evidencia

`pruebas-tanda-09-convenciones-planificadas/validar.py --precompilado` (nuevo) cubre:

- C con `swiftcall`, `swiftasynccall` e `intel_ocl_bicc`;
- C++ en namespace con `swiftcall` y `swiftasynccall`;
- cinco firmas por caso: `int(int,int)`, `double(float,double,int*)`,
  `void(int*,long long)`, `int*(int*)` y `unsigned long long(unsigned,long long)`.

Para cada caso la prueba:

- compara con un control C/C++ compilado por Clang 23.1.0, en estática y DLL O0–O3;
- consume las funciones en directo y desde una plantilla precompilada, con y sin encabezado;
- exige en el IR de NSC la convención LLVM y el símbolo de la definición de Clang, en cada
  declaración y en cada llamada;
- verifica los valores devueltos.

El rechazo de `swiftcall` con un record por valor se mantiene.

Evidencia:

- `A:/temp/nsc-t09-convenciones-planificadas-ehjf0ivl` (estática);
- `A:/temp/nsc-t09-convenciones-planificadas-q7wunf8u` (DLL).

Otras comprobaciones:

- Matriz de los 15 atributos de convención: las cinco filas pasan a aceptarse en llamada
  directa con la convención y el símbolo de Clang y con el resultado correcto. Por puntero
  siguen rechazadas con el diagnóstico del alias (`A:/temp/nsc-t09-convenciones-dkkdmnrl`).
- Matriz de firmas: pasa 252 de 252 celdas (`A:/temp/nsc-t09-matriz-firmas-roi_12qh`).
- Pasan los CTest `clang-abi-probe`, `regcall-record-plan`, `variadic-pointer`,
  `project-loader` e `item331`, `item332`, `item333`, `item344` e `item345`, y la prueba de
  identidad C/C++.

## Límites

- Solo escalares y punteros. Records por valor, variádicas y DLL con enlace en ejecución por
  alias TLS se rechazan con diagnóstico.
- No hay llamadas por puntero. NativeSharp no tiene tipos de función con estas convenciones:
  un alias de puntero con ellas se rechaza con
  `unsupported C callback calling convention '...' for alias '...'`. Añadirlas exige una
  extensión del lenguaje NativeSharp (sintaxis de convención en tipos de función), fuera del
  alcance de interoperabilidad de esta tanda.
- Clang 23.1.0 no decora `intel_ocl_bicc` en C++ (`cannot mangle this calling convention
  yet`), así que no hay nada que consumir.
