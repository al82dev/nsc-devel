# SelfHost.IR — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 843 líneas, 46690 bytes, 110 declaraciones físicas.
- **Diseño:** 12 contratos y 92 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Materializar HIR y MIR/CFG, verificar cleanup de ownership y control, calcular dominancia y admitir sólo programas validados.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Syntax;SelfHost.Sema;SelfHost.Types;SelfHost.Symbols.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000599 | `selfhost/nsc/IR/IntermediateRepresentation.ns` | 843 | 110 |

## 4. Contratos

### 4.1 CSH-IR-001 — HIR record and module storage

- **Símbolos:** NativeSharp.Compiler.IR.HirRecord;NativeSharp.Compiler.IR.HirRecord.HirRecord;NativeSharp.Compiler.IR.HirRecord.Kind;NativeSharp.Compiler.IR.HirRecord.Text;NativeSharp.Compiler.IR.HirRecord.SyntaxId;NativeSharp.Compiler.IR.HirRecord.TypeId;NativeSharp.Compiler.IR.HirRecord.SymbolId;NativeSharp.Compiler.IR.HirRecord.Depth;NativeSharp.Compiler.IR.HirRecord.Operation;NativeSharp.Compiler.IR.HirRecord.Conversions;NativeSharp.Compiler.IR.HirRecord.BoundsCheck;NativeSharp.Compiler.IR.HirModule;NativeSharp.Compiler.IR.HirModule.HirModule;NativeSharp.Compiler.IR.HirModule.Add;NativeSharp.Compiler.IR.HirModule.RecordAt;NativeSharp.Compiler.IR.HirModule.Count;NativeSharp.Compiler.IR.HirModule.AddError;NativeSharp.Compiler.IR.HirModule.ErrorCount;NativeSharp.Compiler.IR.FlowEdge.Source;NativeSharp.Compiler.IR.FlowEdge.Target;NativeSharp.Compiler.IR.FlowEdge.Kind;NativeSharp.Compiler.IR.FlowBlock.Id;NativeSharp.Compiler.IR.FlowBlock.Label;NativeSharp.Compiler.IR.FlowBlock.OperationCount;NativeSharp.Compiler.IR.FlowBlock.OperationAt;NativeSharp.Compiler.IR.FlowBlock.AddOperation;NativeSharp.Compiler.IR.FlowBlock.Terminated;NativeSharp.Compiler.IR.FlowBlock.Terminate;NativeSharp.Compiler.IR.FlowBlock.Reachable;NativeSharp.Compiler.IR.FlowFunction.Name;NativeSharp.Compiler.IR.FlowFunction.SymbolId;NativeSharp.Compiler.IR.FlowFunction.BlockCount;NativeSharp.Compiler.IR.FlowFunction.BlockAt;NativeSharp.Compiler.IR.FlowFunction.EdgeCount;NativeSharp.Compiler.IR.FlowFunction.EdgeAt;NativeSharp.Compiler.IR.FlowFunction.LocalCount;NativeSharp.Compiler.IR.FlowFunction.LocalAt;NativeSharp.Compiler.IR.FlowFunction.LocalTypeAt;NativeSharp.Compiler.IR.FlowFunction.Current;NativeSharp.Compiler.IR.FlowFunction.AddBlock;NativeSharp.Compiler.IR.FlowFunction.Select;NativeSharp.Compiler.IR.FlowFunction.AddEdge;NativeSharp.Compiler.IR.FlowFunction.AddOperation;NativeSharp.Compiler.IR.FlowFunction.Terminate;NativeSharp.Compiler.IR.FlowFunction.AddLocal;NativeSharp.Compiler.IR.FlowFunction.MarkAddressed;NativeSharp.Compiler.IR.FlowFunction.IsAddressed;NativeSharp.Compiler.IR.FlowFunction.PredecessorCount;NativeSharp.Compiler.IR.FlowFunction.PredecessorAt;NativeSharp.Compiler.IR.FlowFunction.SuccessorCount;NativeSharp.Compiler.IR.FlowModule.Add;NativeSharp.Compiler.IR.FlowModule.FunctionCount;NativeSharp.Compiler.IR.FlowModule.FunctionAt;NativeSharp.Compiler.IR.FlowModule.AddError;NativeSharp.Compiler.IR.FlowModule.ErrorCount;NativeSharp.Compiler.IR.OwnershipCleanupVerifier.FirstProblem;NativeSharp.Compiler.IR.DominanceInfo.Count;NativeSharp.Compiler.IR.DominanceInfo.Idom;NativeSharp.Compiler.IR.DominanceInfo.SetIdom;NativeSharp.Compiler.IR.DominanceInfo.Dominates;NativeSharp.Compiler.IR.DominanceInfo.SetDominates;NativeSharp.Compiler.IR.ValidatedIrProgram.Valid;NativeSharp.Compiler.IR.ValidatedIrProgram.Syntax;NativeSharp.Compiler.IR.ValidatedIrProgram.Semantic;NativeSharp.Compiler.IR.ValidatedIrProgram.Flow.
- **Entradas:** kind text syntax/type/symbol IDs depth conversions and bounds flag.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered HIR record sequence and error count; ordered HIR record sequence and error count are coherent and observable.
- **Invariante:** records preserve source order and semantic provenance.
- **Errores:** invalid data is detected by verifier not silently rewritten.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact records order fields and count.

### 4.2 CSH-IR-002 — HIR construction

- **Símbolos:** NativeSharp.Compiler.IR.HirBuilder;NativeSharp.Compiler.IR.HirBuilder.LowerKind;NativeSharp.Compiler.IR.HirBuilder.Visit;NativeSharp.Compiler.IR.HirBuilder.Build;NativeSharp.Compiler.IR.MirBuilder.Build;NativeSharp.Compiler.IR.DominanceBuilder.Build.
- **Entradas:** syntax tree and semantic model.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** HIR module; HIR module are coherent and observable.
- **Invariante:** each visited syntax node yields ordered source-derived records.
- **Errores:** invalid child or missing semantic fact increments owned failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** independent walk of node/record provenance.

### 4.3 CSH-IR-003 — HIR verification

- **Símbolos:** NativeSharp.Compiler.IR.HirVerifier.
- **Entradas:** HIR module and source models.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** verification result; verification result are coherent and observable.
- **Invariante:** IDs depths operations conversions and bounds facts are coherent.
- **Errores:** first structural inconsistency rejects the module.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** independent invariant checker result.

### 4.4 CSH-IR-004 — Flow block edge and function model

- **Símbolos:** NativeSharp.Compiler.IR.FlowEdge;NativeSharp.Compiler.IR.FlowEdge.FlowEdge;NativeSharp.Compiler.IR.FlowBlock;NativeSharp.Compiler.IR.FlowBlock.FlowBlock;NativeSharp.Compiler.IR.FlowFunction;NativeSharp.Compiler.IR.FlowFunction.FlowFunction;NativeSharp.Compiler.IR.FlowModule;NativeSharp.Compiler.IR.FlowModule.FlowModule.
- **Entradas:** blocks operations edges locals and symbols.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** mutable flow graph model; mutable flow graph model are coherent and observable.
- **Invariante:** block IDs indexes endpoints termination and local tables agree.
- **Errores:** invalid selection or endpoint remains observable to verifier.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** node/edge/local tables and mutations.

### 4.5 CSH-IR-005 — MIR expression lowering

- **Símbolos:** NativeSharp.Compiler.IR.MirBuilder.Describe;NativeSharp.Compiler.IR.MirBuilder.EmitExpression.
- **Entradas:** HIR/syntax expression and current flow block.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** MIR operations and values; MIR operations and values are coherent and observable.
- **Invariante:** evaluation order and source provenance are preserved.
- **Errores:** unsupported or invalid expression records failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** operation sequence and result description.

### 4.6 CSH-IR-006 — MIR control-flow lowering

- **Símbolos:** NativeSharp.Compiler.IR.MirBuilder;NativeSharp.Compiler.IR.MirBuilder.Jump;NativeSharp.Compiler.IR.MirBuilder.LowerBlock;NativeSharp.Compiler.IR.MirBuilder.CollectFunctions.
- **Entradas:** syntax bodies semantic model.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** functions blocks edges and terminators; functions blocks edges and terminators are coherent and observable.
- **Invariante:** short-circuit loops branches and returns form coherent CFG.
- **Errores:** failed child or malformed control transfer rejects the flow module.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** independent CFG reconstruction and block operations.

### 4.7 CSH-IR-007 — Ownership cleanup insertion

- **Símbolos:** NativeSharp.Compiler.IR.MirBuilder.AddOwnershipCleanup;NativeSharp.Compiler.IR.OwnershipCleanupVerifier.
- **Entradas:** scope exits locals and ownership facts.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered cleanup operations or first problem; ordered cleanup operations or first problem are coherent and observable.
- **Invariante:** owned values clean exactly once in reverse acquisition order.
- **Errores:** missing duplicate or misordered cleanup is rejected.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** cleanup identities order and verifier problem.

### 4.8 CSH-IR-008 — Flow graph verification

- **Símbolos:** NativeSharp.Compiler.IR.FlowVerifier.
- **Entradas:** flow module.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** verification result; verification result are coherent and observable.
- **Invariante:** all blocks edges terminals and symbol references are coherent.
- **Errores:** first structural problem rejects acceptance.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** independent graph invariant result.

### 4.9 CSH-IR-009 — Dominance fixed-point construction

- **Símbolos:** NativeSharp.Compiler.IR.DominanceInfo;NativeSharp.Compiler.IR.DominanceInfo.DominanceInfo;NativeSharp.Compiler.IR.DominanceBuilder.
- **Entradas:** verified CFG.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** dominance sets and immediate dominators; dominance sets and immediate dominators are coherent and observable.
- **Invariante:** entry dominates reachable blocks and fixed point is stable.
- **Errores:** invalid predecessor graph prevents accepted dominance.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** independent dominator oracle and iteration stability.

### 4.10 CSH-IR-010 — HIR MIR CFG dominance and SSA dumps

- **Símbolos:** NativeSharp.Compiler.IR.IrDumper;NativeSharp.Compiler.IR.IrDumper.Indent;NativeSharp.Compiler.IR.IrDumper.Hir;NativeSharp.Compiler.IR.IrDumper.Mir;NativeSharp.Compiler.IR.IrDumper.Cfg;NativeSharp.Compiler.IR.IrDumper.Dominance;NativeSharp.Compiler.IR.IrDumper.Ssa;NativeSharp.Compiler.IR.ValidatedIrProgram.Hir.
- **Entradas:** validated IR models.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** deterministic textual dump; deterministic textual dump are coherent and observable.
- **Invariante:** ordering follows stable function/block/value ordinals.
- **Errores:** none beyond invalid input precondition.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact normalized structural text.

### 4.11 CSH-IR-011 — Optimization pass pipeline description

- **Símbolos:** NativeSharp.Compiler.IR.IrDumper.PassPipeline.
- **Entradas:** O0 O1 O2 or O3.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered pass description; ordered pass description are coherent and observable.
- **Invariante:** each level has a stable declared pass sequence.
- **Errores:** unknown level follows documented default branch.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact pass list by level.

### 4.12 CSH-IR-012 — IR admission lifecycle

- **Símbolos:** NativeSharp.Compiler.IR.HirVerifier.Verify;NativeSharp.Compiler.IR.FlowVerifier.Verify;NativeSharp.Compiler.IR.OwnershipCleanupVerifier.Verify;NativeSharp.Compiler.IR.IrPipeline;NativeSharp.Compiler.IR.IrPipeline.Prepare;NativeSharp.Compiler.IR.IrPipeline.Dump;NativeSharp.Compiler.IR.IrPipeline.Verify;NativeSharp.Compiler.IR.ValidatedIrProgram;NativeSharp.Compiler.IR.ValidatedIrProgram.ValidatedIrProgram.
- **Entradas:** syntax semantic model and optimization level.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** valid HIR/flow bundle or failure; valid HIR/flow bundle or failure are coherent and observable.
- **Invariante:** no backend observes IR until every producer and verifier accepts it.
- **Errores:** producer/verifier error blocks construction of a valid handle.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** valid flag model identities dumps and first failure.

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

La integración directa es: SelfHost.Syntax;SelfHost.Sema;SelfHost.Types;SelfHost.Symbols. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 110 declaraciones, 12 contratos y 92 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
