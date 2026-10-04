# NS3-T06-F04 — Lifetimes nombrados e inferidos / Named and inferred lifetimes

## Resultado

NativeSharp 3.0 dispone de binders de lifetime borrados para tipos prestados, delegates, métodos y
funciones libres. Los parámetros/resultados `ref` e `in`, los campos `borrowed` y el receiver pueden
declarar procedencia; la elisión resuelve casos no ambiguos y el solver valida relaciones outlives
transitivas. El mismo contrato cruza una `LibraryReference` mediante NSMETA 1.34.

## Cambios principales

- AST y parser reconocen `<lifetime a>`, `ref[a]`, `in[a]`, `borrowed[a]` y
  `where longer : shorter`, exclusivamente en edición 3.
- El modelo de miembros y funciones de namespace valida declaraciones, elisión, fuentes de retorno,
  ciclos, subtyping outlives y equivalencia alfa entre métodos de trait e implementación.
- El análisis de cuerpos conserva procedencia en calls/indexers y rechaza retornos cuyo storage no
  satisface el lifetime publicado.
- Una closure ya no puede capturar un parámetro `ref`/`in`; el nombre de lifetime no convierte el
  entorno almacenable en seguro.
- Dumps de miembros/cuerpos exponen bindings y constraints sin modificar el ABI.
- NSMETA sube a 1.34 y serializa contratos de tipos, fields, delegates, miembros, funciones libres
  y parámetros. Merger, API diff y materialización pública consumen los mismos datos.
- El downgrade a 1.33 falla si el modelo contiene contratos de lifetime.

## Archivos nuevos

- `tests/unit/NativeSharp3T06LifetimeParameterTests.cpp`: prueba dirigida con positivos O0/NSLink,
  diagnósticos, traits, closures, productor/consumidor separado y round-trip NSMETA.
- `tests/fixtures/nsmeta/historical-1.33.nsmeta`: golden inmutable de la versión que pasa a ser
  histórica al publicar NSMETA 1.34.
- `docs/3.0/language/named-inferred-lifetimes.md`: contrato normativo bilingüe.
- Este documento: mapa y evidencia del commit.

## Archivos modificados

- AST/parser: `include/nativesharp/AST/AstNode.h`, `include/nativesharp/AST/TypeSyntax.h`,
  `include/nativesharp/Parser/Parser.h`, `src/AST/*` y `src/Parser/Parser.cpp`.
- Frontend: modelos de members, interfaces, funciones de namespace, genéricos y cuerpos bajo
  `include/nativesharp/Frontend` y `src/Frontend`.
- Metadata/driver: `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp`,
  `src/Interop/LibraryMetadataMerger.cpp`, `src/Interop/LibraryApiDiff.cpp` y `src/Driver/Driver.cpp`.
- Pruebas/versionado: `CMakeLists.txt`, `NativeSharp3T06MoveOnlyTests.cpp` y
  `Item405FinalCompatibilityCertificationTests.cpp`.
- Roadmaps y tracking de T06/F04.

## Mejora observable

Una API ya puede expresar sin ambigüedad que un retorno referencia a un argumento, al receiver o a
storage estático, y puede reducir un lifetime largo a uno corto mediante una prueba declarada. El
consumidor precompilado recibe exactamente ese contrato. No se añade runtime, heap, thunk ni cambio
de layout/calling convention.

## Validación dirigida

- `nsc.ns3.t06.lifetime-parameters`: 19 checks; ejecución O0 con NSLink, dump, elisión, outlives,
  traits, cierre de escapes, edición 3, NSMETA 1.34 y consumidor separado.
- Regresiones directas de closures, reference locals, F01/F03, traits y análisis de lifetimes.
- Construcción incremental del target dirigido y del ejecutable `nsc` antes del commit.
- Baseline: no ejecutado por prohibición explícita del proyecto.

