# Tanda operativa 33 — precompiled-only, provenance y coherencia de paquetes

## Resultado

El comando de proyecto puede demostrar de dónde salió cada dependencia y puede operar en modo
cerrado sobre una closure ya restaurada. `--precompiled-only` impide que una dependencia vuelva a
fuente; `--no-restore` no consulta el origen del paquete ni modifica lock/store; y
`--dependency-report` publica una lista ordenada y fingerprinted tanto en texto como en JSON.

El cierre se validó con las fuentes reales de `NativeSharp.Core` y `NativeSharp.Core.IO`, no con
una API ficticia. Ambas se producen como `.lib + .nsmeta`; Core.IO consume Core precompilado; el
paquete conjunto se restaura, se elimina su directorio fuente y un ejecutable consumidor se
construye y ejecuta usando exclusivamente los artefactos verificados del store.

## Contrato de comandos

### `--precompiled-only`

- La raíz solicitada sigue siendo el proyecto que se está construyendo.
- Toda dependencia de proyecto debe llegar por `LibraryReference`/NSMETA o store verificado.
- Si la closure contiene un `ProjectReference`, el comando falla antes de invocar al compilador y
  nombra módulo, ruta y la acción requerida: publicar `.lib + .nsmeta`.
- Implica la disciplina de no restore: no se permite resolver o recompilar una dependencia para
  “arreglar” silenciosamente la closure.

### `--no-restore`

- Lee el lock sólo como autoridad inmutable y verifica tamaño + SHA-256 de cada artefacto ya
  materializado.
- No abre la fuente configurada del paquete y no crea ni reemplaza lock/store.
- Si falta lock o artefacto, el diagnóstico empieza por el switch responsable y conserva la causa
  física concreta.

### `--dependency-report`

Cada registro contiene:

- `kind`: `source`, `project-reference`, `library-reference` o `store-cache`;
- identidad estable de módulo o `package@version`;
- ruta normalizada;
- SHA-256 de su conjunto ordenado de entradas.

La salida se ordena por kind/identidad/ruta. En JSON se publica como
`dependencyProvenance`; en texto aparece bajo `Dependency provenance:`. El nombre temporal del
proyecto generado no participa del fingerprint de la raíz, por lo que dos builds iguales producen
el mismo reporte byte por byte.

## Coherencia de closure

El graph existente ya rechaza nombres de módulo duplicados entre fuente y metadata y asigna un
owner único a cada módulo precompilado. T33 conecta ese veredicto con el modo de proyecto cerrado:
la selección source/precompiled ocurre antes de compilar y no puede caer silenciosamente a fuente.
La suite de graph conserva el rechazo determinista para la mezcla del mismo módulo/API.

## Correcciones necesarias encontradas por el caso real

- `NativeSharp.Core.nsproj` ahora declara `OutputType=StaticLibrary`; su contrato de distribución
  ya no depende de un override externo que el archivo de proyecto no expresaba.
- El análisis de lifetime de vistas ya distingue el slot local de un raw pointer de la memoria
  apuntada. Sigue rechazando VLA, `stackalloc`, `&local` y aliases que propagan una raíz de frame,
  pero permite devolver una vista construida desde un puntero que un allocator/arena obtuvo del
  heap. Esto elimina el falso positivo real de `ArenaAllocator.AllocateSpan` sin abrir el escape
  de memoria automática.

## Archivos modificados

- `src/Driver/ProjectCommands.cpp`: switches, carga previa del graph, política cerrada, diagnóstico
  causal, fingerprints y reportes texto/JSON.
- `src/Driver/Driver.cpp`: ayuda pública de los nuevos switches.
- `src/Frontend/ProjectMethodBodyModel.cpp`: provenance de raw pointer basada en su origen real.
- `lib/NativeSharp.Core/NativeSharp.Core.nsproj`: salida StaticLibrary declarada.
- `tests/unit/ProjectCommandsTests.cpp`: no-mutación, reproducibilidad, texto/JSON, rechazo de
  source dependency y ausencia causal de lock.
- `CMakeLists.txt`: target vertical T33.

## Archivo nuevo

- `tests/unit/NativeSharp3T33PackageCoherenceTests.cpp`: produce Core/Core.IO reales, crea el
  paquete versionado, restaura la closure, elimina el origen, construye con los dos modos cerrados
  y ejecuta el consumidor. Es independiente porque valida el límite completo entre compiler,
  NSMETA, package store y linker sin convertirlo en baseline.

## Validación dirigida

- `nsc` Debug/O0: correcto.
- `nsc_project_commands_tests`: 24/24.
- `nsc_project_graph_tests`: 25/25.
- `nsc_ns3_t33_package_coherence_tests`: 6/6.
- Producción real O0: `NativeSharpCore.lib/.nsmeta` y
  `NativeSharpCoreIO.lib/.nsmeta`.
- Consumidor final: build `--precompiled-only --no-restore`, reporte JSON y ejecución exit 0.
- No se ejecutó baseline ni CTest global.

## Mejora obtenida

Un build reproducible ya no necesita confiar en que “probablemente” usó el binario correcto. Puede
probar qué closure consumió, impedir fallback a fuente y repetir la selección sin acceso al origen
del paquete. Esto convierte package restore y LibraryReference en una frontera verificable apta
para una distribución 3.0 congelada.
