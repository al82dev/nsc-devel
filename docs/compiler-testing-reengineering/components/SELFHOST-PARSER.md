# SelfHost.Parser — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 1478 líneas, 77265 bytes, 97 declaraciones físicas.
- **Diseño:** 16 contratos y 127 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Construir el árbol sintáctico self-host completo, primero mutable y después congelado, preservando precedencia, declaraciones y progreso de recuperación.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Lexer;SelfHost.Syntax;SelfHost.Diagnostics.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000605 | `selfhost/nsc/Parser/Parser.ns` | 1478 | 97 |

## 4. Contratos

### 4.1 CSH-PAR-001 — Token cursor expectation and recovery

- **Símbolos:** NativeSharp.Compiler.Parser.Parser;NativeSharp.Compiler.Parser.Parser.Parser;NativeSharp.Compiler.Parser.Parser.Current;NativeSharp.Compiler.Parser.Parser.Peek;NativeSharp.Compiler.Parser.Parser.At;NativeSharp.Compiler.Parser.Parser.ConsumeIf;NativeSharp.Compiler.Parser.Parser.Consume;NativeSharp.Compiler.Parser.Parser.Expect;NativeSharp.Compiler.Parser.Parser.AddError;NativeSharp.Compiler.Parser.Parser.LexemeAt;NativeSharp.Compiler.Parser.Parser.CurrentText;NativeSharp.Compiler.Parser.Parser.AtText;NativeSharp.Compiler.Parser.Parser.ErrorCount;NativeSharp.Compiler.Parser.Parser.FirstErrorIndex.
- **Entradas:** token stream and parser cursor.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** consumed token text or first error; consumed token text or first error are coherent and observable.
- **Invariante:** every recovery path advances or terminates and first error remains stable.
- **Errores:** unexpected token records owned parse error.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** cursor trace consumed ranges and first error.

### 4.2 CSH-PAR-002 — Known types nodes and modifier vocabulary

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.IsKnownType;NativeSharp.Compiler.Parser.Parser.AddKnownType;NativeSharp.Compiler.Parser.Parser.NewNode;NativeSharp.Compiler.Parser.Parser.Node;NativeSharp.Compiler.Parser.Parser.ApplyType;NativeSharp.Compiler.Parser.Parser.Builtin;NativeSharp.Compiler.Parser.Parser.Modifier;NativeSharp.Compiler.Parser.Parser.Ownership;NativeSharp.Compiler.Parser.Parser.CopyTypeModifier;NativeSharp.Compiler.Parser.Parser.ParseLayoutModifiers;NativeSharp.Compiler.Parser.Parser.JoinAuxiliary.
- **Entradas:** tokens known-type set and syntax tree.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** typed node/modifier metadata; typed node/modifier metadata are coherent and observable.
- **Invariante:** known types and auxiliary spelling are deterministic without duplicates.
- **Errores:** invalid modifier combinations are rejected by owning parse path.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** node fields known-type set and auxiliary text.

### 4.3 CSH-PAR-003 — Type allocation generic constraints and throws grammar

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.AtGenericGreater;NativeSharp.Compiler.Parser.Parser.ConsumeGenericGreater;NativeSharp.Compiler.Parser.Parser.ParseType;NativeSharp.Compiler.Parser.Parser.ParseAllocationType;NativeSharp.Compiler.Parser.Parser.ParseGenericParameters;NativeSharp.Compiler.Parser.Parser.ParseGenericConstraints;NativeSharp.Compiler.Parser.Parser.ParseThrowsClause.
- **Entradas:** type/generic/constraint token sequences.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** syntax nodes and normalized type text; syntax nodes and normalized type text are coherent and observable.
- **Invariante:** nested generic closers and constraints preserve token boundaries.
- **Errores:** missing identifiers closers constraints or throws type report first error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** tree shape type text cursor and diagnostics.

### 4.4 CSH-PAR-004 — Literal and string decoding grammar

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.LiteralExpression;NativeSharp.Compiler.Parser.Parser.HexValue;NativeSharp.Compiler.Parser.Parser.DecodeString.
- **Entradas:** literal token and escape payload.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** literal syntax node and decoded text; literal syntax node and decoded text are coherent and observable.
- **Invariante:** source range and decoded semantic spelling remain attributable.
- **Errores:** invalid hex/escape produces parse error.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** node kind text and diagnostic.

### 4.5 CSH-PAR-005 — Primary cast and lambda disambiguation

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.IdentifierExpression;NativeSharp.Compiler.Parser.Parser.TypeStartsHere;NativeSharp.Compiler.Parser.Parser.LooksLikeCast;NativeSharp.Compiler.Parser.Parser.LooksLikeParenthesizedLambda;NativeSharp.Compiler.Parser.Parser.ParseLambda;NativeSharp.Compiler.Parser.Parser.ParsePrimary.
- **Entradas:** ambiguous parenthesized and primary token sequences.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** selected primary/cast/lambda syntax shape; selected primary/cast/lambda syntax shape are coherent and observable.
- **Invariante:** lookahead never consumes and parsing selects exactly one grammar branch.
- **Errores:** malformed selected branch reports without infinite retry.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** cursor before/after and syntax shape.

### 4.6 CSH-PAR-006 — Postfix member call index and argument grammar

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParsePostfix;NativeSharp.Compiler.Parser.Parser.ConsumeGenericCallArguments;NativeSharp.Compiler.Parser.Parser.ParseArgument.
- **Entradas:** primary followed by calls members generics indexes postfix operators.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** left-associated postfix syntax tree; left-associated postfix syntax tree are coherent and observable.
- **Invariante:** postfix operations preserve source order and argument modes.
- **Errores:** missing delimiters/member names produce owned error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** nested tree arguments and cursor.

### 4.7 CSH-PAR-007 — Unary allocation destruction and ownership expressions

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParseUnary.
- **Entradas:** unary new placement stackalloc delete destroy move sizeof/alignof/default tokens.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** unary or construction syntax tree; unary or construction syntax tree are coherent and observable.
- **Invariante:** operator precedence and allocation element/count structure are preserved.
- **Errores:** missing operand/type/delimiter reports and advances.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** node kind operator children type text and diagnostics.

### 4.8 CSH-PAR-008 — Binary conditional and assignment precedence

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.BinaryNode;NativeSharp.Compiler.Parser.Parser.ParseMultiplicative;NativeSharp.Compiler.Parser.Parser.ParseAdditive;NativeSharp.Compiler.Parser.Parser.ParseShift;NativeSharp.Compiler.Parser.Parser.ParseRelational;NativeSharp.Compiler.Parser.Parser.ParseEquality;NativeSharp.Compiler.Parser.Parser.ParseBitwiseAnd;NativeSharp.Compiler.Parser.Parser.ParseBitwiseXor;NativeSharp.Compiler.Parser.Parser.ParseBitwiseOr;NativeSharp.Compiler.Parser.Parser.ParseLogicalAnd;NativeSharp.Compiler.Parser.Parser.ParseLogicalOr;NativeSharp.Compiler.Parser.Parser.Assignment;NativeSharp.Compiler.Parser.Parser.ParseExpression.
- **Entradas:** expression token stream.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** precedence/associativity syntax tree; precedence/associativity syntax tree are coherent and observable.
- **Invariante:** precedence ladder and assignment associativity are stable.
- **Errores:** missing operand records error without fabricating valid tree.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** tree shape operators ranges and cursor.

### 4.9 CSH-PAR-009 — Variables blocks and basic statements

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.LooksLikeVariable;NativeSharp.Compiler.Parser.Parser.ParseVariable;NativeSharp.Compiler.Parser.Parser.ParseBlock;NativeSharp.Compiler.Parser.Parser.ParseIf;NativeSharp.Compiler.Parser.Parser.ParseReturn;NativeSharp.Compiler.Parser.Parser.ParseThrow;NativeSharp.Compiler.Parser.Parser.ParseStatement.
- **Entradas:** statement token stream.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** statement/block syntax nodes; statement/block syntax nodes are coherent and observable.
- **Invariante:** statement boundaries and nested block ownership are preserved.
- **Errores:** missing terminator/expression reports at owned boundary.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** tree order cursor and diagnostics.

### 4.10 CSH-PAR-010 — Loop statement grammar

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParseWhile;NativeSharp.Compiler.Parser.Parser.ParseDoWhile;NativeSharp.Compiler.Parser.Parser.ParseFor;NativeSharp.Compiler.Parser.Parser.ParseForeach.
- **Entradas:** loop tokens including declarations expressions and bodies.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** loop syntax nodes; loop syntax nodes are coherent and observable.
- **Invariante:** initializer condition increment enumerable and body occupy stable child roles.
- **Errores:** missing separators parentheses or body report and recover.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** node children flags cursor and diagnostics.

### 4.11 CSH-PAR-011 — Switch labels and sections

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.AtSwitchLabel;NativeSharp.Compiler.Parser.Parser.ParseSwitch.
- **Entradas:** switch case/default tokens.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** switch tree with ordered sections; switch tree with ordered sections are coherent and observable.
- **Invariante:** labels remain associated with their following statement sequence.
- **Errores:** duplicate malformed or missing label expression remains diagnosable downstream.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** section ordering labels and cursor.

### 4.12 CSH-PAR-012 — Exception and using statement grammar

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParseCatch;NativeSharp.Compiler.Parser.Parser.ParseTry;NativeSharp.Compiler.Parser.Parser.ParseUsingStatement.
- **Entradas:** try catch finally and using tokens.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** structured exception/resource nodes; structured exception/resource nodes are coherent and observable.
- **Invariante:** catch/finally order and using declaration/body remain explicit.
- **Errores:** missing handler/body/delimiter reports at grammar boundary.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** tree roles order and diagnostics.

### 4.13 CSH-PAR-013 — Static assertion and inline assembly grammar

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParseStaticAssert;NativeSharp.Compiler.Parser.Parser.ParseAssemblyConstraint;NativeSharp.Compiler.Parser.Parser.ParseAssemblyOperands;NativeSharp.Compiler.Parser.Parser.NonvolatileAssemblyRegister;NativeSharp.Compiler.Parser.Parser.ParseAssemblyClobbers;NativeSharp.Compiler.Parser.Parser.ParseInlineAssembly.
- **Entradas:** static_assert and asm token payloads.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** assertion/assembly syntax metadata; assertion/assembly syntax metadata are coherent and observable.
- **Invariante:** operand constraint clobber and volatility order is retained.
- **Errores:** invalid register/constraint/delimiter reports without consuming next statement.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** node metadata operand order and diagnostic.

### 4.14 CSH-PAR-014 — Callable parameters constructors and functions

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParseParameter;NativeSharp.Compiler.Parser.Parser.ParseParameters;NativeSharp.Compiler.Parser.Parser.ParseConstructorInitializer;NativeSharp.Compiler.Parser.Parser.ParseFunction.
- **Entradas:** declaration tokens modifiers generics parameters throws and body.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** callable declaration syntax node; callable declaration syntax node are coherent and observable.
- **Invariante:** parameter order modes defaults variadic state and body remain coherent.
- **Errores:** duplicate/missing names types delimiters or body report first error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** node fields children modifiers throws and diagnostics.

### 4.15 CSH-PAR-015 — Enums aggregates members and operators

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParseEnum;NativeSharp.Compiler.Parser.Parser.ParseAggregate;NativeSharp.Compiler.Parser.Parser.ParseDelegate;NativeSharp.Compiler.Parser.Parser.OperatorMethodName;NativeSharp.Compiler.Parser.Parser.ParseOperator;NativeSharp.Compiler.Parser.Parser.MakeAccessor;NativeSharp.Compiler.Parser.Parser.ParseProperty;NativeSharp.Compiler.Parser.Parser.ParseIndexer.
- **Entradas:** type/member declaration tokens.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** aggregate member syntax hierarchy; aggregate member syntax hierarchy are coherent and observable.
- **Invariante:** nested members accessors operators layout and underlying types retain declaration order.
- **Errores:** unsupported/malformed member reports and recovers at member boundary.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** tree hierarchy metadata order and diagnostics.

### 4.16 CSH-PAR-016 — Namespaces imports extern globals and compilation unit

- **Símbolos:** NativeSharp.Compiler.Parser.Parser.ParseUsing;NativeSharp.Compiler.Parser.Parser.ParseNamespace;NativeSharp.Compiler.Parser.Parser.ParseExternC;NativeSharp.Compiler.Parser.Parser.ParseGlobal;NativeSharp.Compiler.Parser.Parser.ParseCompilationUnit.
- **Entradas:** complete token stream.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** frozen compilation-unit syntax tree; frozen compilation-unit syntax tree are coherent and observable.
- **Invariante:** root contains every accepted top-level declaration in source order and freezes once.
- **Errores:** unrecognized top-level token reports and synchronizes.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** root children frozen state first error and EOF consumption.

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

La integración directa es: SelfHost.Lexer;SelfHost.Syntax;SelfHost.Diagnostics. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 97 declaraciones, 16 contratos y 127 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
