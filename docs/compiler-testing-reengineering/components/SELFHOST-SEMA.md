# SelfHost.Sema — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 7 archivos, 2049 líneas, 118956 bytes, 163 declaraciones físicas.
- **Diseño:** 20 contratos y 153 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Declarar, ligar y tipar el programa; cerrar unidades/módulos; validar ownership, escapes y no-heap; exponer grafos semánticos deterministas.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Syntax;SelfHost.Types;SelfHost.Symbols;SelfHost.Projects.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000609 | `selfhost/nsc/Sema/CallableGraph.ns` | 53 | 6 |
| PF-000610 | `selfhost/nsc/Sema/ExceptionRegionGraph.ns` | 29 | 4 |
| PF-000611 | `selfhost/nsc/Sema/GenericInstantiationSet.ns` | 79 | 8 |
| PF-000612 | `selfhost/nsc/Sema/ModuleSet.ns` | 603 | 56 |
| PF-000613 | `selfhost/nsc/Sema/SemanticAnalyzer.ns` | 1032 | 33 |
| PF-000614 | `selfhost/nsc/Sema/SemanticDumper.ns` | 133 | 7 |
| PF-000615 | `selfhost/nsc/Sema/SemanticModel.ns` | 120 | 49 |

## 4. Contratos

### 4.1 CSH-SEM-001 — Semantic facts conversions and model storage

- **Símbolos:** NativeSharp.Compiler.Sema.CallableGraph.Add;NativeSharp.Compiler.Sema.CallableGraph.Visit;NativeSharp.Compiler.Sema.ExceptionRegionGraph.Visit;NativeSharp.Compiler.Sema.GenericInstantiationSet.Count;NativeSharp.Compiler.Sema.GenericInstantiationSet.At;NativeSharp.Compiler.Sema.GenericInstantiationSet.Add;NativeSharp.Compiler.Sema.GenericInstantiationSet.Visit;NativeSharp.Compiler.Sema.ModuleExport.Kind;NativeSharp.Compiler.Sema.ModuleExport.Name;NativeSharp.Compiler.Sema.ModuleExport.QualifiedName;NativeSharp.Compiler.Sema.ModuleExport.ParentQualifiedName;NativeSharp.Compiler.Sema.ModuleExport.TypeDisplay;NativeSharp.Compiler.Sema.ModuleExport.Unit;NativeSharp.Compiler.Sema.ModuleExport.IsType;NativeSharp.Compiler.Sema.ModuleExport.IsCallable;NativeSharp.Compiler.Sema.ModuleIndex.Count;NativeSharp.Compiler.Sema.ModuleIndex.At;NativeSharp.Compiler.Sema.ModuleIndex.ProblemCount;NativeSharp.Compiler.Sema.ModuleIndex.ProblemAt;NativeSharp.Compiler.Sema.ModuleIndex.Report;NativeSharp.Compiler.Sema.ModuleIndex.FindType;NativeSharp.Compiler.Sema.ModuleIndex.FindCallable;NativeSharp.Compiler.Sema.ModuleIndex.Add;NativeSharp.Compiler.Sema.ModuleIndex.OverloadCount;NativeSharp.Compiler.Sema.ModuleUnit.Owner;NativeSharp.Compiler.Sema.ModuleUnit.RelativePath;NativeSharp.Compiler.Sema.ModuleUnit.Order;NativeSharp.Compiler.Sema.ModuleUnit.Tree;NativeSharp.Compiler.Sema.ModuleUnit.LexErrors;NativeSharp.Compiler.Sema.ModuleUnit.ParseErrors;NativeSharp.Compiler.Sema.ModuleUnit.FrontEndErrors;NativeSharp.Compiler.Sema.ModuleUnit.ExternalResolutionCount;NativeSharp.Compiler.Sema.ModuleUnit.SetModel;NativeSharp.Compiler.Sema.ModuleSet.UnitCount;NativeSharp.Compiler.Sema.ModuleSet.UnitAt;NativeSharp.Compiler.Sema.ModuleSet.Index;NativeSharp.Compiler.Sema.ModuleSet.ExternalProjectCount;NativeSharp.Compiler.Sema.ModuleSet.ExternalProjectAt;NativeSharp.Compiler.Sema.ModuleSet.ExternalMetadataCount;NativeSharp.Compiler.Sema.ModuleSet.Diagnostic;NativeSharp.Compiler.Sema.ModuleSet.Failed;NativeSharp.Compiler.Sema.ModuleSet.ExternalResolutionCount;NativeSharp.Compiler.Sema.ModuleSet.Fail;NativeSharp.Compiler.Sema.SemanticAnalyzer;NativeSharp.Compiler.Sema.SemanticAnalyzer.SemanticAnalyzer;NativeSharp.Compiler.Sema.SemanticDumper.Indent;NativeSharp.Compiler.Sema.ValueCategory;NativeSharp.Compiler.Sema.SemanticConversion;NativeSharp.Compiler.Sema.SemanticConversion.SemanticConversion;NativeSharp.Compiler.Sema.SemanticConversion.Kind;NativeSharp.Compiler.Sema.SemanticConversion.SourceType;NativeSharp.Compiler.Sema.SemanticConversion.DestinationType;NativeSharp.Compiler.Sema.SemanticConversion.OperandIndex;NativeSharp.Compiler.Sema.SemanticConversion.Role;NativeSharp.Compiler.Sema.SemanticFact;NativeSharp.Compiler.Sema.SemanticFact.SemanticFact;NativeSharp.Compiler.Sema.SemanticFact.Present;NativeSharp.Compiler.Sema.SemanticFact.TypeId;NativeSharp.Compiler.Sema.SemanticFact.Category;NativeSharp.Compiler.Sema.SemanticFact.Modifiable;NativeSharp.Compiler.Sema.SemanticFact.Addressable;NativeSharp.Compiler.Sema.SemanticFact.BoundsCheck;NativeSharp.Compiler.Sema.SemanticFact.SymbolId;NativeSharp.Compiler.Sema.SemanticFact.IsConstant;NativeSharp.Compiler.Sema.SemanticFact.ConstantText;NativeSharp.Compiler.Sema.SemanticFact.HasOperator;NativeSharp.Compiler.Sema.SemanticFact.OperatorKind;NativeSharp.Compiler.Sema.SemanticFact.OperationType;NativeSharp.Compiler.Sema.SemanticFact.ResultType;NativeSharp.Compiler.Sema.SemanticFact.ConversionCount;NativeSharp.Compiler.Sema.SemanticFact.ConversionAt;NativeSharp.Compiler.Sema.SemanticFact.SetType;NativeSharp.Compiler.Sema.SemanticFact.Hide;NativeSharp.Compiler.Sema.SemanticFact.SetStorage;NativeSharp.Compiler.Sema.SemanticFact.SetBoundsCheck;NativeSharp.Compiler.Sema.SemanticFact.SetSymbol;NativeSharp.Compiler.Sema.SemanticFact.SetConstant;NativeSharp.Compiler.Sema.SemanticFact.SetOperator;NativeSharp.Compiler.Sema.SemanticModel;NativeSharp.Compiler.Sema.SemanticModel.SemanticModel;NativeSharp.Compiler.Sema.SemanticModel.Types;NativeSharp.Compiler.Sema.SemanticModel.AddSymbol;NativeSharp.Compiler.Sema.SemanticModel.GetSymbol;NativeSharp.Compiler.Sema.SemanticModel.SymbolCount;NativeSharp.Compiler.Sema.SemanticModel.AddGlobal;NativeSharp.Compiler.Sema.SemanticModel.GlobalCount;NativeSharp.Compiler.Sema.SemanticModel.GlobalAt;NativeSharp.Compiler.Sema.SemanticModel.AddScope;NativeSharp.Compiler.Sema.SemanticModel.GetScope;NativeSharp.Compiler.Sema.SemanticModel.Fact;NativeSharp.Compiler.Sema.SemanticModel.BindNode;NativeSharp.Compiler.Sema.SemanticModel.NodeSymbol;NativeSharp.Compiler.Sema.SemanticModel.AddError;NativeSharp.Compiler.Sema.SemanticModel.ErrorCount.
- **Entradas:** types symbols scopes node bindings conversions and errors.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** mutable semantic model with per-node facts; mutable semantic model with per-node facts are coherent and observable.
- **Invariante:** fact/symbol/scope indexes remain coherent and Hide removes only presence.
- **Errores:** invalid indexes remain caller errors; analyzer owns semantic diagnostics.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** fields conversions bindings counts and error transitions.

### 4.2 CSH-SEM-002 — Callable graph construction and dump

- **Símbolos:** NativeSharp.Compiler.Sema.CallableGraph;NativeSharp.Compiler.Sema.CallableGraph.CallableGraph.
- **Entradas:** syntax tree semantic callable bindings.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered callable dependency graph and dump; ordered callable dependency graph and dump are coherent and observable.
- **Invariante:** visited set terminates cycles and each callable appears once.
- **Errores:** unresolved callable remains absent or analyzer-owned error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** nodes edges order and text.

### 4.3 CSH-SEM-003 — Exception region graph

- **Símbolos:** NativeSharp.Compiler.Sema.ExceptionRegionGraph;NativeSharp.Compiler.Sema.ExceptionRegionGraph.ExceptionRegionGraph.
- **Entradas:** syntax tree try/catch/finally nodes.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered nested exception region report; ordered nested exception region report are coherent and observable.
- **Invariante:** region nesting and source order are preserved.
- **Errores:** malformed syntax is rejected before this consumer.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** region hierarchy and dump.

### 4.4 CSH-SEM-004 — Generic instantiation closure

- **Símbolos:** NativeSharp.Compiler.Sema.GenericInstantiationSet;NativeSharp.Compiler.Sema.GenericInstantiationSet.GenericInstantiationSet.
- **Entradas:** syntax and semantic generic applications.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** deduplicated ordered instantiation set and dump; deduplicated ordered instantiation set and dump are coherent and observable.
- **Invariante:** same closed instantiation is recorded once and traversal terminates.
- **Errores:** open/unresolved instantiations do not become accepted closed entries.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** count identities ordering and dump.

### 4.5 CSH-SEM-005 — Module export identity and classification

- **Símbolos:** NativeSharp.Compiler.Sema.ModuleExport;NativeSharp.Compiler.Sema.ModuleExport.ModuleExport.
- **Entradas:** symbol kind names parent type and unit.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** classified module export; classified module export are coherent and observable.
- **Invariante:** type/callable predicates agree with symbol kind.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact fields and predicate truth table.

### 4.6 CSH-SEM-006 — Module index insertion lookup and overloads

- **Símbolos:** NativeSharp.Compiler.Sema.ModuleIndex;NativeSharp.Compiler.Sema.ModuleIndex.ModuleIndex.
- **Entradas:** module exports and queries.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable lookup/overload results and problems; stable lookup/overload results and problems are coherent and observable.
- **Invariante:** qualified identity is unique while callable overloads retain declaration order.
- **Errores:** duplicates/conflicts become indexed problems.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** index rows lookups overload counts and problems.

### 4.7 CSH-SEM-007 — Module unit lifecycle and external resolution

- **Símbolos:** NativeSharp.Compiler.Sema.ModuleUnit;NativeSharp.Compiler.Sema.ModuleUnit.ModuleUnit.
- **Entradas:** source unit tree lexer/parser/model state.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** analyzed module-unit record; analyzed module-unit record are coherent and observable.
- **Invariante:** model publication changes analyzed/front-end error state coherently.
- **Errores:** lex parse or sema failure remains attributable to unit.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** state transitions counts model and errors.

### 4.8 CSH-SEM-008 — External project and NSMETA loading

- **Símbolos:** NativeSharp.Compiler.Sema.ModuleSet.NoteExternalProject;NativeSharp.Compiler.Sema.ModuleSet.MetadataKind;NativeSharp.Compiler.Sema.ModuleSet.MetadataLine;NativeSharp.Compiler.Sema.ModuleSet.LoadExternalMetadata.
- **Entradas:** project references and NSMETA/1 or NSMETA/2 text.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** external projects metadata and exports; external projects metadata and exports are coherent and observable.
- **Invariante:** only declared compatible formats contribute public symbols.
- **Errores:** missing malformed or unsupported metadata fails module set.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** external counts parsed records exports and diagnostic.

### 4.9 CSH-SEM-009 — Module symbol collection and two-pass build

- **Símbolos:** NativeSharp.Compiler.Sema.CallableGraph.Build;NativeSharp.Compiler.Sema.GenericInstantiationSet.Build;NativeSharp.Compiler.Sema.ModuleSet;NativeSharp.Compiler.Sema.ModuleSet.ModuleSet;NativeSharp.Compiler.Sema.ModuleSet.Collect;NativeSharp.Compiler.Sema.ModuleSet.CollectSymbol;NativeSharp.Compiler.Sema.ModuleSet.Build.
- **Entradas:** SourceGraph and external metadata.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered units global index and analyzed models; ordered units global index and analyzed models are coherent and observable.
- **Invariante:** all units declare before cross-unit injection and analysis; order is deterministic.
- **Errores:** any unit or external producer failure marks set failed.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** unit states index exports resolutions and diagnostic.

### 4.10 CSH-SEM-010 — Module and export dumps

- **Símbolos:** NativeSharp.Compiler.Sema.CallableGraph.Dump;NativeSharp.Compiler.Sema.ExceptionRegionGraph.Dump;NativeSharp.Compiler.Sema.GenericInstantiationSet.Dump;NativeSharp.Compiler.Sema.ModuleSetDumper;NativeSharp.Compiler.Sema.ModuleSetDumper.Dump;NativeSharp.Compiler.Sema.ModuleSetDumper.DumpExports;NativeSharp.Compiler.Sema.SemanticDumper.Dump.
- **Entradas:** ModuleSet and index.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable module/export text; stable module/export text are coherent and observable.
- **Invariante:** dump order follows unit/export identity and does not mutate model.
- **Errores:** none beyond admitted set.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact normalized report.

### 4.11 CSH-SEM-011 — Scope lookup declaration and parent attachment

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.Lookup;NativeSharp.Compiler.Sema.SemanticAnalyzer.LookupGlobal;NativeSharp.Compiler.Sema.SemanticAnalyzer.Declare;NativeSharp.Compiler.Sema.SemanticAnalyzer.AttachToParent.
- **Entradas:** scope symbol name and parent.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** resolved or declared symbol relation; resolved or declared symbol relation are coherent and observable.
- **Invariante:** nearest scope wins and parent/member relations are bidirectional.
- **Errores:** duplicate declaration increments semantic errors.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** selected symbol scope/member tables and errors.

### 4.12 CSH-SEM-012 — Type lookup generic substitution and member resolution

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.TypeSymbol;NativeSharp.Compiler.Sema.SemanticAnalyzer.GenericArgument;NativeSharp.Compiler.Sema.SemanticAnalyzer.ValueGenericArgument;NativeSharp.Compiler.Sema.SemanticAnalyzer.SubstituteMemberType;NativeSharp.Compiler.Sema.SemanticAnalyzer.LookupMember.
- **Entradas:** type text receiver generic context and member name.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** semantic type/symbol identity; semantic type/symbol identity are coherent and observable.
- **Invariante:** closed substitution is deterministic and preserves member owner.
- **Errores:** unknown/open/incompatible type or member reports semantic error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** type display symbol and conversion facts.

### 4.13 CSH-SEM-013 — Overload resolution

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.SelectCallOverload.
- **Entradas:** candidate set argument types and receiver.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** selected callable and conversions; selected callable and conversions are coherent and observable.
- **Invariante:** best viable score wins and ambiguity is not silently ordered away.
- **Errores:** no viable or ambiguous call reports semantic error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** candidate scores selection conversions and diagnostic.

### 4.14 CSH-SEM-014 — Aggregate enum function and global declaration

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.Align;NativeSharp.Compiler.Sema.SemanticAnalyzer.AuxiliaryValue;NativeSharp.Compiler.Sema.SemanticAnalyzer.DeclareAggregate;NativeSharp.Compiler.Sema.SemanticAnalyzer.DeclareEnum;NativeSharp.Compiler.Sema.SemanticAnalyzer.DeclareFunction;NativeSharp.Compiler.Sema.SemanticAnalyzer.DeclareGlobalNode.
- **Entradas:** declaration syntax and current scope.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** symbols layouts signatures and constants; symbols layouts signatures and constants are coherent and observable.
- **Invariante:** sizes alignments offsets parents and definitions are coherent before bodies.
- **Errores:** duplicates invalid layout/type/constant record semantic errors.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** symbol table layout signature constants and errors.

### 4.15 CSH-SEM-015 — Literal numeric conversion and operator typing

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.CommonNumeric;NativeSharp.Compiler.Sema.SemanticAnalyzer.Comparison;NativeSharp.Compiler.Sema.SemanticAnalyzer.AddConversion;NativeSharp.Compiler.Sema.SemanticAnalyzer.NumericConversion;NativeSharp.Compiler.Sema.SemanticAnalyzer.AnalyzeLiteral;NativeSharp.Compiler.Sema.SemanticFact.AddConversion.
- **Entradas:** literal/operator operand types.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** semantic fact operation/result types and conversions; semantic fact operation/result types and conversions are coherent and observable.
- **Invariante:** numeric promotion yields one common physical semantic type.
- **Errores:** invalid literal or conversion reports semantic error.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** fact types constants conversion list and error.

### 4.16 CSH-SEM-016 — Expression semantic analysis

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.AnalyzeExpression.
- **Entradas:** expression syntax scopes and expected type.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** bound symbol type category storage and conversions; bound symbol type category storage and conversions are coherent and observable.
- **Invariante:** each accepted expression has a present coherent fact and ownership category.
- **Errores:** invalid operand call member cast allocation or condition reports owned error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** semantic fact symbol and diagnostic per expression.

### 4.17 CSH-SEM-017 — Statement and callable body analysis

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.AnalyzeStatement;NativeSharp.Compiler.Sema.SemanticAnalyzer.AnalyzeFunction.
- **Entradas:** statement/body syntax callable scope and return type.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** scopes facts symbols and control diagnostics; scopes facts symbols and control diagnostics are coherent and observable.
- **Invariante:** locals and parameters remain scoped and returns agree with callable result.
- **Errores:** invalid control return exception or declaration reports semantic error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** scopes facts and diagnostics in source order.

### 4.18 CSH-SEM-018 — Escape and no-heap validation

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticAnalyzer.FirstStorageSymbol;NativeSharp.Compiler.Sema.SemanticAnalyzer.ValidateEscapes;NativeSharp.Compiler.Sema.SemanticAnalyzer.ValidateNoHeap.
- **Entradas:** analyzed body ownership/storage graph and noheap mode.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** validation result and diagnostics; validation result and diagnostics are coherent and observable.
- **Invariante:** borrowed/owned values do not escape forbidden lifetime and noheap paths allocate nothing.
- **Errores:** first prohibited escape/allocation records semantic failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** storage origin traversal and diagnostic location.

### 4.19 CSH-SEM-019 — Declaration-only external and full analysis modes

- **Símbolos:** NativeSharp.Compiler.Sema.ModuleUnit.Model;NativeSharp.Compiler.Sema.ModuleUnit.Analyzed;NativeSharp.Compiler.Sema.SemanticAnalyzer.DeclareOnly;NativeSharp.Compiler.Sema.SemanticAnalyzer.Model;NativeSharp.Compiler.Sema.SemanticAnalyzer.DeclareExternal;NativeSharp.Compiler.Sema.SemanticAnalyzer.Analyze.
- **Entradas:** syntax tree optional external exports and analysis mode.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** semantic model; semantic model are coherent and observable.
- **Invariante:** declaration pass precedes body pass and external exports enter only through explicit injection.
- **Errores:** declaration/external/body error prevents successful model.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** symbol/scope/fact counts exports and errors.

### 4.20 CSH-SEM-020 — Semantic symbol node and metadata dumps

- **Símbolos:** NativeSharp.Compiler.Sema.SemanticDumper;NativeSharp.Compiler.Sema.SemanticDumper.DumpSymbol;NativeSharp.Compiler.Sema.SemanticDumper.DumpSymbols;NativeSharp.Compiler.Sema.SemanticDumper.DumpNode;NativeSharp.Compiler.Sema.SemanticDumper.DumpMetadata.
- **Entradas:** syntax and semantic model.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** deterministic semantic/metadata text; deterministic semantic/metadata text are coherent and observable.
- **Invariante:** stable IDs and source order determine output.
- **Errores:** none beyond admitted model.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact normalized structural dump.

## 5. Estructuras, estados y algoritmos

Las estructuras concretas son las declaraciones trazadas en `inventory/symbols.csv`. Los algoritmos se dividen por contrato para impedir que una prueba de integración oculte una falla de cursor, identidad, grafo, ownership, ABI o publicación. Los builders conservan estado sólo dentro de su owner; los modelos aceptados se publican después de sus verificadores.

## 6. Invariantes y transiciones

El estado inicial, las mutaciones admitidas, la publicación y el fallo se prueban por contrato. Los contratos de valor son puros; los de builder, grafo, pipeline, proceso o emisión comprueban transiciones y ausencia de éxito parcial.

## 7. Ownership y lifetime

Los tipos `owned`, `raii`, handles, arenas, listas y punteros se caracterizan según la implementación congelada. Una prueba no debe forzar un feature de ownership ajeno al contrato ni convertir una observación actual en política futura del lenguaje.

## 8. Complejidad y límites

Las búsquedas lineales, recorridos de árboles/grafos, cierres y pipelines se ejercitan con vacío, caso nominal y escala. La Fase 1 no promete cotas no declaradas: cualquier recorrido recursivo vigente queda como riesgo observable y contradicción, no como refactor autorizado.

## 9. Errores y recuperación

El oracle identifica categoría, propietario, progreso y publicación. Parser/Lexer deben avanzar o terminar; builders y pipelines detienen consumidores; diagnósticos conservan identidad; emisores no publican éxito si un productor falla.

## 10. Integración

La integración directa es: SelfHost.Syntax;SelfHost.Types;SelfHost.Symbols;SelfHost.Projects. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 7 archivos, 163 declaraciones, 20 contratos y 153 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
