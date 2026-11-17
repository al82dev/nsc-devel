# T19-002: la dirección de una global C `_Atomic`, también `_Thread_local`

Fixture nuevo: `pruebas-tanda-19-atomicas-direccion/validar.py`, con y sin `--dll`. Sonda de desarrollo:
`tools/t19c/d12/sonda-t19-002b.py` (`logs/sonda2b.log`, `logs/sonda2c.log`).

## Defecto

`&a_int` de una global C `_Atomic` (también `_Thread_local`) daba «cannot take the address of C atomic storage until
atomic-qualified pointers are supported», y una función C con un parámetro o un resultado `_Atomic int*` no se
importaba («C/C++ parameter 0 is not yet mapped»). NativeSharp no tenía un puntero cuyo dato apuntado fuera
atómico.

## Diseño

Como `T volatile*` (T08-D03), el calificador es una marca interna del dato apuntado, sin sintaxis de usuario y sin
cambio de la clave de tipo: el puntero es `int*` con `cppPointerPointeeAtomic`. Viaja en `ExpressionResult`,
`MethodExpressionTypeBinding`, `MethodLocalSymbol`, `MethodParameterSymbol` y en el resultado de un
`ClassMemberSymbol` (`cppDeclaredPointerPointeeAtomic`).

- Origen:
  - `&a` de una global C `_Atomic` (escalar o record, también `volatile` o `const`, también `_Thread_local`), en
    `IdentifierAnalyzer`. El almacenamiento sigue sin ser direccionable, así que `ref`, `in` y `out` no lo
    alcanzan sin el acceso atómico (rechazo propio de NativeSharp).
  - un parámetro o un resultado C `_Atomic T*` (`cAtomicPointeeCarrier` en `CppSemanticFunction`: el mismo puntero a
    T, con los calificadores del tipo atómico).
- Propagación: `var p = &a`, `p + i`, `c ? p : q` y `p ?? q` con la misma marca, y `void*`, como en C.
- Rechazos, también en Clang: convertir entre un puntero a atómico y uno ordinario (inicialización con tipo
  escrito, asignación, argumento, rama condicional), pasarlo a un callback o a una posición variádica, un miembro de
  un record atómico por el puntero (`p->a`, T08-D07) y escribir por un puntero a `const _Atomic int`.
- Acceso: `*p`, `p[i]`, `*p = v`, `*p += v`, `(*p)++`:
  - HIR (`HirCompletionTerminalFrames`, `HirCompletionAccessFrames`): el `PointerLoad`, el `AddressValue` o el
    `RawElementAddress` llevan `cppAtomicAccess` y el ancho de `cAtomicPointeeWidth` (el ancho natural del escalar,
    del puntero o del entero de un enum; el de un record, según la política de Clang de T08-D07);
  - LLVM: `load atomic ... seq_cst` del ancho natural (un puntero por `i64`, un bool por `i8`, un flotante por el
    entero de su ancho), volatile si el dato apuntado lo es; un record por `lowerCAtomicRecord`; la escritura y la
    lectura-modificación-escritura reutilizan las de las globales atómicas, que ahora aceptan también estos destinos.

No se modificó Clang ni NSLink. El contrato NSMeta no cambia: una firma pública NativeSharp no puede escribir el
calificador, y una función C del catálogo precompilado vuelve a proyectarse desde su tipo de Clang, que conserva
`_Atomic`.

## Qué funciona, como en Clang

Con `int`, `long long`, un puntero (`_Atomic(int*)`), `_Bool`, `double`, un record de 8 bytes, `volatile _Atomic
int` y `const _Atomic int`, y con globales `_Thread_local _Atomic int` y `long long` en la biblioteca estática (con
el CRT): pasar `&a` a un parámetro `_Atomic T*`, recibir `_Atomic int*` de una función C, guardarlo en un local
`var`, compararlo, convertirlo a `void*`, y leer, escribir y modificar por él. En el IR de NSC hay lecturas,
escrituras y actualizaciones atómicas `seq_cst`.

## Evidencia

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática, con TLS | 12188 | `A:/temp/nsc-t19-atomicas-zvj7mlg7` |
| DLL | 643918 | `A:/temp/nsc-t19-atomicas-sgdb27ag` |

Los dos, O0–O3, directo y desde la plantilla precompilada con y sin encabezado. Con la DLL, Clang rechaza una global
`_Thread_local` dllimport y NSC también, así que la parte TLS no aplica.

Rechazos genuinos, también en Clang: un `int*` desde `&a_int`, `&a_int` a un parámetro `int*`, un `int*` a un
parámetro `_Atomic int*`, `rp->a` con `rp` un `_Atomic R*` y `*cp = 4` con `cp` un `const _Atomic int*`.

## Negativos obsoletos convertidos

`pruebas-tanda-08-globales-c` esperaba que `&a_int`, `&t_ai`, `&a_r16`, `&t_v8`, `&v_v16` y `&d_d8` se rechazaran.
La comparación con Clang de esas direcciones está ahora en el fixture nuevo. Cada suite conserva un rechazo
genuino, también en Clang: un puntero ordinario desde esa dirección (`int* p = &a_int;`, `R16* p = &a_r16;`).
Las suites aceptan ahora `NSC_T08_NSC`, `NSC_T08_CLANG`, `NSC_T08_RUNTIME`, `NSC_T08_NSLINK` y `NSC_T08_TEMP`
para repetirse con el NSC y el Clang actuales.

Las tres suites convertidas pasan enteras con el NSC actual, O0–O3, directo y desde la plantilla precompilada, con
sus rechazos (`tools/t19c/d12/p40.cmd`, `logs/fx40-*.log`):

| Suite | Evidencia |
|---|---|
| `validar-atomicas.py` | `A:/nsc-interop-build/t08-globales-atomicas-k6hseyo2` |
| `validar-tls.py` (estática y DLL) | `A:/nsc-interop-build/t08-globales-tls-yebsw3ve` |
| `validar-atomicas-record.py` (básica, variantes y DLL) | `A:/nsc-interop-build/t08-atomicas-record-am99r88o` |

Regresión con este cambio, antes del commit:

- Tandas 10 a 19: 73 de 73 en estática (`logs/regresion-d41e`) y 54 de 54 en DLL (`logs/regresion-dll-d41d`).
- Los nueve CTest de interop y la identidad de NSC pasan (`lote-ct2`, lanzado por `tools/t19c/d12/p41.cmd`).
- Los tres guiones de T08 convertidos terminan con 0 (`logs/p40.log`).

Nota del 2026-11-17: este registro se publicó con el marcador sin reemplazar; los totales se tomaron de esos logs.

## Límites

Siguen rechazados con diagnóstico, en T19-038:

Nota del 2026-11-17: T19-038 cubrió estos límites; ver [atómicas punteros](t19-atomicas-punteros.md).

- campos y globales C de tipo `_Atomic T*` («has no compatible member», «C/C++ global type is not yet mapped»);
- tipos de función C con `_Atomic T*` (la función que los toma no se importa);
- la dirección de una global atómica cuyo valor es un puntero a dato calificado y la de una global `volatile` no
  atómica («until qualified pointers are supported»).
