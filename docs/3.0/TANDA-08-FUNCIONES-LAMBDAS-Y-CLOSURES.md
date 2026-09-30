# Tanda 08 — Funciones, lambdas y closures / Functions, lambdas, and closures

> **Estado físico 2026-09-29:** `IMPLEMENTING`. T08-F02 y la porción productiva de T08-F05 ya
> atraviesan parser, semántica, HIR, MIR 1.19, LLVM, PE y ejecución. El compilador distingue
> callbacks C sin environment, closures administradas con celdas compartidas y métodos de
> instancia ligados. Los contratos declarativos restantes no se presentan como implementación.

## Resultado / Outcome

ES: Las lambdas sin captura pueden cruzar como punteros C reales, las closures administradas tienen
un environment ejecutable y los métodos ligados conservan su receptor. MIR 1.19 publica el costo y
procedencia de cada captura. EN: Non-capturing lambdas can cross as real C function pointers,
managed closures have an executable environment, and bound methods preserve their receiver. MIR
1.19 publishes every capture's cost and provenance.

## Archivos / Files

- `ProjectMethodBodyModel.h/.cpp` y dumper: procedencia estable de la captura.
- `ProjectOopHirModel.h/.cpp`: la metadata HIR conserva esa procedencia.
- `ProjectOopMirModel.h/.cpp`, dumper e identidad de caché: MIR 1.19 hace visible layout,
  asignación, storage y origen del environment.
- `tests/cc1/c66_ns3_callable_values`: callback C, closure capturante y método ligado ejecutables.
- `CallableSemantics.h/.cpp` y su prueba aislada continúan siendo contratos de diseño para las
  partes aún no conectadas.

## Mejora / Improvement

Antes el dump sólo informaba `captures=N`, por lo que una herramienta no podía explicar si una
lambda asignaba memoria ni de dónde provenía una celda. Ahora cada lambda informa
`closure-layout`, `closure-allocation` y, por captura, `source` y `storage`. Esa información también
forma parte de la identidad de caché: no puede reutilizarse silenciosamente un objeto cuya ABI de
closure ya no coincide.

## Límite / Boundary

Este vertical no implementa sintaxis explícita `copy/move/ref`, `Fn/FnMut/FnOnce`, lambdas genéricas,
identidad completa de tipos función, `must_tail`, metadata cross-module ni soporte de debugger. La
representación actual es honesta: toda captura productiva es una referencia a celda compartida y el
environment capturante usa heap administrado.

## Validación / Validation

`nsc` Debug/O0; c66 con lld-link y NSLink (exit 42); dumps MIR/method-body; negativo callback C
capturante. Sin baseline, CTest general, optimizaciones O1+ ni instalación.

El expediente reproducible está en
[TANDA-OPERATIVA-16-T08-CALLABLE-VALUES.md](TANDA-OPERATIVA-16-T08-CALLABLE-VALUES.md).
