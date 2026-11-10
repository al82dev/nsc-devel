# T08-D21 / T08-GATE-05: propuesta de agrupación de commits de la tanda 8

Fecha: 2026-11-09. Instrucción del usuario: preparar una propuesta concreta de agrupación por cambios relacionados,
conservando los respaldos. No se reescribe la rama publicada ni se hace push forzado hasta presentarla.

## Alcance medido

- **Rango:** `97911ad25` (`interop-t07-close`) .. `HEAD` de `development-cpp-clang`. Va desde `2910c54e0`
  (`interop-t08-scope`, 2026-11-05) hasta el último commit de esta tanda.
- **Contenido:** historial lineal, sin merges. Todos los commits son de `al82dev`; no hay commits de otros
  autores intercalados. Los cuatro que no llevan prefijo `interop`/`T08` también son de la tanda:
  - `runtime-evitar-colision-fltused`;
  - `tracking-t08-tls-atomicos-calificados`;
  - los dos `docs-archivar-*-nsmeta`.
- **Tamaño:** 2399 commits al preparar esta propuesta, más los commits de cierre posteriores, que se conservan
  uno a uno.
- **Publicación:** `github/development-cpp-clang` está en `1140e1bd6`. Esos pushes no los hizo esta sesión; el
  reflog los registra como pushes externos. Los commits posteriores están solo en local.

## Método: sin reaplicar cambios

Cada commit agrupado se crea con `git commit-tree`, usando el árbol exacto del último commit original de su rango
y como padre el commit agrupado anterior. Así:

- no se reaplica ningún parche, de modo que no puede haber conflictos;
- en cada frontera de grupo el contenido es idéntico al del historial original;
- el árbol final es idéntico al de `HEAD`, y el script lo comprueba;
- el autor y la fecha de cada grupo son los de su último commit original. El cuerpo del mensaje lista todos los
  commits originales que agrupa (hash corto y asunto), para conservar la trazabilidad.

Los grupos son rangos contiguos. Reordenar por tema daría grupos más puros, pero obligaría a reaplicar parches
con conflictos y produciría estados intermedios que nunca existieron ni se compilaron. No se recomienda.

Script: `A:\nsc-interop-build\claude\d21-agrupar.py`. Sin argumentos imprime el plan. Con `--crear` crea solo la
rama local `propuesta/t08-agrupado`, sin tocar ninguna otra referencia ni hacer push.

## Grupos propuestos

**Parte 1: trabajo fino del 5 al 8 de noviembre, 8 commits en lugar de 2337.**

| # | Último commit | Commits | Fechas | Contenido |
|---|---|---|---|---|
| 1 | `d7d5cdf51` | 446 | 11-05 | auditoría inicial; contratos de tipos C: alias, `volatile` y `const` en arreglos, records, enum, globales, typedef, punteros y callbacks; gate de identidad de codegen |
| 2 | `06bd98420` | 372 | 11-05 y 11-06 | identidad y catálogo de records C (USR, empaquetados, anidados, campos), metadata C de precompilados, gate de metadata C |
| 3 | `48e3fb33d` | 469 | 11-06 | validación por familias: identidad, enum, campos de bits, uniones, prototipos, globales `const`, opacos, typedef y callbacks; validación agrupada |
| 4 | `ff07ab051` | 211 | 11-06 | TLS C en DLL y estático, selección de LLD, emisión automática, bitcode y dependencias transitivas |
| 5 | `63baabce7` | 101 | 11-06 y 11-07 | TLS atómico; validación integral del 2026-11-07 |
| 6 | `2af2921f8` | 300 | 11-07 | ABI de `bool` C, globales, punteros calificados, atómicos y enum atómico; identidad en el gate |
| 7 | `b608144cd` | 342 | 11-07 y 11-08 | plantillas con dependencias C, records anónimos, miembros flexibles, ThinLTO, modo `volatile` del consumidor |
| 8 | `7735294f3` | 97 | 11-08 | DLL TLS homónimas, rutas Unicode, callbacks calificados y globales externas (detalle de T08-002) |

**Parte 2: bloques y decisiones, uno a uno.** Desde `ca78094a8` hasta el último commit (60 al preparar la
propuesta, más los de cierre):

- validaciones de los bloques T08-002 a T08-005;
- los bloques T08-006 a T08-020;
- T08-GATE;
- las decisiones T08-D01 a T08-D24, con D13b, D11/D12, D05b y D04b.

Ya son commits de bloque o de decisión y conservan su mensaje. Solo se unen `ea45997d4` y `d63632954`: el
segundo corrige un párrafo del documento del primero.

**Resultado: 68 commits en lugar de 2399**, más los commits de cierre que se agreguen. `python d21-agrupar.py`
imprime la lista completa.

## Respaldos

- Se conservan `claude\respaldo\development-cpp-clang-20261109.bundle` y `…-20261109-noche.bundle`.
- Antes de crear la rama propuesta se guarda un bundle nuevo con la rama completa hasta el último commit de cierre:
  `claude\respaldo\development-cpp-clang-20261109-cierre.bundle`. Se verifica con `git bundle verify`.

## Decisión del usuario

1. **Aprobar los grupos** o pedir otra partición; por ejemplo, partir el grupo 3 por familia de tipos.
2. **Qué hacer con la rama agrupada:**
   - **A, recomendada:** publicarla como rama nueva (`development-cpp-clang-t08`) y conservar
     `development-cpp-clang` tal como está;
   - **B:** reemplazar `development-cpp-clang` con `git push --force-with-lease`. Hay que coordinarlo con quien
     hace los pushes externos observados, porque cualquier trabajo que salga del historial actual tendría que
     rebasarse.

Mientras no se decida, no se publica nada y `development-cpp-clang` no se toca.

## Verificación de la propuesta local (2026-11-09, cierre de D25)

**Contenido final.**

- La rama local `propuesta/t08-agrupado` (`fa4104cb4`) tiene 70 commits sobre `97911ad25`.
- Su árbol final es `1821402789ea…`, el mismo que el de `48ca47aef`, el `HEAD` desde el que se creó
  (`git rev-parse propuesta/t08-agrupado^{tree} HEAD^{tree}`).
- Cada commit agrupado reutiliza el árbol exacto del último commit original de su rango, así que cada frontera es
  idéntica al historial original.
- Los commits posteriores a `48ca47aef` (D25 y esta validación) no están en la rama. Al regenerarla se añaden uno a
  uno, con el script y un nombre de rama nuevo; la rama actual no se reemplaza.

**¿Sirven los grupos para revisar?** Cambios de cada grupo de la parte 1, contados entre fronteras:

| # | Archivos | `src/` e `include/` | Pruebas | Documentos | Zonas de `src/` con más cambios |
|---|---|---|---|---|---|
| 1 | 479 | 96 | 95 | 285 | Frontend/Bodies, Frontend/Generics, LLVM/Lowering |
| 2 | 493 | 54 | 62 | 376 | LLVM/Lowering, Interop/Cpp, Frontend/HIR |
| 3 | 620 | 57 | 285 | 277 | LLVM/Lowering, Interop/Cpp, Frontend/HIR |
| 4 | 365 | 32 | 152 | 180 | LLVM/Lowering, LLVM/Internal, Driver/Internal |
| 5 | 212 | 30 | 82 | 99 | Frontend/HIR, LLVM/Lowering, Frontend/MIR |
| 6 | 949 | 42 | 592 | 312 | LLVM/Lowering, Interop/Cpp, Frontend/HIR |
| 7 | 796 | 85 | 343 | 366 | Frontend/Bodies, Frontend/HIR, Interop/Cpp |
| 8 | 361 | 73 | 190 | 97 | Interop/Cpp, LLVM/Internal, Frontend/Bodies |

**Valoración.**

- En código cada grupo es revisable: de 30 a 96 archivos de `src/` e `include/`.
- Los grupos son cronológicos. Las mismas zonas (LLVM/Lowering, Interop/Cpp, Frontend/HIR) aparecen en varios,
  porque el trabajo de esos días alternaba familias de tipos C.
- Los grupos 3, 6 y 7 son grandes por las pruebas y los documentos.
- **Variante más fina, para revisar los cambios:** partir esos tres grupos en sus commits de validación intermedios
  (en el grupo 3, por ejemplo, en `294347178` campos de bits, `6ad07431a` TLS en DLL y `7ce79da8f` `volatile`). Se
  llega a unos 16 grupos sin reordenar nada y con el mismo método.

No se reemplazó `development-cpp-clang` ni se hizo push. Los tres bundles de `claude\respaldo` se conservan.

## Propuesta revisada (2026-11-10, después de T08-D26)

Indicación del usuario: actualizar la propuesta para que incluya todos los commits nuevos y organizarla en grupos
relacionados que se puedan revisar, sin buscar un número concreto. El contenido final tiene que ser idéntico al de
la rama original y se conservan los respaldos. No se reemplaza la rama publicada ni se hace push forzado.

**Método.** Es el de la primera propuesta (`git commit-tree` con el árbol exacto de cada frontera), con el script
`A:\nsc-interop-build\claude\d21-agrupar-2.py`. Se mantienen dos comprobaciones:

- cada frontera tiene que seguir a la anterior en el historial lineal;
- el árbol final tiene que ser idéntico al de `HEAD`.

El script crea la rama local `propuesta/t08-agrupado-2`. La primera rama, `propuesta/t08-agrupado`, no se toca.

**Parte 1: grupos por familia.** Los grupos 1, 2, 3, 6 y 7 de la primera propuesta se parten en sus commits de
validación, y el grupo 8 en dos. Quedan 23 grupos en lugar de 8. Ningún commit cambia de orden.

Columnas: «Commits» es el número de originales que agrupa; «Archivos» y «`src/`» se cuentan entre fronteras.

| # | Último commit | Commits | Archivos | `src/` e `include/` | Contenido |
|---|---|---|---|---|---|
| 1 | `63c849735` | 221 | 263 | 42 | inventario de tipos C, modo de lenguaje de records, enumeradores, punteros a función, callbacks y metadata de typedef |
| 2 | `8473f317f` | 100 | 155 | 28 | arreglos, records y globales `const`; `volatile` en arreglos y campos |
| 3 | `d7d5cdf51` | 125 | 126 | 57 | punteros `volatile`, alias `volatile` de record y gate de identidad de codegen |
| 4 | `35e9cf099` | 101 | 159 | 31 | anchos y alineación `volatile`, USR de funciones, enums y dependencias de campo, records empaquetados |
| 5 | `70b42a663` | 50 | 83 | 8 | arreglos de agregados `const` y `volatile`, modo `volatile` de los campos |
| 6 | `1753aa723` | 90 | 134 | 17 | USR de campos y globales, tipo del valor de los enumeradores, valores C en espacios de nombres |
| 7 | `06bd98420` | 131 | 163 | 20 | catálogo de snapshots de records C, metadata C en precompilados, clausura externa, gate de metadata C |
| 8 | `3b9f47d8f` | 91 | 104 | 22 | comparación, procedencia y destino de records C; codec, snapshot y cabecera compartida |
| 9 | `df48b8e45` | 100 | 128 | 29 | enum C, metadata de callbacks, formatos de punto flotante |
| 10 | `af8ab2fa4` | 43 | 65 | 8 | campos de bits, uniones y `volatile` en records C |
| 11 | `ed2a018ec` | 128 | 224 | 25 | prototipos y redeclaraciones, globales `const`, opacos, typedef, arreglos y operadores enum |
| 12 | `48e3fb33d` | 107 | 167 | 18 | TLS C en DLL, punteros y arreglos `volatile`, modo del consumidor, callbacks con records |
| 13 | `ff07ab051` | 211 | 365 | 32 | TLS C en DLL y estático, LLD, emisión automática, bitcode, dependencias transitivas |
| 14 | `63baabce7` | 101 | 212 | 30 | TLS atómico y validación integral del 2026-11-07 |
| 15 | `261c9223e` | 47 | 87 | 9 | ABI de `bool` C |
| 16 | `8595a7c2d` | 67 | 177 | 7 | globales atómicas, punteros calificados y atómicos, TLS con punteros |
| 17 | `2f3cd3860` | 32 | 87 | 6 | globales C estáticas e internas |
| 18 | `2af2921f8` | 154 | 651 | 37 | globales internas calificadas, atómicos enum, `bool` por puntero y callback, primeros pasos de T08-002 |
| 19 | `ce0f8c13f` | 88 | 271 | 38 | arreglo flexible, records precompilados compatibles, H3 con dependencias de plantillas |
| 20 | `f14b22da4` | 144 | 274 | 35 | records anónimos C, conciliación de alias y enum, miembros flexibles |
| 21 | `b608144cd` | 110 | 305 | 39 | sombras H3, ThinLTO, modo `volatile` del consumidor, firmas C externas |
| 22 | `4e80a2adc` | 49 | 251 | 41 | identidad inline y de globales entre bibliotecas, TLS en DLL homónimas, rutas Unicode |
| 23 | `7735294f3` | 48 | 128 | 46 | variádicas en DLL, typedef de record y enum, huellas de caché, callbacks calificados |

Cada grupo toca entre 6 y 57 archivos de `src/` e `include/`. Los que más archivos tocan (13 y 18 a 22) lo son por
las pruebas y los documentos.

**Parte 2: bloques y decisiones, uno a uno.** Son los commits desde `ca78094a8` hasta el último de la rama:
bloques T08-002 a T08-020, T08-GATE, las decisiones T08-D01 a T08-D26, el cierre técnico, `item341`/`item347` y
esta validación. Conservan su mensaje. Como antes, solo se unen `ea45997d4` y `d63632954`.

**Respaldo.** Al crear la rama se guarda `claude\respaldo\development-cpp-clang-20261110-d26.bundle`, con
`development-cpp-clang` y las dos ramas de propuesta, y se verifica con `git bundle verify`. Se conservan los
cuatro bundles anteriores.

**Hashes.** La rama se crea desde el último commit de la tanda, que es el que contiene este documento. Los hashes
de la rama, de su árbol y del bundle se informan al presentar la propuesta, porque escribirlos aquí cambiaría ese
commit.
