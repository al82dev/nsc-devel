# SelfHost.CodeGen — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 4038 líneas, 264715 bytes, 122 declaraciones físicas.
- **Diseño:** 19 contratos y 136 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Bajar IR y hechos semánticos self-host validados a LLVM 23, verificar el módulo y emitir objeto o assembly Windows x64.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.IR;SelfHost.Sema;NativeSharp.LLVM.C;Windows x64 ABI.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000596 | `selfhost/nsc/CodeGen/LlvmBackend.ns` | 4038 | 122 |

## 4. Contratos

### 4.1 CSH-CG-001 — Function-local and loop lowering state

- **Símbolos:** NativeSharp.Compiler.CodeGen.BackendArtifactKind;NativeSharp.Compiler.CodeGen.BackendOptimization;NativeSharp.Compiler.CodeGen.LocalBinding;NativeSharp.Compiler.CodeGen.LocalBinding.LocalBinding;NativeSharp.Compiler.CodeGen.LocalBinding.Symbol;NativeSharp.Compiler.CodeGen.LocalBinding.Name;NativeSharp.Compiler.CodeGen.LocalBinding.Storage;NativeSharp.Compiler.CodeGen.LocalBinding.ValueType;NativeSharp.Compiler.CodeGen.LoopBinding;NativeSharp.Compiler.CodeGen.LoopBinding.LoopBinding;NativeSharp.Compiler.CodeGen.LoopBinding.HasBreak;NativeSharp.Compiler.CodeGen.FunctionLoweringState;NativeSharp.Compiler.CodeGen.FunctionLoweringState.FunctionLoweringState;NativeSharp.Compiler.CodeGen.FunctionLoweringState.SetThis;NativeSharp.Compiler.CodeGen.FunctionLoweringState.ThisValue;NativeSharp.Compiler.CodeGen.FunctionLoweringState.AddLocal;NativeSharp.Compiler.CodeGen.LlvmBackend;NativeSharp.Compiler.CodeGen.LlvmBackend.ParameterCount;NativeSharp.Compiler.CodeGen.LlvmBackend.ClosedSpanElementSize.
- **Entradas:** symbol/name storage type and loop targets.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** lookup receiver loop targets and break state; lookup receiver loop targets and break state are coherent and observable.
- **Invariante:** nearest binding and innermost loop win; pop restores previous state.
- **Errores:** missing binding yields null without inventing storage.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** lookup precedence push/pop and break-state transitions.

### 4.2 CSH-CG-002 — Entry discovery and reachability closure

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.FindEntryExpression;NativeSharp.Compiler.CodeGen.LlvmBackend.EntryExpression;NativeSharp.Compiler.CodeGen.LlvmBackend.MarkReachableFunction;NativeSharp.Compiler.CodeGen.LlvmBackend.OpenGenericFunction;NativeSharp.Compiler.CodeGen.LlvmBackend.MarkLibraryFunctions;NativeSharp.Compiler.CodeGen.LlvmBackend.MarkReachableNode;NativeSharp.Compiler.CodeGen.LlvmBackend.BackendEligibleFunction;NativeSharp.Compiler.CodeGen.LlvmBackend.BackendDeclarableFunction.
- **Entradas:** syntax tree symbols and executable/library mode.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** marked reachable declarations and entry expression; marked reachable declarations and entry expression are coherent and observable.
- **Invariante:** closure is deterministic and cycles terminate through reachability marks.
- **Errores:** missing executable Main or unsupported declaration is rejected at emission.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** reachable symbol set and selected entry.

### 4.3 CSH-CG-003 — Semantic type and closed-generic lowering

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.SupportedScalar;NativeSharp.Compiler.CodeGen.LlvmBackend.IntegerEnum;NativeSharp.Compiler.CodeGen.LlvmBackend.FindTypeSymbol;NativeSharp.Compiler.CodeGen.LlvmBackend.FindTypeSymbolByDisplay;NativeSharp.Compiler.CodeGen.LlvmBackend.ClosedGenericArgument;NativeSharp.Compiler.CodeGen.LlvmBackend.ScalarType;NativeSharp.Compiler.CodeGen.LlvmBackend.SemanticValueType;NativeSharp.Compiler.CodeGen.LlvmBackend.GenericPlaceholder;NativeSharp.Compiler.CodeGen.LlvmBackend.PhysicalValueScalar.
- **Entradas:** semantic type display and closed generic context.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** LLVM physical type or unsupported result; LLVM physical type or unsupported result are coherent and observable.
- **Invariante:** same semantic/ABI context maps to one physical representation.
- **Errores:** unknown unsupported or open type returns no LLVM type.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** LLVM kind width element and aggregate representation.

### 4.4 CSH-CG-004 — Win64 ABI signatures and aggregates

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.AggregateSize;NativeSharp.Compiler.CodeGen.LlvmBackend.EnumSize;NativeSharp.Compiler.CodeGen.LlvmBackend.DirectAggregateSize;NativeSharp.Compiler.CodeGen.LlvmBackend.DirectAggregateType;NativeSharp.Compiler.CodeGen.LlvmBackend.AbiValueType;NativeSharp.Compiler.CodeGen.LlvmBackend.ParameterDeclarationAt;NativeSharp.Compiler.CodeGen.LlvmBackend.HasReceiver;NativeSharp.Compiler.CodeGen.LlvmBackend.UsesNativeAbi;NativeSharp.Compiler.CodeGen.LlvmBackend.ScalarFunctionType;NativeSharp.Compiler.CodeGen.LlvmBackend.DelegateFunctionType.
- **Entradas:** function declaration semantic model and aggregate layout.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** physical function signature and passing mode; physical function signature and passing mode are coherent and observable.
- **Invariante:** receiver and direct aggregate rules match declaration and calling convention.
- **Errores:** incomplete layout or signature prevents declaration.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** parameter count types receiver return and call convention.

### 4.5 CSH-CG-005 — Stable LLVM names and declarations

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.LlvmFunctionName;NativeSharp.Compiler.CodeGen.LlvmBackend.LlvmGlobalName;NativeSharp.Compiler.CodeGen.LlvmBackend.ConstantInitializer;NativeSharp.Compiler.CodeGen.LlvmBackend.DeclareData;NativeSharp.Compiler.CodeGen.LlvmBackend.DeclareFunctions.
- **Entradas:** symbols syntax declarations and constants.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** named LLVM globals and functions; named LLVM globals and functions are coherent and observable.
- **Invariante:** names are stable and declarations are unique per semantic identity.
- **Errores:** duplicate incompatible or unrepresentable declaration fails before bodies.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** exact names linkage initializers and declaration set.

### 4.6 CSH-CG-006 — Callable lookup and overload selection

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.FindFunctionDeclaration;NativeSharp.Compiler.CodeGen.LlvmBackend.FindFunctionDeclarationByArity;NativeSharp.Compiler.CodeGen.LlvmBackend.FindDelegateDeclaration;NativeSharp.Compiler.CodeGen.LlvmBackend.ExternDelegateType;NativeSharp.Compiler.CodeGen.LlvmBackend.RawFunctionReference;NativeSharp.Compiler.CodeGen.LlvmBackend.FindBestOverload;NativeSharp.Compiler.CodeGen.LlvmBackend.DestructorDeclaration.
- **Entradas:** callee name arity arguments parent and delegate context.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** selected declaration or unresolved result; selected declaration or unresolved result are coherent and observable.
- **Invariante:** selection follows semantic identity before fallback search and preserves arity.
- **Errores:** ambiguous absent or incompatible callable remains unresolved.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** selected symbol signature and unresolved diagnostics.

### 4.7 CSH-CG-007 — Scalar conversion and operation normalization

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.IntegerBits;NativeSharp.Compiler.CodeGen.LlvmBackend.SemanticIntegerBits;NativeSharp.Compiler.CodeGen.LlvmBackend.UnsignedInteger;NativeSharp.Compiler.CodeGen.LlvmBackend.FloatingScalar;NativeSharp.Compiler.CodeGen.LlvmBackend.GenericResultPosition;NativeSharp.Compiler.CodeGen.LlvmBackend.BoxGenericScalar;NativeSharp.Compiler.CodeGen.LlvmBackend.ConvertScalar;NativeSharp.Compiler.CodeGen.LlvmBackend.ApplyConversion;NativeSharp.Compiler.CodeGen.LlvmBackend.SamePhysicalType.
- **Entradas:** source value semantic conversion fact and destination type.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** LLVM value with destination physical type; LLVM value with destination physical type are coherent and observable.
- **Invariante:** both operands of a physical operation have identical LLVM types.
- **Errores:** unsupported narrowing boxing or mismatched physical types fail locally.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** opcode source/destination widths signedness and result type.

### 4.8 CSH-CG-008 — Memory contracts and bit-fields

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.BitfieldStorageType;NativeSharp.Compiler.CodeGen.LlvmBackend.ByteOffset;NativeSharp.Compiler.CodeGen.LlvmBackend.LValueSymbol;NativeSharp.Compiler.CodeGen.LlvmBackend.ApplyMemoryContract;NativeSharp.Compiler.CodeGen.LlvmBackend.LoadMemory;NativeSharp.Compiler.CodeGen.LlvmBackend.StoreMemory.
- **Entradas:** address physical type symbol flags and value.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** loaded or stored value with volatile/atomic/bit-field policy; loaded or stored value with volatile/atomic/bit-field policy are coherent and observable.
- **Invariante:** symbol memory policy is applied symmetrically to load and store.
- **Errores:** invalid address layout or conversion publishes no instruction result.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** instruction kind ordering alignment mask and stored bytes.

### 4.9 CSH-CG-009 — Native string allocation copy move and comparison

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.StringType;NativeSharp.Compiler.CodeGen.LlvmBackend.StringIndexExpression;NativeSharp.Compiler.CodeGen.LlvmBackend.StringExpression;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitGlobalString;NativeSharp.Compiler.CodeGen.LlvmBackend.AllocateNativeString;NativeSharp.Compiler.CodeGen.LlvmBackend.CopyNativeBytes;NativeSharp.Compiler.CodeGen.LlvmBackend.MoveNativeBytes;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitClosedSpanCopy;NativeSharp.Compiler.CodeGen.LlvmBackend.CompareNativeStrings.
- **Entradas:** string or byte spans lengths and allocation context.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** native string storage copy/move result or equality; native string storage copy/move result or equality are coherent and observable.
- **Invariante:** length header and byte region remain coherent and copies respect overlap policy.
- **Errores:** allocation or operand failure returns no accepted value.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** length bytes allocation calls and comparison result.

### 4.10 CSH-CG-010 — Runtime hooks core intrinsics and atomics

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.RuntimeHook;NativeSharp.Compiler.CodeGen.LlvmBackend.AtomicOrderingFor;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitCoreIntrinsic.
- **Entradas:** recognized core call arguments and memory order.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** runtime call or intrinsic LLVM value; runtime call or intrinsic LLVM value are coherent and observable.
- **Invariante:** only named intrinsic shapes bypass ordinary call lowering.
- **Errores:** unrecognized calls remain unhandled; malformed recognized calls fail.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** selected hook signature opcode ordering and result.

### 4.11 CSH-CG-011 — Managed and native delegate lowering

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.EmitManagedDelegateCell;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitManagedDelegateCall.
- **Entradas:** code pointer receiver delegate declaration and arguments.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** delegate cell or direct/indirect call; delegate cell or direct/indirect call are coherent and observable.
- **Invariante:** managed cells preserve code receiver destroy/free slots; extern delegates stay raw.
- **Errores:** missing target receiver or signature fails without fabricated call.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** cell layout call branch arguments and result phi.

### 4.12 CSH-CG-012 — L-value address lowering

- **Símbolos:** NativeSharp.Compiler.CodeGen.FunctionLoweringState.FindLocal;NativeSharp.Compiler.CodeGen.FunctionLoweringState.FindLocalType;NativeSharp.Compiler.CodeGen.FunctionLoweringState.FindLocalByName;NativeSharp.Compiler.CodeGen.FunctionLoweringState.FindLocalTypeByName;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitLValue.
- **Entradas:** identifier dereference index or member expression.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** address element type and validity; address element type and validity are coherent and observable.
- **Invariante:** address denotes the semantic storage and indirect parameters are not double-loaded.
- **Errores:** unbound storage invalid index receiver or field layout returns invalid.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** address provenance GEP offsets and value type.

### 4.13 CSH-CG-013 — Scalar expression lowering

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.EmitIntegerOperation;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitFloatingOperation;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitScalarExpression;NativeSharp.Compiler.CodeGen.LlvmBackend.Comparison;NativeSharp.Compiler.CodeGen.LlvmBackend.ComparisonPredicate;NativeSharp.Compiler.CodeGen.LlvmBackend.FloatingComparisonPredicate.
- **Entradas:** typed expression tree and lowering state.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** LLVM scalar/address/delegate value; LLVM scalar/address/delegate value are coherent and observable.
- **Invariante:** result physical type agrees with semantic fact after conversion.
- **Errores:** operand conversion call or allocation failure stops the expression.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** opcode constants branches calls memory and final LLVM type.

### 4.14 CSH-CG-014 — Condition and short-circuit lowering

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.IsAlwaysTrue;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitCondition.
- **Entradas:** condition or logical expression.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** i1 condition and CFG blocks; i1 condition and CFG blocks are coherent and observable.
- **Invariante:** right side executes only on the required path and phi inputs match predecessors.
- **Errores:** invalid operand or block construction rejects the condition.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** CFG edges phi values and side-effect reachability.

### 4.15 CSH-CG-015 — Inline assembly lowering

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.InlineAssemblyTemplate;NativeSharp.Compiler.CodeGen.LlvmBackend.InlineAssemblyConstraints;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitInlineAssembly.
- **Entradas:** assembly template operands constraints clobbers and volatility.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** LLVM inline-asm call and output stores; LLVM inline-asm call and output stores are coherent and observable.
- **Invariante:** operand order and constraint positions remain aligned.
- **Errores:** invalid input output l-value or physical type fails locally.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** template constraint string call convention side effects and outputs.

### 4.16 CSH-CG-016 — Statement and structured control lowering

- **Símbolos:** NativeSharp.Compiler.CodeGen.LoopBinding.BreakTarget;NativeSharp.Compiler.CodeGen.LoopBinding.ContinueTarget;NativeSharp.Compiler.CodeGen.LoopBinding.MarkBreak;NativeSharp.Compiler.CodeGen.FunctionLoweringState.PushLoop;NativeSharp.Compiler.CodeGen.FunctionLoweringState.PopLoop;NativeSharp.Compiler.CodeGen.FunctionLoweringState.BreakTarget;NativeSharp.Compiler.CodeGen.FunctionLoweringState.ContinueTarget;NativeSharp.Compiler.CodeGen.FunctionLoweringState.MarkBreak;NativeSharp.Compiler.CodeGen.FunctionLoweringState.CurrentLoopHasBreak;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitEntryAlloca;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitStatement.
- **Entradas:** statement tree function and lowering state.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** terminated or fallthrough CFG with stores/calls; terminated or fallthrough CFG with stores/calls are coherent and observable.
- **Invariante:** each emitted block has coherent termination and loop targets are properly nested.
- **Errores:** child lowering or termination failure rejects the function body.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** block/edge sequence terminators return values and cleanup calls.

### 4.17 CSH-CG-017 — Function body emission and LLVM verification

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.LoweringFailure;NativeSharp.Compiler.CodeGen.LlvmBackend.ReturnExpression;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitFunctions.
- **Entradas:** declared reachable functions semantic model and builder.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** verified LLVM function bodies; verified LLVM function bodies are coherent and observable.
- **Invariante:** every eligible body terminates and verifies before module acceptance.
- **Errores:** first body or verifier failure stops traversal with named diagnostic.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** function bodies verifier result and diagnostic identity.

### 4.18 CSH-CG-018 — Optimization and target configuration

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.CodeGenLevel;NativeSharp.Compiler.CodeGen.LlvmBackend.PassPipeline.
- **Entradas:** O0 O1 O2 or O3.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** target codegen level and pass pipeline; target codegen level and pass pipeline are coherent and observable.
- **Invariante:** each optimization selects its declared fixed pipeline.
- **Errores:** unknown enum follows documented default branch.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact level and pipeline text.

### 4.19 CSH-CG-019 — COFF or assembly object emission lifecycle

- **Símbolos:** NativeSharp.Compiler.CodeGen.LlvmBackend.Dispose;NativeSharp.Compiler.CodeGen.LlvmBackend.PrintLlvmMessage;NativeSharp.Compiler.CodeGen.LlvmBackend.EmitExecutableObject.
- **Entradas:** validated IR output path optimization and artifact kind.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** object/assembly file or stable backend exit code; object/assembly file or stable backend exit code are coherent and observable.
- **Invariante:** LLVM major 23 target/data-layout verify/pass/debug/emission stages occur in order and resources are disposed.
- **Errores:** stage-specific codes 1..16 or 90 identify the first failed producer.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** exit code artifact kind module verification and emitted file structure.

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

La integración directa es: SelfHost.IR;SelfHost.Sema;NativeSharp.LLVM.C;Windows x64 ABI. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 122 declaraciones, 19 contratos y 136 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
