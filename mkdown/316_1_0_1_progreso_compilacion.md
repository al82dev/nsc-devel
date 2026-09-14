# 316 — Progreso observable de compilación / Observable compilation progress

## Resumen / Summary

### Español

Se agregó la opción `--progress` para que una compilación larga deje de ser indistinguible de un proceso bloqueado. La opción transmite actividad mientras el compilador trabaja: anuncia el inicio y final de cada fase, incluye el tiempo transcurrido al cerrar la fase y muestra el archivo completado durante lectura, análisis léxico y análisis sintáctico.

Cada línea se vacía inmediatamente al flujo de salida. Los reportes producidos desde workers paralelos están serializados, de modo que `-j N` no mezcla fragmentos de distintas líneas.

### English

The `--progress` option was added so a long compilation is no longer indistinguishable from a stalled process. It streams activity while the compiler works: phase begin/end boundaries, elapsed time when a phase ends, and the completed file during source reading, lexing, and parsing.

Every line is flushed immediately. Reports produced by parallel workers are serialized, so `-j N` cannot interleave fragments from different lines.

## Uso / Usage

```bat
C:\native-sharp\bin\nsc.exe --emit-project-oop-lib Project.nsproj -O0 -o Project.lib -j 16 --progress
```

Formato estable de las líneas / Stable line format:

```text
progress phase=project-graph state=begin
progress phase=syntax-read file="A:/project/Source.ns" completed=1 total=310
progress phase=syntax-read state=end elapsed_ms=18
progress phase=method-bodies state=begin
progress phase=llvm-object-generation state=end elapsed_ms=742
```

`--progress` es independiente de `--time`. La primera opción transmite actividad inmediata; la segunda conserva el informe acumulativo y determinista al terminar. Pueden utilizarse juntas.

`--progress` is independent from `--time`. The former streams immediate activity; the latter retains the deterministic accumulated report emitted at process exit. They may be used together.

## Archivos modificados / Modified files

- `include/nativesharp/Frontend/ProjectCompilation.h`
  - Agrega `ProjectCompilation::setProgressSink`.
  - Conserva el transporte desacoplado mediante callback, sin introducir dependencias del frontend hacia la consola.
- `src/Frontend/ProjectCompilation.cpp`
  - `ProjectPhaseTimer` transmite límites de fase y duración.
  - `reportSourceProgress` publica archivo y contador durante lectura, lexer y parser.
  - Los contadores son atómicos porque esas tres operaciones pueden terminar desde workers diferentes.
- `src/Driver/Driver.cpp`
  - Reconoce `--progress` como opción global.
  - `DriverExecutionScope::writeProgress` serializa, escribe y vacía cada línea.
  - `DriverPhaseTimer` cubre también metadata precompilada, lowering LLVM, optimización, emisión de objetos y construcción de EXE/LIB/DLL.
  - La ayuda integrada describe la opción.
- `tests/unit/DriverTests.cpp`
  - Comprueba que la ayuda publique `--progress`.
  - Comprueba límites de fase, actividad por archivo, orden observable y que la opción no active implícitamente `--time`.

## Decisiones de diseño / Design decisions

1. **Opción explícita / Explicit option.** La salida normal permanece sin ruido ni costo de sincronización.
2. **Flushing por línea / Per-line flushing.** Un supervisor externo recibe vida útil real sin esperar a que se llene el buffer o termine `nsc`.
3. **Una cerradura sólo en modo progreso / One lock only in progress mode.** La compilación ordinaria no adquiere la cerradura; bajo `--progress` se priorizan líneas válidas y observabilidad.
4. **Fases compartidas / Shared phases.** Se reutilizan los límites de fase que alimentan `--time`, evitando que ambos mecanismos diverjan.
5. **Archivos en las fases naturalmente independientes / Files in naturally independent phases.** Lectura, lexer y parser ya operan por unidad de fuente. Las fases semánticas y de backend reportan su nombre sin inventar una atribución de archivo que no existe en esos modelos.
6. **Sin cambio semántico / No semantic change.** La opción no altera orden de diagnósticos, artefactos, optimización ni código de salida.

## Evidencia focalizada / Focused evidence

```text
cmake --build build-test-lldlinker --target nsc_driver_tests -j 8
build-test-lldlinker\tests\nsc_driver_tests.exe
PASS: 217 checks
```

No se ejecutó baseline ni se compiló el runtime, bindings o tutoriales.

No baseline, runtime, bindings, or tutorials were run.

## Mensaje de commit de referencia / Reference commit message

### Título / Title

```text
Expone progreso de compilación por fase / Expose phase-aware compilation progress
```

### Descripción / Description

```text
ES:
Agrega --progress con límites de fase transmitidos y vaciados inmediatamente,
duración por fase y avance de archivos durante lectura, lexer y parser.
Serializa la salida generada por workers paralelos y mantiene --time como
un informe acumulativo independiente.

EN:
Add --progress with streamed and immediately flushed phase boundaries,
per-phase elapsed time, and file progress during reading, lexing, and parsing.
Serialize output produced by parallel workers and keep --time as an
independent accumulated report.
```
