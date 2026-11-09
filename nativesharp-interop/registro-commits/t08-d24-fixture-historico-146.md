# T08-D24: por qué no se ejecutan las comprobaciones del fixture histórico NSMETA 1.46

Fecha: 2026-11-09. Instrucción del usuario: hay un único contrato vigente de NSMeta y no se reintroduce
compatibilidad entre versiones históricas. Las pruebas históricas no se eliminan ni se modifican por este motivo.
Hay que investigar el fallo y documentar qué impide ejecutarlas.

## Fallo

`nsc.stage48_2.library-metadata` pasa 442 de 444 comprobaciones. Las dos que fallan son de
`testIntegrityAndCompatibility` (`tests/unit/Stage48_2LibraryMetadataTests.cpp`):

- «archived NSMETA 1.46 golden remains readable»;
- «archived NSMETA 1.46 with Object ABI 3 remains compatible».

**Qué hace la prueba.** Para cada minor de 0 al vigente (49):

1. Escribe con `codec.writeModel` el modelo actual proyectado a ese minor, lo lee y lo verifica. Esa parte pasa
   también para 1.46.
2. Si el minor es anterior al vigente, lee además los bytes archivados en
   `tests/fixtures/nsmeta/historical-1.<minor>.nsmeta`.

El archivo de 1.46 no existe, así que la lectura falla. No es un fallo del lector ni del cambio de D24.

## Por qué falta el archivo

**El formato 1.46 vivió menos de una hora.** Según el historial de `kMetadataFormatMinor`
(`include/nativesharp/Interop/LibraryMetadata.h`):

| Commit | Fecha | Minor |
|---|---|---|
| `d801af165` interop-t08-h3-emision-alias-nsmeta | 2026-11-07 23:54 | introduce 1.46 |
| `311431667` interop-t08-h3-nsmeta-147-enumeradores | 2026-11-08 00:51 | introduce 1.47 |

**Las muestras vecinas sí se recuperaron** (commits `4bfdfecfa`, `2d4761656` y `b3dd18578`):

- 1.45, 1.47 y 1.48, a partir de salidas que escribió el compilador de su época;
- la de 1.48 es el `current-format.nsmeta` de la suite.

La de 1.46 nunca se archivó.

**Búsqueda en `A:\nsc-interop-build`.** Se exploraron 9617 archivos `.nsmeta`
(`claude\buscar-nsmeta-146.py` y `claude\fechas-146.py`, solo lectura). Hay tres con minor 46, todos llamados
`historical-1.46.nsmeta` dentro de `nativesharp_stage48_2_library_metadata_tests\Integrity`:

| Carpeta | Fecha | SHA-256 (prefijo) | `current-format` de esa ejecución |
|---|---|---|---|
| `tanda08-meta-compat\temp` | 2026-11-08 01:05 | `f3ed5f4018fc838f` | 1.47 |
| `tanda08-nsmeta-temp` | 2026-11-08 01:58 | `0ed464baae4e69a6` | 1.48 |
| `temp` | 2026-11-09 06:10 | `b55e6485710e2b1e` | 1.49 |

Los tres son proyecciones que la propia prueba escribió en el paso 1, con un escritor cuyo formato vigente ya era
1.47 o posterior. Ninguno es de la ventana de 1.46, y por eso ninguno es una muestra auténtica de un escritor 1.46.

## Qué impide ejecutar las dos comprobaciones

1. **No existe una muestra auténtica.** Ni en el repositorio ni en `A:`.
2. **El mecanismo de la prueba no sirve.** `NSC_P2_REFRESH_GOLDENS=1` copiaría como archivo la proyección del
   escritor actual. Sería circular: la prueba leería lo que acaba de escribir el mismo códec, y la comprobación
   dejaría de ser evidencia independiente. No se hizo.
3. **Una muestra auténtica exige el compilador de la época.** Habría que construir el escritor en `d801af165` (en
   un worktree y un directorio de build aparte, sin tocar el árbol actual) y ejecutar su suite para capturar su
   `current-format.nsmeta`. Eso solo añadiría un archivo de prueba; no reintroduce compatibilidad en el código.
4. **Choca con el contrato único.** La prueba exige que el lector actual lea los formatos 1.0 a 1.48, y eso es
   precisamente compatibilidad entre versiones. La prueba es histórica y se conserva sin cambios, pero producir más
   evidencia para ella va en contra del contrato vigente (`docs/NSMETA-CONTRATO-ACTUAL.md`).

## Opciones para el usuario

| Opción | Efecto |
|---|---|
| **A. Dejarlo así (vigente)** | las dos comprobaciones siguen fallando con esta causa documentada; ni la prueba ni el contrato cambian |
| **B. Construir el escritor en `d801af165` y archivar su muestra** | las 444 comprobaciones pasan; solo se añade un fixture; requiere un build histórico completo aparte |
| **C. Usar la proyección actual** | no se recomienda: no es evidencia independiente |

Mientras no se decida, rige la opción A. T08-D24 queda cerrada por el cambio del productor; este fixture es un
pendiente aparte, que el tracking registra junto a T08-D24.
