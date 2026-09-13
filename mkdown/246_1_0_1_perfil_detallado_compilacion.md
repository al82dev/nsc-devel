# Entrega 246 — Perfil detallado de compilación / Detailed compilation profiling

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 246.
- Resultado previsto / Intended result: `--time` deja de informar solamente el total y expone el
  costo de cada fase real recorrida por una compilación de proyecto.
- Baseline: reservado para el cierre de la Tanda 1; no pertenece a esta entrega individual.

## Problema / Problem

### Español

La opción `--time` existente medía exclusivamente el tiempo de pared total. Esa cifra demostraba
que una compilación era lenta, pero no permitía distinguir si el costo provenía de cargar el grafo,
analizar sintaxis, construir HIR/MIR, bajar a LLVM, optimizar, emitir COFF o producir los artefactos
de biblioteca. Sin esa separación, cualquier intento de paralelizar o adelgazar O0 dependería de
suposiciones.

### English

The existing `--time` option measured only total wall-clock time. That value proved that a build was
slow but could not distinguish graph loading, syntax, HIR/MIR construction, LLVM lowering,
optimization, COFF emission, or library artifact generation. Parallelization and O0 cleanup need
phase evidence instead of guesses.

## Implementación / Implementation

### Medición del frontend / Frontend measurement

`ProjectCompilation` now accepts a phase-timing sink. Each phase owns an RAII timer around only the
work introduced by that phase; prerequisite phases keep their independent measurements. The syntax
path is separated into:

- `project-graph`;
- `syntax-lex`;
- `precompiled-syntax`;
- `syntax-parse`.

The semantic chain reports:

- `generics`;
- `names`;
- `members`;
- `inheritance`;
- `interfaces`;
- `generic-constraints`;
- `method-bodies`;
- `virtual-dispatch`;
- `destruction`;
- `construction`;
- `layout`;
- `oop-hir`;
- `oop-mir`.

El timer RAII también se cierra cuando una fase devuelve error. Por ello el informe muestra las
fases realmente alcanzadas, sin inventar ceros para trabajo que nunca comenzó.

### Medición del driver / Driver measurement

`DriverExecutionScope` collects the phase observations in execution order. Project entry points
share one setup function for cancellation and timing, including object, executable, static-library,
dynamic-library, header, and metadata commands. Artifact paths add stable names for major work such
as `precompiled-metadata`, `llvm-lowering`, `target-configuration`, `llvm-optimization`,
`object-emission`, library construction, header generation, and metadata generation.

### Formato / Format

```text
timing phase=syntax-parse us=317
timing phase=method-bodies us=1852
timing phase=oop-hir us=946
timing phase=oop-mir us=221
timing total_ms=6
```

- Las fases usan microsegundos para que el trabajo corto no desaparezca como `0 ms`.
- El total conserva milisegundos y su spelling anterior para compatibilidad.
- Los nombres y el orden son deterministas.
- Los valores son observaciones y nunca cambian hashes, metadata ni código generado.
- Sin `--time`, no se conservan mediciones ni se agrega salida.

## Decisiones de diseño / Design decisions

1. **Extender `--time`, no crear un modo incompatible.** Los usuarios actuales conservan
   `timing total_ms=N` y reciben detalle adicional solamente cuando ya solicitaron medición.
2. **Reloj monotónico.** Cambios del reloj del sistema no pueden producir duraciones negativas.
3. **Microsegundos por fase.** Es resolución suficiente para diagnóstico humano sin publicar ruido
   de nanosegundos como contrato textual.
4. **Orden de ejecución, no orden por costo.** Esto permite reconstruir hasta dónde llegó una
   compilación fallida y mantiene el reporte reproducible estructuralmente.
5. **Callback desacoplado.** El frontend no conoce streams ni opciones de línea de comandos; sólo
   entrega nombre y duración al consumidor configurado por el driver.
6. **Sin concurrencia todavía.** Esta entrega mide la tubería actual. Los commits posteriores de la
   Tanda 1 usarán esta evidencia para introducir workers sin mezclar medición y scheduling.

## Archivos modificados / Modified files

- `include/nativesharp/Frontend/ProjectCompilation.h`
  - agrega el sink de timing por fase.
- `src/Frontend/ProjectCompilation.cpp`
  - mide grafo, sintaxis y cada modelo semántico mediante RAII.
- `src/Driver/Driver.cpp`
  - recolecta, ordena y publica mediciones;
  - conecta timing y cancelación en todas las entradas de proyecto;
  - mide las operaciones principales de generación de artefactos;
  - actualiza la ayuda de `--time`.
- `tests/unit/DriverTests.cpp`
  - verifica que una compilación hasta MIR publique todas las fases en el orden esperado y conserve
    el total al final.
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`
  - formaliza formato, cobertura y carácter observacional del reporte.
- `docs/1.0.1/compiler/NativeSharp_Compiler_1_0_1_conformance.map`
  - enlaza el contrato con implementación y evidencia.
- `docs/1.0.1/NATIVESHARP_2_0_ITEMS_FINAL.md`
  - registra el estado de la entrega dentro del plan maestro.

## Criterios de aceptación / Acceptance criteria

- `nsc --check-project-oop-mir <project> --time` informa todas las fases alcanzadas.
- Cada línea usa `timing phase=<name> us=<value>`.
- Las líneas aparecen en orden determinista.
- `timing total_ms=<value>` permanece como la última medición.
- Una consulta sin fases, como `--version --time`, conserva el total.
- La opción no cambia éxito, diagnósticos, optimización ni artefactos.
- La reconstrucción limpia completa del compilador pasa antes de crear el commit.
- No se ejecuta baseline en esta entrega.

## Commit de referencia / Reference commit

**Título / Title**

`Perfila cada fase de compilación / Profile every compilation phase`

**Descripción / Description**

`Extend --time with deterministic microsecond measurements for project frontend and artifact phases, preserve the existing total wall-time record, connect every project-producing driver path to the shared timing sink, and document and test the observational contract.`
