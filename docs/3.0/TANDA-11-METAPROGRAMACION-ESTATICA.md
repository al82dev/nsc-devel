# Tanda 11 — Metaprogramación estática / Static metaprogramming

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. Capabilities y budgets sólo existen como
> contrato aislado; faltan evaluator, macros, derive, sandbox y source mapping. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se fijaron introspección tipada, atributos, derive, nombres higiénicos, capacidades herméticas,
inputs incrementales, diagnósticos de paquete y budgets. EN: Typed introspection, attributes, derive,
hygienic names, hermetic capabilities, incremental inputs, package diagnostics, and budgets are set.

## Archivos / Files

- Nuevos `MetaprogrammingSemantics.h/.cpp`: contratos reproducibles y sandbox T11.
- Nueva `NativeSharp3T11MetaprogrammingTests.cpp`: ocho pruebas específicas.
- `CMakeLists.txt`: integra modelo y ejecutable dirigido.

## Mejora / Improvement

La introspección no exige metadata runtime; derive registra dependencias estables; macros no capturan
nombres por accidente; reloj/FFI quedan fuera del perfil hermético; toda entrada declarada invalida
caché; y un límite excedido produce error determinista. Introspection remains compile-time only,
derive is cacheable, hygiene prevents capture, inputs are hashed, and budgets terminate evaluation.

## Límite / Boundary

T11 congela sandbox e identidades. El frontend completo de atributos/macros/#run y su persistencia se
conectan en T15. Full syntax and persistence integration belongs to T15.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t11_metaprogramming_tests`; sin baseline ni CTest general.

## Integración productiva 2026-09-29

La tanda operativa 24 conserva por unidad fuente la evidencia generada por el `ComptimeEngine` real
y agrega `nsc --dump-project-comptime`. El manifiesto `NSCOMPTIME 1.0` publica evaluaciones, pasos,
cache hits/misses, dependencias y digest SHA-256 con rutas relativas al proyecto. Evidencia:
[TANDA-OPERATIVA-24-T11-COMPTIME-MANIFEST.md](TANDA-OPERATIVA-24-T11-COMPTIME-MANIFEST.md).

Esto no conecta todavía introspection/attributes/derive/macros ni autoriza filesystem, env, clock,
random o FFI. T11 permanece `IMPLEMENTING`; E008–E011 requieren trabajo de libraries separado.
