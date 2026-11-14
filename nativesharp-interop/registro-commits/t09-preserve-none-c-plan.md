# T09-003: firmas C `preserve_none` aceptadas por el plan ABI de Clang

## Defecto

La [matriz de convenciones](t09-matriz-convenciones-win64.md) mostró que
`int __attribute__((preserve_none)) f(int, int)` se acepta en C++ y se rechaza en C
con `preserve_none does not yet support this signature`.

## Causa comprobada

- La ruta C++ recibe el plan ABI de Clang de la declaración exacta
  (`ClangSession.cpp`, `collectFreeFunctions`, condición `getLangOpts().CPlusPlus`) y
  lo valida con `isPreserveNoneCppScalarPlan`. Con ese plan,
  `HirBuildCoordinator.cpp` acepta la firma (`preserveNonePlannedScalar`).
- La ruta C solo pide el plan (`cFunctionAbiPlan`) para `sysv_abi` y `regcall` con
  records por valor (`CppSemanticInventory.cpp`, `collectCppSemanticFunctions`). Sin
  plan, `preserve_none` en C depende de una lista fija de firmas comprobadas, y
  `int(int,int)` no figura en ella.

## Corrección

`attachCPreserveNonePlan` (`CppSemanticInventory.cpp`) pide a la sesión C el plan de
la declaración exacta (por nombre y tipo canónico). Las condiciones son:
`preserve_none`, con prototipo, no variádica, enlace externo y sin records por valor.
El plan se conserva solo si su símbolo coincide con el símbolo externo y el
validador compartido `isPreserveNoneCppScalarPlan` lo acepta para la firma observada.
No cambian HIR, emisión, codec ni contrato NSMeta. El plan viaja en el campo opcional
que el catálogo vigente ya usa para planes de funciones y se vuelve a validar al
decodificar. Los records C por valor siguen por la ruta existente.

## Validación

- `pruebas-tanda-09-preserve-none-c-plan/validar.py --precompilado` (nuevo). Cinco
  funciones C: `int(int,int)`, `double(float,int,double)`, `void(int*,long long)`,
  `unsigned short(_Bool,signed char,unsigned char)` e `int*(int*,int)`. El control
  Clang emite `preserve_nonecc`. NSC declara y llama cada símbolo con
  `preserve_nonecc` (con `dllimport` en DLL) y la ejecución devuelve los valores
  esperados, O0–O3. Una plantilla precompilada pasa con y sin encabezado. Un record C
  de 36 bytes por valor sigue rechazado con el diagnóstico existente.
  - Estática: `A:/temp/nsc-t09-preserve-none-c-plan-r9y9q4o_`.
  - DLL: `A:/temp/nsc-t09-preserve-none-c-plan-pqmkerr6`.
- Antes del cambio, `int(int,int)` en C se rechazaba
  (`A:/temp/nsc-t09-convenciones-5awmn7lk`).
- REGRESION

## Límites

No se abren records C por valor ni punteros a records C con `preserve_none`, ni
variádicas, ni llamadas por puntero a función C. El rechazo de la variádica se
conserva por la anomalía documentada en
[t09-preserve-none-variadica-observacion.md](t09-preserve-none-variadica-observacion.md).
No se ejecutaron la batería completa ni el gate de la tanda. T09-003 sigue abierto.
