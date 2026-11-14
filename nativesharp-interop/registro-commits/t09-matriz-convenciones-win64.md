# T09-003: matriz de convenciones de llamada Clang 23.1 y NSC en Windows x64

## Objetivo

Inventariar todos los atributos de convención que Clang 23.1.0 reconoce para
`x86_64-pc-windows-msvc` y comprobar, para una función `int(int, int)` en C y en
C++ (namespace), que NSC no acepta en silencio una convención distinta de la que
emite Clang. Se trata de una matriz de inventario con llamada directa, O0 y
biblioteca estática. No sustituye las pruebas de firmas, DLL, punteros ni
precompilación de cada convención.

## Método

`pruebas-tanda-09-convenciones/validar-matriz.py` compila cada caso con Clang a IR,
toma la convención LLVM y el símbolo de la definición, y consume la función desde
NativeSharp. Si NSC acepta el caso, el script exige que la declaración tenga la misma
convención y el mismo símbolo que Clang, y que el ejecutable devuelva el resultado
esperado. Si no, registra el primer diagnóstico. Una aceptación con convención,
símbolo o resultado distintos hace fallar la prueba.

## Resultado

Evidencia: `A:/temp/nsc-t09-convenciones-5awmn7lk` (`matriz.md` y un registro por
paso). Código de salida 0; no hubo aceptaciones con convención distinta.

| Lenguaje | Atributo | Clang 23.1 O0 | NSC | Detalle |
|---|---|---|---|---|
| C | cdecl | ccc @t09_cc | acepta | resultado correcto |
| C | stdcall | ccc @t09_cc | acepta | resultado correcto |
| C | fastcall | ccc @t09_cc | acepta | resultado correcto |
| C | thiscall | ccc @t09_cc | acepta | resultado correcto |
| C | pascal | ccc @t09_cc (atributo ignorado) | acepta | resultado correcto |
| C | vectorcall | x86_vectorcallcc @"\01t09_cc@@16" | acepta | resultado correcto |
| C | regcall | x86_regcallcc @__regcall3__t09_cc | acepta | resultado correcto |
| C | ms_abi | ccc @t09_cc | acepta | resultado correcto |
| C | sysv_abi | x86_64_sysvcc @t09_cc | acepta | resultado correcto |
| C | preserve_most | preserve_mostcc @t09_cc | acepta | resultado correcto |
| C | preserve_all | preserve_allcc @t09_cc | acepta | resultado correcto |
| C | preserve_none | preserve_nonecc @t09_cc | rechazo | nsc: error: preserve_none does not yet support this signature for t09_cc |
| C | swiftcall | swiftcc @t09_cc | rechazo | nsc: error: C/C++ calling convention 'swiftcall' is not yet supported in HIR for t09_cc |
| C | swiftasynccall | swifttailcc @t09_cc | rechazo | nsc: error: C/C++ calling convention 'swiftasynccall' is not yet supported in HIR for t09_cc |
| C | intel_ocl_bicc | intel_ocl_bicc @t09_cc | rechazo | nsc: error: C/C++ calling convention 'intel_ocl_bicc' is not yet supported in HIR for t09_cc |
| C++ | cdecl | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto |
| C++ | stdcall | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto |
| C++ | fastcall | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto |
| C++ | thiscall | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto |
| C++ | pascal | ccc @"?t09_cc@T09Cc@@YAHHH@Z" (atributo ignorado) | acepta | resultado correcto |
| C++ | vectorcall | x86_vectorcallcc @"?t09_cc@T09Cc@@YQHHH@Z" | acepta | resultado correcto |
| C++ | regcall | x86_regcallcc @"?t09_cc@T09Cc@@YwHHH@Z" | acepta | resultado correcto |
| C++ | ms_abi | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto |
| C++ | sysv_abi | x86_64_sysvcc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto |
| C++ | preserve_most | preserve_mostcc @"?t09_cc@T09Cc@@YUHHH@Z" | acepta | resultado correcto |
| C++ | preserve_all | Clang rechaza: .\api.h:3:1: error: cannot mangle this calling convention yet | - | no evaluado |
| C++ | preserve_none | preserve_nonecc @"?t09_cc@T09Cc@@YVHHH@Z" | acepta | resultado correcto |
| C++ | swiftcall | swiftcc @"?t09_cc@T09Cc@@YSHHH@Z" | rechazo | nsc: error: C/C++ calling convention 'swiftcall' is not yet supported in HIR for ?t09_cc@T09Cc@@YSHHH@Z |
| C++ | swiftasynccall | swifttailcc @"?t09_cc@T09Cc@@YWHHH@Z" | rechazo | nsc: error: C/C++ calling convention 'swiftasynccall' is not yet supported in HIR for ?t09_cc@T09Cc@@YWHHH@Z |
| C++ | intel_ocl_bicc | Clang rechaza: .\api.h:3:1: error: cannot mangle this calling convention yet | - | no evaluado |

## Hallazgos

1. `stdcall`, `fastcall`, `thiscall`, `pascal` y `ms_abi` producen en este destino
   la convención predeterminada (`ccc`). Clang avisa de que ignora `pascal`; los
   demás no cambian el símbolo C ni el decorado C++ (`YA`). NSC los consume como
   llamada predeterminada.
2. NSC reproduce `vectorcall`, `regcall`, `sysv_abi`, `preserve_most`, `preserve_all`
   (C) y `preserve_none` (C++) con la misma convención y el mismo símbolo, incluidos
   el prefijo `__regcall3__`, el decorado `@@16` y los códigos MS `YQ`, `Yw`, `YU`
   y `YV`.
3. **Inconsistencia:** la misma firma `int(int,int)` con `preserve_none` se acepta en
   C++ y se rechaza en C (`preserve_none does not yet support this signature`). La
   ruta C conserva una lista de firmas por caso; la ruta C++ valida el plan ABI de
   Clang.
4. `swiftcall`, `swiftasynccall` (LLVM `swifttailcc`) e `intel_ocl_bicc` se rechazan
   con un diagnóstico que nombra la convención y el símbolo. Siguen sin implementar.
5. Clang 23.1.0 no puede decorar en C++ las convenciones `preserve_all` e
   `intel_ocl_bicc` (`cannot mangle this calling convention yet`). No hay nada que
   consumir desde NSC.

## Límites

Solo llamada directa, O0, biblioteca estática y una firma. Quedan fuera de este
inventario los punteros a función, DLL, precompilación y otras firmas. T09-003 sigue
abierto. No se modificó producción.


## Actualización 2026-11-14: llamada por puntero a función

El script añade a cada caso un alias de puntero a función con la misma convención, una
función C/C++ que lo devuelve y un segundo consumidor NativeSharp que llama por ese
puntero. Si NSC lo acepta, toda llamada indirecta `i32 %p(i32 ...)` del IR debe usar
la convención de Clang y el ejecutable debe devolver el resultado esperado. La
corrida se hizo con el compilador que ya incluye
[el plan C de `preserve_none`](t09-preserve-none-c-plan.md). Evidencia:
`A:/temp/nsc-t09-convenciones-4lri3y7x`, código 0, sin aceptaciones con convención
distinta.

| Lenguaje | Atributo | Clang 23.1 O0 | Directa | Detalle | Por puntero | Detalle |
|---|---|---|---|---|---|---|
| C | cdecl | ccc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | stdcall | ccc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | fastcall | ccc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | thiscall | ccc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | pascal | ccc @t09_cc (atributo ignorado) | acepta | resultado correcto | acepta | resultado correcto |
| C | vectorcall | x86_vectorcallcc @"\01t09_cc@@16" | acepta | resultado correcto | acepta | resultado correcto |
| C | regcall | x86_regcallcc @__regcall3__t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | ms_abi | ccc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | sysv_abi | x86_64_sysvcc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | preserve_most | preserve_mostcc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | preserve_all | preserve_allcc @t09_cc | acepta | resultado correcto | acepta | resultado correcto |
| C | preserve_none | preserve_nonecc @t09_cc | acepta | resultado correcto | rechazo | nsc: error: preserve_none function pointer signature is not yet supported |
| C | swiftcall | swiftcc @t09_cc | rechazo | nsc: error: C/C++ calling convention 'swiftcall' is not yet supported in HIR for t09_cc | rechazo | nsc: error: record observation name lookup failed: unsupported C callback calling convention |
| C | swiftasynccall | swifttailcc @t09_cc | rechazo | nsc: error: C/C++ calling convention 'swiftasynccall' is not yet supported in HIR for t09_cc | rechazo | nsc: error: record observation name lookup failed: unsupported C callback calling convention |
| C | intel_ocl_bicc | intel_ocl_bicc @t09_cc | rechazo | nsc: error: C/C++ calling convention 'intel_ocl_bicc' is not yet supported in HIR for t09_cc | rechazo | nsc: error: record observation name lookup failed: unsupported C callback calling convention |
| C++ | cdecl | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | stdcall | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | fastcall | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | thiscall | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | pascal | ccc @"?t09_cc@T09Cc@@YAHHH@Z" (atributo ignorado) | acepta | resultado correcto | acepta | resultado correcto |
| C++ | vectorcall | x86_vectorcallcc @"?t09_cc@T09Cc@@YQHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | regcall | x86_regcallcc @"?t09_cc@T09Cc@@YwHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | ms_abi | ccc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | sysv_abi | x86_64_sysvcc @"?t09_cc@T09Cc@@YAHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | preserve_most | preserve_mostcc @"?t09_cc@T09Cc@@YUHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | preserve_all | Clang rechaza: .\api.h:3:1: error: cannot mangle this calling convention yet | - | no evaluado | - | no evaluado |
| C++ | preserve_none | preserve_nonecc @"?t09_cc@T09Cc@@YVHHH@Z" | acepta | resultado correcto | acepta | resultado correcto |
| C++ | swiftcall | swiftcc @"?t09_cc@T09Cc@@YSHHH@Z" | rechazo | nsc: error: C/C++ calling convention 'swiftcall' is not yet supported in HIR for ?t09_cc@T09Cc@@YSHHH@Z | rechazo | nsc: error: record observation name lookup failed: unsupported C callback calling convention |
| C++ | swiftasynccall | swifttailcc @"?t09_cc@T09Cc@@YWHHH@Z" | rechazo | nsc: error: C/C++ calling convention 'swiftasynccall' is not yet supported in HIR for ?t09_cc@T09Cc@@YWHHH@Z | rechazo | nsc: error: record observation name lookup failed: unsupported C callback calling convention |
| C++ | intel_ocl_bicc | Clang rechaza: .\api.h:3:1: error: cannot mangle this calling convention yet | - | no evaluado | - | no evaluado |

Hallazgos adicionales:

1. Por puntero se aceptan con la convención de Clang todas las filas aceptadas en
   directo, salvo C `preserve_none` `int(int,int)`. Esa fila se rechaza en el emisor
   (`preserve_none function pointer signature is not yet supported`), mientras que la
   fila C++ equivalente pasa. Causa: `cFunctionPointerAbiPlan` (`ClangSession.cpp`) y
   su llamador en `CppProjectSessions.cpp` solo observan planes de punteros C
   `sysv_abi` y `regcall`.
2. `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` por puntero se rechazan con
   `record observation name lookup failed: unsupported C callback calling convention`.
   El rechazo es correcto, pero el mensaje no nombra el alias ni la convención.
   Queda como pendiente de diagnóstico.
