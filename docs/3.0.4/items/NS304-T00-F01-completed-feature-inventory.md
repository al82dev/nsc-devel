# NS304-T00-F01 — Inventario de features 3.0 declaradas terminadas

## Estado

`CERRADO` el 2026-10-12 como inventario y localización documental. Este cierre no
recertifica las 55 features ni anticipa la clasificación por capas de
`NS304-T00-F02`.

## SCOPE LOCK

- Objetivo: inventariar todas las filas `TERMINADA` del documento maestro 3.0 y
  localizar sus anclas actuales de producto, herramienta y prueba.
- Naturaleza: auditoría documental y trazabilidad; sin cambio funcional.
- Revisión base: `08c19e4d58c94c06f9d2ba9ae8ff531b1522f9c7` en
  `development`.
- Archivos modificables: este expediente y el tracking maestro 3.0.3–3.0.6.
- Protegido: compilador, runtime, CMake, toolchain, Builder, manifests y pruebas.
- Validación: extracción determinista de filas, unicidad, existencia de
  expedientes, resolución/ancestría de commits y existencia de anclas.
- Terminación: 55/55 filas localizadas, sin omitidos ni desconocidos, y toda
  contradicción registrada sin presentarla como implementación vigente.

## Autoridad y método

- Fuente original: `A:\DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md`, tabla de
  cierres 3.0.
- Repositorio contrastado: `A:\nsc-devel`, revisión base indicada.
- Extracción: filas `NS3-Txx-Fxx` cuyo estado literal es `TERMINADA`.
- Localización: expediente atómico, commit declarado, hasta dos anclas actuales
  de productor/herramienta y hasta dos anclas actuales de prueba.
- `ANCLADA; capas en F02` significa solamente que las rutas actuales existen.
  No sustituye la comprobación parser/Sema/HIR/MIR/LLVM/NSMETA/runtime/ejecución
  exigida por F02.

## Resultado mecánico

- Filas `TERMINADA`: **55**, únicas: **55**.
- Distribución: T00=6, T01=7, T02=6, T03=6, T04=7, T05=7, T06=7,
  T07=6, T08=3.
- Expedientes presentes: **55/55**.
- Commits resolubles y ancestros de la revisión base: **55/55**.
- Verticales con anclas actuales para F02: **54/55**.
- Verticales retiradas con contradicción explícita: **1/55**.
- Omitidas: **0**; desconocidas: **0**.

## Inventario completo

| ID histórico | Feature | Commit / expediente | Anclas actuales | Prueba/evidencia actual | Clasificación F01 |
|---|---|---|---|---|---|
| `NS3-T00-F01` | Ediciones de lenguaje / Language editions | `e0be188d4`<br>`docs/3.0/items/NS3-T00-F01-language-editions.md` | `src/Compilation/ProjectGraphLoader.cpp`<br>`src/Compilation/ProjectLoader.cpp` | `tests/conformance/t00/edition3-project.json`<br>`tests/unit/NativeSharp3T00ConstitutionTests.cpp` | RETIRADA; contradicción |
| `NS3-T00-F02` | Modelo de etapas / Compilation-stage model | `b6b597709`<br>`docs/3.0/items/NS3-T00-F02-compilation-stage-model.md` | `include/nativesharp/Frontend/CompilationStage.h`<br>`include/nativesharp/Frontend/ProjectCompilation.h` | `tests/unit/NativeSharp3T00ConstitutionTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T00-F03` | Categorías de valores / Value categories | `9218da39f`<br>`docs/3.0/items/NS3-T00-F03-value-categories.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/Stage49RaiiMoveSemanticsTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T00-F04` | Contextos de efectos / Effect contexts | `32748e4e1`<br>`docs/3.0/items/NS3-T00-F04-effect-contexts.md` | `include/nativesharp/Frontend/ProjectMemberModel.h`<br>`include/nativesharp/Frontend/ProjectOopHirModel.h` | `tests/unit/Item321NoHeapContextTests.cpp`<br>`tests/unit/Item322NoHeapPropagationTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T00-F05` | Catálogo estable de diagnósticos / Stable diagnostic catalog | `10bef18b9`<br>`docs/3.0/items/NS3-T00-F05-stable-diagnostic-catalog.md` | `include/nativesharp/Diagnostics/DiagnosticId.h`<br>`src/Diagnostics/DiagnosticRenderer.cpp` | `tests/unit/NativeSharp3T00ConstitutionTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T00-F06` | Harness de conformidad / Conformance harness | `a0a964ecc`<br>`docs/3.0/items/NS3-T00-F06-conformance-harness.md` | `tools/ns3_conformance.py` | — | ANCLADA; capas en F02 |
| `NS3-T01-F01` | Argumentos nombrados / Named arguments | `f237213a2`<br>`docs/3.0/items/NS3-T01-F01-named-arguments.md` | `src/Frontend/ProjectMethodBodyModel.cpp`<br>`src/Frontend/ProjectOopHirModel.cpp` | `tests/unit/NativeSharp3T01NamedArgumentTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T01-F02` | Parámetros variádicos tipados / Typed parameter packs | `4b43fae1f`<br>`docs/3.0/items/NS3-T01-F02-typed-parameter-packs.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T01TypedParameterPackTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T01-F03` | Operadores nulos / Null-aware operators | `5e1696b3e`<br>`docs/3.0/items/NS3-T01-F03-null-aware-operators.md` | `include/nativesharp/Frontend/ProjectOopHirModel.h`<br>`include/nativesharp/Frontend/ProjectOopMirModel.h` | `tests/unit/NativeSharp3T01NullAwareOperatorTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T01-F04` | Operadores de rango e índice / Range and index operators | `6ec07280c`<br>`docs/3.0/items/NS3-T01-F04-range-index.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`include/nativesharp/Frontend/ProjectOopHirModel.h` | `tests/unit/NativeSharp3T01RangeIndexTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T01-F05` | Inicializadores generalizados / Generalized initializers | `f10766de7`<br>`docs/3.0/items/NS3-T01-F05-generalized-initializers.md` | `src/Frontend/ProjectMethodBodyModel.cpp`<br>`src/Frontend/ProjectOopHirModel.cpp` | `tests/unit/NativeSharp3T01GeneralizedInitializerTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T01-F06` | Interpolación estática / Static string interpolation | `d9c160fd2`<br>`docs/3.0/items/NS3-T01-F06-static-interpolation.md` | `include/nativesharp/Language/ExpressionErgonomics.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T01ExpressionTests.cpp`<br>`tests/unit/NativeSharp3T01StaticInterpolationTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T01-F07` | Expresiones with y actualización no destructiva / Non-destructive mutation | `212357a63`<br>`docs/3.0/items/NS3-T01-F07-with-expressions.md` | `src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T01WithExpressionTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T02-F01` | Tipos tupla nativos / Native tuple types | `f85e5a63b`<br>`docs/3.0/items/NS3-T02-F01-native-tuples.md` | `src/Frontend/ProjectMethodBodyModel.cpp`<br>`src/Frontend/ProjectObjectLayoutModel.cpp` | `tests/unit/NativeSharp3T02NativeTupleTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T02-F02` | Literales y conversión de tuplas / Tuple literals and conversions | `dadb94c67`<br>`docs/3.0/items/NS3-T02-F02-tuple-literals.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectConversionClassifier.cpp` | `tests/unit/NativeSharp3T02TupleLiteralTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T02-F03` | Deconstrucción / Deconstruction | `3c3291d5f`<br>`docs/3.0/items/NS3-T02-F03-deconstruction.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`include/nativesharp/Frontend/ProjectOopHirModel.h` | `tests/unit/NativeSharp3T02DeconstructionTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T02-F04` | Registros nativos / Native records | `7c88044ab`<br>`docs/3.0/items/NS3-T02-F04-native-records.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`include/nativesharp/Interop/LibraryMetadata.h` | `tests/fixtures/nsmeta/historical-1.25.nsmeta`<br>`tests/unit/NativeSharp3T02NativeRecordTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T02-F05` | Campos y parámetros promovidos / Promoted fields and parameters | `74ed4d3e0`<br>`docs/3.0/items/NS3-T02-F05-promoted-fields.md` | `src/Parser/Parser.cpp`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T02PromotedFieldTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T02-F06` | Spreads de agregados / Aggregate spreads | `2c117fe15`<br>`docs/3.0/items/NS3-T02-F06-aggregate-spreads.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T02AggregateSpreadTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T03-F01` | Variants etiquetados / Tagged variants | `6f02e2fda`<br>`docs/3.0/items/NS3-T03-F01-tagged-variants.md` | `include/nativesharp/Interop/LibraryMetadata.h`<br>`src/Driver/BreakingChangeLedger.cpp` | `tests/unit/Item402BreakingChangeLedgerTests.cpp`<br>`tests/unit/Item405FinalCompatibilityCertificationTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T03-F02` | Patterns fundamentales / Fundamental patterns | `83b57425c`<br>`docs/3.0/items/NS3-T03-F02-fundamental-patterns.md` | `src/Frontend/ProjectMethodBodyModel.cpp`<br>`src/Parser/Parser.cpp` | `tests/unit/NativeSharp3T03FundamentalPatternTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T03-F03` | Expresión match exhaustiva / Exhaustive match expression | `324c9c4c7`<br>`docs/3.0/items/NS3-T03-F03-exhaustive-match.md` | `include/nativesharp/AST/AstNodeKind.h`<br>`include/nativesharp/Frontend/ProjectMethodBodyModel.h` | `tests/cc1/c59_ns3_variants/Main.ns`<br>`tests/unit/NativeSharp3T03ExhaustiveMatchTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T03-F04` | if let, while let y let else / Conditional patterns | `b7c52d931`<br>`docs/3.0/items/NS3-T03-F04-conditional-patterns.md` | `include/nativesharp/Parser/Parser.h`<br>`src/Parser/Parser.cpp` | `tests/unit/NativeSharp3T03ConditionalPatternTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T03-F05` | Guards y refinamiento / Guards and narrowing | `788464f0d`<br>`docs/3.0/items/NS3-T03-F05-pattern-guards.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T03PatternGuardTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T03-F06` | Variants compatibles con C / C-compatible unions | `e25459e68`<br>`docs/3.0/items/NS3-T03-F06-c-unions.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T03CUnionTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T04-F01` | Constraints declarativos / Declarative constraints | `da2df7709`<br>`docs/3.0/items/NS3-T04-F01-where-constraints.md` | `include/nativesharp/AST/AstNode.h`<br>`include/nativesharp/Frontend/ProjectGenericConstraintModel.h` | `tests/unit/NativeSharp3T04DeclarativeConstraintTests.cpp`<br>`tests/unit/NativeSharp3T04GenericTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T04-F02` | Parámetros const genéricos / Const generics | `3d05018d7`<br>`docs/3.0/items/NS3-T04-F02-const-generics.md` | `include/nativesharp/AST/TypeSyntax.h`<br>`include/nativesharp/Frontend/ProjectGenericModel.h` | `tests/unit/NativeSharp3T04ConstGenericTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T04-F03` | Packs de tipos y valores / Type and value packs | `357339c11`<br>`docs/3.0/items/NS3-T04-F03-generic-packs.md` | `include/nativesharp/Frontend/ProjectGenericModel.h`<br>`src/Frontend/ProjectGenericModel.cpp` | `tests/unit/NativeSharp3T04GenericPackTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T04-F04` | Inferencia bidireccional / Bidirectional inference | `7721c504a`<br>`docs/3.0/items/NS3-T04-F04-bidirectional-inference.md` | `include/nativesharp/Frontend/ProjectGenericModel.h`<br>`src/Frontend/ProjectGenericModel.cpp` | `tests/unit/NativeSharp3T04BidirectionalInferenceTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T04-F05` | Alias de tipo y alias opacos / Type aliases and opaque aliases | `04b6d737f`<br>`docs/3.0/items/NS3-T04-F05-type-aliases.md` | `include/nativesharp/Parser/Parser.h`<br>`src/Frontend/ProjectCompilation.cpp` | `tests/unit/NativeSharp3T04TypeAliasTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T04-F06` | Especialización controlada / Controlled specialization | `c74cebe1f`<br>`docs/3.0/items/NS3-T04-F06-controlled-specialization.md` | `include/nativesharp/Frontend/ProjectGenericConstraintModel.h`<br>`include/nativesharp/Frontend/ProjectGenericModel.h` | `tests/unit/NativeSharp3T04SpecializationTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T04-F07` | Límites de monomorfización / Monomorphization limits | `9e51b1221`<br>`docs/3.0/items/NS3-T04-F07-generic-resource-limits.md` | `include/nativesharp/Compilation/ProjectLoader.h`<br>`include/nativesharp/Frontend/ProjectGenericModel.h` | `tests/unit/NativeSharp3T04GenericResourceLimitTests.cpp`<br>`tests/unit/ProjectLoaderTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T05-F01` | Declaraciones trait / Trait declarations | `f0d4a54b5`<br>`docs/3.0/items/NS3-T05-F01-traits.md` | `include/nativesharp/Frontend/ProjectInterfaceModel.h`<br>`include/nativesharp/Interop/LibraryMetadata.h` | `tests/fixtures/nsmeta/historical-1.26.nsmeta`<br>`tests/unit/NativeSharp3T05TraitDeclarationTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T05-F02` | Implementaciones y coherencia / Implementations and coherence | `531e23cd5`<br>`docs/3.0/items/NS3-T05-F02-impl-coherence.md` | `include/nativesharp/AST/TypeSyntax.h`<br>`include/nativesharp/Frontend/ProjectGenericModel.h` | `tests/unit/NativeSharp3T05ImplCoherenceTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T05-F03` | Tipos y constantes asociados / Associated types and constants | `1da5e26b5`<br>`docs/3.0/items/NS3-T05-F03-associated-items.md` | `include/nativesharp/AST/AstNode.h`<br>`include/nativesharp/AST/AstNodeKind.h` | `tests/fixtures/nsmeta/historical-1.28.nsmeta`<br>`tests/unit/NativeSharp3T05AssociatedItemTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T05-F04` | Existenciales opacos / Opaque result types | `c1a26c9ef`<br>`docs/3.0/items/NS3-T05-F04-opaque-result-types.md` | `include/nativesharp/AST/TypeSyntax.h`<br>`include/nativesharp/Frontend/ProjectGenericModel.h` | `tests/fixtures/nsmeta/historical-1.29.nsmeta`<br>`tests/unit/NativeSharp3T05OpaqueResultTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T05-F05` | Objetos trait explícitos / Explicit trait objects | `3c795b631`<br>`docs/3.0/items/NS3-T05-F05-trait-objects.md` | `include/nativesharp/Frontend/ProjectInterfaceModel.h`<br>`include/nativesharp/Interop/LibraryMetadata.h` | `tests/fixtures/nsmeta/historical-1.30.nsmeta`<br>`tests/unit/Item405FinalCompatibilityCertificationTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T05-F06` | Extensiones estáticas / Extension methods | `c2bc2bb51`<br>`docs/3.0/items/NS3-T05-F06-extension-methods.md` | `include/nativesharp/AST/AstNode.h`<br>`include/nativesharp/AST/AstNodeKind.h` | `tests/cc1/c69_ns3_extension_methods/`<br>`tests/fixtures/nsmeta/historical-1.31.nsmeta` | ANCLADA; capas en F02 |
| `NS3-T05-F07` | Operadores mediante traits / Trait-based operators | `1d6cba266`<br>`docs/3.0/items/NS3-T05-F07-operator-traits.md` | `include/nativesharp/Language/TraitSemantics.h`<br>`src/Frontend/ProjectOopHirModel.cpp` | `tests/cc1/c70_ns3_operator_traits/`<br>`tests/unit/NativeSharp3T05OperatorTraitTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T06-F01` | Tipos move-only y Copy explícito / Move-only types and explicit Copy | `6fbe05efa`<br>`docs/3.0/items/NS3-T06-F01-move-only-defaults.md` | `include/nativesharp/Frontend/ProjectConstructionModel.h`<br>`include/nativesharp/Frontend/ProjectMemberModel.h` | `tests/fixtures/nsmeta/historical-1.32.nsmeta`<br>`tests/unit/NativeSharp3T06MoveOnlyTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T06-F03` | Reborrow y préstamos divididos / Reborrowing and split borrows | `194c4b067`<br>`docs/3.0/items/NS3-T06-F03-reborrow-split.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectCompilation.cpp` | `tests/unit/NativeSharp3T06ReborrowSplitTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T06-F04` | Lifetimes nombrados e inferidos / Named and inferred lifetimes | `5870b9651`<br>`docs/3.0/items/NS3-T06-F04-lifetime-parameters.md` | `include/nativesharp/AST/AstNode.h`<br>`include/nativesharp/AST/TypeSyntax.h` | `tests/fixtures/nsmeta/historical-1.33.nsmeta`<br>`tests/unit/NativeSharp3T06LifetimeParameterTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T06-F05` | Destructores, defer y orden de salida / Destructors, defer, and exit order | `b8ac5c154`<br>`docs/3.0/items/NS3-T06-F05-deterministic-cleanup.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`include/nativesharp/Parser/Parser.h` | `tests/unit/NativeSharp3T06DeterministicCleanupTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T06-F06` | Pinning y direcciones estables / Pinning and stable addresses | `2294876b3`<br>`docs/3.0/items/NS3-T06-F06-pinning.md` | `src/LLVM/ProjectOopLlvmLowerer.cpp`<br>`src/LLVM/ProjectOopMirCacheIdentity.cpp` | `tests/unit/NativeSharp3T06PinningTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T06-F07` | Análisis de escape interprocedural / Interprocedural escape analysis | `9e5691805`<br>`docs/3.0/items/NS3-T06-F07-escape-summaries.md` | `include/nativesharp/Frontend/ProjectCompilation.h`<br>`include/nativesharp/Frontend/ProjectMemberModel.h` | `tests/unit/NativeSharp3T06EscapeSummaryTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T06-F08` | Drop glue genérico e indirecto / Generic and indirect drop glue | `15e32ef52`<br>`docs/3.0/items/NS3-T06-F08-generic-drop-glue.md` | `include/nativesharp/Frontend/ProjectOopMirModel.h`<br>`include/nativesharp/Interop/LibraryMetadata.h` | `tests/unit/NativeSharp3T06DropGlueTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T07-F01` | Protocolo Result y operador ? / Result protocol and propagation | `bf66f2d84`<br>`docs/3.0/items/NS3-T07-F01-result-propagation.md` | `include/nativesharp/AST/AstNodeKind.h`<br>`include/nativesharp/Frontend/ProjectMethodBodyModel.h` | `tests/unit/NativeSharp3T07ErrorTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T07-F02` | Funciones throws nativas / Native throwing functions | `71156c746`<br>`docs/3.0/items/NS3-T07-F02-typed-throws.md` | `include/nativesharp/AST/AstNode.h`<br>`include/nativesharp/Frontend/ProjectMemberModel.h` | `tests/unit/Item405FinalCompatibilityCertificationTests.cpp`<br>`tests/unit/NativeSharp3T06DropGlueTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T07-F03` | try, catch tipado y finally / Typed try-catch-finally | `79d47da4f`<br>`docs/3.0/items/NS3-T07-F03-typed-catch.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T07ErrorTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T07-F04` | Expresiones divergentes y tipo never / Diverging expressions and never type | `940663118`<br>`docs/3.0/items/NS3-T07-F04-never-type.md` | `include/nativesharp/Frontend/ProjectTypeConversion.h`<br>`src/Frontend/ProjectMethodBodyModel.cpp` | `tests/unit/NativeSharp3T07ErrorTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T07-F05` | Loop expressions y break con valor / Loop expressions | `55117494e`<br>`docs/3.0/items/NS3-T07-F05-loop-expressions.md` | `include/nativesharp/AST/AstNodeKind.h`<br>`include/nativesharp/Frontend/ProjectMethodBodyModel.h` | `tests/unit/NativeSharp3T07ErrorTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T07-F06` | Assertions y panic profiles / Assertions and panic profiles | `b35821604`<br>`docs/3.0/items/NS3-T07-F06-assertions-panic.md` | `include/nativesharp/Compilation/Project.h`<br>`include/nativesharp/Frontend/ProjectMethodBodyModel.h` | `tests/unit/NativeSharp3T07ErrorTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T08-F01` | Tipos función completos / Complete function types | `006afa22d`<br>`docs/3.0/items/NS3-T08-F01-function-types.md` | `include/nativesharp/AST/TypeSyntax.h`<br>`include/nativesharp/Frontend/ProjectTypeConversion.h` | `tests/unit/NativeSharp3T08CallableTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T08-F02` | Lambdas sin captura / Non-capturing lambdas | `7909c46eb`<br>`docs/3.0/items/NS3-T08-F02-noncapturing-lambdas.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`include/nativesharp/Language/CallableSemantics.h` | `tests/unit/NativeSharp3T08CallableTests.cpp` | ANCLADA; capas en F02 |
| `NS3-T08-F03` | Closures con captura explícita / Explicit-capture closures | `4eba3e07d`<br>`docs/3.0/items/NS3-T08-F03-explicit-capture-closures.md` | `include/nativesharp/Frontend/ProjectMethodBodyModel.h`<br>`include/nativesharp/Frontend/ProjectOopHirModel.h` | `tests/unit/NativeSharp3T08ClosureCaptureTests.cpp` | ANCLADA; capas en F02 |

## Contradicción registrada

`NS3-T00-F01` no conserva su vertical original en la revisión actual. El cierre
histórico `e0be188d4` introdujo `LanguageEdition`, `language = "2" | "3"`,
`NS3001`/`NS3002` y transporte por NSMETA. El commit posterior
`0bea623fbf108aa1336f89586d0bf347287812e9` eliminó deliberadamente
`include/nativesharp/Language/LanguageEdition.h` y
`src/Language/LanguageEdition.cpp`, neutralizó los manifests de edición y adoptó
semántica acumulativa. Las rutas aún existentes de loader/conformance son
consumidores históricos, no la vertical retirada. Por tanto:

- estado histórico: `TERMINADA` en la fuente 3.0;
- estado actual: `IMPLEMENTACIÓN RETIRADA`;
- tratamiento en 3.0.4: contradicción contractual que F02/F03 debe resolver,
  no trabajo ya hecho ni feature vigente.

## Gate F01

```text
inventariados = 55
clasificados = 55
trazados = 55
omitidos = 0
desconocidos = 0
contradicciones_sin_registrar = 0
```

Siguiente acción determinista: `NS304-T00-F02`, clasificación exhaustiva de cada
vertical por parser, semántica, HIR/MIR, LLVM, NSMETA, runtime y ejecución.
