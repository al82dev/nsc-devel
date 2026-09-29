# Tanda 18: sesión de proyecto e índices del frontend

## Correspondencia con el plan NativeSharp 3.0

Este commit cierra la **tanda operativa 02** del documento maestro: P03/P04 sobre startup, carga de
proyecto y rescaneos del frontend. El objetivo no es declarar terminada toda optimización posible
del frontend, sino demostrar y congelar que los caminos auditados consumen cada entrada una sola
vez o mediante índices explícitos.

## Objetivo

- Evitar la segunda lectura y segundo parse del `.nsproj` raíz cuando un build consume defaults
  de proyecto y luego construye el grafo.
- Separar el costo `project-graph` dentro del trace.
- Eliminar productos cartesianos encontrados entre metadata precompilada y modelos locales.
- Revalidar los índices de miembros, fast path de overloads y cola incremental de genéricos con
  tests individuales.

## Hallazgos de la auditoría

1. Los builds OBJ/EXE/LIB/DLL llamaban `loadBuildSettings()` y después
   `ProjectGraphLoader::load()`. Ambos caminos abrían y parseaban el mismo `.nsproj` raíz.
2. `bindPrecompiledPhysicalNames()` recorría todas las identidades NSMETA para cada callable
   precompilado: costo `O(callables × identities)`.
3. La construcción de aliases de especializaciones buscaba linealmente cada especialización
   importada dentro de todas las especializaciones locales: `O(imported × local)`.
4. La clasificación de especializaciones de método preguntaba por nombre mediante un recorrido
   completo de `metadata.members` por cada especialización: `O(specializations × members)`.
5. `typeTransitivelyOwnsStorage()` reconstruía su memo desde cero en cada consulta, aun cuando el
   modelo de miembros ya era inmutable.

## Cambios realizados

### Caché de descriptor por sesión

- `ProjectLoader` posee ahora un caché compartido por sus copias y limitado a la vida de esa
  instancia/sesión.
- El caché guarda el descriptor XML ya validado por ruta canónica.
- `loadBuildSettings()` y la carga posterior del proyecto raíz reutilizan el mismo descriptor.
- Las cargas paralelas de dependencias comparten el caché con mutex; no existe caché global ni
  persistencia entre invocaciones de NSC.
- `ProjectLoaderStatistics` reporta lecturas físicas y hits del descriptor.
- El driver mantiene un `ProjectLoader` por ejecución y lo comparte con `ProjectGraphLoader`, los
  comandos de proyecto y el frontend precompilado.
- El trace JSON añade `project_descriptor_file_reads` y `project_descriptor_cache_hits`.
- La carga del grafo tiene fase propia `project-graph`, en lugar de quedar absorbida dentro de
  `precompiled-metadata`.

### Índices del frontend

- `PrecompiledMemberIdentityIndex` construye una tabla hash una sola vez sobre identidades con
  nombre físico publicable.
- Cada callable hace una sola consulta promedio `O(1)` y conserva los tres resultados semánticos:
  missing, unique y ambiguous.
- Las especializaciones locales se indexan una vez por `specializationKey` antes de procesar las
  importadas.
- Los nombres concretos de miembros NSMETA se indexan una vez antes de clasificar
  especializaciones de método.
- El resultado de ownership transitivo por tipo se memoiza durante toda la vida inmutable de
  `ProjectMemberModel`.

### Contratos ya existentes revalidados

- El índice persistente `type → name → members` conserva orden físico y promociones.
- El fast path de overloads posicionales conserva resolución y diagnósticos.
- La cola semántica consume call/delegate demands con cursores, sin reescanear las colecciones
  completas al descubrir una especialización genérica.

## Archivos nuevos

- `include/nativesharp/Frontend/PrecompiledMemberIdentityIndex.h`: contrato explícito del índice y
  sus resultados missing/unique/ambiguous.
- `src/Frontend/PrecompiledMemberIdentityIndex.cpp`: tabla hash integrada en el binder real.
- `tests/unit/PrecompiledMemberIdentityIndexTests.cpp`: fixture sintético sin linker ni instalación.
- `tests/unit/ProjectLoadSessionTests.cpp`: grafo de dos proyectos que prueba lecturas/hits por
  sesión.
- `docs/compiler-optimization/TANDA-18-project-session-and-frontend-indexes.md`: este expediente.

## Archivos modificados

- `CMakeLists.txt`
- `include/nativesharp/Compilation/ProjectLoader.h`
- `include/nativesharp/Frontend/ProjectMemberModel.h`
- `src/Compilation/ProjectLoader.cpp`
- `src/Driver/CompilationTelemetry.cpp`
- `src/Driver/Driver.cpp`
- `src/Frontend/ProjectCompilation.cpp`
- `src/Frontend/ProjectMemberModel.cpp`
- `tests/unit/CompilationTelemetryTests.cpp`

## Mejora obtenida

- En el fixture real con defaults dentro de `.nsproj`, la ruta completa OBJ reportó exactamente
  `project_descriptor_file_reads=1`, `project_descriptor_cache_hits=1` y una sola fase
  `project-graph`.
- El binder físico pasa de `O(C×I)` a `O(I+C)` promedio, donde `C` son callables importados e `I`
  identidades NSMETA.
- Aliases genéricos pasan de `O(P×L)` a `O(P+L)` promedio.
- La detección de declaración concreta pasa de `O(S×M)` a `O(S+M)` promedio.
- Las consultas repetidas de ownership reutilizan el resultado por tipo en vez de reconstruir un
  vector y recorrer el grafo de fields otra vez.
- La caché desactivada/no usada cuesta sólo un `shared_ptr`; no hace I/O ni mantiene estado entre
  compilaciones.

## Verificación dirigida

- Build `nsc` Debug/O0 en `A:\nsc-devel\build-clang`: correcto.
- `nsc.project-load-session`: correcto; grafo de dos proyectos, dos lecturas físicas totales y un
  hit del root solicitado antes por settings.
- `nsc.precompiled-member-identity-index`: correcto; exacto, missing, ambiguous, identidad vacía y
  contadores de indexación/lookup.
- `nsc.project-member-model`: correcto.
- `nsc.project-method-body-model`: correcto.
- `nsc.stage59.closed-generic-member`: correcto.
- `nsc.compilation-telemetry`: correcto con los nuevos contadores del trace.
- Selección final: **6/6 tests correctos en 0.73 s**.
- Fixture CLI OBJ O0 con `OptimizationLevel`, `CompilerJobs` y `OutputPath` sólo en `.nsproj`:
  correcto; no se pasaron `-O`, `-j` ni `-o`.

## Incidente de prueba y limpieza

`nsc.language.cross-library-inlining` se intentó como test de identidad, pero este build de tests
no tiene configurado el bibliotecario instalado que el escenario necesita. Falló antes del
consumer y, por su helper heredado, conservó evidencia en `%TEMP%` de `C:`. Esa carpeta se eliminó
inmediatamente. El escenario no se contabiliza como aprobado ni como fallo de esta implementación;
se reemplazó por el test sintético del índice, que trabaja únicamente en `A:` y prueba directamente
el contrato modificado.

Además se corrigió `CompilationTelemetryTests`: cerraba el NSDIAG después, no antes, del intento de
limpieza. El stream ahora se cierra primero y no queda el fixture `tanda01-telemetry-fixture`.

## Límites

- No se ejecutó baseline.
- No se ejecutaron matrices O1/O2/O3.
- No se instaló nada.
- No se conservó evidencia ni código fuente en `C:`.
- Las mejoras algorítmicas están demostradas por contadores y contratos pequeños; las metas de
  wall time de Math.Core y del corpus completo se medirán sólo en los checkpoints autorizados.
