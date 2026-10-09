# SelfHost.Syntax — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 170 líneas, 7905 bytes, 50 declaraciones físicas.
- **Diseño:** 5 contratos y 36 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Poseer nodos y relaciones indexadas, congelar el árbol y producir un dump estructural determinista.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Basic;SelfHost.Lexer.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000619 | `selfhost/nsc/Syntax/SyntaxTree.ns` | 170 | 50 |

## 4. Contratos

### 4.1 CSH-SYN-001 — Syntax node construction and scalar fields

- **Símbolos:** NativeSharp.Compiler.Syntax.SyntaxNode;NativeSharp.Compiler.Syntax.SyntaxNode.SyntaxNode;NativeSharp.Compiler.Syntax.SyntaxNode.Kind;NativeSharp.Compiler.Syntax.SyntaxNode.Text;NativeSharp.Compiler.Syntax.SyntaxNode.Auxiliary;NativeSharp.Compiler.Syntax.SyntaxNode.TypeText;NativeSharp.Compiler.Syntax.SyntaxNode.SemanticTypeText;NativeSharp.Compiler.Syntax.SyntaxNode.GenericText;NativeSharp.Compiler.Syntax.SyntaxNode.ConstraintsText;NativeSharp.Compiler.Syntax.SyntaxNode.ModifiersText;NativeSharp.Compiler.Syntax.SyntaxNode.ThrowsTypeText;NativeSharp.Compiler.Syntax.SyntaxNode.OperatorKind;NativeSharp.Compiler.Syntax.SyntaxNode.Flag;NativeSharp.Compiler.Syntax.SyntaxNode.IsReference;NativeSharp.Compiler.Syntax.SyntaxNode.IsVariadic;NativeSharp.Compiler.Syntax.SyntaxNode.SetText;NativeSharp.Compiler.Syntax.SyntaxNode.SetAuxiliary;NativeSharp.Compiler.Syntax.SyntaxNode.SetType;NativeSharp.Compiler.Syntax.SyntaxNode.SetSemanticType;NativeSharp.Compiler.Syntax.SyntaxNode.SetGenericText;NativeSharp.Compiler.Syntax.SyntaxNode.SetConstraintsText;NativeSharp.Compiler.Syntax.SyntaxNode.SetModifiersText;NativeSharp.Compiler.Syntax.SyntaxNode.SetThrowsTypeText;NativeSharp.Compiler.Syntax.SyntaxNode.SetOperator;NativeSharp.Compiler.Syntax.SyntaxNode.SetFlag;NativeSharp.Compiler.Syntax.SyntaxNode.SetReference;NativeSharp.Compiler.Syntax.SyntaxNode.SetVariadic.
- **Entradas:** node kind and parsed metadata.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** syntax node value; syntax node value are coherent and observable.
- **Invariante:** getters exactly reflect constructor/setter state.
- **Errores:** mutation after freeze is prohibited by tree contract.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** all field values before/after setter.

### 4.2 CSH-SYN-002 — Syntax node relationships

- **Símbolos:** NativeSharp.Compiler.Syntax.SyntaxNode.First;NativeSharp.Compiler.Syntax.SyntaxNode.Second;NativeSharp.Compiler.Syntax.SyntaxNode.Third;NativeSharp.Compiler.Syntax.SyntaxNode.ChildCount;NativeSharp.Compiler.Syntax.SyntaxNode.ChildAt;NativeSharp.Compiler.Syntax.SyntaxNode.SetFirst;NativeSharp.Compiler.Syntax.SyntaxNode.SetSecond;NativeSharp.Compiler.Syntax.SyntaxNode.SetThird;NativeSharp.Compiler.Syntax.SyntaxNode.AddChild.
- **Entradas:** node IDs and ordered children.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable relationship graph; stable relationship graph are coherent and observable.
- **Invariante:** fixed slots and child vector retain independent ordered identities.
- **Errores:** invalid IDs are detected by later verifier/analyzer.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** slots children order and count.

### 4.3 CSH-SYN-003 — Syntax node and tree freeze

- **Símbolos:** NativeSharp.Compiler.Syntax.SyntaxNode.Freeze;NativeSharp.Compiler.Syntax.SyntaxTree.Freeze;NativeSharp.Compiler.Syntax.SyntaxTree.IsFrozen.
- **Entradas:** mutable node/tree.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** immutable frozen state; immutable frozen state are coherent and observable.
- **Invariante:** freeze is monotone and accepted tree structure no longer changes.
- **Errores:** post-freeze mutation follows documented rejection/no-op.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** state transition and structural hash.

### 4.4 CSH-SYN-004 — Syntax tree node arena and root

- **Símbolos:** NativeSharp.Compiler.Syntax.SyntaxTree;NativeSharp.Compiler.Syntax.SyntaxTree.SyntaxTree;NativeSharp.Compiler.Syntax.SyntaxTree.Create;NativeSharp.Compiler.Syntax.SyntaxTree.Node;NativeSharp.Compiler.Syntax.SyntaxTree.Count;NativeSharp.Compiler.Syntax.SyntaxTree.Root;NativeSharp.Compiler.Syntax.SyntaxTree.SetRoot.
- **Entradas:** nodes and root ID.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** indexed syntax tree; indexed syntax tree are coherent and observable.
- **Invariante:** IDs are insertion-stable and root refers to one existing node.
- **Errores:** invalid root/node access remains invalid caller state.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** IDs count root and node equality.

### 4.5 CSH-SYN-005 — Deterministic syntax dump

- **Símbolos:** NativeSharp.Compiler.Syntax.SyntaxDumper;NativeSharp.Compiler.Syntax.SyntaxDumper.Indent;NativeSharp.Compiler.Syntax.SyntaxDumper.DumpNode;NativeSharp.Compiler.Syntax.SyntaxDumper.Dump.
- **Entradas:** syntax tree root.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** hierarchical normalized text; hierarchical normalized text are coherent and observable.
- **Invariante:** child order and metadata determine output.
- **Errores:** deep recursion risk is documented rather than hidden.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact dump and depth stress outcome.

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

La integración directa es: SelfHost.Basic;SelfHost.Lexer. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 50 declaraciones, 5 contratos y 36 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
