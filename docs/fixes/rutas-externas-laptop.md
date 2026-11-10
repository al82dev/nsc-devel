# 102-outside: investigación en laptop

Estado: corrección implementada y pruebas dirigidas principales aprobadas; límites de validación abiertos abajo.

## Evidencia

- Base de la investigación: `5a4bf6e56ccdac3e75df25ad5f1a39df6f5aa47a`, rama `development-fixes-limitaciones-laptop`.
- El registro histórico `docs/fixes/decisiones-pendientes.md` cita `cases/102-outside/build.log`, que no está disponible en esta laptop; la siguiente reproducción es nueva, no pretende sustituir ese archivo histórico.
- Proyecto aislado `A:/temp/nsc-fixes-102-baseline/Project/Probe.nsproj`: `Compile Include="../Shared/*.ns"`; fuente `A:/temp/nsc-fixes-102-baseline/Shared/Main.ns`.
- El compilador aislado `A:/temp/nsc-fixes-laptop-build/bin/nsc.exe --check-project-method-bodies A:/temp/nsc-fixes-102-baseline/Project/Probe.nsproj` terminó con código 1 y `invalid glob pattern '../Shared/*.ns': glob pattern cannot contain '.' or '..' segments`.
- `GlobPattern::validate` rechaza los segmentos; `collectSourceCandidates` solo enumera bajo el proyecto; `ProjectSourceSet::add` rechaza fuentes cuya ruta canónica sale de él. Cada uno es un obstáculo independiente que debe tratarse sin desproteger la inclusión implícita ni cambiar silenciosamente otros consumidores.
- `tests/unit/ProjectLoaderTests.cpp` espera que `../Shared/*.ns` sea inválido. `tests/unit/CompilationModelTests.cpp` espera que `ProjectSourceSet::add` rechace fuentes externas. El cambio debe preservar este último contrato para los consumidores que no soliciten explícitamente fuentes externas.
- Consumidores de `ProjectSource::projectRelativePath`: orden y duplicados de fuentes, `ProjectGraphLoader`, `ProjectDumper` y `DiagnosticBundle`. Debe verificarse la representación de rutas externas en esos recorridos.
- La comparación local con `origin/development-fixes-limitaciones` desde `23508249e` identificó cambios del colega en `src/Compilation/PrecompiledProjectPreparation.cpp` y `src/Compilation/PrecompiledSurfaceReachability.cpp`; no se propone modificarlos. Este cotejo usa la referencia remota local y no demuestra el estado actual del servidor.

## SCOPE LOCK preliminar

- Objetivo: permitir las inclusiones explícitas de archivos y globs relativos con `..` conforme al encargo, con límites y diagnósticos verificables.
- Cambio correctivo; conservar descubrimiento predeterminado limitado al proyecto, exclusiones, `Remove`, deduplicación, orden estable, extensiones, rutas absolutas rechazadas y demás reglas existentes, salvo el contrato expresamente cambiado para patrones relativos con `..`.
- Archivos candidatos: `src/Compilation/GlobPattern.cpp`, `src/Compilation/ProjectLoader.cpp`, `src/Compilation/ProjectSourceSet.cpp`, `include/nativesharp/Compilation/ProjectSourceSet.h`, pruebas dirigidas de estos componentes y este registro. También se identificó la expectativa del contrato en `tests/compiler-suite/host/modules/CompilationCore001To112Module.cpp`, que se actualizó sin modificar el manifiesto de la batería. No se cambió `ProjectSourceSet.cpp`.
- Protegidos: los archivos y áreas de punteros compartidos, préstamos, seguridad y consumo de SDK binario asignados a la otra instancia; instalación funcional, repositorios ajenos y archivos compartidos no necesarios.
- No merge, rebase sobre `development-cpp-clang`, push ni deploy; no modificar C/C++, Daikon, fuentes ni builds de LLVM/Clang o interoperabilidad. Solo compilaciones incrementales y pruebas dirigidas con temporales aislados en A:.
- Cierre: contrato y límites elegidos y documentados; reproducción resuelta; pruebas reales de archivos y globs, inclusión/exclusión, duplicados, rutas inválidas y límites; comprobación de consumidores y regresiones dirigidas; commit independiente con evidencia y notas. Aún no cumplido.

## Siguiente acción

Diseñar la enumeración explícita acotada y la autorización de fuentes externas dentro del cargador sin relajar `ProjectSourceSet::add` para otros consumidores; precisar conducta de segmentos `..` posteriores a comodines, enlaces, raíces inexistentes y errores antes de implementar.

## Decisión para el primer diferencial

- La ruta se interpreta respecto del directorio del proyecto. Se normalizan componentes literales `.` y `..` antes del primer comodín; `..` después de un comodín se rechaza con diagnóstico, pues no identifica una raíz de enumeración inequívoca. Las rutas absolutas siguen rechazadas.
- El patrón explícito define la raíz de búsqueda mediante su prefijo literal; la búsqueda externa no se activa para el glob predeterminado. Las exclusiones y eliminaciones se evalúan con la misma ruta relativa al proyecto. Las raíces inexistentes no generan coincidencias, coherentemente con la selección existente de globs sin coincidencia.
- Se conserva `ProjectSourceSet::add` y su rechazo de rutas externas. El cargador solo puede introducir una fuente externa si fue seleccionada por una inclusión explícita externa y verificó archivo regular, extensión, ruta canónica y duplicados; otras fuentes siguen pasando por `add`.
- Los enlaces no deben convertir la inclusión normal en una vía para incorporar silenciosamente una fuente externa. Una fuente externa seleccionada por nombre o glob se valida contra su raíz de búsqueda canónica; una resolución fuera de ella se rechaza. Este límite se probará donde el sistema permita crear enlaces.
- Candidatos de cambio: constructor y validación de `GlobPattern`; `collectSourceCandidates` y `populateSourceSet` del cargador; pruebas propias y pruebas unitarias existentes que expresan el contrato anterior. `ProjectSourceSet` y su interfaz permanecen protegidos si puede usarse de manera segura `addCanonical`.
- El contrato de `addCanonical` exige procedencia validada: antes de usarlo debe hacerse explícita la validación de archivo regular, extensión `.ns`, ruta canónica y relación con la raíz de búsqueda. Las rutas se deduplican por identidad canónica, conservando el orden determinista del conjunto.
- Continúan sin verificar: los límites exactos de enlace al atravesar componentes, los diagnósticos de E/S y la compatibilidad en consumidores de rutas y grafos. No se declara cerrada la decisión hasta pasar las pruebas dirigidas.

## Implementación y primeras verificaciones

- `GlobPattern` normaliza componentes literales, acepta `..` antes de los comodines y conserva el rechazo de rutas absolutas y `**` mal formado; `..` después de un comodín produce un diagnóstico explícito.
- El cargador enumera cada raíz externa explícita; sin `**` limita la profundidad a los componentes restantes. Conserva las carpetas reservadas, `Exclude`, `Remove`, orden y deduplicación. Rechaza intentos de subir por encima de la raíz del sistema de archivos y verifica la ruta canónica frente a la raíz externa antes de agregar la fuente.
- `ProjectSourceSet::add` mantiene el rechazo previo de fuentes exteriores; el cargador usa `addCanonical` tras validar explícitamente la fuente externa. Se precisó el contrato de esa función en su cabecera.
- Se habilitaron los destinos de prueba en el build aislado existente con `cmake -S A:/nsc-devel-fixes -B A:/temp/nsc-fixes-laptop-build -DNSC_BUILD_TESTS=ON`; no se creó otro build. Las pruebas unitarias dirigidas `nsc.project-loader` y `nsc.compilation-model` pasaron con la versión anterior al último diagnóstico de raíz; se repetirán al terminar el build incremental activo.
- `tests/integration/project_external_sources.py` pasó contra el compilador de build `c2cb327f1370ec37`: archivo explícito y glob compilados y ejecutados en O0/O2 con código 42, grafo, rutas anidadas, separadores Windows, exclusión, `Remove`, duplicados, rutas absolutas y globs mal formados. Hay un cambio posterior de límite de raíz aún pendiente de compilar y repetir.
- El intento de crear enlace simbólico de archivo falló con `WinError 1314`; la defensa contra escape de enlaces no cuenta con verificación dinámica. Registro: `A:/temp/nsc-fixes-102-directed-4.log`.
- El caso original citado en la evidencia histórica sigue sin estar disponible. No afirmar equivalencia con esa reproducción sin obtenerla.
- Build incremental final `ff27013311179dd8`: `A:/temp/nsc-fixes-102-build-5.log`. La prueba dirigida ampliada terminó con código 0 (`A:/temp/nsc-fixes-102-directed-5.log`): incluye, además, búsqueda predeterminada, límite por encima de la raíz y carpeta `bin` explícita y recursiva. `nsc.project-loader` y `nsc.compilation-model` terminaron con código 0 (`A:/temp/nsc-fixes-102-project-loader-5.log` y `A:/temp/nsc-fixes-102-compilation-model-5.log`).
- `clang-tidy` con `--checks=-*,clang-analyzer-*` terminó con código 0 sobre ambos archivos modificados. `GlobPattern.cpp` no produjo avisos; el análisis de `ProjectLoader.cpp` notificó dos avisos en `xfilesystem_abi.h` de MSVC, con trazas desde operaciones existentes de sistema de archivos. Registros `A:/temp/nsc-fixes-102-analysis-0.log` y `A:/temp/nsc-fixes-102-analysis-1.log`; no se verificó una línea base diferencial de esos avisos.
- La prueba de `tests/compiler-suite/host/modules/CompilationCore001To112Module.cpp` se actualizó para preservar su caso inválido y comprobar la inclusión externa, pero su ejecutable pertenece a una configuración separada y no se ha compilado ni ejecutado en esta laptop. La evidencia dirigida anterior no equivale a haber ejecutado esa batería.
- Registro final ampliado `A:/temp/nsc-fixes-102-directed-6.log`, código 0: repite los casos anteriores y confirma que el recorrido recursivo no siguió una unión de directorio creada mediante `cmd.exe /c mklink /J`. Esto no verifica el caso distinto de enlace simbólico de archivo, cuyo intento sigue fallando con `WinError 1314`.

## Pendientes y límites

- No verificado dinámicamente: enlace simbólico de archivo que apunta fuera de la raíz de búsqueda; quedó una regresión condicionada en la prueba dirigida, que informa si el sistema no permite crear el enlace. La unión de directorio sí se verificó por separado.
- No verificado: ejecución de la prueba de contrato del compiler-suite en su configuración independiente y reproducción histórica original ausente.
- Sin modificaciones de los archivos de consumo de SDK binario ni de los dos archivos de precompilación encontrados en la referencia local del colega. `NativeSharp.Text` sigue pendiente.
