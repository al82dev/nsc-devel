# Tanda 11 — Metaprogramación estática / Static metaprogramming

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
