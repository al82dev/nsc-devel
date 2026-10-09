# SelfHost.Types — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 246 líneas, 13881 bytes, 32 declaraciones físicas.
- **Diseño:** 7 contratos y 47 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Internar tipos semánticos canónicos, firmas compuestas, ownership y hechos de layout físico.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Basic;NativeSharp.Core.Collections.Generic.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000620 | `selfhost/nsc/Types/TypeSystem.ns` | 246 | 32 |

## 4. Contratos

### 4.1 CSH-TYP-001 — Semantic and ownership type vocabularies

- **Símbolos:** NativeSharp.Compiler.Types.SemanticTypeKind;NativeSharp.Compiler.Types.OwnershipKind.
- **Entradas:** enum values.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable type/ownership categories; stable type/ownership categories are coherent and observable.
- **Invariante:** category identity is stable within frozen model.
- **Errores:** unknown value remains unsupported.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exhaustive enum behavior in consumers.

### 4.2 CSH-TYP-002 — Semantic type value and layout

- **Símbolos:** NativeSharp.Compiler.Types.SemanticType;NativeSharp.Compiler.Types.SemanticType.SemanticType;NativeSharp.Compiler.Types.SemanticType.Kind;NativeSharp.Compiler.Types.SemanticType.Display;NativeSharp.Compiler.Types.SemanticType.ElementType;NativeSharp.Compiler.Types.SemanticType.ReturnType;NativeSharp.Compiler.Types.SemanticType.ElementCount;NativeSharp.Compiler.Types.SemanticType.Size;NativeSharp.Compiler.Types.SemanticType.Alignment;NativeSharp.Compiler.Types.SemanticType.Ownership;NativeSharp.Compiler.Types.SemanticType.SetElementType;NativeSharp.Compiler.Types.SemanticType.SetReturnType;NativeSharp.Compiler.Types.SemanticType.SetElementCount;NativeSharp.Compiler.Types.SemanticType.SetLayout;NativeSharp.Compiler.Types.SemanticType.SetOwnership.
- **Entradas:** type kind display relations layout and ownership.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** mutable semantic type descriptor; mutable semantic type descriptor are coherent and observable.
- **Invariante:** relations and layout preserve explicit values.
- **Errores:** invalid relations are rejected by producer analysis.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** all fields and transition table.

### 4.3 CSH-TYP-003 — Function parameter type list

- **Símbolos:** NativeSharp.Compiler.Types.SemanticType.ParameterCount;NativeSharp.Compiler.Types.SemanticType.ParameterType;NativeSharp.Compiler.Types.SemanticType.AddParameterType.
- **Entradas:** ordered parameter type IDs.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** function parameter signature; function parameter signature are coherent and observable.
- **Invariante:** insertion order and count/index agree.
- **Errores:** invalid index is caller error.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact ordered IDs and count.

### 4.4 CSH-TYP-004 — Builtin type table initialization

- **Símbolos:** NativeSharp.Compiler.Types.TypeTable;NativeSharp.Compiler.Types.TypeTable.TypeTable;NativeSharp.Compiler.Types.TypeTable.AddBuiltin.
- **Entradas:** new table.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** canonical builtin types and IDs; canonical builtin types and IDs are coherent and observable.
- **Invariante:** builtin spelling maps to stable unique identity.
- **Errores:** duplicate internal builtin is not published twice.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** complete builtin name/kind/layout inventory.

### 4.5 CSH-TYP-005 — Type lookup and interning

- **Símbolos:** NativeSharp.Compiler.Types.TypeTable.EndsWith;NativeSharp.Compiler.Types.TypeTable.Count;NativeSharp.Compiler.Types.TypeTable.Get;NativeSharp.Compiler.Types.TypeTable.Find;NativeSharp.Compiler.Types.TypeTable.Intern.
- **Entradas:** type display strings including pointers arrays generics delegates and ownership.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** canonical type ID and descriptor; canonical type ID and descriptor are coherent and observable.
- **Invariante:** same normalized display interns once and composite relations are coherent.
- **Errores:** malformed or unknown composite text follows documented unknown descriptor.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** ID equality descriptors count and mutation stability.

### 4.6 CSH-TYP-006 — String literal type construction

- **Símbolos:** NativeSharp.Compiler.Types.TypeTable.StringLiteral.
- **Entradas:** literal length/content category.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** string-literal semantic type ID; string-literal semantic type ID are coherent and observable.
- **Invariante:** literal types retain declared element/count semantics.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** kind display element count and ID reuse.

### 4.7 CSH-TYP-007 — Function type construction and numeric predicates

- **Símbolos:** NativeSharp.Compiler.Types.TypeTable.Function;NativeSharp.Compiler.Types.TypeTable.IsInteger;NativeSharp.Compiler.Types.TypeTable.IsNumeric.
- **Entradas:** return/parameter types and queried type ID.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** function type ID or numeric classification; function type ID or numeric classification are coherent and observable.
- **Invariante:** signature identity is structural and predicates agree with builtin kinds.
- **Errores:** non-numeric/invalid type returns false.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** signature descriptor and predicate truth table.

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

La integración directa es: SelfHost.Basic;NativeSharp.Core.Collections.Generic. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 32 declaraciones, 7 contratos y 47 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
