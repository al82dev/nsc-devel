# Reparaciones de certificación de la tanda 5 / Batch 5 certification repairs

## Propósito / Purpose

Este cambio repara cuatro fallos de infraestructura descubiertos por el baseline final de la tanda
5. Ninguno representa una regresión semántica del compilador: la matriz completa de fixtures y
triage pasó en O0 y O2 con ambos linkers. / This change repairs four infrastructure failures found by
the final batch 5 baseline. None represents a compiler semantic regression: the complete fixture and
triage matrix passed in O0 and O2 with both linkers.

## Archivos y funciones / Files and functions

- `tests/unit/CrossLibraryInliningTests.cpp`, prueba principal: deja de fijar NSMETA 1.13 como formato
  actual y verifica `kMetadataFormatMinor`, conservando la precondición de que el contrato de
  inlining existe desde 1.13. / The main test stops pinning NSMETA 1.13 as the current format and
  verifies `kMetadataFormatMinor`, while preserving the precondition that the inline contract exists
  since 1.13.
- `tests/unit/Stage37StaticInitializationTests.cpp`, `testMemberFlagsAndStaticAbi()`: actualiza la
  forma textual esperada del campo MIR para incluir `volatile=false`, añadido legítimamente por el
  ítem 308. / `testMemberFlagsAndStaticAbi()` updates the expected MIR field text to include
  `volatile=false`, legitimately added by item 308.
- `tests/unit/Stage48_5CSharpPInvokeHostTests.cpp`, `testManagedCompilationWhenSdkExists()`: conserva
  la compilación estricta cuando .NET 9 es utilizable, pero reconoce `NU1100` para los targeting packs
  del framework como una instalación incompleta del host y no como un fallo del contrato P/Invoke.
  Cualquier otro error de compilación continúa siendo rojo. / `testManagedCompilationWhenSdkExists()`
  keeps strict compilation when .NET 9 is usable, but recognizes framework-targeting-pack `NU1100`
  as an incomplete host installation rather than a P/Invoke contract failure. Every other compiler
  error remains red.
- `tests/fixtures/nsmeta/historical-1.13.nsmeta`: archiva los bytes del último formato anterior ahora
  que NSMETA 1.14 es el formato vigente. / Archives the bytes of the last previous format now that
  NSMETA 1.14 is current.

## Decisiones de diseño / Design decisions

Las pruebas que siguen el formato producido por el compilador usan la constante pública del codec;
los golden históricos siguen fijados por versión y permanecen inmutables. La prueba administrada no
convierte cualquier fallo de `dotnet build` en un salto: sólo acepta la firma específica de paquetes
de referencia ausentes, después de que todas las validaciones estáticas del contrato C# ya pasaron.
/ Tests that track the compiler-produced format use the codec's public constant; historical goldens
remain version-pinned and immutable. The managed test does not turn every `dotnet build` failure into
a skip: it accepts only the specific missing-reference-pack signature, after all static C# contract
checks have already passed.

## Evidencia requerida / Required evidence

- Las cuatro pruebas focalizadas deben pasar con lld-link y NSLink. / All four focused tests must pass
  with lld-link and NSLink.
- El baseline final debe pasar CTest, fixtures y triage para ambos linkers antes de crear el milestone.
  / The final baseline must pass CTest, fixtures, and triage for both linkers before creating the
  milestone.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Repara la certificación de NSMETA 1.14 / Repair NSMETA 1.14 certification`

**Descripción / Description**

`Actualiza contratos de prueba obsoletos, archiva el golden NSMETA 1.13 y evita atribuir a NativeSharp una instalación .NET sin targeting packs. / Update stale test contracts, archive the NSMETA 1.13 golden, and avoid attributing a .NET installation without targeting packs to NativeSharp.`
