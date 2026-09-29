# Tanda operativa 00 — Auditoría real del trabajo NativeSharp 3.0 T00–T17

> **Fecha:** 29 de septiembre de 2026
>
> **Rama:** `development`
>
> **Tipo de entrega:** auditoría documental y reclasificación; no cambia código del compilador
>
> **Fuente maestra:** `A:\DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md`
>
> **Tracking:** `A:\NATIVESHARP-3.0-TRACKING.md`

## Objetivo

Determinar qué existe realmente detrás de los commits `ns3(t00)` a `ns3(t17)` y dejar de usar el
nombre del commit o su prueba aislada como evidencia de que una feature está terminada.

La auditoría clasifica cada pieza como:

- **ADOPTAR:** implementación o contrato aprovechable sin duplicar arquitectura;
- **INTEGRAR:** pieza útil que todavía debe conectarse al pipeline productivo;
- **REESCRIBIR:** prototipo que expresa intención, pero cuya forma actual no puede ser la
  implementación definitiva;
- **ELIMINAR DESPUÉS:** duplicado que sólo se retira cuando el reemplazo vertical esté integrado y
  sus pruebas cubran el mismo contrato.

No se eliminó código durante esta tanda. La preservación permite usar los prototipos como oráculos y
evita confundir limpieza con progreso.

## Evidencia objetiva

### Historia y alcance de commits

| Tanda | Commit | Archivos | Alcance observado |
|---|---|---:|---|
| T00 | `df6cb1045` | 30 | edición, diagnósticos, proyectos, stages, effects, test y harness |
| T01 | `fdf0628bf` | 19 | lexer/parser/AST y contratos de expresiones |
| T02 | `afee1c71e` | 8 | tuple/spread en parser/AST y helper de agregados |
| T03 | `fefa12f15` | 5 | helper de sum types + test aislado |
| T04 | `3349cc424` | 5 | helper de genéricos + test aislado |
| T05 | `a910a6d93` | 5 | helper de traits + test aislado |
| T06 | `3569b1e68` | 5 | helper de ownership + test aislado |
| T07 | `e1e0fdac0` | 5 | helper de errores + test aislado |
| T08 | `b501312e7` | 5 | helper de callables + test aislado |
| T09 | `24bc04680` | 5 | helper de iteración + test aislado |
| T10 | `e433efb9f` | 5 | helper de coroutines + test aislado |
| T11 | `45e2a9027` | 5 | helper de metaprogramación + test aislado |
| T12 | `1f9f53b4d` | 5 | helper de módulos + test aislado |
| T13 | `196b721dd` | 5 | helper ABI + test aislado |
| T14 | `ed0375b6b` | 5 | helper numérico + test aislado |
| T15 | `2a105b2a1` | 5 | helper de gate + test que compone helpers C++ |
| T16 | `4c00cdf3c` | 5 | helper de compatibilidad + test aislado |
| T17 | `daacac963` | 5 | helper de release + test aislado |

Los commits están contenidos en `development` mediante el merge `90674fea6`. Su presencia en la
rama de desarrollo no los convierte en producto certificado.

### Referencias productivas

Los siguientes módulos no tienen consumidores productivos fuera de su propio header/source, prueba,
CMake y documento:

```text
AggregateSemantics
SumTypeSemantics
GenericSemantics
TraitSemantics
OwnershipSemantics
ErrorSemantics
CallableSemantics
IterationSemantics
CoroutineSemantics
MetaprogrammingSemantics
ModuleSemantics
AbiSemantics
NumericSemantics
IntegrationQuality
CompatibilitySemantics
ReleaseSemantics
```

Por tanto, sus funciones prueban modelos C++ locales; no demuestran que `nsc` acepte, analice,
baje, emita, publique o depure las construcciones correspondientes.

### Nodos sintácticos sin lowering completo

Estos nodos aparecen en AST/parser/tests, pero no en HIR, MIR o LLVM:

```text
NullCoalescingExpression
NullConditionalMemberExpression
RangeExpression
FromEndExpression
SpreadElementExpression
InterpolatedStringExpression
WithExpression
TupleExpression
```

Esto demuestra trabajo sintáctico aprovechable, no una feature de lenguaje terminada.

### Constitución T00

- `LanguageEdition` sí está conectado con proyecto, loader, frontend, syntax unit y parser.
- `CompilationStageTracker` sólo aparece en su header/source y prueba T00.
- `EffectContext` sólo aparece en su header, catálogo diagnóstico y prueba T00.
- Los diagnostics ampliados son infraestructura aprovechable, pero todavía no existe un catálogo
  3.0 completo usado por todas las reglas.
- El harness de conformidad es una base, no una suite normativa T00–T17.

## Clasificación por tanda

### T00 — Constitución

**ADOPTAR**

- `LanguageEdition.h/.cpp` y su selección explícita desde `.nsproj`.
- Cambios de `Project`, `ProjectLoader`, graph/dump y frontend que transportan la edición.
- Estructuras de diagnóstico con IDs, severity, ranges y renderer.
- `tools/ns3_conformance.py` como base del harness.
- La prueba T00 como unit test de contratos ya presentes.

**INTEGRAR**

- `CompilationStageTracker` dentro de la orquestación real.
- `EffectContext` dentro de binding, tipos función, HIR, MIR y metadata.
- categorías de valor de `ExpressionInfo` dentro de flow/ownership/lowering.
- códigos NS3xxx dentro de diagnósticos reales de cada vertical.
- edición dentro de cache keys, NSMETA, LSP y consumidores precompilados.

**REESCRIBIR**

- Ningún archivo completo se condena todavía; se reescribirán las partes que dupliquen estados ya
  existentes cuando T00 se conecte al pipeline único.

### T01 — Expresiones

**ADOPTAR**

- tokens, lexer, `AstNodeKind`, parser y edition gating que preservan nodos distintos;
- nombres/dumps sintácticos y pruebas de parse existentes.

**INTEGRAR**

- resolución de argumentos nombrados y params con el overload resolver productivo;
- nodos null/range/index/interpolation/with con binding, tipos, flow y diagnostics;
- lowering HIR/MIR, evaluación única, cleanup y backend;
- contratos `noheap`, metadata, LSP y debugger.

**REESCRIBIR**

- cualquier consulta de `ExpressionErgonomics` que duplique información que debe pertenecer al
  typed semantic model.

### T02 — Tuplas, deconstrucción y records

**ADOPTAR**

- `TupleExpression` y `SpreadElementExpression` como sintaxis separada;
- parser y pruebas sintácticas existentes.

**INTEGRAR**

- identidad, conversiones y layout dentro del type system y ABI classifier reales;
- deconstruction binding, records sintetizados, promoted fields y spreads;
- moves parciales, drop flags, NSMETA, PDB/debugger y consumers precompilados.

**REESCRIBIR**

- `AggregateSemantics` como helper aislado. Sus algoritmos sirven de oráculo, pero la fuente de
  verdad debe vivir en tipos, sema, HIR/MIR, ABI y metadata del compilador.

### T03 — Sum types y patterns

- **ADOPTAR:** documento y asserts del test como intención verificable.
- **REESCRIBIR:** `SumTypeSemantics` dentro de parser, type system, flow, ownership, MIR, ABI,
  NSMETA y debugger.
- **FALTA:** syntax real, exhaustiveness productiva, decision tree, ejecución y C-union witness.

### T04 — Genéricos

- **ADOPTAR:** casos de normalización/budget del test como semillas.
- **REESCRIBIR:** `GenericSemantics` dentro del solver, inference e instantiation graph reales.
- **FALTA:** const generics/packs/aliases/specialization source, monomorphization y NSMETA.

### T05 — Traits

- **ADOPTAR:** reglas de coherence/identity como casos de diseño.
- **REESCRIBIR:** `TraitSemantics` dentro de lookup, constraints, dispatch y metadata.
- **FALTA:** syntax, impl selection, associated items, dyn ABI, LSP y debugger.

### T06 — Ownership

- **ADOPTAR:** estados y asserts como oráculo de flow.
- **REESCRIBIR:** `OwnershipSemantics` sobre CFG/MIR reales, no como state machine paralela.
- **FALTA:** source rules, borrow checking, partial moves, cleanup edges, drop glue genérico,
  metadata y DLL witness.

### T07 — Errores

- **ADOPTAR:** contratos de Result/throws/panic como diseño.
- **REESCRIBIR:** `ErrorSemantics` dentro de types/effects/MIR EH y Win64 funclets.
- **FALTA:** syntax, typed catches, cleanup, NSMETA, ambos linkers y debugger.

### T08 — Callables

- **ADOPTAR:** identidad/representación del helper como casos de ABI.
- **REESCRIBIR:** `CallableSemantics` dentro de tipos función, closures, lowering y metadata.
- **FALTA:** syntax completa, capture analysis, closure layout/drop, callbacks y debugger.

### T09 — Iteración

- **ADOPTAR:** reglas de laziness/fusion como casos de prueba.
- **REESCRIBIR:** `IterationSemantics` sobre traits, borrow checker y generator MIR.
- **FALTA:** foreach/ranges/comprehensions/yield reales y consumers E021–E026.

### T10 — Coroutines

- **ADOPTAR:** estados y memory-order assertions como semillas.
- **REESCRIBIR:** `CoroutineSemantics` dentro de async lowering, pinning, ownership y atomics.
- **FALTA:** source async/await, frames, cancel, Send/Sync, logical debug stack y T10-F08.

### T11 — Metaprogramación

- **ADOPTAR:** capability/budget model como diseño.
- **REESCRIBIR:** `MetaprogrammingSemantics` dentro del evaluator/harness/cache reales.
- **FALTA:** introspection, attributes, derive, hygienic macro, sandbox y source mapping.

### T12 — Módulos

- **ADOPTAR:** canonicalization/fingerprint cases como semillas.
- **REESCRIBIR:** `ModuleSemantics` dentro de graph, lookup, visibility y NSMETA.
- **FALTA:** imports/reexports/deprecation, API diff y T12-F07.

### T13 — ABI/FFI

- **ADOPTAR:** asserts Win64 como corpus inicial.
- **REESCRIBIR:** `AbiSemantics` dentro del layout/call classifier y metadata reales.
- **FALTA:** attributes/bitfields/pointers/calling conventions/unsafe/FFI/SIMD source y T13-F08.

### T14 — Numéricos

- **ADOPTAR:** reglas de overflow/conversion/FP como casos diferenciales.
- **REESCRIBIR:** `NumericSemantics` dentro de types, consteval, MIR y LLVM flags.
- **FALTA:** syntax/context, runtime parity, units consumer y O-level equivalence.

### T15 — Integración

- **ADOPTAR:** categorías de calidad y costo como requisitos.
- **REESCRIBIR:** `IntegrationQuality` como harness que compile programas `.ns` reales y lea
  evidencia de compiler/tooling; no debe componer helpers C++.
- **FALTA:** interaction matrix, whole-program corpus, budgets, debugger y cost audit reales.

### T16 — Compatibilidad

- **ADOPTAR:** categorías source/ABI y catálogo MIG como diseño.
- **REESCRIBIR:** `CompatibilitySemantics` como diagnostics/fix-its/diff/migration ejecutables.
- **FALTA:** consumir artefactos 2.x, migrar proyectos y comprobar la matriz N/N-1.

### T17 — Freeze y promoción

- **ADOPTAR:** fail-closed checklist como requisito de release.
- **REESCRIBIR:** `ReleaseSemantics` como gate que consuma manifests, resultados y hashes reales.
- **FALTA:** freeze, suite candidata, paridad ES/EN, RC1/RC2, baseline de Alberto y promoción.

## Política de eliminación

No existe una eliminación inmediata autorizada por esta auditoría. Un helper aislado se marca
`ELIMINAR DESPUÉS` sólo cuando:

1. el pipeline productivo cubre su contrato;
2. sus asserts fueron trasladados a pruebas de lenguaje/IR/ABI reales;
3. no tiene consumidores;
4. el commit de retiro construye O0 y pasa pruebas dirigidas;
5. su expediente explica el reemplazo.

Esto evita mantener dos fuentes semánticas sin perder prematuramente las decisiones útiles.

## Decisión arquitectónica

El trabajo existente no justifica continuar T01–T17 como tandas horizontales. La ejecución cambia a
tarjetas verticales. Cada feature siguiente debe atravesar, según aplique:

```text
syntax -> binding/types -> HIR -> MIR -> LLVM -> ABI/NSMETA -> tooling -> consumer
```

Antes de abrir nueva sintaxis se completan P00–P07 y T00 transversal. T02-F01 (tupla nativa) queda
como candidata del primer piloto vertical porque ya posee sintaxis parcial y exige demostrar layout,
retorno Win64, metadata y debugger.

## Archivos creados y modificados

### Nuevo

- `docs/3.0/TANDA-00-AUDITORIA-REAL-T00-T17.md`: evidencia, clasificación y decisión.

### Modificados

- Los expedientes T00–T17 reciben un aviso de reclasificación enlazado a esta auditoría.
- `A:\NATIVESHARP-3.0-TRACKING.md` registra cierre, pruebas y commit.
- `A:\DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md` conserva el ledger maestro.

No se modificó código C++, CMake ni pruebas en esta tanda.

## Verificación de la tanda

- Auditoría de los 18 commits y su lista exacta de archivos.
- Búsqueda de referencias productivas de los 16 helpers.
- Búsqueda de consumidores HIR/MIR/LLVM de los ocho nodos AST nuevos.
- Comprobación de que `development` contiene el trabajo mediante el merge histórico.
- Build dirigido `cmake --build A:\nsc-devel\build-clang --target nsc --config Debug --parallel 12`:
  correcto; `nsc.exe` y contratos generados sin error.
- `nsc.ns3.t00.constitution`: correcto, 0.10 s.
- `nsc.ns3.t00.conformance-harness`: correcto, 0.52 s.
- Selección dirigida completa: 2/2 pruebas correctas en 0.63 s.
- Baseline integral: **no ejecutado y prohibido durante las tandas**.
