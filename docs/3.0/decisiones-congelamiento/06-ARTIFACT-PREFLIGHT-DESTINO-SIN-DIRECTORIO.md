# Congelamiento 3.0 — publicar un artefacto cuyo destino no tiene directorio (`-o nombre.exe`)

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `1bde9d387`.

## Propósito y problema

Al cerrar DEC-07/DEC-08 se registró como hallazgo no autorizado que `nsc <archivo.ns> -o <exe>` no publicaba el
ejecutable. Alberto Huerta Luna ordenó corregirlo el 2026-10-26 («corrígelos»).

La reproducción con el nsc `a75737f72e706419` mostró que el modo archivo no es la causa. Lo que falla es una
salida **sin componente de directorio**, en cualquier modo:

| Comando | Resultado |
|---|---|
| `nsc A:\...\p5_plain_copy.ns -O0 -o A:\...\f1.exe` | exit 0 |
| `nsc --emit-project-oop-exe A:\...\p5_plain_copy.nsproj -o A:\...\f3.exe` | exit 0 |
| `nsc p5_plain_copy.ns -O0 -o f5.exe` | exit 1 |
| `nsc A:\...\p5_plain_copy.ns -O0 -o f6.exe` | exit 1 |
| `nsc --emit-project-oop-exe A:\...\p5_plain_copy.nsproj -o f7.exe` | exit 1 |

Los tres fallos dan «executable publication failed: artifact free-space preflight failed for 'fN.exe': io-error
(Win32 error 123)». La nota del documento 04, que decía que también fallaba con ruta absoluta, era incorrecta:
el mensaje de esa corrida muestra el destino sin directorio, así que el script que corrió todavía pasaba la
salida relativa (por qué no se actualizó no se verificó).

## SCOPE LOCK

- **Objetivo.** Que un destino sin directorio se publique en el directorio actual, como cualquier otra ruta relativa.
- **Naturaleza.** Corrección de defecto.
- **Debe quedar idéntico.** Publicación de destinos con directorio (relativos o absolutos, UNC, rutas extendidas
  de más de `MAX_PATH`), stage, rollback, verificación de espacio insuficiente y de límite de ruta.
- **Modificable.** `src/Basic/ArtifactTransaction.cpp` (una expresión de `prepareCommitEntry`) y un caso nuevo en
  `tests/compiler-suite/host/modules/BasicArtifactCommitModule.cpp` con sus filas de manifiesto, inventarios y plan.
- **Protegido.** El resto de `ArtifactTransaction`, el driver, el proyecto implícito de C-05, CMake y la configuración.
- **Prohibido.** Batería completa; cambiar casos existentes.
- **Terminación.** TB-BAS-089 falla antes del arreglo y pasa después; TB-BAS-049..088 siguen pasando; las tres
  invocaciones fallidas de arriba publican.

## Cadena causal

| Paso | Evidencia |
|---|---|
| Entrada | destino `f5.exe` (sin directorio) |
| Operación | `ArtifactTransaction::prepareCommitEntry`, `GetDiskFreeSpaceExW` |
| Causa | `entry.destination.parent_path()` es vacío; `extendedPath("")` no puede hacerlo absoluto y devuelve `\\?\` |
| Resultado | `GetDiskFreeSpaceExW(L"\\\\?\\")` falla con `ERROR_INVALID_NAME` (123) antes de instalar nada |
| Origen | la verificación existe así desde `98d9f1d1a` (2026-09-30, publicación transaccional); el refactor `d67ca50cb` sólo la movió |
| Corrección | consultar `extendedPath(entry.destination).parent_path()`: el padre del destino ya hecho absoluto |

Para un destino con directorio el resultado es el mismo que antes: `extendedPath(destino).parent_path()` y
`extendedPath(destino.parent_path())` dan la misma ruta `\\?\...` (también `\\?\UNC\...` y la raíz `\\?\A:\`). Las
demás operaciones del commit ya usaban `extendedPath(destino)`, que resuelve contra el directorio actual, y
`ensureDirectory` ya aceptaba un padre vacío.

## Prueba

Nuevo caso TB-BAS-089 (CB-BAS-013, PR) en `BasicArtifactCommitModule.cpp`: un destino de sólo nombre de archivo,
único por proceso, se publica en el directorio actual con los bytes exactos y sin siblings ni backups de la
transacción para ese nombre (`hasTransactionResidueFor`, que no mira otros archivos del directorio compartido). El
`CaseTree` del caso borra el archivo publicado. Los casos existentes no cambian. Entrada nueva en `cases.jsonl` y
filas en `suite-case-allocation.csv`, `test-case-design.csv` y `HOST-BASIC-TEST-PLAN.md`.

Contrafactual, con la prueba y sin el arreglo (Debug): TB-BAS-089 FAIL con «artifact free-space preflight failed for
'TB-BAS-089-31224-544446421.lib': io-error (Win32 error 123)», el mismo error que da nsc.

## Consumidores

`ArtifactTransaction` publica los ejecutables, bibliotecas, DLL, objetos, metadata y archivos que escribe
`writeFileTransactionally` (por ejemplo `--log=<archivo>`). Todos dejan de fallar con un destino sin directorio;
ninguno cambia con un destino con directorio.

## Validación

Compilación con el arreglo: `cmake --build A:/nsc-devel/build-clang --target nsc` (exit 0, recompiló
`ArtifactTransaction.cpp`) y `nsc_component_tests` en `build-compiler-suite-v3/debug` y `/release` (exit 0).

- TB-BAS-049..089 (todos los módulos de `ArtifactTransaction`: staging, preflight, preflight controlado, commit,
  rollback, seed y escritor transaccional) con `nsc_component_tests --case`: 82 de 82 PASS (41 casos en Debug y en
  Release), TB-BAS-089 incluido.
- `nsc p5_plain_copy.ns -O0 -o f5.exe`, `nsc A:\...\p5_plain_copy.ns -O0 -o f6.exe` y
  `nsc --emit-project-oop-exe A:\...\p5_plain_copy.nsproj -o f7.exe`: exit 0; los tres ejecutables quedan en el
  directorio actual y terminan 7, como el programa.
- Las dos invocaciones con ruta absoluta siguen publicando (exit 0).

## No verificado

- La batería completa no se ejecutó (prohibido en esta etapa).
