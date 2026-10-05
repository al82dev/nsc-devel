# NativeSharp 3.0 — Limitaciones resueltas

**Fecha de corte:** 2026-10-05
**Rama:** `development`  
**Plataforma contractual:** Windows x64  
**Propósito:** evitar que agentes, convertidores, ejemplos y bibliotecas conserven workarounds que
ya no son necesarios al trabajar con el compilador NativeSharp 3.0.

**Validación acumulada en curso:** corrida externa informada por Alberto en **635/671**, sin fallos
ni errores hasta ese punto. Esta cifra no se presenta como baseline aprobado mientras no termine la
corrida completa.

## Cómo usar este documento

Una limitación de esta lista puede tratarse como resuelta únicamente al usar una construcción de
NSC que contenga el commit indicado o uno posterior. `TERMINADA` significa que existe una vertical
productiva y que pasaron sus pruebas dirigidas. No significa que Alberto haya ejecutado el baseline
integral después de cada cambio.

Al actualizar código convertido desde NativeSharp 2.0 o desde C/C++:

1. buscar el identificador o el síntoma en esta tabla;
2. retirar el workaround descrito en **Acción para agentes**;
3. usar la forma 3.0 indicada;
4. conservar una prueba específica del comportamiento que necesitaba el proyecto;
5. no extender la corrección a construcciones expresamente excluidas.

## Lenguaje, parser y generación de código

| ID | Limitación anterior | Resuelta en 3.0 | Acción para agentes | Evidencia |
|---|---|---|---|---|
| `NS3-LIM-CV-001` | `new C(...)` fallaba dentro de funciones libres de namespace. | Las funciones libres participan en resolución de constructores, accesibilidad, ownership, HIR, MIR y LLVM con un ordinal de callable estable. | Eliminar clases o métodos contenedores creados sólo para poder construir objetos. Escribir la construcción directamente en la función libre. | `9ecf5d16e`; T29: 6 checks; regresiones parser/construction dirigidas. |
| `NS3-LIM-CV-007` | `for` no aceptaba varias expresiones separadas por coma. | Inicialización e incremento admiten listas ordenadas y las ejecutan de izquierda a derecha. | Volver a unir inicializadores o incrementos que fueron separados artificialmente. No usar la coma como operador general: sólo existe en esas dos posiciones del `for`. | `9ecf5d16e`; T29. |
| `NS3-LIM-CV-008` | La interpolación sólo cubría holes constantes o exigía composición manual. | Los holes runtime se parsean como expresiones normales, preservan orden, ownership y cleanup, y usan las operaciones canónicas de `String`. | Sustituir cadenas de concatenaciones auxiliares por interpolación. Mantener el formato dentro de los formatos soportados; `G` con precisión continúa rechazado. | `9ecf5d16e`; T29. |
| `NS3-LIM-CV-011` | `if` y `else` exigían llaves incluso para una sola sentencia. | `if`/`else`, `else if` y la sentencia vacía aceptan cuerpo embebido y se normalizan a un bloque semántico. | Retirar llaves añadidas únicamente por el compilador viejo si el estilo del proyecto lo desea. Las declaraciones locales sin llaves siguen prohibidas; `while`, `do`, `for`, `foreach`, `using`, `switch`, `match`, `try`, `catch`, `finally` y `asm` conservan sus propias reglas. | `9ecf5d16e`; T29. |
| `NS3-LIM-CV-012` | Escrituras mediante parámetros `out` se perdían al atravesar recursión, especialmente en funciones libres. | `ref`, `out` e `in` de funciones libres conservan su representación by-reference hasta HIR/LLVM. | Eliminar retornos o contenedores artificiales usados para simular varios resultados. Volver a usar el parámetro `out` y probar el caso recursivo real. | `9ecf5d16e`; Euclides extendido recursivo en T29. |

Documento técnico: [`TANDA-OPERATIVA-29-ERGONOMIA-PROCEDURAL.md`](TANDA-OPERATIVA-29-ERGONOMIA-PROCEDURAL.md).

## Consola y entrada estándar

| ID | Limitación anterior | Resuelta en 3.0 | Acción para agentes | Evidencia |
|---|---|---|---|---|
| `NS3-LIM-CV-002` | `Console` carecía de llamadas sin argumentos y de overloads primitivos suficientes. | `Write`, `WriteLine`, `ErrorWrite` y `ErrorWriteLine` admiten forma vacía y valores `bool`, `char`, enteros y flotantes comunes. | Retirar conversiones manuales a string y argumentos ficticios usados sólo para imprimir una línea vacía. | `fa38d7e1b`; T30: 8 checks y regresiones dirigidas 76/34/16/22. |
| `NS3-LIM-CV-003` | No existía lectura secuencial tipo `ReadLine` con EOF y redirección definidos. | `Console.ReadLine` distingue línea válida, línea vacía, EOF, error y buffer insuficiente; funciona con consola, archivo y pipe. | Sustituir wrappers de Win32 o lectura por argumentos por `ReadLine`. Tratar `null`/status según el contrato, sin confundir línea vacía con EOF. | `fa38d7e1b`; T30. |
| `NS3-LIM-IO-001` | `GetCh` devolvía `-1` con stdin redirigido y se usaba como lector general. | Los contratos se separaron: `GetCh` es interactivo; `ReadLine` es secuencial y redirigible. | No “arreglar” `GetCh` para pipes. Usar `ReadLine` en automatización, archivos y stdin redirigido. | `fa38d7e1b`; T30. |

Documento técnico: [`TANDA-OPERATIVA-30-CONSOLE-STDIN.md`](TANDA-OPERATIVA-30-CONSOLE-STDIN.md).

## Arrays, vistas y ownership

| ID | Limitación anterior | Resuelta en 3.0 | Acción para agentes | Evidencia |
|---|---|---|---|---|
| `NS3-LIM-CV-004` | No había bridge canónico de `T[]` a memoria contigua prestada. | Arrays dinámicos e inline producen vistas `Span<T>`/`ReadOnlySpan<T>` sin copia ni allocation oculto, con lifetime comprobado. | Retirar copias temporales y bridges pointer+length ad hoc. Usar `Spans.Of` y `AsReadOnly`; no devolver una vista enraizada en storage automático. | `d9877886a`; T31: 12 checks y regresiones dirigidas 23/76/31. |
| `NS3-LIM-CV-005` | No se podían construir/destruir arrays escalonados dinámicos con ownership correcto. | `T[][]`, `new T[rows][]`, `new T[][rows]` e inicializadores jagged usan destrucción recursiva y anulan cada slot antes del drop. | Retirar wrappers manuales de filas. Usar jagged arrays cuando cada fila pueda tener longitud distinta; no confundirlos con `T[,]`. | `d9877886a`; T31. |
| `NS3-LIM-CV-013` | `new T[] { ... }` no podía deducir la longitud. | La longitud del inicializador agregado se infiere, incluso para arrays vacíos, owning y jagged. | Retirar constantes de longitud duplicadas y asignaciones elemento por elemento creadas sólo como workaround. | `d9877886a`; T31. |
| `NS3-LIM-CV-006` | No existía el tipo multidimensional de rango `T[,]`. | Los arrays rectangulares tienen rango, dimensiones, strides, indexación y layout multidimensional propios. | Reemplazar flattening manual cuando el dato sea rectangular. Mantener jagged arrays para filas heterogéneas. | `028f88665`; T32: 10/10 O0. |

Documentos técnicos:

- [`TANDA-OPERATIVA-31-ARRAY-BORROW-JAGGED.md`](TANDA-OPERATIVA-31-ARRAY-BORROW-JAGGED.md)
- [`TANDA-OPERATIVA-32-ARRAYS-MULTIDIMENSIONALES-RANGO.md`](TANDA-OPERATIVA-32-ARRAYS-MULTIDIMENSIONALES-RANGO.md)

## Paquetes, metadata y publicación

| ID | Limitación anterior | Resuelta en 3.0 | Acción para agentes | Evidencia |
|---|---|---|---|---|
| `NS3-LIM-CV-009` | La selección entre fuente y precompilado no exponía provenance ni tenía modo estricto. | El grafo publica provenance/fingerprint y admite consumo `precompiled-only`/`no-restore` sin fallback silencioso. | Eliminar scripts que ocultan o renombran fuentes para forzar metadata. Seleccionar la política explícitamente y conservar el reporte de resolución. | `d89712bc3`; 24/24, 25/25 y 6/6; consumidor sin fuentes ejecutado. |
| `NS3-LIM-PKG-001` | Mezclar `ProjectReference` y metadata podía producir closures incompatibles. | La closure se decide antes de compilar, se valida por fingerprint y rechaza mezclas incompatibles de forma causal. | No copiar artefactos desde instalaciones externas para “completar” el grafo. Publicar/restaurar el paquete y consumir una sola provenance por módulo/API. | `d89712bc3`; T33. |
| `NS3-LIM-CV-010` | Rutas largas fallaban tarde y dejaban errores engañosos o artefactos parciales. | La publicación usa staging corto, rutas Win32 extendidas, preflight, commit transaccional y rollback del conjunto completo. | Retirar acortadores de rutas y copias manuales usados sólo por `MAX_PATH`. Conservar el diagnóstico causal (`path-limit`, `file-locked`, espacio, permisos, etc.). | `98d9f1d1a`; T34: 7/7, 24/24 y 6/6; artefactos finales a más de 260 caracteres. |

Documentos técnicos:

- [`TANDA-OPERATIVA-33-PRECOMPILED-PROVENANCE-PAQUETES.md`](TANDA-OPERATIVA-33-PRECOMPILED-PROVENANCE-PAQUETES.md)
- [`TANDA-OPERATIVA-34-RUTAS-LARGAS-PUBLICACION-TRANSACCIONAL.md`](TANDA-OPERATIVA-34-RUTAS-LARGAS-PUBLICACION-TRANSACCIONAL.md)

## Depuración y CodeView/PDB

| ID | Limitación anterior | Resuelta en 3.0 | Acción para agentes | Evidencia |
|---|---|---|---|---|
| `NS3-LIM-DBG-001` | Las locales aparecían visibles antes de declararse. | Cada declaración inicia un scope CodeView desde su inicialización y termina con su bloque léxico. | Retirar filtros del depurador que ocultaban locales “futuras”. Conservar el alcance fuente real. | `473bc9ff5`; T36 O0. |
| `NS3-LIM-DBG-002` | Algunas sentencias ejecutables no tenían sitio estable de breakpoint. | Las líneas ejecutables aprobadas conservan ubicación y dirección utilizables para breakpoint/step. | Dejar de mover sentencias a líneas auxiliares únicamente para obtener un breakpoint. | `473bc9ff5`; T36 O0. |
| `NS3-LIM-DBG-006` | Variables homónimas de loops consecutivos tenían lifetimes solapados. | Cada `for` publica su propio scope y rango CodeView. | Eliminar renombrados artificiales como `i1`, `i2` cuando sólo evitaban la confusión del depurador. | `473bc9ff5`; T36 O0. |
| `NS3-LIM-DBG-003` | Argumentos o literales podían mostrarse como `<optimized out>` a O0. | Los argumentos O0 conservan homes completos y valores del caller; el caso quedó congelado contra regresión. | No introducir locales espejo sólo para inspeccionar parámetros en Debug/O0. | `e9004e430`; T37 O0, fixture exit 42. |
| `NS3-LIM-DBG-004` | Un parámetro `out` se mostraba como dirección cruda. | El ABI sigue pasando pointer, pero CodeView publica una referencia al tipo fuente (`int&`, etc.). | Retirar visualizadores que desreferenciaban manualmente todo parámetro `out`. | `e9004e430`; T37. |
| `NS3-LIM-DBG-005` | Estados `_nsc_state_` carecían de nombre/tipo/ancho fuente fiables. | Los estados de namespace se publican como globals CodeView tipados y con nombre calificado. | Resolverlos por identidad fuente; no depender del nombre físico `_nsc_state_*`. | `e9004e430`; T37. |
| `NS3-LIM-DBG-007` | El parámetro tipado de `catch` aparecía como local ordinaria y con rango incorrecto. | El binding se publica como parámetro dentro del scope físico del handler. | Retirar heurísticas que clasificaban la primera local del handler como excepción. | `e9004e430`; T37. |
| `NS3-LIM-DBG-008` | Los campos estáticos faltaban en el catálogo global. | Campos estáticos y estados de namespace generan entradas globales calificadas con tipo fuente. | Consultar el catálogo global directamente; no sintetizar campos desde símbolos de linker. | `e9004e430`; T37. |

Documentos técnicos:

- [`TANDA-OPERATIVA-36-CODEVIEW-SCOPES-BREAKPOINTS.md`](TANDA-OPERATIVA-36-CODEVIEW-SCOPES-BREAKPOINTS.md)
- [`TANDA-OPERATIVA-37-CODEVIEW-VALORES-GLOBALS.md`](TANDA-OPERATIVA-37-CODEVIEW-VALORES-GLOBALS.md)

## NSLink y observabilidad de compilación

| ID | Limitación anterior | Resuelta en 3.0 | Acción para agentes | Evidencia |
|---|---|---|---|---|
| `NS3-LIM-LINK-001` | El enlace O0 de proyectos grandes tardaba minutos sin fase, progreso o causa observable. | NSLink publica progreso/fases, telemetría y caché medible para localizar el costo real. | No sustituir NSLink por `lld-link` para ocultar el problema. Conservar logs de fase y revisar hits/misses antes de atribuir el tiempo al frontend. | `6d026d0` en `A:\nslink-devel`; 55/55, 157/157 y 84/84; probe de Math.Core medido. |

Documento técnico externo al repositorio del compilador:
`A:\nslink-devel\docs\3.0\TANDA-35-NSLINK-O0-TELEMETRIA-CACHE.md`.

## Elementos que **no** deben tratarse todavía como resueltos

Estos elementos estaban inventariados junto con las limitaciones anteriores, pero el tracking no
los marca como `TERMINADA`. Se mantienen fuera de la tabla de cierres para evitar falsos positivos:

| ID | Estado actual | Regla para agentes |
|---|---|---|
| `NS3-LIM-OWN-001` | `ENRUTADA` | No asumir que todas las relaciones internas borrowed están cerradas sólo porque existen vistas y bridges de arrays. Confirmar el caso concreto o mantener el workaround. |
| `NS3-LIM-PKG-002` | `ENRUTADA` | No asumir que `NativeSharp.Core.IO` ya se distribuye en toda configuración como paquete estático instalable. Confirmar el artefacto del staging usado. |

## Resumen de cierre

- Limitaciones marcadas como resueltas en este corte: **24**.
- Lenguaje/parser/codegen: **5**.
- Consola/entrada: **3**.
- Arrays/vistas: **4**.
- Paquetes/publicación: **3**.
- Depuración/CodeView: **8**.
- NSLink/observabilidad: **1**.
- Limitaciones explícitamente aún no cerradas: **2**.

Ante cualquier discrepancia de estado posterior a este corte, manda
`A:\NATIVESHARP-3.0-TRACKING.md`.

## Correcciones de baseline posteriores al inventario

Estas entradas documentan regresiones descubiertas después de consolidar las 24 limitaciones
importadas. No constituyen limitaciones canónicas adicionales ni alteran los conteos del inventario.

| Identificador | Síntoma | Corrección | Evidencia |
|---|---|---|---|
| `NS3-BASELINE-UNSAFE-EDITION-COMPAT` | Fixtures Language 2 recibían NS3030 porque la política `unsafe` de NativeSharp 3 se aplicaba sin distinguir edición; además, una prueba T15 tenía inicialización incompleta bajo `-Werror`. | La auditoría `unsafe` se limita a Language 3, Language 2 conserva su contrato legado de punteros/FFI, c56 declara el bloque `unsafe` correspondiente y el agregado T15 se inicializa por completo. | Commit `576e61343`; T13 9/9; T15 11/11; c56 O0 correcto; `docs/3.0/items/NS3-BASELINE-UNSAFE-EDITION-COMPAT.md`. |
| `NS3-BASELINE-SELFHOST-RESERVED-ALIASES` | El self-host usaba `i8`, `i32` e `i64` como identificadores después de que esos alias se reservaron como tipos; una ruptura sintáctica causaba 70 cierres derivados. | Se migraron sólo los identificadores internos a `int8Type`, `int32Type` e `int64Type`, preservando los alias del lenguaje y la semántica LLVM. | Commit `80e4e959a`; auditoría sin identificadores incompatibles; sintaxis 98/98; emisión O0/NSLink correcta en 2.73 s; `docs/3.0/items/NS3-BASELINE-SELFHOST-RESERVED-ALIASES.md`. |
| `NS3-BASELINE-NSLINK-SELECTION` | Nueve pruebas T13/T14 sustituían la ruta de NSLink elegida por NSCBuilder por `build-debug\bin\nslink.exe`. | Las pruebas conservan el `NSC_LINKER` seleccionado por el usuario o por el entorno de validación y dejan de depender del layout interno del repositorio NSLink. | Commit `2f69ae28b`; `docs/3.0/items/NS3-BASELINE-NSLINK-SELECTION.md`. |
| `NS3-BASELINE-UNSAFE-FIXTURES` | Fixtures positivas de null-aware operators, pinning y escape summaries realizaban operaciones de puntero fuera de `unsafe` y fallaban antes de medir su feature. | Las operaciones peligrosas quedaron dentro de bloques `unsafe` mínimos; los casos negativos conservan el diagnóstico estructural que pretendían probar. | Commit `66cec836b`; `docs/3.0/items/NS3-BASELINE-UNSAFE-FIXTURES.md`. |
| `NS3-BASELINE-NATIVE-INTEGER-PROMOTION` | La clasificación por ancho/signo ocultaba aristas normativas de `nint`/`nuint` y producía seis resultados incorrectos en la matriz de 91 pares. | Las conversiones nativas nombradas se consultan antes que la comparación física de ancho y signo; un destino nativo ya no se infiere sólo porque Windows x64 tenga el mismo ancho. | Commit `292aa5412`; matriz dirigida de 91 pares; `docs/3.0/items/NS3-BASELINE-NATIVE-INTEGER-PROMOTION.md`. |
| `NS3-BASELINE-NSMETA-138-CLOSURE` | Codec, fixtures y contratos finales discrepaban entre NSMETA 1.37/1.38 y MIR 1.29/1.30. | El productor actual, la matriz N/N-1 y el archivo histórico 1.37 quedaron alineados sin reescribir fixtures anteriores ni ampliar la ventana certificada. | Commit `661708953`; `docs/3.0/items/NS3-BASELINE-NSMETA-138-CLOSURE.md`. |
| `NS3-BASELINE-STAGE46-BOUND-DELEGATE-OWNERSHIP` | Stage 46 devolvía un método ligado a un receptor `borrowed` que podía escapar de la llamada y no llegaba a producir `NativeMath.lib`. | El receptor se transfiere como `owned`, el método ligado captura mediante `move` y los consumidores transfieren la instancia de forma explícita. | Commit `47522c1bd`; `docs/3.0/items/NS3-BASELINE-STAGE46-BOUND-DELEGATE-OWNERSHIP.md`. |
| `NS3-BASELINE-STAGE46-LEGACY-110-DOWNGRADE` | Una fixture marcada NSMETA 1.10 conservaba contratos genéricos de 1.38 y formaba una combinación históricamente imposible. | La degradación elimina contratos, lifetimes y restricciones `outlives` no representables antes de escribir la copia 1.10; el codec permanece estricto. | Commit `6bdef6a1c`; `docs/3.0/items/NS3-BASELINE-STAGE46-LEGACY-110-DOWNGRADE.md`. |
| `NS3-BASELINE-POSITIVE-UNSAFE-BOUNDARIES` | Fixtures positivas T39/T42/T44 y aliases opacos conservaban operaciones peligrosas sin frontera `unsafe` explícita. | Se añadieron fronteras mínimas y justificadas sin relajar la semántica; los negativos atraviesan deliberadamente la auditoría para aislar su error original. | Commit `ee668b327`; `docs/3.0/items/NS3-BASELINE-POSITIVE-UNSAFE-BOUNDARIES.md`. |
| `NS3-BASELINE-SELFHOST-I32-IDENTIFIER` | La fixture LLVM self-hosted usaba `i32` —palabra reservada de tipo en 3.0— como nombre de un `LLVMTypeRef`. | El identificador se cambió a `int32Type` sin alterar tipo LLVM, ABI ni objeto emitido. | Commit `195cef914`; `docs/3.0/items/NS3-BASELINE-SELFHOST-I32-IDENTIFIER.md`. |
| `NS3-BASELINE-SELFHOST-ATOMIC-LOAD-TYPE` | El backend self-hosted confundía `MemoryOrder` con el valor de `Interlocked.Load`, contaminando el tipo físico de la carga atómica. | Sólo `Store`, `Add`, `Exchange` y `CompareExchange` consultan un argumento de valor; `Load`, `Increment` y `Decrement` conservan el tipo derivado de la ubicación `ref`. | Commit `f9a655c22`; `Interlocked.Load(ref long, ...)` vuelve a emitir carga `i64`; la corrida acumulada completa continúa en validación; `docs/3.0/items/NS3-BASELINE-SELFHOST-ATOMIC-LOAD-TYPE.md`. |
| `NS3-BASELINE-STAGE50-NSMETA-138-GENERIC` | El positivo de Stage 50.9 añadía texto fuente a `Box<T>`, pero omitía el contrato posicional obligatorio desde NSMETA 1.38. | La fixture positiva incluye el contrato del parámetro `T`; los negativos y la validación estricta de aridad/ownership/lifetimes permanecen intactos. | Commit `9c4d2b520`; `docs/3.0/items/NS3-BASELINE-STAGE50-NSMETA-138-GENERIC.md`. |
| `NS3-BASELINE-RUNTIME-SOURCE-INVENTORY` | SH48 esperaba 81 fuentes aunque `ReferenceCounting.ns` elevó el inventario runtime real a 82. | El contrato registra 82 fuentes: 41 Core, 23 Collections.Generic, 4 Core.IO, 4 Diagnostics y 10 Threading. | Commit `2c2c5910f`; `docs/3.0/items/NS3-BASELINE-RUNTIME-SOURCE-INVENTORY.md`. |
| `NS3-BASELINE-T07-NSMETA-138` | T07 seguía exigiendo NSMETA 1.37 al productor actual aunque typed throws se emitía válidamente en 1.38. | La expectativa actual usa 1.38 y conserva 1.37 como versión mínima histórica de typed throws. | Commit `1dab2d685`; T07 54/54; `docs/3.0/items/NS3-BASELINE-T07-NSMETA-138.md`. |
| `NS3-BASELINE-T37-STABLE-PE-ENTRY` | T37 enlazaba directamente un `Main` que retorna `int` como entry point freestanding y dependía de que ese retorno se transformara incidentalmente en código de salida. | Un shim C `void` entrega el resultado de `Program.Main()` a `Environment.Exit`/`ExitProcess`, preservando el cuerpo y los símbolos que inspecciona la prueba PDB. | Commit `ca9599122`; T37 dirigido con salida 42; `docs/3.0/items/NS3-BASELINE-T37-STABLE-PE-ENTRY.md`. |

Las correcciones se desarrollaron con pruebas dirigidas. La corrida acumulada externa indicada al
inicio de este documento sigue en curso; no se declara baseline aprobado hasta conocer su resultado
final.
