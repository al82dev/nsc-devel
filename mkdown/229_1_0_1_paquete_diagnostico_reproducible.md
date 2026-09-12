# Entrega 229 — Paquete diagnóstico reproducible / Reproducible diagnostic bundle

## Español

`nsc --write-diagnostic-bundle <proyecto.nsproj> <informe.nsdiag>` ejecuta la verificación completa
del OOP LLVM a O0 y conserva en un documento `NSDIAG 1` la versión y build exactos del compilador,
versión del lenguaje, etapa, target, resultado, stdout y stderr.

El inventario incluye cada `.nsproj`, fuente y artefacto de `LibraryReference` alcanzable. Se ordena
por un rol canónico y registra tamaño y SHA-256, no rutas absolutas ni copias del código. Dos árboles
idénticos colocados en directorios distintos producen la misma identidad de entradas; un solo byte
distinto queda localizado. El informe se escribe incluso cuando la compilación falla, siempre que
el grafo del proyecto pueda cargarse, que es precisamente el caso útil para reportar un defecto.

Se creó `DiagnosticBundle.h/.cpp`, se conectó el comando en `Driver`, se documentó en `--help` y se
añadió una prueba que exige el encabezado versionado, la acción O0, proyectos/fuentes con hash y la
ausencia de la raíz absoluta. No se ejecuta ahora: forma parte del baseline único tras la 234.

## English

`nsc --write-diagnostic-bundle <project.nsproj> <report.nsdiag>` runs full O0 OOP LLVM verification
and records the exact compiler version/build, language version, stage, target, result, stdout, and
stderr in an `NSDIAG 1` document.

The inventory covers every reachable project, source, and `LibraryReference` artifact. Entries are
sorted by canonical role and carry size plus SHA-256 rather than absolute roots or source copies.
Identical trees at different locations therefore have identical input identities, while a one-byte
change remains attributable. A report is still written for compilation failures whenever the
project graph itself is loadable.

`DiagnosticBundle.h/.cpp`, the Driver command/help entry, and a deterministic Driver test were
added. The test is queued for the single baseline after delivery 234.
