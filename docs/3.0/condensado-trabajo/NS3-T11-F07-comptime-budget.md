# NS3-T11-F07 — Presupuesto y sandbox comptime / Comptime budget and sandbox

## ES

`ComptimeLimits` controla bytes fuente, memoria total observada, directivas, pasos, anidamiento y
expansiones de macros. El evaluador consulta cancelación en cada paso y durante el escaneo. Una
entrada declarada se carga contra el presupuesto antes de analizar su valor. Exceder cualquier
límite produce texto determinista; la recursión de macros incluye hasta ocho frames recientes de
backtrace.

El perfil de conformidad es hermético: `#run` no ejecuta código nativo, identificadores arbitrarios,
unsafe ni llamadas; FFI, reloj y aleatoriedad se rechazan antes de contactar al host. Archivos y
entorno sólo se observan mediante capacidades explícitas de F05. Por ello una función host inválida
no puede derribar NSC: nunca se carga ni se invoca dentro del sandbox.

Los mínimos predeterminados son 100 000 pasos, 128 niveles, 1 024 directivas, 64 expansiones y
128 MiB de memoria observada. Herramientas pueden reducirlos; elevarlos no habilita capacidades.

## EN

`ComptimeLimits` controls source bytes, observed memory, directives, steps, nesting, and macro
expansions. The evaluator polls cancellation on every step and during scanning. Declared input is
charged before its value is parsed. Every exhausted limit produces deterministic text; recursive
macros include up to eight recent expansion frames.

The conformance profile is hermetic: `#run` executes no native code, arbitrary identifier, unsafe
operation, or call; FFI, clock, and randomness are rejected before contacting the host. Files and
environment are visible only through F05 explicit capabilities. Consequently bad host code cannot
crash NSC because the sandbox never loads or invokes it.

Defaults guarantee 100,000 steps, 128 nesting levels, 1,024 directives, 64 expansions, and 128 MiB
of observed memory. Tools may lower these bounds; raising them never grants capabilities.

## Evidencia / Evidence

`nsc_ns3_t11_f07_comptime_budget_tests` covers step, memory, nesting, cancellation and macro
recursion limits plus unsafe/FFI rejection and reproducible diagnostics.
