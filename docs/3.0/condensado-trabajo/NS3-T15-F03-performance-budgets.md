# NS3-T15-F03 — Presupuestos de rendimiento / Performance budgets

## ES

Se definen perfiles separados para archivos gigantes, carga genérica, macros, fanout de módulos, frames async y diagnósticos patológicos. Cada perfil congela hardware Windows x64, mínimo de siete muestras, tolerancia de 5% y límites independientes para frontend, memoria pico, MIR, monomorfizaciones y code size.

El gate del lenguaje excluye explícitamente tiempo de backend y toolchain para no atribuir a semántica una regresión de LLVM o NSLink. Cada métrica falla por separado; la tolerancia tiene techo de 25% y una corrida submuestreada nunca certifica. La medición larga queda para el checkpoint autorizado, no para cada commit.

Prueba dirigida: `nsc_ns3_t15_performance_budget_tests` valida catálogo, tolerancia, muestreo, separación backend/toolchain y fallos causales por métrica sin ejecutar benchmarks largos.

## EN

Separate profiles cover giant files, generic-heavy input, macros, module fanout, async frames, and pathological diagnostics. Each profile freezes Windows x64 reference hardware, a seven-sample minimum, 5% tolerance, and independent frontend, peak-memory, MIR, monomorphization, and code-size limits.

The language gate explicitly excludes backend and toolchain time so an LLVM or NSLink regression is not attributed to semantics. Each metric fails independently; tolerance is capped at 25%, and an undersampled favorable run cannot certify. Long measurement remains reserved for an authorized checkpoint rather than every commit.

Focused test: `nsc_ns3_t15_performance_budget_tests` validates catalog shape, tolerance, sampling, backend/toolchain separation, and causal metric failures without running long benchmarks.
