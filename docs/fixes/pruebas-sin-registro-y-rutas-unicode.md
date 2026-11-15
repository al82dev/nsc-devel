# Pruebas sin registro, JSON autoalojado y rutas Unicode

## Pruebas que llegaron sin registro

Los commits de integración del 2026-11-13 añadieron scripts de prueba sin su `add_test`, así que
ninguna ejecución de ctest los corría. Quedan registrados:

| Prueba | Script | Commit | Ficha |
| --- | --- | --- | --- |
| `nsc.selfhost.file-directory-exists` | `selfhost_file_directory_exists.py` | `e15697d04` | LKF-214 |
| `nsc.selfhost.cast-lookahead` | `selfhost_cast_lookahead.py` | `ea222ed8f` | LKF-223 |
| `nsc.selfhost.raw-literals` | `selfhost_raw_literals.py` | `8bbf3ff8a` | LKF-224 |
| `nsc.selfhost.raw-comptime` | `selfhost_raw_comptime.py` | `5e1d76a21` | LKF-236 |
| `nsc.selfhost.comptime-line-mapping` | `selfhost_comptime_line_mapping.py` | `cb7ce31fe` | LKF-237 |
| `nsc.selfhost.source-graph-overlap` | `selfhost_source_graph_overlap.py` | `79d6ebf60` | LKF-230 |
| `nsc.selfhost.module-set-problems` | `selfhost_module_set_problems.py` | `46fe31d6b` | LKF-232 |
| `nsc.selfhost.unresolved-identifiers` | `selfhost_unresolved_identifiers.py` | `077e481e2` | LKF-233 |
| `nsc.sdk.thinlto-project-warning` | `thinlto_project_warning.py` | `c03de37d7` | LKF-245 |
| `nsc.sdk.package-restore-backup-cleanup` | `package_restore_backup_cleanup.py` | `f5366afdd` | LKF-264 |
| `nsc.sdk.package-restore-failed-rollback` | `package_restore_failed_rollback.py` | `f5366afdd` | LKF-264 |
| `nsc.sdk.package-restore-changed-source` | `package_restore_changed_source.py` | `c58229e92` | LKF-265 |
| `nsc.sdk.content-failed-rollback` | `content_failed_rollback.py` | `813f39c77` | LKF-267 |
| `nsc.sdk.package-mixed-runtime` | `package_mixed_runtime.py` | `071c90ed8` | LKF-135 |

Los autoalojados pasan por `cmake/RunSelfhostCompilerTest.cmake`, que construye `selfhost/nsc` con el
compilador bajo prueba y entrega ese `nsc-next.exe` al script; los demás por
`cmake/RunPythonTestInFreshDirectory.cmake`, que borra el `--work-dir` de la ejecución anterior porque
los scripts lo crean con `exist_ok=False`. Los catorce pasan.

Ya estaban registradas y pasan: `nsc.project-commands` (LKF-266 `f4cc36bc7`/`a2ea740cd`, LKF-268
`413a5cbbd`, LKF-270 `d4b4dbe79`, LKF-271 `aece8918b`, LKF-264 `4b63d29f9`) y `nsc.selfhost.sbyte-type`
(LKF-238 `05a105dfc`). Los 28 casos `SelfHost.Diagnostics` del compiler-suite (LKF-218, `6c77b9c30`
y `36c705604`) se ejecutaron contra el fixture con su código de salida esperado; el compiler-suite es un
proyecto CMake aparte y no forma parte de ctest.

## Defectos corregidos

- **LKF-219.** `--dump-source-info` del compilador autoalojado concatenaba la ruta en el JSON sin
  escapar. Pasa ahora por `DiagnosticRenderer.JsonEscape` (hecho público para esto).
  `nsc.selfhost.source-info-json` comprueba una ruta con espacios, letras no ASCII y puntuación.
- **NSC-NEW-019.** Al escribir esa prueba, una ruta con `ñ` hacía fallar al autoalojado con 135:
  `File.GetSize` devolvía -1 y `SourceBuffer.Load` devolvía null. En el runtime, `GetSize`,
  `ReadAllBytes`, `ReadBytes`, `WriteAllBytes`, `WriteAllText` y `AppendAllText` abrían el archivo con
  `CreateFileA`, que lee la ruta UTF-8 en la página ANSI, mientras `Exists` y `ReadAllText` ya usaban
  las variantes `W`. Las seis usan ahora `CreateFileW` con la copia UTF-16 de la ruta (no cambia la
  lista de importaciones: `CreateFileW` ya estaba). `nsc.runtime.unicode-file-paths` escribe, añade,
  mide y lee en una carpeta no ASCII en O0 y O2.

## Límite que sigue

`File.Delete`, `Directory.Create`, `Directory.Delete`, `File.CopyRaw`/`MoveRaw`, el directorio actual,
las variables de entorno y `Process.Run` siguen con las variantes `A` (la conversión SH48 completa
requiere importar `DeleteFileW`, `CreateDirectoryW`, `RemoveDirectoryW`, `CopyFileW`, `MoveFileW` y las
demás en todas las listas de importación del runtime).
