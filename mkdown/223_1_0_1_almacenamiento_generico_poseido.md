# 223 — Almacenamiento genérico poseído / Owned generic storage

## Resultado / Outcome

**ES.** NativeSharp incorpora `alignof(T)` y `alignof(expresión)` como consultas constantes del
layout objetivo. Junto con `sizeof`, `new T`, `T*`, indexado y `move`, una implementación genérica
puede reservar almacenamiento nativo para un `T` cerrado sin inventar tamaños o alineaciones. La
entrega añade además el contrato acumulativo para trasladar un `owned T` hacia y desde un slot
`T*`, tanto cuando `T` es valor como cuando es referencia.

**EN.** NativeSharp now provides `alignof(T)` and `alignof(expression)` as constant target-layout
queries. Together with `sizeof`, `new T`, `T*`, indexing, and `move`, generic code can reserve
native storage for a closed `T` without hard-coded size or alignment tables. The delivery also adds
the cumulative contract for moving an `owned T` into and out of a `T*` slot for both value and
reference substitutions.

## Superficie del lenguaje / Language surface

```nativesharp
public class NativeSlot<T>
{
    private T* storage;

    public NativeSlot() { storage = new T; }
    ~NativeSlot() { delete storage; }

    public void Put(owned T value) { storage[0] = move(value); }
    public owned T Take() { return move(storage[0]); }

    public static nuint Size() { return sizeof(T); }
    public static nuint Alignment() { return alignof(T); }
}
```

Las dos formas de `alignof` devuelven `nuint`. La forma de expresión es un operando no evaluado:
resuelve el tipo, pero no ejecuta llamadas, asignaciones, movimientos ni traps escritos dentro del
operando. `alignof(void)` y los grupos de métodos se rechazan porque no describen objetos
alineables.

Both forms return `nuint`. The expression form is unevaluated: its type is resolved, but calls,
assignments, moves, and traps written in the operand are not executed. `alignof(void)` and method
groups are rejected because they do not denote alignable objects.

## Implementación / Implementation

### Léxico, sintaxis y AST / Lexing, syntax, and AST

- `alignof` es una palabra reservada propia (`KwAlignof`).
- El parser comparte exactamente la desambiguación tipo/expresión de `sizeof`; no mantiene un
  segundo reconocedor de tipos.
- El AST distingue `AlignofTypeExpression` y `AlignofExpression`, preservando si el usuario midió
  un tipo o el tipo de una expresión.

- `alignof` is a dedicated reserved keyword.
- Parsing shares `sizeof`'s exact type-versus-expression disambiguation rather than introducing a
  second type recognizer.
- The AST preserves type and expression forms independently.

### Semántica y layout canónico / Semantics and canonical layout

Los dos frontends producen una constante `nuint`. El frontend clásico consulta
`SemanticAnalyzer::layoutOfType`; el pipeline de proyectos conserva el tipo medido en
`MethodCoreBinding` y consulta después
`ProjectObjectLayoutModel::valueLayoutForTypeKey`. En ambos casos la respuesta es el campo
`alignment` de la misma pareja tamaño/alineación que gobierna:

- offsets de campos;
- stride de arreglos;
- `sizeof`;
- `stackalloc` y `new`;
- aritmética e indexado de punteros.

Both frontends emit a `nuint` constant. The classic frontend reads `layoutOfType`; project OOP
lowering stores the measured closed type in `MethodCoreBinding` and later reads
`valueLayoutForTypeKey`. Both select alignment from the same size/alignment result used by fields,
arrays, allocation, `sizeof`, and pointer stepping.

### Ownership / Propiedad

No se agrega una copia implícita. El corpus usa únicamente transferencias explícitas:

1. `Put(owned T)` recibe el propietario;
2. `storage[0] = move(value)` invalida el parámetro y establece el slot como nuevo hogar;
3. `return move(storage[0])` vacía el slot y devuelve un único propietario;
4. para `T=int`, las mismas formas se monomorfizan como operaciones por valor sin disciplina de
   destrucción de referencia;
5. para `T=Payload`, el origen movido queda nulo y sólo el destino conserva la responsabilidad.

No implicit copy is introduced. The corpus uses explicit ownership transfers only. Value-type
substitutions remain ordinary values; reference-type substitutions retain one destruction
responsibility and null the moved-from storage.

## Archivos / Files

- `include/nativesharp/Lexer/TokenKind.h`, `src/Lexer/*`: token reservado.
- `include/nativesharp/AST/AstNodeKind.h`, `src/AST/AstNodeKind.cpp`: nodos de ambas formas.
- `src/Parser/Parser.cpp`: desambiguación compartida con `sizeof`.
- `include/nativesharp/Sema/SemanticAnalyzer.h`, `src/Sema/SemanticAnalyzer.cpp`: constante de
  alineación para el frontend clásico.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`,
  `src/Frontend/ProjectMethodBodyModel.cpp`: binding semántico tardío y preservación del estado de
  ownership durante operandos no evaluados.
- `src/Frontend/ProjectOopHirModel.cpp`: selección de la alineación canónica al construir HIR.
- `src/HIR/HirBuilder.cpp`: constante HIR en el frontend clásico.
- `tests/unit/LexerTests.cpp`, `ParserTests.cpp`, `SemanticTests.cpp`: superficie del operador.
- `tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp`: slot `T*` cerrado con `int` y una
  clase poseída.

## Decisiones / Decisions

1. **`alignof`, no un método de runtime.** La alineación es una propiedad del target y del tipo
   cerrado; no necesita ejecutar código ni cargar una biblioteca.
2. **Una autoridad de layout.** No se introducen tablas paralelas para genéricos.
3. **Resultado `nuint`.** Tamaños, alineaciones y aritmética de direcciones comparten el entero
   nativo sin signo.
4. **Sin relajación de ownership.** Obtener el layout de `T` no autoriza copiar un `owned T`.
5. **Sin baseline intermedio.** Las pruebas nuevas se ejecutarán en el baseline acumulativo final;
   después del commit sólo se reconstruye `nsc` desde cero.

## Commit de referencia / Reference commit

**Título ES:** Habilita almacenamiento genérico poseído

**Title EN:** Enable owned generic storage

**Descripción ES:** Incorpora `alignof` sobre el layout canónico y certifica slots `T*` que
transfieren `owned T` para sustituciones de valor y referencia sin copias ni doble destrucción.

**Description EN:** Add `alignof` over the canonical layout and certify `T*` slots that transfer
`owned T` for value and reference substitutions without copies or double destruction.
