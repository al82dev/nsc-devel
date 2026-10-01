# Tanda operativa 34 — rutas largas y publicación transaccional Windows

Fecha de cierre: 2026-09-30  
Rama: `development`  
Plataforma contractual: Windows x64  
Configuración de validación: Debug/O0

## Problema cerrado

La salida del compilador estaba fragmentada entre varios escritores. Object, PE/PDB, archive,
header, NSMETA, archivos de proyecto y diagnostic bundles podían escribir directamente sobre el
destino o eliminar primero la versión anterior. Además de exponer artefactos parciales, eso hacía
que una ruta Windows larga pudiera terminar reportada como un genérico `path not found` y obligaba
a LLVM/lld a procesar el nombre final de más de 260 caracteres.

## Implementación

- `ArtifactTransaction` asigna rutas de staging cortas para LLVM y lld.
- El commit usa rutas Win32 wide con prefijo extendido, copia y vacía los bytes junto al destino,
  preserva cada versión anterior, instala el conjunto y elimina los backups sólo al terminar.
- Cualquier fallo durante replace restaura en orden inverso todos los artefactos ya sustituidos.
- El preflight crea los directorios finales con rutas extendidas, comprueba el límite real de
  32,767 unidades UTF-16 y el espacio disponible antes de tocar una versión publicada.
- Los errores se clasifican como `path-limit`, `missing-component`, `permission-denied`,
  `file-locked`, `insufficient-space` o `io-error`, conservando también el código Win32.
- Object, executable/PDB/incremental state/runtime closure, static library/header/NSMETA y
  DLL/import library/PDB/header/NSMETA/runtime closure se publican como conjuntos.
- Los escritores de `.nsproj`, sidecar/lock, C header, NSMETA y `.nsdiag` reutilizan el mismo
  publicador en lugar de borrar el destino antes de renombrar.

## Archivos modificados

- `CMakeLists.txt`: incorpora el publicador a `nsc_basic` y registra la prueba dirigida T34.
- `include/nativesharp/Basic/ArtifactTransaction.h`: contrato común, clasificación causal y
  transacción de conjuntos.
- `src/Basic/ArtifactTransaction.cpp`: preflight, Win32 extended paths, flush, replace y rollback.
- `src/Driver/Driver.cpp`: staging corto y commit por conjunto para todos los artefactos de build.
- `src/Driver/ProjectCommands.cpp`: escritura transaccional de proyecto, sidecar y lock.
- `src/Driver/DiagnosticBundle.cpp`: publicación long-path-safe de diagnostic bundles.
- `src/Interop/CHeaderGenerator.cpp`: publicación transaccional del header C.
- `src/Interop/LibraryMetadata.cpp`: publicación transaccional de NSMETA.
- `tests/unit/NativeSharp3T34LongPathPublicationTests.cpp`: vertical Windows de más de 260
  caracteres y rollback provocado por un archivo bloqueado.

## Archivos nuevos y motivo

- `ArtifactTransaction.h/.cpp`: una sola autoridad evita que cada formato implemente un replace
  distinto y permite tratar una salida multiarchivo como una unidad recuperable.
- `NativeSharp3T34LongPathPublicationTests.cpp`: conserva la reproducción productiva; no es una
  prueba textual ni una simulación del driver.

## Mejora observable

LLVM y lld ya no reciben el nombre final largo. Una compilación completa puede publicar `.obj`,
`.exe`, `.pdb`, `.lib`, `.h`, `.nsmeta` y `.nsdiag` debajo de un directorio que supera MAX_PATH.
Si un archivo del conjunto está ocupado, el compilador informa `file-locked`, revierte lo ya
instalado y conserva byte por byte las versiones anteriores; no quedan `.new`, `.old` ni stages.

## Validación dirigida

- Build `nsc` Debug/O0: correcto.
- `nsc_ns3_t34_long_path_publication_tests`: 7/7.
- `nsc_project_commands_tests`: 24/24.
- `nsc_ns3_t33_package_coherence_tests`: 6/6.
- El fixture T34 valida físicamente object, executable, PDB, library, header, NSMETA y diagnostic
  bundle en una ruta final mayor a 260 caracteres.
- No se ejecutó baseline.
