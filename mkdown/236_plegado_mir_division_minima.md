# 236 — Plegado MIR de división mínima / MIR minimum-division folding

## Resultado / Outcome

**ES.** Los inicializadores `const int` y `const long` materializan `MinValue / -1` con la
semántica envolvente de NativeSharp y `MinValue % -1` como cero. La evaluación no ejecuta una
operación indefinida del C++ que implementa el compilador.

**EN.** `const int` and `const long` initializers materialize `MinValue / -1` using NativeSharp's
wrapping semantics and `MinValue % -1` as zero. Evaluation never executes the corresponding
undefined operation in the C++ compiler host.

## Causa / Cause

La entrega 227 alineó el evaluador semántico y los dos backends LLVM, pero el materializador de
constantes desde OOP HIR conservó la política anterior: devolvía `nullopt` para el par mínimo/-1.
Como la construcción de campos estáticos exige un valor MIR, un solo campo nuevo invalidaba el
módulo completo y ocultaba también las comprobaciones de promoción constante existentes.

Delivery 227 aligned the semantic evaluator and both LLVM backends, but the OOP-HIR constant
materializer retained the old policy and returned `nullopt` for the minimum/-1 pair. Static-field
construction requires a MIR value, so one new field invalidated the complete module and also
masked the existing constant-promotion checks.

## Implementación / Implementation

- División mínima entre menos uno: se emite el mismo mínimo normalizado.
- Resto mínimo entre menos uno: se emite cero.
- Los demás pares siguen usando división o módulo con signo.
- División por cero continúa rechazándose.

Minimum divided by minus one emits the same normalized minimum, while the corresponding remainder
emits zero. Every other pair retains signed division/remainder, and division by zero remains
rejected.

## Archivos / Files

- `src/Frontend/ProjectOopMirModel.cpp`
- `mkdown/236_plegado_mir_division_minima.md`

## Verificación dirigida / Targeted verification

Se ejecuta únicamente `nsc.cc1.correctness`, además de una reconstrucción limpia del compilador.
No se ejecuta baseline intermedio.

Only `nsc.cc1.correctness` is run, followed by a clean compiler rebuild. No intermediate baseline
is executed.

## Commit de referencia / Reference commit

**Título ES:** Alinea el plegado MIR de división mínima

**Title EN:** Align MIR folding for minimum division

**Descripción ES:** Materializa `MinValue / -1` y `% -1` con la semántica envolvente definida por
NativeSharp sin invocar comportamiento indefinido del host.

**Description EN:** Materialize `MinValue / -1` and `% -1` with NativeSharp's defined wrapping
semantics without invoking undefined host behavior.
