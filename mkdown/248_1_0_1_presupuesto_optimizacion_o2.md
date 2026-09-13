# Entrega 248 — Presupuesto de optimización O2 / O2 optimization budget

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 248.
- Resultado / Result: O2 normal deja de pagar instrumentación LLVM extrema, acumula el tiempo real
  de todas sus invocaciones y expone un presupuesto configurable que advierte sin romper builds.
- Baseline: reservado para el cierre de la Tanda 1.

## Problema medido / Measured problem

La entrega 246 permitió separar el frontend, lowering, optimización y emisión. Al atravesar la ruta
real de Windows se comprobó que un proyecto pequeño pagaba 8,759,229 us dentro de
`llvm-optimization`. La transformación O2 no era por sí sola la causa: cada compilación relanzaba el
proceso, generaba miles de líneas del registro de pases y verificaba el módulo completo después de
cada pase.

That instrumentation is essential during an optimizer investigation, but it is not optimization
itself. Charging it to every normal O2 build made the development profile behave like a permanent
diagnostic session and hid whether LLVM's useful work actually fit a reasonable budget.

## Implementación / Implementation

### Ruta rápida normal / Normal fast path

An ordinary O1, O2, or O3 command now runs directly. It preserves mandatory verification before and
after the LLVM pipeline, but does not enable the verbose pass log or verify the full module after
every individual pass.

`--llvm-pass-trace` explicitly restores the diagnostic path for O1 through O3:

- supervisor child process on Windows;
- framed capture of LLVM standard error;
- pass-by-pass debug logging;
- verification after every pass;
- last observed pass evidence on failure.

O0 never enables that path because it has no optimized transformation pipeline to diagnose and
continues to preserve its targeted SEH legalization.

### Presupuesto acumulativo / Cumulative budget

`LlvmOptimizationBudgetTracker` receives every optimizer invocation in the current compiler thread.
It counts only O2 and accumulates program and runtime modules, so EXE, static-library and DLL builds
cannot appear within budget by reporting only one of their internal modules.

The default budget is 120,000 ms. `--o2-budget-ms=N` accepts 1 through 86,400,000 ms. When `--time`
is active and O2 work occurred, the driver emits one stable record:

```text
optimization-budget level=O2 budget_ms=120000 elapsed_us=452193 invocations=1 status=within
```

If cumulative time exceeds the budget, `nsc` emits a warning. It deliberately does not terminate
`LLVMRunPasses`: the public LLVM C API exposes that pipeline as one opaque call, so forcibly stopping
it would require unsafe process termination and could leave partial output. The artifact and exit
status therefore remain governed by compilation correctness, not machine speed.

### Aislamiento / Isolation

`LlvmOptimizationBudgetScope` installs the tracker with thread-local lifetime and restores any
previous tracker. Nested compiler calls cannot mix measurements. The future worker-pool delivery
must install the same scope explicitly in each worker; that requirement is recorded in the API
comment instead of silently sharing mutable global state.

## Resultado de rendimiento / Performance result

Misma máquina, proyecto `tests/language/l1_conditional`, `-O2 --time`:

| Ruta / Path | `llvm-optimization` | Total |
|---|---:|---:|
| Instrumentación implícita anterior / Previous implicit instrumentation | 8,759,229 us | 9,259 ms |
| O2 normal presupuestado / Budgeted normal O2 | 452,193 us | 970 ms |

La fase LLVM observada bajó aproximadamente 94.8%. These numbers are local engineering evidence,
not a promise that every project will compile in a fixed duration. The stable contract is the
normal/diagnostic policy and the structured cumulative measurement.

## Decisiones de diseño / Design decisions

1. **Presupuesto consultivo.** Una máquina lenta no convierte código válido en inválido.
2. **Acumulativo por invocación.** Cuenta módulos de programa y runtime, no sólo la llamada más
   conveniente.
3. **Microsegundos observados, milisegundos configurados.** La salida conserva resolución útil y la
   opción permanece humana.
4. **Diagnóstico extremo opt-in.** `--llvm-pass-trace` conserva toda la evidencia anterior cuando se
   investiga LLVM, pero el ciclo diario no la paga.
5. **Verificación de frontera siempre activa.** El cambio de rendimiento no permite emitir IR
   inválido.
6. **Sin timeout destructivo.** No se mata un proceso dentro de una operación opaca de LLVM.

## Archivos y funciones / Files and functions

- `include/nativesharp/LLVM/LlvmOptimizationObservation.h`
  - agrega `LlvmOptimizationBudgetTracker`, `LlvmOptimizationBudgetScope` y el presupuesto default.
- `src/LLVM/LlvmOptimizationObservation.cpp`
  - acumula duraciones O2, restaura scopes anidados y genera el reporte estable.
- `include/nativesharp/LLVM/LlvmOptimizer.h`
  - hace explícita la decisión de verify-each bajo diagnóstico detallado.
- `src/LLVM/LlvmOptimizer.cpp`
  - reporta cada duración al tracker activo;
  - activa debug logging y verify-each sólo para la ruta solicitada.
- `src/LLVM/LlvmProcessTrace.cpp`
  - exige nivel optimizado y `--llvm-pass-trace` para lanzar el supervisor.
- `src/nsc/main.cpp`
  - aplica la misma activación explícita en plataformas sin supervisor de Windows.
- `src/Driver/Driver.cpp`
  - analiza `--o2-budget-ms=N` y `--llvm-pass-trace`;
  - publica medición y warning acumulativos;
  - documenta correctamente verify-each en `--print-llvm-pipeline`.
- `tests/unit/LlvmOptimizationTests.cpp`
  - cubre niveles, acumulación, scopes anidados y formato.
- `tests/unit/DriverTests.cpp`
  - cubre ayuda, validación de rango, O2 real y reporte integrado.
- Contrato, mapa de conformidad y roadmap 1.0.1
  - registran la política pública, evidencia y avance.

## Criterios de aceptación / Acceptance criteria

- O2 normal no lanza el supervisor ni habilita verify-each.
- `--llvm-pass-trace -O2` conserva ambos mecanismos de diagnóstico.
- Todo módulo O2 sigue verificándose antes y después del pipeline.
- El tracker ignora O0, O1 y O3 y suma todas las invocaciones O2.
- El presupuesto configurable valida rango y duplicados.
- Superar el presupuesto genera warning sin alterar salida ni exit code.
- `--time` publica un registro determinista y mantiene `timing total_ms` al final.
- Las pruebas enfocadas y una reconstrucción limpia del compilador pasan.
- No se ejecuta baseline dentro de esta entrega.

## Commit de referencia / Reference commit

**Título / Title**

`Presupuesta O2 sin instrumentación implícita / Budget O2 without implicit instrumentation`

**Descripción / Description**

`Track cumulative O2 optimization time with a configurable advisory budget, keep boundary
verification mandatory, move expensive LLVM pass tracing and verify-each behind an explicit
diagnostic option, and document and test the normal and investigative execution paths.`
