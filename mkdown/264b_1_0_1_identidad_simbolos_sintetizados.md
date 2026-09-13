# Corrección 264B — Identidad de símbolos sintetizados / Synthesized symbol identity

## Origen / Origin

El baseline 1/16 (`20260912-222114-1bf38198`) aisló tres colisiones de enlace causadas por una
misma omisión de diseño: algunos símbolos generados no expresaban quién era su dueño físico.
Closures en distintas unidades definían los mismos helpers fuertes, inicializadores estáticos de
productos independientes compartían un ordinal local y constantes reconstruidas desde NSMETA se
materializaban como definiciones fuertes tanto en productor como en consumidor.

## Implementación / Implementation

`ProjectOopLlvmLowerer` now assigns linkage from the real boundary of each synthesized entity:

- capture-cell retain/release helpers use internal linkage because their calls never leave the LLVM
  module that emits them;
- type-initializer entry points derive their name from the declaring nominal type identity; private
  guard/owner globals retain their compact local ordinal, monolithic cctors remain private and
  partitioned peer units reference the coordinator's unique definition;
- every compile-time constant definition uses weak ODR linkage with a symbol-specific COMDAT, so a
  DLL consumer can own its metadata-materialized copy and a static producer/consumer pair can merge
  equivalent copies without a duplicate symbol.

Mutable imported statics are unchanged: the producer owns their storage and consumers use the
published accessor. La corrección tampoco convierte helpers locales en ABI pública.

## Archivos / Files

- `src/LLVM/ProjectOopLlvmLowerer.cpp`: implements local helper linkage, nominal cctor identities and
  selectable constant definitions.
- `tests/unit/Stage37StaticInitializationTests.cpp`: accepts the quoted nominal physical cctor name
  while retaining the private monolithic contract.
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`: defines `NSC-CODEGEN-004`.
- Compiler conformance map and 1.0.1 changelog: add traceability and consumer-facing status.

## Decisiones / Decisions

No se aplicó COMDAT indiscriminadamente. Un helper puramente local es `internal`; un dato mutable
importado sigue siendo propiedad exclusiva del productor; únicamente las constantes inmutables con
valor certificado pueden tener definiciones equivalentes. El `.cctor` no usa weak ODR porque su
cuerpo y efectos pertenecen exactamente a un producto: se elimina la colisión dando identidad
nominal al símbolo correcto.

## Evidencia dirigida / Focused evidence

- Stage 37 static initialization: 60 checks pass.
- E11 shared capture-cell execution: 4 checks pass.
- Stage 46 native/precompiled libraries: 456 checks pass, including static libraries, DLLs,
  partitioned O0/O2 consumers and NSMETA constant materialization.
- No additional baseline was run; this is a focused repair after baseline 1/16.

## Commit de referencia / Reference commit

**Título / Title**

`Aísla identidades de símbolos sintetizados / Isolate synthesized symbol identities`

**Descripción / Description**

`Give module-local closure helpers internal linkage, derive partition-crossing type-initializer names
from nominal identity, materialize certified constants as symbol-specific ODR definitions, and cover
static-library and DLL consumers without weakening mutable producer-owned storage.`
