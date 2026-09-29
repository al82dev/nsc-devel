# Tanda 17: telemetría estructurada, memoria y failure bundles

## Correspondencia con el plan NativeSharp 3.0

Este commit cierra la **tanda operativa 01** del documento maestro: el vertical pequeño y
verificable de P00/P01/P02. No declara certificados todos los corpus, presupuestos ni herramientas
del programa P00–P02; deja una base estable de timings, eventos y logs sobre la cual medirlos sin
parsear mensajes localizados.

## Objetivo

- Conservar el progreso humano estilo CMake ya disponible.
- Exponer el mismo trabajo como eventos JSON versionados.
- Hacer que `--time-trace` sea un trace de tiempos y memoria, no una copia del log completo.
- Conservar un fallo explícitamente solicitado en un NSDIAG transaccional y redactado.
- Verificar que el log automático secuencial continúa funcionando.

## Estado previo que se reutilizó

- `--language=en|es`, con inglés como idioma predeterminado.
- Progreso de fases, fuentes y módulos LLVM.
- `--quiet`, `--verbose`, `--time` y `--time-report`.
- `--log`, `--log-on-error`, `--log-format=text|json`, `--no-error-log` y la secuencia
  `error.log`, `error1.log`, ... con claims entre procesos.
- El formato NSDIAG y los fingerprints SHA-256 de proyecto, fuentes y bibliotecas.

## Cambios realizados

### Eventos de compilación

- Se añadió `CompilationEvent`, con tipos estables `phase-started`, `phase-finished`,
  `work-started`, `work-finished`, `cache-hit`, `cache-miss`, `skipped` y `failed`.
- Frontend, fases del driver y particiones LLVM ya no entregan texto preformateado: entregan datos
  estructurados a un único renderer serializado.
- `--progress=json` emite una línea JSON por evento con schema
  `nativesharp-progress-event`, versión 1.
- `--progress-format=text|json` selecciona el renderer; el texto conserva el contrato estilo CMake.
- Proyecto y target tienen eventos de inicio y terminación; un resultado no exitoso produce eventos
  `failed`.
- Los nombres del schema JSON permanecen en inglés aun bajo `--language=es`; sólo se localiza el
  texto destinado a personas.

### Trace de tiempo y memoria

- Cada fase conserva duración en microsegundos y el peak working set del proceso observado al
  terminar la fase.
- El total conserva wall time y peak working set final.
- `--time-trace=<archivo>` publica atómicamente `nativesharp-time-trace` versión 1 con esos campos.
- El trace ya no contiene stdout/stderr arbitrario ni finge ser un log JSON.
- La consulta de memoria usa la API Win64, coherente con el target exclusivo actual de NSC.

### Logs y failure bundle

- `--save-failure-bundle=<archivo.nsdiag>` escribe un NSDIAG solamente después de un fallo.
- El bundle contiene identidad completa del compilador, target, resultado, argumentos efectivos,
  stdout/stderr y fingerprints de las entradas que pudieron cargarse.
- Si el grafo del proyecto no puede cargarse, el bundle todavía se publica, conserva el error de
  carga y fingerprinta el `.nsproj` raíz cuando existe.
- Argumentos con prefijos `--token=`, `--secret=`, `--password=` y `--api-key=` se redactan.
- La publicación del NSDIAG cambió de escritura directa a temporal + rename.
- Los logs automáticos JSON mantienen publicación transaccional y no dejan directorios `.claim`.

## Archivos nuevos

- `include/nativesharp/Basic/CompilationEvent.h`: contrato de datos independiente del renderer.
- `include/nativesharp/Driver/CompilationTelemetry.h`: API pública pequeña de formato, memoria y
  serialización.
- `src/Driver/CompilationTelemetry.cpp`: renderers text/JSON y schema del time trace.
- `tests/unit/CompilationTelemetryTests.cpp`: prueba dirigida sin recorrer el baseline.
- `docs/compiler-optimization/TANDA-17-structured-compilation-telemetry.md`: este expediente.

Separar evento, renderer y prueba evita que frontend/LLVM dependan de textos localizados o de una
interfaz gráfica particular.

## Archivos modificados

- `CMakeLists.txt`
- `include/nativesharp/Driver/DiagnosticBundle.h`
- `include/nativesharp/Frontend/ProjectCompilation.h`
- `include/nativesharp/LLVM/ProjectOopObjectGenerator.h`
- `src/Driver/DiagnosticBundle.cpp`
- `src/Driver/Driver.cpp`
- `src/Frontend/ProjectCompilation.cpp`
- `src/LLVM/ProjectOopObjectGenerator.cpp`
- `src/nsc/main.cpp`

## Mejora obtenida

- CMake, nscbuilder, Project Manager y VS Code pueden consumir eventos por campos estables sin
  reconocer frases en inglés o español.
- En trabajo paralelo, el mutex del sink conserva una línea completa por evento.
- Una compilación lenta ahora permite atribuir simultáneamente latencia y crecimiento del working
  set a las fases principales.
- Un fallo controlado puede transportarse en un único NSDIAG reproducible sin perder el diagnóstico
  aunque el grafo esté dañado.
- Con la telemetría desactivada no se consulta memoria por fase y no se renderizan eventos.

## Verificación dirigida

- Build `nsc` Debug/O0 en `A:\nsc-devel\build-clang`: correcto.
- Build exclusivo de `nsc_compilation_telemetry_tests`: correcto.
- `nsc.compilation-telemetry`: **1/1 correcto**; cubre formatos, localización, JSON, trace con
  memoria, bundle ante proyecto inválido, redacción y publicación transaccional.
- Compilación real O0 de `tests/bug_triage/b1_this_copy`: correcta; 206 eventos JSON parseados,
  incluidos `project` y `target`.
- Trace real O0: correcto; 19 fases con `duration_us` y `peak_working_set_bytes`.
- Fallo CLI controlado: código 2 y NSDIAG con identidad, argumentos, 43 entradas fingerprintadas y
  stderr.
- Fallo controlado con log JSON automático: `error.log` válido y cero claims residuales.
- `--progress=xml`: rechazado de forma determinista con código 2.

## Limpieza y límites

- Se eliminaron traces, stdout/stderr capturados, NSDIAG y directorio de logs usados como fixtures
  manuales.
- No se ejecutó baseline.
- No se ejecutaron matrices O1/O2/O3.
- No se instaló ni escribió código en `C:`.
- La certificación cuantitativa completa de P00–P02 (corpus grande, CPU/I/O, overhead <5%,
  retención por proyecto, `nsc reproduce` y consumidores gráficos) permanece en sus gates futuros;
  esta tanda sólo congela el vertical estable exigido por el tren 01.
