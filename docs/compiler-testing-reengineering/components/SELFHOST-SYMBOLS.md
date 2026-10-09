# SelfHost.Symbols — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 142 líneas, 7297 bytes, 56 declaraciones físicas.
- **Diseño:** 5 contratos y 35 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Representar declaraciones, miembros, layout, constantes, reachability y scopes léxicos.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Types;SelfHost.Syntax.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000618 | `selfhost/nsc/Symbols/Symbols.ns` | 142 | 56 |

## 4. Contratos

### 4.1 CSH-SYM-001 — Symbol kind vocabulary

- **Símbolos:** NativeSharp.Compiler.Symbols.SymbolKind;NativeSharp.Compiler.Symbols.SymbolFacts;NativeSharp.Compiler.Symbols.SymbolFacts.Name;NativeSharp.Compiler.Symbols.Symbol.Kind;NativeSharp.Compiler.Symbols.Symbol.Name;NativeSharp.Compiler.Symbols.Symbol.HasConstantValue;NativeSharp.Compiler.Symbols.Symbol.SetQualifiedName;NativeSharp.Compiler.Symbols.Symbol.SetTypeId;NativeSharp.Compiler.Symbols.Symbol.SetConst;NativeSharp.Compiler.Symbols.Symbol.SetDefined;NativeSharp.Compiler.Symbols.Symbol.SetExternC;NativeSharp.Compiler.Symbols.Symbol.SetVirtual;NativeSharp.Compiler.Symbols.Symbol.SetOverride;NativeSharp.Compiler.Symbols.Symbol.SetStatic;NativeSharp.Compiler.Symbols.Symbol.SetVolatile;NativeSharp.Compiler.Symbols.Symbol.SetNoHeap;NativeSharp.Compiler.Symbols.Symbol.SetGenericText;NativeSharp.Compiler.Symbols.Symbol.SetConstraintsText;NativeSharp.Compiler.Symbols.Scope.Count.
- **Entradas:** symbol kind including unknown.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable name; stable name are coherent and observable.
- **Invariante:** every declared kind has one stable spelling.
- **Errores:** unknown kind uses documented fallback.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exhaustive enum/name table.

### 4.2 CSH-SYM-002 — Symbol identity and declaration metadata

- **Símbolos:** NativeSharp.Compiler.Symbols.Symbol;NativeSharp.Compiler.Symbols.Symbol.Symbol;NativeSharp.Compiler.Symbols.Symbol.QualifiedName;NativeSharp.Compiler.Symbols.Symbol.TypeId;NativeSharp.Compiler.Symbols.Symbol.Declaration;NativeSharp.Compiler.Symbols.Symbol.Parent;NativeSharp.Compiler.Symbols.Symbol.IsConst;NativeSharp.Compiler.Symbols.Symbol.IsDefined;NativeSharp.Compiler.Symbols.Symbol.IsExternC;NativeSharp.Compiler.Symbols.Symbol.IsVirtual;NativeSharp.Compiler.Symbols.Symbol.IsOverride;NativeSharp.Compiler.Symbols.Symbol.IsStatic;NativeSharp.Compiler.Symbols.Symbol.IsVolatile;NativeSharp.Compiler.Symbols.Symbol.IsNoHeap;NativeSharp.Compiler.Symbols.Symbol.GenericText;NativeSharp.Compiler.Symbols.Symbol.ConstraintsText;NativeSharp.Compiler.Symbols.Scope.Parent.
- **Entradas:** kind name type declaration and modifiers.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** symbol record; symbol record are coherent and observable.
- **Invariante:** construction and setters preserve explicit semantic metadata.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact fields before/after mutation.

### 4.3 CSH-SYM-003 — Symbol member and layout metadata

- **Símbolos:** NativeSharp.Compiler.Symbols.Symbol.MemberCount;NativeSharp.Compiler.Symbols.Symbol.MemberAt;NativeSharp.Compiler.Symbols.Symbol.Offset;NativeSharp.Compiler.Symbols.Symbol.Size;NativeSharp.Compiler.Symbols.Symbol.Alignment;NativeSharp.Compiler.Symbols.Symbol.BitWidth;NativeSharp.Compiler.Symbols.Symbol.BitOffset;NativeSharp.Compiler.Symbols.Symbol.SetParent;NativeSharp.Compiler.Symbols.Symbol.AddMember;NativeSharp.Compiler.Symbols.Symbol.SetLayout;NativeSharp.Compiler.Symbols.Symbol.SetBitField.
- **Entradas:** parent/member identities and layout values.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** coherent aggregate symbol metadata; coherent aggregate symbol metadata are coherent and observable.
- **Invariante:** member order and layout fields remain stable.
- **Errores:** invalid layout is analyzer-owned error.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** member table and exact layout values.

### 4.4 CSH-SYM-004 — Symbol constants and reachability

- **Símbolos:** NativeSharp.Compiler.Symbols.Symbol.ConstantValue;NativeSharp.Compiler.Symbols.Symbol.IsReachable;NativeSharp.Compiler.Symbols.Symbol.SetConstantValue;NativeSharp.Compiler.Symbols.Symbol.SetReachable.
- **Entradas:** constant/reachability transitions.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** updated symbol flags and value; updated symbol flags and value are coherent and observable.
- **Invariante:** presence is distinct from numeric zero and reachability is monotone in closure.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** flag/value transition table.

### 4.5 CSH-SYM-005 — Lexical scope declaration and lookup

- **Símbolos:** NativeSharp.Compiler.Symbols.Scope;NativeSharp.Compiler.Symbols.Scope.Scope;NativeSharp.Compiler.Symbols.Scope.Declare;NativeSharp.Compiler.Symbols.Scope.LookupLocal;NativeSharp.Compiler.Symbols.Scope.SymbolAt.
- **Entradas:** parent scope symbol name and declaration order.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** local lookup and ordered symbol set; local lookup and ordered symbol set are coherent and observable.
- **Invariante:** local lookup never searches parent and first declared identity remains indexed.
- **Errores:** duplicate handling belongs to analyzer caller.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** count/order/lookup and parent identity.

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

La integración directa es: SelfHost.Types;SelfHost.Syntax. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 56 declaraciones, 5 contratos y 35 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
