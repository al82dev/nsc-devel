# T09-003: records C `preserve_none` mezclados, distintos y por puntero

## Defectos

Con el record C de ida y vuelta ya admitido, quedaban tres formas que Clang 23.1.0
compila y ejecuta, pero que NSC rechazaba:

- Records C mezclados con escalares o de identidades distintas en llamada directa.
  - `double t09_pn_record_mixed(float, int*, T09Pair, double)`.
  - `T09Out t09_pn_convert(T09In)`.
  - `int t09_pn_two(T09In, T09Pair, int)`.
  - `attachCPreserveNonePlan` solo conservaba el plan cuando lo aceptaba el validador escalar o el de ida y vuelta.
  - La puerta HIR `preserveNonePlannedScalar` solo aceptaba records C++ triviales (`CppImportedTypeOrigin::CppRecord`), no `CStruct`.
- Punteros a función C `preserve_none` con records por valor, en consumo directo.
  - `T09PnWide = T09In(*)(T09In)` y `T09PnRecordMixed = double(*)(float, int*, T09Pair, double)`.
  - `attachCPreserveNonePointerPlan` descartaba cualquier alias con record.
  - `reconcileCppFunctionPointerAbiPlan` solo admitía el validador escalar.
- Los mismos punteros dentro de una plantilla precompilada, con y sin encabezado.
  - El plan del alias viaja en el catálogo C vigente.
  - `planPrecompiledCppFunctionPointerForAlias` solo lo aceptaba si los records eran C++ (`CppRecord`); para records C no producía plan.
  - El emisor fallaba con `preserve_none function pointer signature is not yet supported` (`A:/temp/nsc-t09-pn-sonda-bmtrjm7u`, `A:/temp/nsc-t09-pn-sonda-mn6tem8c`).

## Cambio

- `CppSemanticInventory.cpp`:
  - La función C y el alias C `preserve_none` conservan el plan de Clang cuando lo acepta alguno de los validadores compartidos: escalar, ida y vuelta, o `isPreserveNoneCppScalarWithRecord`.
  - La exclusión de records se retiró.
  - Las firmas variádicas y sin prototipo siguen excluidas.
- `HirBuildCoordinator.cpp`:
  - Un record C (`CStruct`) entra en `preserveNonePlannedScalar` con la misma conciliación de ordinal, USR, completitud, layout y error diferido que un record C++.
  - Para un record C++ se sigue exigiendo `cppTrivialValueEligible`.
- `CppProjectSessions.cpp`:
  - `reconcileCppFunctionPointerAbiPlan` acepta para punteros C los mismos tres validadores.
  - La nueva función `planPrecompiledCPreserveNonePointer` atiende los alias precompilados `preserve_none` con algún record C por valor.
  - Esa función usa el plan del alias del catálogo C solo si:
    - el alias es único por nombre, USR y tipo codificado;
    - cada record, por valor o apuntado, coincide con un único record C completo del catálogo y con el tipo importado (`CStruct`, o `CUnion` si es apuntado), con identidad, USR, tamaño y alineación;
    - la firma la acepta un validador compartido.
  - Los records C++ siguen por la ruta anterior.

No cambia el contrato NSMeta: el codec ya transportaba y revalidaba estos planes de alias. Tampoco se modificó Clang ni NSLink.

## Control negativo de `pruebas-tanda-09-preserve-none-c-plan`

El control negativo de esta prueba se cambió con la autorización del usuario.

- Después de `54ac26932` usaba `T09Out(T09In)`. Ese caso ya se admite y se comprueba como positivo en el fixture nuevo.
- La comprobación del diagnóstico HIR `preserve_none does not yet support this signature` se conserva sin cambios. Su entrada pasa a una función de 33 parámetros `int`, fuera del límite de 32 que aplican los validadores. Clang compila la misma declaración antes de pedir el rechazo a NSC.
- Se añadió un segundo rechazo separado: una función variádica `preserve_none`, que Clang compila. NSC la rechaza al importar con `variadic function calling convention is not yet projected`. Esa capacidad sigue abierta ([observación](t09-preserve-none-variadica-observacion.md)).

## Evidencia

El fixture nuevo `pruebas-tanda-09-preserve-none-c-record-mixto/validar.py --precompilado` cubre:

- un control C de Clang 23.1.0;
- consumo NativeSharp directo;
- una plantilla precompilada que hace las mismas llamadas directas y por puntero, consumida con y sin encabezado.

Comprueba cada campo devuelto, que la entrada no cambia y `preserve_nonecc` en las declaraciones y en las llamadas indirectas `void` (sret) y `double`. Esto último también se verifica en el IR del consumidor precompilado. Se ejecutó en O0–O3:

| Prueba | Estática | DLL |
|---|---|---|
| Fixture nuevo | `A:/temp/nsc-t09-preserve-none-c-record-mixto-gndlbagk` y `A:/temp/nsc-t09-preserve-none-c-record-mixto-k8qwplxo` | `A:/temp/nsc-t09-preserve-none-c-record-mixto-htnq9djt` y `A:/temp/nsc-t09-preserve-none-c-record-mixto-57rnt4ve` |
| `pruebas-tanda-09-preserve-none-c-plan/validar.py --precompilado`, con los dos rechazos | `A:/temp/nsc-t09-preserve-none-c-plan-o39u9ooy` | `A:/temp/nsc-t09-preserve-none-c-plan-11z7i7e7` |
| `pruebas-tanda-09-preserve-none-c-record/validar-matriz.py --precompilado` | `A:/temp/nsc-t09-preserve-c-record-matriz-7_h4dy43` | `A:/temp/nsc-t09-preserve-c-record-matriz-jra5hbuy` |

Otras comprobaciones:

- Matriz de firmas por convención: 245 de 252 celdas pasan, con 7 rechazos y ningún fallo. Las tres celdas C `preserve_none` pendientes ya pasan (`A:/temp/nsc-t09-matriz-firmas-emeowri9`).
- Matriz de los 15 atributos de convención: pasa (`A:/temp/nsc-t09-convenciones-lt2y84hv`).
- Regresión de 12 validadores C `preserve_*`: pasa; por ejemplo, `A:/temp/nsc-t09-preserve-bool-061h1un6` y `A:/temp/nsc-t09-preserve-indirecta-mixta-precompilado-r0ipo9ln`.
- Pasan los CTest `clang-abi-probe`, `regcall-record-plan`, `variadic-pointer`, `project-loader` e `item331`, `item332`, `item333`, `item344` e `item345`.
- Pasa la prueba de identidad C/C++.

La construcción completa de `build-tanda09` se detiene al enlazar dos ejecutables de prueba ajenos a este cambio:

- `nsc_implicit_language_edition_semantics_tests`;
- `nsc_implicit_language_feature_recognition_tests`.

En ambos falta el símbolo `PrecompiledSemanticSnapshotCodec::decode`. `nsc` y las pruebas usadas sí se construyen.

## Límites

La evidencia cubre los tamaños y las formas listados, con biblioteca estática y DLL, y C `struct` sin bitfields ni empaquetado. No acredita:

- records C `union` por valor;
- funciones variádicas `preserve_none`;
- firmas de más de 32 parámetros.

T09-003 y la tanda 9 siguen abiertos.
