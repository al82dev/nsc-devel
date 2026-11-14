# T09-003: punteros a función C `preserve_none` con plan ABI de Clang

## Defecto

La [matriz de convenciones](t09-matriz-convenciones-win64.md) (corrida
`A:/temp/nsc-t09-convenciones-4lri3y7x`) mostró que el puntero C++
`int (preserve_none *)(int,int)` se acepta. El mismo puntero en C se rechaza en el
emisor con `preserve_none function pointer signature is not yet supported`.

## Causa comprobada

- `ClangSession::cFunctionPointerAbiPlan` solo observa prototipos `sysv_abi` y
  `regcall`. Su único conciliador en vivo, `reconcileCppFunctionPointerAbiPlan`
  (`CppProjectSessions.cpp`), solo admite esas convenciones (códigos LLVM 78 y 92).
- Sin plan, `node->cppFunctionPointerPlan` está vacío.
  `OopLlvmClosuresDelegatesEvents.cpp` solo admite entonces la lista fija de firmas
  `preserve_none`, y `int(int,int)` no está en ella.
- Para consumidores precompilados, `planPrecompiledCppFunctionPointerForAlias` toma el
  plan del alias del catálogo. Pero `collectCppSemanticTypeAliases` solo captura ese
  plan para alias C++.

## Corrección

- `ClangSession.cpp`: `cFunctionPointerAbiPlan` también observa `CC_PreserveNone`.
- `CppProjectSessions.cpp`: el conciliador C admite `preserve_none` con código 21 y el
  símbolo de sonda. Acepta el plan solo si `isPreserveNoneCppScalarPlan` lo valida
  para la firma observada.
- `CppSemanticInventory.cpp`: `attachCPreserveNonePointerPlan` adjunta ese plan a los
  alias C `preserve_none` sin records por valor, para que el catálogo vigente lo
  transporte. No hay campo ni versión nuevos.

El emisor y el codec no cambian: ya validaban planes con el símbolo de sonda,
cualquiera que fuera el lenguaje de origen.

## Validación

- `pruebas-tanda-09-preserve-none-c-plan/validar.py --precompilado` se amplía con dos
  punteros `int(int,int)` y `double(float,int,double)` devueltos por C. Además de
  los resultados, exige `call preserve_nonecc` indirecto en el IR. Directo,
  plantilla con y sin encabezado, O0–O3:
  - estática: `A:/temp/nsc-t09-preserve-none-c-plan-jgopf4bl`;
  - DLL: `A:/temp/nsc-t09-preserve-none-c-plan-rv45cbw9`.
- Matriz: C `preserve_none` pasa ahora directo y por puntero. No hay aceptaciones
  con convención distinta (`A:/temp/nsc-t09-convenciones-98td73i_`).
- Regresiones C `preserve_*`: 12 validadores (6 fixtures, comparaciones vivas y 16
  consumidores precompilados cada uno). Pasaron todos, por ejemplo
  `A:/temp/nsc-t09-preserve-bool-zxc8940h` e
  `A:/temp/nsc-t09-preserve-indirecta-mixta-precompilado-4b9yiqhw`.
- CTest `nsc.cpp-interop.clang-abi-probe`, `nsc.cpp-interop.regcall-record-plan` y
  los tres `clang-variadic-pointer-*`: 5/5. Su control de que un puntero `cdecl` no
  tiene plan se conserva. `nsc_cpp_c_identity_tests` pasa.
- Construcción incremental: `3.0.0+build.20261114T160148Z.a02393454c7d8965`.

## Límites

No se cubren punteros C `preserve_none` con records por valor, punteros variádicos ni
alias C dentro de records. El diagnóstico de punteros `swiftcall`, `swiftasynccall`
e `intel_ocl_bicc` sigue impreciso. No se ejecutaron la batería completa ni el gate
de la tanda. T09-003 sigue abierto.
