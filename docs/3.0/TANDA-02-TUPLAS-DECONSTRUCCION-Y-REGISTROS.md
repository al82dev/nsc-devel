# Tanda 02 — Tuplas, deconstrucción y registros

> **Reclasificación 2026-09-29 (tanda operativa 07):** `IMPLEMENTING`. F01–F03 ya poseen corte
> vertical parser→semántica→HIR→MIR→LLVM y ABI Win64, con fixture NativeSharp O0. T02 permanece
> abierto por F04–F06, metadata pública de nombres, ownership/drop y tooling. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado

Esta tanda establece el modelo estructural de agregados de NativeSharp 3.0: identidad de tuplas,
conversiones elemento por elemento, layout Windows x64, clasificación de retorno, bindings de
deconstrucción, opt-ins de records, campos promovidos y spreads. La sintaxis nueva queda separada
por edición y conserva nodos explícitos en el AST.

## Cambios realizados

- Se añadió `AggregateSemantics`, un contrato sin runtime administrado para identidad y conversión
  de tuplas, layout físico, retorno Win64, deconstrucción, derives, campos promovidos y spreads.
- Se añadió `TupleExpression` al final del catálogo AST para no renumerar identidades 2.x.
- El parser reconoce tuplas vacías y de varias posiciones, nombres de elementos, targets de
  deconstrucción y spreads dentro de agregados.
- `record` permanece contextual: sólo `record struct` lo consume, requiere edición 3 y se almacena
  como marcador del mismo `StructDeclaration`. No introduce identidad de objeto ni wrapper ABI.
- Se añadió una prueba unitaria exclusiva de T02; no se ejecuta el baseline general.

### Integración productiva F01–F03

- `TypeSyntax` representa tuplas estructurales anidadas y conserva nombres sólo como metadata de
  fuente; la clave canónica usa longitudes y tipos, nunca nombres.
- Parser, name lookup, genéricos y member model aceptan tipos tupla en firmas, parámetros y locals.
- `ProjectObjectLayoutModel` calcula offsets/alineación recursivos y MIR publica layouts sintéticos
  deterministas con clasificación Win64 direct/indirect.
- Los literales infieren una clave estructural, preservan evaluación izquierda→derecha y reutilizan
  la inicialización in-place de agregados.
- Las conversiones de tupla son elemento por elemento, incluyendo anidamiento, y LLVM materializa
  cada elemento una vez.
- La deconstrucción soporta asignación, swap/aliasing, descartes y declaración `var (a, _)`; la
  fuente completa se materializa antes de evaluar cualquier destino.
- MIR schema 1.17 hace explícitos `tuple-deconstruct` y `tuple-deconstruct-element-store`.

## Archivos nuevos y razón

- `include/nativesharp/Language/AggregateSemantics.h`: API normativa y comprobable de agregados.
- `src/Language/AggregateSemantics.cpp`: implementación determinista independiente del runtime.
- `tests/unit/NativeSharp3T02AggregateTests.cpp`: prueba dirigida de contratos, parser y gate 2/3.
- Este documento: evidencia de alcance, decisiones y validación de la tanda.

## Archivos modificados

- `CMakeLists.txt`: registra el modelo y el único target de prueba T02.
- `include/nativesharp/AST/AstNodeKind.h` y `src/AST/AstNodeKind.cpp`: nodo de tupla estable.
- `src/Parser/Parser.cpp`: sintaxis de tuplas, deconstrucción, spread y `record struct` contextual.

## Mejora obtenida

Los agregados ligeros ya tienen reglas únicas y auditables antes del lowering: los nombres no
alteran identidad/ABI, el layout y retorno están fijados para Windows x64, los movimientos pueden
expresarse en bindings sin copias implícitas, y un record no sintetiza igualdad, hash, impresión ni
clone salvo opt-in. Esto evita que azúcar de superficie esconda asignaciones o identidad runtime.

## Límite consciente vigente

F01–F03 ya no son helpers aislados: compilan y ejecutan hasta PE x64. No se afirma todavía el gate
T02 completo. `Deconstruct` nominal, ownership/move-only, partial drops y drop flags dependen de
T06 (tandas 13–14); nombres públicos en NSMETA y consumo precompilado se cierran en integración;
debugger/LSP corresponden a la tanda 29. Records, promoted fields y spreads siguen en la tanda 08.

## Validación dirigida

- Construcción del target `nsc` en `-O0`.
- Construcción y ejecución directa de `nsc_ns3_t02_aggregate_tests`.
- Restauración de `NSC_BUILD_TESTS=OFF` al cerrar.
- Sin baseline general y sin CTest amplio.
- Fixture `c57_ns3_tuples` O0: exit 42 con retornos/parámetros directos e indirectos, conversión,
  nesting, swap, discard y declaración inferida.
- Dumps HIR/MIR: layouts 8/16/12/24 bytes, ABI Win64 direct/sret y schema 1.17.
- Negativa dirigida: aridad, fuente no-tupla y binding duplicado reciben diagnósticos causales.

---

# Batch 02 — Tuples, deconstruction, and records

## Outcome

This batch establishes NativeSharp 3.0's structural aggregate model: tuple identity, element-wise
conversion, Windows x64 layout, return classification, deconstruction bindings, record opt-ins,
promoted fields, and spreads. New syntax is edition-gated and retained as explicit AST nodes.

## Changes

- Added `AggregateSemantics`, a managed-runtime-free contract for tuple identity and conversion,
  physical layout, Win64 return, deconstruction, derives, promoted fields, and spreads.
- Appended `TupleExpression` to preserve every serialized 2.x AST identity.
- Parser support covers unit and multi-element tuples, element names, deconstruction targets, and
  aggregate spreads.
- `record` stays contextual: only `record struct` consumes it, edition 3 is required, and the
  existing value-type declaration carries the semantic marker without an ABI wrapper.
- Added one T02-only test executable; the general baseline is not run.

## Improvement

Lightweight aggregates now have one auditable contract before lowering. Names cannot alter identity
or ABI, Windows x64 layout and return rules are explicit, deconstruction modes do not imply hidden
copies, and records acquire generated behavior only through an explicit derive.

## Deliberate boundary

This batch freezes syntax, AST, and semantic contracts. Full OOP/MIR/LLVM materialization, including
partial drops and nominal `Deconstruct`, belongs to T15's end-to-end integration and is not claimed
as complete code generation here.
