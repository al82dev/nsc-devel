# Tanda 19: punteros a función C++ con records por valor, `__vectorcall` y C de Win64 (T19-036)

## Defectos

1. **`__vectorcall`.** Un tipo puntero a función C++ `__vectorcall` con un record C++ por valor no se importaba.
   `T::pick` daba `C/C++ return type is not yet mapped` y `T::callh(Fh)` daba `parameter 0 is not yet mapped`.
   `mapFunctionType` (`CppSemanticFunction.cpp`) admitía records C++ en punteros `cdecl`, `preserve_*`,
   `regcall` y `sysv_abi`, pero no en `vectorcall`.
2. **Retorno en memoria por un puntero.** Una vez importado, un callback NativeSharp `__vectorcall` que devuelve
   un record C++ con constructor (MSVC no lo devuelve en registros, `cppIndirectReturn`) volvía con el valor en XMM.
   Clang lo espera por un puntero oculto. `callc(MakeMine())` daba 300 donde Clang da 400.
3. **Lo mismo con la convención C de Win64, defecto previo.**
   - Un puntero `cdecl` que devuelve un record C++ de 8 bytes con constructor se llamaba esperando el valor en RAX.
     Clang usa `sret`.
   - El callback NativeSharp convertido a ese puntero tampoco usaba `sret`.
   - El programa terminaba con `0xC0000005`.

   IR de Clang:
   - llamada: `call void %8(ptr sret(%"struct.T::K8") ..., i64 ..., ptr ..., i32 3)`;
   - callback: `define internal void @"?minek@@..."(ptr sret ...)`.

   En NSC, la llamada era `call i64 %local.load(...)` y el límite `define weak_odr i64 @__nsc_lambda_12_0__c_boundary`.

## Causa

El retorno en memoria de T12-001 solo se aplicaba a las declaraciones directas, y en las llamadas por puntero solo
con `__vectorcall`. Los límites de callback (`createCallbackGuardThunkType`, `OopLlvmThunksAndWrappers.cpp`) no lo
aplicaban nunca. La regla de MicrosoftCXXABI (`classifyReturnType`) depende del record, no de la convención.

## Cambio

- `CppSemanticFunction.cpp`: un puntero `__vectorcall` admite records C++ triviales por valor, como los demás.
- `OopLlvmClosuresDelegatesEvents.cpp`, `emitExternCDelegateCall`: una llamada por un puntero con la convención C
  de Win64 (`cdecl`, `ms_abi`; `stdcall` y `fastcall`, que x64 ignora) o `__vectorcall` devuelve un record con
  `cppIndirectReturn` por un puntero oculto.
- `OopLlvmThunksAndWrappers.cpp`, límites de callback C y `__vectorcall`:
  - un record con `cppIndirectReturn` sale por el puntero oculto del llamador;
  - si el cuerpo NativeSharp lo devuelve directo (8 bytes), el límite lo guarda por ese puntero.

## Evidencia

Casos nuevos de `pruebas-tanda-19-traslados`. Cada uno compara el resultado con Clang. También compara las formas
(convención, `sret`, `inreg`, tipos) de cada llamada por puntero y de cada callback con las del IR de Clang para
el mismo tipo de puntero (`pointer_shapes`).

| Caso | Contenido | Estática y DLL, O0–O3 |
|---|---|---|
| `vectorcall-puntero` | HVA `{float x3}`, record de 8 bytes y `{double, double}` con constructor; llamadas por puntero y callbacks NativeSharp que llama la biblioteca | 876413109, igual que Clang; 2 formas como Clang |
| `cdecl-puntero` | record de 8 bytes con constructor (resultado por `sret`), record de 16 bytes por dirección; llamada y callback | 110712238, igual que Clang; 1 forma como Clang |

Registros: `logs/fx-fp10*.log`.

Regresión (`lista-fp12.txt`: todas las tandas 9 a 18, porque el cambio toca todos los límites de callback C):

- estática: 1154 de 1160 pasan y 2 se omiten (requieren mutador). Fallan 4:
  - las dos `cpp-variadica-preserve-none` (T19-032);
  - `cpp-tamano-objeto/validar-indice-expresion` y `validar-indice-llamada`, que fallan bajo carga (T19-026) y
    pasan repetidas con un proceso (`logs/regresion-t09-fp14.log`);
- DLL: 61 de 61 (`logs/regresion-dll-fp12d.log`);
- los guiones de la tanda 8 (vectorcall C, callbacks C e identidad de funciones C) pasan en estática, DLL y
  precompilado;
- `lote-ct2`: `ninja -k 0` sin errores; las nueve CTests de interoperabilidad y la de identidad pasan;
- `ctest -R "snapshot|precompiled|record|cpp-interop|member|callback|delegate"`: 33 de 35. Fallan
  `nsc.cpp-interop.session` (faltan sus fixtures, T19-037) y `nsc.stage42.delegates-events`, que también falla en
  la línea base `152654e5a` (`logs/ctest-completo-base.log`).

El fixture completo de traslados (17 casos) pasa en estática y en DLL (`logs/fx-fp12t.log`, `logs/fx-fp12td.log`).

## Sigue pendiente

- Los mismos punteros desde una plantilla NativeSharp precompilada. El consumidor no ve los constructores C++
  usados en el cuerpo (`struct has only an implicit parameterless constructor`) y, con un valor devuelto, falla
  `experimental C++ lifetime requires direct local or discarded construction`. Son flujos de construcción C++ en
  plantillas precompiladas: T19-031.
- Lambda escrita directamente como argumento: T19-033 (el caso usa una variable local).

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
