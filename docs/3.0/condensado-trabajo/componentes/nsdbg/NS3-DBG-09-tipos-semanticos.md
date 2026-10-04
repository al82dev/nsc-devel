# NS3-DBG-09 — Visualización semántica 3.0 / NativeSharp 3.0 semantic visualization

## Resultado / Result

ES: NSDBG ya no muestra tuples, records, variants, bindings de patrones, objetos trait y `Result`
como agregados CodeView anónimos. Una capa pura posterior a la lectura segura de tipos reconoce el
contrato NativeSharp 3.0, conserva el árbol expandible y genera un resumen semántico acotado. Si el
tag de un variant fue optimizado o no está disponible, el debugger lo dice y no fabrica un caso.

EN: NSDBG no longer displays tuples, records, variants, pattern bindings, trait objects, and
`Result` as anonymous CodeView aggregates. A pure layer applied after safe type materialization
recognizes the NativeSharp 3.0 contract, preserves the expandable tree, and produces a bounded
semantic summary. If a variant tag was optimized out or is unavailable, the debugger reports that
fact instead of inventing an active case.

## Cambios / Changes

- `TypeKind` distingue `Tuple`, `Record`, `Variant`, `TraitObject` y `Result`.
- `presentNativeSharp3Value` recorre el árbol materializado, elimina sólo los prefijos internos de
  bindings de patrones conocidos y limita resúmenes a 16 hijos/512 bytes.
- Tuples conservan orden; records conservan nombres; variants publican tag y payload; `Result`
  traduce el ABI estable `0/1` a `Ok/Error`; `dyn Trait` conserva identidad nominal y dirección de
  una palabra.
- `TypeService` aplica la presentación después de leer CodeView. No ejecuta expresiones del target,
  no vuelve a leer memoria y no cambia layouts.

- `TypeKind` distinguishes `Tuple`, `Record`, `Variant`, `TraitObject`, and `Result`.
- `presentNativeSharp3Value` walks the materialized tree, removes only known internal pattern-binding
  prefixes, and bounds summaries to 16 children/512 bytes.
- Tuples retain order; records retain names; variants expose tag and payload; `Result` maps the
  stable `0/1` ABI to `Ok/Error`; `dyn Trait` retains its nominal identity and one-word address.
- `TypeService` applies presentation after CodeView materialization. It executes no target code,
  performs no extra memory read, and changes no layout.

## Archivos / Files

- `include/nsdbg/Variables.h`
- `src/core/NativeSharp3Visualizers.cpp`
- `src/core/TypeService.cpp`
- `CMakeLists.txt`
- `tests/NativeSharp3ValueTests.cpp`

## Prueba puntual / Targeted test

`A:\nsc-debugger\build\nsdbg_ns3_value_tests.exe`

Resultado / Result: `PASS: NativeSharp 3 tuples, records, variants, patterns, traits, and Result render honestly`.

No se ejecutó baseline, mini-baseline, CTest general, instalación ni deploy. / No baseline,
mini-baseline, broad CTest, installation, or deployment was run.
