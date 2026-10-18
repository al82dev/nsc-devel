# H02S15C - construcción de arreglos HIR iterativa

## SCOPE LOCK

- Base: `7d43a94afe0883abaff977de4f73b5bb113106ee`.
- Alcance: dimensiones Range, count e initializers Array y fuentes spread.
- Protegido: CollectionBuilder, stackalloc, construcción de objetos S15D/E, aggregates S15F,
  parser, Sema, MIR, LLVM, ABI y NSMETA.
- Validación única: `nsc_project_oop_lowering_tests` con `hir-array-construction-depth`.

## Implementación

`ArrayConstruction` usa el scheduler común y agenda un operando por reanudación. Congela core,
variante Range/segmentada/no segmentada, flag inferred, orden lógico y clasificación spread. Las
conversiones y diagnósticos de initializer se aplican después de bajar el hijo. Las rutas legacy
ya no llaman `lowerExpression` para dimensiones, initializers o fuentes spread; sólo conservan las
formas hoja y guardas internas ante un escape del scheduler.

## Prueba focal

El mismo selector cubre nueve comprobaciones: 32,768 arrays anidados por count; forma y terminal;
dimensiones Range en orden; element/spread/element con vectores paralelos; count inferido y count
físico; conversiones no identidad exactamente una vez; diagnósticos exactos por tipo ausente o
conversión inválida; rechazo address context; y Range sin dimensiones con el diagnóstico histórico
`null expression reached Stage 34 OOP HIR`. Los satélites de error usan rangos no vacíos y afirman
`lastErrorRange`; spread afirma además valor, syntax y provenance de sus tres hijos.

- Build: `A:\NSC-devel-paralelo\logs\agente-1\h02s15c-build-amend.log`.
- Focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s15c-focus-amend.log`.
- Resultado: `PASS: 9 checks`.
- No se ejecutó CTest, suite, minibaseline, baseline ni otro selector.

La profundidad reside en frames del heap; el finalizador por reanudación no vuelve a bajar syntax.
