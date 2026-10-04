# Tanda 08 — Funciones, lambdas y closures / Functions, lambdas, and closures

> **Estado físico 2026-10-04:** `IMPLEMENTING`. T08-F01, T08-F02, T08-F03 y la porción productiva de T08-F05 ya
> atraviesan parser, semántica, HIR, MIR 1.29, LLVM, PE y ejecución. El compilador distingue
> callbacks C sin environment, closures administradas con celdas compartidas y métodos de
> instancia ligados. Los tipos estructurales `fn*`/`fn&` conservan ABI, efectos, nullability,
> lifetime y componentes anidados hasta NSMETA y LLVM. Los contratos restantes no se presentan
> como implementación.

## Resultado / Outcome

ES: Los tipos función son valores estructurales completos; una lambda sin captura no convertida tiene
tipo nominal anónimo zero-size, invoca directamente su cuerpo estático y sólo materializa un puntero
cuando el contexto lo exige. También puede cruzar como puntero C real. Las closures administradas tienen
un environment ejecutable y los métodos ligados conservan su receptor. MIR 1.29 publica costo,
procedencia, modo, ownership y trait callable de cada captura. EN: Non-capturing lambdas can cross as real C function pointers,
function types have complete structural identity, managed closures have an executable environment, and bound methods preserve their receiver. MIR
1.19 publishes every capture's cost and provenance.

## Archivos / Files

- `ProjectMethodBodyModel.h/.cpp` y dumper: procedencia estable de la captura.
- `ProjectOopHirModel.h/.cpp`: la metadata HIR conserva esa procedencia.
- `ProjectOopMirModel.h/.cpp`, dumper e identidad de caché: MIR 1.29 hace visible layout,
  asignación, storage y origen del environment.
- `tests/cc1/c66_ns3_callable_values`: callback C, closure capturante y método ligado ejecutables.
- `CallableSemantics.h/.cpp`, `TypeSyntax`, parser, frontend, NSMETA y LLVM: identidad y ejecución
  productiva de `fn*`/`fn&`.
- `tests/unit/NativeSharp3T08CallableTests.cpp`: compatibilidad, identidad nominal, invocación
  directa, inferencia contextual, negativos, O0/NSLink y NSMETA.
- `docs/3.0/language/noncapturing-lambdas.md`: contrato bilingüe de F02.
- `docs/3.0/items/NS3-T08-F03-explicit-capture-closures.md`: contrato y evidencia de F03.

## Mejora / Improvement

Antes el dump sólo informaba `captures=N`, por lo que una herramienta no podía explicar si una
lambda asignaba memoria ni de dónde provenía una celda. Ahora cada lambda informa
`closure-layout`, `closure-allocation` y, por captura, `source` y `storage`. Esa información también
forma parte de la identidad de caché: no puede reutilizarse silenciosamente un objeto cuya ABI de
closure ya no coincide.

## Límite / Boundary

Este vertical no implementa lambdas genéricas, `must_tail` ni soporte de debugger. F03 agrega
capturas explícitas `copy`/`move`/`ref`/`ref mut`, environments con layout visible y capacidades
equivalentes a `Fn`/`FnMut`/`FnOnce`. El caller materializa el environment; `noheap` rechaza la
asignación necesaria. Los `static local` siguen diferidos a edición 4; F02 ejercita almacenamiento
estático mediante un campo `fn*`.

## Validación / Validation

`nsc` O0; prueba T08 22/22 con NSLink; c66 histórico; dumps MIR/method-body; negativos de callback
C capturante, lambda anónima capturante, parámetro sin tipo e invocación `unsafe`. Sin baseline,
CTest general, optimizaciones O1+ ni instalación.

F03 añade `nsc_ns3_t08_closure_capture_tests`: 7/7 en O0/NSLink, sin ampliar la ejecución.

El expediente reproducible está en
[TANDA-OPERATIVA-16-T08-CALLABLE-VALUES.md](TANDA-OPERATIVA-16-T08-CALLABLE-VALUES.md).
