# T08: cierre técnico del 2026-11-10 (preparación de la conclusión)

Indicación del usuario:

- revisar la propuesta D21 `propuesta/t08-agrupado-2` junto con la revisión de Codex
  (`A:\investigacion-cpp-interop\REVISION-D21.md`);
- registrar la limitación del fixture NSMeta 1.46 sin tocar la prueba;
- confirmar que la validación final corresponde a un solo commit, con los gates en secuencia;
- separar las pruebas aprobadas, las que fallan y las que no se ejecutaron.

La tanda 8 queda abierta hasta que el usuario revise D21 y `stage48_2`. No se tocó la rama de trabajo ni la
publicada y no se hizo push.

## Commit validado

- **Código validado:** `0ead9a1f6` (`HEAD` al empezar, 2026-11-10 18:16 UTC).
- **Árbol de trabajo:** fuera de `0ead9a1f6` solo tiene los cambios locales del usuario:
  - `docs/*` y `mkdown/*`;
  - `tests/unit/WindowsLinkerTests.cpp`, modificado localmente; entra en `nsc.windows-linker`, que mide el árbol
    de trabajo y no el commit;
  - los archivos sin seguimiento `AGENTS.md`, `docs/CONTRATO-VIGENTE.md`, `docs/REVISION-DOCUMENTAL-CONTRATO-UNICO.md`
    y los archivos basura de la raíz.
- **Commit de este documento:** cambia solo documentos Markdown, así que los resultados de abajo valen para él.
- **Publicado:** `github/development-cpp-clang` apunta a `0ead9a1f6`. Ese push no lo hizo esta sesión. El commit
  de este documento queda solo en local.

## Gates, en secuencia

`claude\scripts\integral-cierre.cmd` ejecuta los dos gates uno tras otro (`call`, no `start`). Cada uno reconstruye
el compilador y los ejecutables de prueba.

| Gate | Inicio (UTC) | Fin (UTC) | Resultado |
|---|---|---|---|
| `validar-tanda-08.cmd` (histórico) | 18:16:41 | 18:27:03 | código 0 |
| `validar-tanda-08-complementaria.cmd` | 18:27:03 | 18:38:16 | código 0, incluye las pruebas de D25 y D26 |

Las comprobaciones de D21 (solo lectura de Git y restauración del bundle en una carpeta nueva) corrieron a la vez
que el gate histórico. No comparten archivos con él.

## ctest

### Conjunto acordado de la tanda (29 pruebas)

Se ejecutó después de los gates.

- **Pasan 28:**
  - `cpp-interop.session`, `compilation-model`, `project-loader`, `project-graph`,
    `namespace-function-metadata`;
  - `item340` a `item349`;
  - `project-oop-lowering`, `stage46`, `stage46-1`, `stage47`, `stage48-2` (dos pruebas), `stage48_1`, `stage48_3`,
    `stage48_4`;
  - `library-metadata-version-tolerance`, `library-metadata-merger`, `stage50_9`, `windows-linker`.
- **Falla 1:** `nsc.stage48_2.library-metadata`, ver abajo.

**Familia SIMD** (15 pruebas: `t14.floating-point-semantics`, `item345`, `item347` a `item359`): pasan todas.

### `stage48_2`: dos comprobaciones pendientes

`nsc.stage48_2.library-metadata` ejecuta 444 comprobaciones: pasan 442 y fallan 2.

- `archived NSMETA 1.46 golden remains readable`;
- `archived NSMETA 1.46 with Object ABI 3 remains compatible`.

Las dos abren `tests/fixtures/nsmeta/historical-1.46.nsmeta`, que no existe en el repositorio ni en `A:`
([investigación](t08-d24-fixture-historico-146.md)).

- La prueba se conserva intacta, no se agrega compatibilidad entre versiones y no se reconstruye un compilador
  antiguo.
- **La suite no cuenta como aprobada.** Las otras 442 comprobaciones pasan, pero no cubren lo que miden esas dos:
  la lectura de una biblioteca escrita por el productor 1.46.
- **Los demás resultados se interpretan con esta limitación.** Las pruebas de metadata de la tanda 8 (D24, D25,
  D26, `stage50_9`, `library-metadata-version-tolerance`) usan el contrato vigente. Ninguna sustituye la muestra
  1.46 que falta.

### ctest completo (696 pruebas)

Se ejecutó para separar lo aprobado, lo fallido y lo no ejecutado de toda la suite configurada en `tanda1-ON`, no
solo del conjunto acordado. Log: `claude\logs\ctest-completo.log`.

| Resultado | Pruebas |
|---|---|
| Pasan | 131 |
| No se ejecutaron (el ejecutable de prueba no está construido en `tanda1-ON`) | 457 |
| Fallan | 108 |

**Las 457 no ejecutadas** son, por familia: `ns3` 131, `cc1` 72, `language` 26, `project` 15, `stage45` 14,
`namespace` 12, `stage50` 10, `compilation` 10, y el resto en grupos más pequeños. No se cuentan ni como aprobadas
ni como fallidas.

**Clasificación de las 108 que fallan:**

| Causa | Pruebas | Con el compilador de `97911ad25` (antes de la tanda 8) |
|---|---|---|
| El compilador está fuera del árbol del repositorio y no encuentra `lib/NativeSharp.Core/NativeSharp.Core.nsproj` (`integration.*`, `dump-*`, `verify-*`, `emit-exe*`) | 42 | falla igual (`dump-tokens` y `integration.full` comprobados) |
| `selfhost.*`: faltan requisitos que esta construcción no genera ni configura (`NativeSharpCore.lib` precompilado en 23, imágenes e import libraries de `selfhost-sh41` en 12, `NS_ROOT`, `nsc0` y raíces en 13) | 48 | falla igual (`selfhost.frontend` comprobado) |
| `stage30`, `stage34`, `stage35-recovery` (2), `stage36-recovery` y `stage37` a `stage44`: esperan una salida de driver anterior | 12 | fallan igual las 12 |
| `debug.codeview-pdb`, `debug.extreme-o0`, `ns3.t36-debug-scopes`, `ns3.t37-debug-values` | 4 | fallan igual las 4, en la misma línea del script |
| `parser`: espera que `true ? 1 : 0` se rechace | 1 | el compilador de `97911ad25` ya acepta el ternario; expectativa anterior a la tanda |
| `stage48_2`: fixture 1.46 ausente | 1 | pendiente, ver arriba |

**Conclusión.** Ninguno de los 108 fallos es atribuible a la tanda 8:

- las 17 que no dependen del entorno (12 de stages, 4 de depuración y `parser`) fallan igual antes de la tanda,
  comprobado con el compilador construido en `97911ad25` (`claude\bisect`, script `claude\comparar-base.py`);
- las 90 de entorno nombran un archivo, producto o variable que esta construcción no tiene. Se comprobaron muestras
  de cada causa con el compilador de `97911ad25`, con el mismo mensaje;
- `stage48_2` es el fixture ausente.

No se corrigió ninguno porque ninguno es un fallo funcional de la tanda 8.

## D21: revisión de la propuesta `propuesta/t08-agrupado-2`

### Comprobaciones con Git

Git funciona en este equipo (Windows), a diferencia de la sesión de la revisión de Codex. Script independiente:
`claude\d21-verificar-2.py`. No usa el plan de `d21-agrupar-2.py`: recorre las dos historias y compara.

| Comprobación | Resultado |
|---|---|
| Las dos historias son lineales desde `97911ad25` | sí: 2406 originales, 90 propuestos |
| Cada commit propuesto tiene el árbol exacto de un original, en el mismo orden | sí, en las 90 fronteras |
| Los originales de cada grupo son exactamente los que lista su mensaje | sí, en los 23 grupos |
| Los commits uno a uno conservan el mensaje original completo | sí, en los 66 |
| Uniones | una sola, `8716c3d5b` = `ea45997d4` + `d63632954`, la acordada |
| Cada original queda cubierto una vez | sí |
| Autor y fecha de cada commit propuesto son los del último original de su rango | sí |
| Árbol final | `b7820e5b8`, idéntico en `0ead9a1f6` y en `0876836b5` |
| Control negativo: el mismo script contra `0ead9a1f6~1` | rechaza (código 1), así que el script detecta una frontera que no coincide |

Como cada frontera reproduce un árbol original, los cambios y la documentación de cada tramo son los del historial
original. Todo documento de `registro-commits`, tracking y prueba está en el árbol final.

### Respaldo

`claude\respaldo\development-cpp-clang-20261110-d26.bundle` (959 630 605 bytes):

- `git bundle verify`: correcto. Contiene `development-cpp-clang` (`0ead9a1f6`), `propuesta/t08-agrupado`
  (`fa4104cb4`) y `propuesta/t08-agrupado-2` (`0876836b5`), con la historia completa;
- **restauración:** `git clone --mirror` del bundle en `A:\nsc-interop-build\temp\d21-restauracion-20261110.git`
  y luego `git fsck --full --strict` sin errores. En la copia restaurada los tres refs, sus árboles y los recuentos
  (2406 y 90) coinciden con el repositorio, y `d21-verificar-2.py` vuelve a dar la verificación correcta;
- se conservan los cuatro bundles anteriores.

Esa carpeta de restauración ocupa espacio en `A:`; no se borró porque eliminar archivos requiere autorización.

### Lo que no se comprobó

- **Compilación de los estados intermedios.** Las fronteras son commits originales, pero no se construyó ni se
  probó cada uno.
- **El remoto en vivo.** Se usó el ref de seguimiento local `github/development-cpp-clang`.

### Cómo quedaron agrupados los temas

**Parte 1: trabajo fino del 5 al 8 de noviembre.** 23 grupos en lugar de 2337 commits. Son rangos contiguos y
cronológicos, y ningún commit se reordena.

| Tema | Grupos |
|---|---|
| Inventario de tipos C, `const` y `volatile` en arreglos, records, punteros y agregados; USR y records empaquetados | 1 a 5 |
| Identidad y metadata C: USR de campos y globales, catálogo de snapshots, procedencia, enum, callbacks, formatos flotantes | 6 a 9 |
| Records C: campos de bits, uniones, prototipos, globales `const`, opacos, typedef | 10 a 12 |
| TLS C en DLL y estático, LLD, emisión, bitcode; TLS atómico | 13 y 14 |
| ABI de `bool`, globales atómicas y calificadas, globales estáticas e internas | 15 a 18 |
| Plantillas con dependencias C (H3), records anónimos, miembros flexibles, ThinLTO, modo `volatile` del consumidor | 19 a 21 |
| Detalle de T08-002: identidad entre bibliotecas, DLL homónimas, rutas Unicode, variádicas, typedef de record | 22 y 23 |

**Parte 2: 67 commits que ya eran de bloque o de decisión.** Conservan su mensaje.

| Tema | Commits |
|---|---|
| Bloques T08-002 a T08-020 con su validación | 30 |
| T08-GATE: validaciones integrales | 4 (una es la unión acordada) |
| Decisiones T08-D01 a T08-D26, con D04b, D05b, D11/D12, D13b y T08-015-11 | 27 |
| Cierres: fixtures de cierre, cierre técnico, cierre de D25 y de D26 | 4 |
| `item341`/`item347` | 1 |

**Sobre los grupos grandes** (observación de la revisión de Codex):

- en código, cada grupo toca entre 6 y 57 archivos de `src/` e `include/`;
- los que más archivos tocan son el 13 y del 18 al 22, por las pruebas y los documentos (el 18 llega a 651
  archivos, 462 de ellos de prueba);
- como son cronológicos, una misma zona (LLVM/Lowering, Interop/Cpp, Frontend/HIR) aparece en varios grupos;
- revertir un tema concreto de la parte 1 obliga a revertir el grupo entero que lo contiene. En la parte 2 cada
  bloque o decisión se revierte por separado.

### Estado

- La propuesta es fiel al historial original: mismas fronteras, mismo contenido final y respaldo restaurable.
- **Decisión del usuario:** conservarla como rama separada o sustituir el historial publicado. La recomendación es
  conservar ambas ramas mientras se revisa.
- La propuesta cubre hasta `0ead9a1f6`. El commit de este documento sería un commit más, uno a uno, si se regenera.
