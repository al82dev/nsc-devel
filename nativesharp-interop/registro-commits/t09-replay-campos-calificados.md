# T09-003: replay precompilado de records C con campos `const` y `volatile`

## Defecto

Sonda `build-claude-t08/tools/sonda-volatil.py`, evidencia
`A:/temp/nsc-t09-sonda-volatil-59ab6h_k`. Un record C pasado por valor a un puntero
`sysv_abi` o `regcall`, consumido desde una plantilla precompilada sin encabezado:

| Campo | Antes |
|---|---|
| `int x; int y; int z;` | pasa |
| `volatile int z` | rechazado |
| `const int y` | rechazado |
| `const volatile int y` | rechazado |
| `volatile int y[2]` | rechazado |
| `_Alignas(8) int z` | rechazado |

El diagnóstico es `<convención> function pointer signature is not yet supported`. Clang
compila los seis casos y su IR clasifica igual el record calificado y el plano.

Causa en el código. El consumidor reconstruye con Clang los records del catálogo (replay)
para obtener su plan ABI, y dos puntos excluían cualquier calificador:

- `makeSimpleCRecordAbiReplay` (`CppCRecordAbiReplay.h`) rechazaba un campo o un nivel de
  arreglo `const` o `volatile`. Sin replay no hay plan y la firma se rechaza.
- En el productor, `hasReplayBuiltinOrArray` (`ClangSession.cpp`) devolvía falso ante
  cualquier calificador local. Ese resultado decide si el record queda marcado como
  reproducible en el catálogo.

Una tercera causa apareció con el fixture: un record anidado calificado
(`volatile T09In sub;`) se rechazaba incluso con el encabezado, porque el consumidor de la
plantilla usa también el catálogo. Las dos búsquedas de dependencias anidadas del replay
(`planPrecompiledCppFunctionPointerForAlias` y `planPrecompiledCppVariadicCall`, en
`CppProjectSessions.cpp`) exigían que la grafía del campo fuera exactamente `struct T09In`;
la del campo es `volatile struct T09In`. La sonda `sonda-anidado.py`
(`A:/temp/nsc-t09-sonda-anidado-60fzr5i0`) confirmó que en sesión viva, sin plantilla, el mismo
record pasa con `cdecl`, `sysv_abi` y `regcall`, en llamada directa y por puntero.

`_Alignas` es otro caso: el campo lleva un atributo de alineación que el catálogo no
guarda, así que no puede reconstruirse sin cambiar el contrato NSMeta.

## Cambio

- `cppReplayQualifiers` devuelve el prefijo `const `/`volatile ` de un tipo.
- `makeSimpleCRecordAbiReplay`:
  - admite campos `const` y `volatile`; `restrict` sigue rechazado;
  - un nivel de arreglo solo puede llevar los calificadores de su elemento;
  - exige que la grafía canónica empiece por esos calificadores y la clasifica sin ellos;
  - escribe el campo con sus calificadores (`volatile int __nsc_f2;`).
- `matchesSimpleCRecordAbiReplay` ya comparaba `isConst` e `isVolatile` campo por campo.
  Para un campo record compara además la grafía con calificadores.
- `hasReplayBuiltinOrArray` quita `const` y `volatile` de los calificadores locales y
  rechaza cualquier otro, por ejemplo un espacio de direcciones.
- `cppReplayUnqualifiedSpelling` devuelve la grafía sin el prefijo `const`/`volatile`, o vacía
  si el prefijo no coincide con los indicadores o el tipo es `restrict`. Las cuatro
  comprobaciones de dependencias anidadas comparan esa grafía con `struct <nombre>` o
  `union <nombre>`; la identidad (USR, nombre, tamaño, alineación, procedencia) se sigue
  comprobando igual.

Se conservan las demás comprobaciones: layout, desplazamiento de cada campo, identidad de
los records anidados y procedencia. No se modificó Clang ni NSLink. El catálogo usa los
campos existentes y el contrato NSMeta vigente no cambia.

## Evidencia

### Sonda tras el cambio

`A:/temp/nsc-t09-sonda-volatil-p0d_x1q8` y, tras la corrección de dependencias anidadas,
`A:/temp/nsc-t09-sonda-volatil-9_4aqjnd`: con `sysv_abi` y `regcall`, el caso plano y los
cuatro calificados devuelven 7, el valor esperado. `_Alignas` sigue rechazado con el diagnóstico
de la convención.

### Otras rutas que leen el mismo catálogo

El indicador de replay del productor también lo leen las llamadas directas variádicas con
records C. La sonda `sonda-directa.py` (`A:/temp/nsc-t09-sonda-directa-zmx93lc4`) probó, sin
encabezado, `sysv_abi` y `regcall` directas, `sysv_abi` variádica y `cdecl` directa y variádica,
con records planos, calificados, empaquetados y `_Alignas`:

- las llamadas directas `sysv_abi` y `regcall` y las `cdecl` no usan el replay (su plan viaja
  en el catálogo) y pasan en todos los casos, también los empaquetados y `_Alignas`;
- la llamada `sysv_abi` variádica pasa con los records calificados y sigue rechazando los
  empaquetados y `_Alignas` con `sysv_abi record call plan is not available`;
- los punteros con records empaquetados siguen rechazados aunque no tengan calificadores
  (`sonda-empaquetado.py`, `A:/temp/nsc-t09-sonda-empaquetado-acri2rao`). El cambio no los
  alcanza.

Por eso el fixture cubre también la llamada `sysv_abi` variádica.

### Fixture nuevo

`pruebas-tanda-09-precompilado-campos-calificados/validar.py`, con y sin `--dll`. Para
`sysv_abi` y `regcall`, de O0 a O3, prueba estos records:

- con un campo `volatile`;
- con un campo `const`;
- con un campo `const volatile` y un `double`;
- con un arreglo `volatile int[3]`;
- con un record anidado `volatile`.

En cada caso:

- un control C compilado por Clang 23.1.0 da el resultado esperado;
- la plantilla precompilada se consume con y sin encabezado y devuelve el mismo valor;
- el IR del consumidor llama al puntero con la convención de Clang (`x86_64_sysvcc` o
  `x86_regcallcc`).

Para la llamada directa `sysv_abi` variádica `t09_v(r, 4, 9, r)`, de O0 a O3 (los cuatro
records sin record anidado en este commit; el anidado `volatile` se añadió con
[t09-variadica-records-anidados.md](t09-variadica-records-anidados.md)), el record va como argumento fijo y como argumento variable. Se exige que
los argumentos físicos del IR de NSC (tipo, extensión, `byval` y alineación) sean los de la
llamada de Clang, y que el resultado coincida con el control C.

Los controles negativos comprueban que un campo `_Alignas(16)` sigue rechazado sin
encabezado: en el puntero, con el diagnóstico de la convención, y en la llamada variádica, con
`sysv_abi variadic argument type is not yet supported`, porque el record también va como
argumento variable.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (punteros: 2 convenciones × 5 records × O0–O3; variádica: 4 records × O0–O3) | `A:/temp/nsc-t09-campos-calificados-7d5ckc63` |
| DLL | pasa (mismas celdas) | `A:/temp/nsc-t09-campos-calificados-6gq6_pcn` |

Los tres controles negativos `_Alignas` pasan en las dos variantes.

### Prueba unitaria actualizada

`tests/unit/CppClangAbiProbeTests.cpp` exigía que `struct QualifiedField { const int x; }` no
fuera reproducible (código 209). Ese caso pasa a positivo, se añaden `VolatileField` y
`ConstVolatileArray` también positivos, y el rechazo se conserva con
`UnalignedField { __unaligned int x; }`: `__unaligned` es un calificador que el replay no
admite.

### Controles negativos convertidos

Catorce guiones `pruebas-tanda-09-sysv-indirecta-*/validar-precompilado.py` usaban un campo
`volatile` como rechazo. Siguiendo la política acordada:

- cada variante `volatile` se ejecuta ahora como comparación positiva;
- el rechazo se conserva en una variante `alignas` nueva, con la misma comprobación: el
  mismo campo declarado `_Alignas(16) int` y el diagnóstico
  `sysv_abi function pointer signature is not yet supported`.

Tres guiones tenían dos variantes `volatile` y las dos se convirtieron. El cambio lo
aplicó `build-claude-t08/tools/transformar-volatil.py` sobre la lista
`lista-volatil.txt`. Los catorce pasan con el compilador nuevo
(`logs/vol-transformar.log` y `logs/vol-<guion>.log`).

Regresión de la tanda: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- `_Alignas` y otros atributos de campo quedaban fuera del replay precompilado: el catálogo
  no los guarda. Los atributos de alineación se resolvieron después, reconstruidos a partir del
  layout: [t09-replay-alineacion.md](t09-replay-alineacion.md). Los demás atributos siguen fuera.
- `restrict` y cualquier calificador distinto de `const` y `volatile` siguen rechazados.
- Los records C empaquetados quedaban fuera de los punteros `regcall`/`sysv_abi` y de la llamada
  `sysv_abi` variádica precompilados, con o sin calificadores. Se resolvió después:
  [t09-replay-empaquetado-char.md](t09-replay-empaquetado-char.md).
- Los records C++ no cambian: su productor sigue exigiendo campos sin calificar.
- Un record C con un record anidado no tenía plan en una llamada `sysv_abi` variádica
  precompilada, con o sin calificadores. Era un límite previo, no de este cambio
  (`sonda-var-anidado.py`, `A:/temp/nsc-t09-sonda-var-anidado-4igx4zrv`). Se resolvió después:
  [t09-variadica-records-anidados.md](t09-variadica-records-anidados.md).
- Solo cubre los records que ya entraban en el replay: tipos incorporados, arreglos y
  records C con nombre y sin contexto anónimo.
