# Host.AST — inventario semántico y contrato reconstructivo

## Estado y alcance

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE` y `HECHO COMPROBADO` por lectura estática.
- **Revisión:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** diez archivos, 1,372 líneas y 58,082 bytes.
- **Superficie inventariada:** 155 símbolos y 20 contratos.
- **Validación ejecutada:** ninguna; sólo auditoría estática y diseño.

## 1. Propósito y frontera

`Host.AST` proporciona almacenamiento con lifetime común (`AstArena`), el catálogo de 100 clases de nodo,
records sintácticos para genéricos/lifetimes, el contenedor flexible `AstNode`, el modelo fuente `TypeSyntax` y
un dump textual parcial. No valida coherencia semántica ni ownership de punteros entre nodos.

## 2. Archivos

| ID | Archivo | Responsabilidad |
|---|---|---|
| PF-000079 | `include/nativesharp/AST/AstArena.h` | arena heterogénea y factory template |
| PF-000080 | `include/nativesharp/AST/AstDumper.h` | API del dump AST/tipos |
| PF-000081 | `include/nativesharp/AST/AstNode.h` | records genéricos y contenedor AST |
| PF-000082 | `include/nativesharp/AST/AstNodeKind.h` | catálogo estable de nodos |
| PF-000083 | `include/nativesharp/AST/TypeSyntax.h` | modelo fuente de tipos |
| PF-000621 | `src/AST/AstArena.cpp` | destrucción LIFO |
| PF-000622 | `src/AST/AstDumper.cpp` | formato y recorrido recursivo |
| PF-000623 | `src/AST/AstNode.cpp` | operaciones de `AstNode` |
| PF-000624 | `src/AST/AstNodeKind.cpp` | nombre de los 100 kinds |
| PF-000625 | `src/AST/TypeSyntax.cpp` | operaciones de `TypeSyntax` |

## 3. Arena — `CA-AST-001..002`

`AstArena::make<T>` asigna con `new`, registra `{pointer, destroy<T>}` y devuelve el puntero. El destructor
recorre `entries_` en reversa y llama cada deleter exactamente una vez. La arena no es copiable y los punteros
son estables aunque el vector de entries se mueva.

Hay una ventana de excepción vigente: si `entries_.push_back` falla después de `new T`, el objeto recién creado
no queda registrado y se filtra. Si el constructor de `T` lanza, `new` libera su almacenamiento normalmente.
Si un destructor lanza durante `~AstArena`, se aplica el comportamiento de destructores C++ y pueden quedar
entries posteriores sin destruir; el código no captura excepciones. No existe thread-safety.

## 4. Catálogo de nodo — `CA-AST-003`

`AstNodeKind` contiene 100 valores contiguos en el orden declarado. `astNodeKindName` cubre los 100 mediante un
switch y devuelve el identificador exacto. Cualquier valor fuera del enum devuelve `UnknownAstNode`.
Los valores nuevos se agregaron al final para preservar identidades serializadas previas; esta es una propiedad
observable del orden ordinal, no sólo del nombre.

## 5. Records genéricos y lifetime — `CA-AST-004`

- `GenericParameterSyntaxKind`: Type, Constant, TypePack y ConstantPack.
- `GenericParameterSyntax`: name, kind, constantType y range.
- `LifetimeParameterSyntax`: name y range.
- `LifetimeOutlivesConstraint`: longer, shorter y range.
- `FloatingPointModeSyntaxKind`: Inherit, Strict, Precise y Fast.
- `FfiUnwindSyntaxKind`: Default, Forbidden y Abort.
- `GenericConstraintClause`: nombre/rango de parámetro, ocho capabilities, equality/projection, lista de tipos,
  rango total y argumento sustituido no propietario.

Son records públicos sin validación interna. La coherencia entre punteros, flags y parameter name corresponde a
Parser/Sema.

## 6. Identidad y payload escalar de AstNode — `CA-AST-005`

El constructor fija kind/range. Kind es inmutable; range sí se reemplaza. Tres strings propietarios conservan
text, auxiliaryText y argumentName. TokenKind inicia `Invalid`. Los setters de string mueven su argumento.

El nodo ofrece flag genérico, reference, variadic, unsafe, modo FP, unwind, uint64 y double como canales
ortogonales. Defaults: false, Inherit, Default, cero y 0.0. No hay validación dependiente de kind.

## 7. Modifiers y tipos adjuntos — `CA-AST-006`

Los modifiers TokenKind conservan inserción y duplicados; `hasModifier` es búsqueda lineal. `typeSyntax` y
`throwsTypeSyntax` son slots no propietarios y admiten null. `addTypeSyntax` ignora null; la lista restante
preserva orden y `clearTypeSyntaxes` sólo vacía el vector.

## 8. Genéricos de AstNode — `CA-AST-007`

`addGenericParameter(string)` crea un record Type. La sobrecarga de record agrega simultáneamente el name al
vector legado y el record completo al vector moderno. `clearGenericParameters` limpia ambos. Ésta es la única
ruta que mantiene la alineación; las referencias const expuestas permiten observar ambos vectores, no
modificarlos.

## 9. Lifetimes y constraints de AstNode — `CA-AST-008`

Lifetime parameters, outlives y generic constraints se mueven a vectores en orden. `lifetimeName` es un string
independiente. Sólo constraints tienen clear; no existe clear de lifetime parameters/outlives en esta API.

## 10. Topología AstNode — `CA-AST-009`

`first`, `second` y `third` son slots independientes que admiten null. `children` admite null y duplicados;
`addChild` agrega al final y `prependChild` inserta al principio en O(n). Ninguna operación adopta ownership,
impide ciclos ni verifica que el nodo pertenezca al mismo arena.

## 11. Estado transversal AstNode — `CA-AST-010`

Los canales booleanos, policies y valores numéricos no se excluyen mutuamente. Un nodo puede almacenar
cualquier combinación aunque su kind no la use. Esta flexibilidad es parte del modelo productivo, pero obliga a
que los consumers conozcan el contrato por kind.

## 12. Identidad y reescritura TypeSyntax — `CA-AST-011`

El constructor fija range, baseTokenKind y baseName. Range, token y nombre pueden reemplazarse. Para expansión
de alias, `prependModifiers` inserta el bloque recibido antes de los modifiers existentes conservando el orden
interno de ambos bloques. Un bloque vacío no altera ni invalida referencias.

## 13. Ownership y marcadores nominales — `CA-AST-012`

Ownership tiene None/Owned/Borrowed; el nombre textual devuelve `none`, `owned`, `borrowed` y hace fallback a
`none`. Lifetime, dynamic trait, opaque result/trait name, explicit trait implementation y pointee const son
campos independientes. Ningún setter cambia baseName ni identidad física por sí solo.

## 14. Argumentos y tuplas — `CA-AST-013`

`addTypeArgument` acepta cualquier puntero, incluido null. `clearTypeArguments` no limpia tupleElementNames.
`addTupleElement` fuerza `isTuple`, agrega tipo y nombre en paralelo. La API permite desalinear ambos vectores
si se mezclan llamadas; no existe validación ni clear de nombres.

## 15. Tipos función — `CA-AST-014`

Un function type conserva carrier pointer/reference, calling convention (NativeSharp, C, StdCall, FastCall,
VectorCall, Win64), parámetros en typeArguments, retorno/error, variadic, unsafe, noheap, async, nullable y
lifetime. Todos los campos son independientes; `setFunction` no inicializa/limpia los restantes.

## 16. Const genérico y pack — `CA-AST-015`

`constExpression` es un puntero no propietario y admite null. `isPackExpansion` es un bool independiente. No se
verifica que la forma se use sólo en un argumento genérico.

## 17. Modifiers de tipo — `CA-AST-016`

`TypeModifierSyntaxKind` cubre Pointer, Array, DynamicArray, RangeArray y FlexibleArray. El record conserva
arrayCount, range, rank y garantías pointer: nullability LegacyNullable/Nullable/NonNull, provenance, alignment
y address space. Defaults: Pointer, null, rank 0, LegacyNullable, provenance true, alignment 1, address space 0.
`addModifier` conserva orden y duplicados.

## 18. Origen de sustitución — `CA-AST-017`

`originProjectOrdinal` y `originNamespace` son optionals independientes. Los setters sólo asignan valor; no hay
clear explícito ni mutación de baseName. Esto permite resolución en el proyecto/namespace del argumento sin
cambiar la spelling usada por la identidad de especialización.

## 19. Dump de nodo — `CA-AST-018`

Por nodo no null se imprimen indentación de dos espacios por nivel, edge, kind y opcionalmente text, aux,
argument name, nombres genéricos, lifetimes/outlives, lifetime-name, constraints, op, type, throws, flag,
reference, variadic y modo FP. Strings se escriben sin escape adicional.

Constraints imprimen igualdad o capabilities en orden fijo y luego type constraints. No imprimen
associatedProjection ni substitutedArgument. El dump tampoco imprime modifiers del nodo, generic parameter
kinds/types/ranges, unsafe, unwind, integerValue ni floatingValue.

## 20. Dump de tipo — `CA-AST-019`

El formato distingue ownership/lifetime, dyn, función, tuple o base nominal. Para función imprime carrier,
nullable, lifetime, calling convention siempre (incluido `native`), parámetros/varargs, retorno o `<missing>`,
unsafe/noheap/async y error. Para tuple alinea nombres disponibles. Tipos nominales imprimen argumentos.

Después agrega pointee const y modifiers: pointer con `?`/`!`, alignment distinto de 1, address space distinto de
0 y provenance none; arrays dynamic/range/flexible/fixed como `[]`, commas, `[*]` y `[expr]`.

No imprime opaque result/trait name, explicit trait implementation, baseTokenKind, constExpression, pack
expansion, origin overrides, range, arrayCount real ni modifier range. `LegacyNullable` no agrega símbolo.

## 21. Recorrido, determinismo y límites del dumper — `CA-AST-020`

`dump(nullptr)` no produce bytes. El recorrido es preorden: nodo, first, second, third y children. Es recursivo
para nodos y tipos, no detecta ciclos y no impone profundidad. Un ciclo no termina y una estructura profunda
puede agotar stack. La salida depende del orden de vectores, pero no de direcciones.

El dumper es una vista diagnóstica parcial, no una serialización reconstructiva. Dos árboles que difieran sólo
en campos omitidos pueden producir el mismo texto. El test plan debe probar tanto bytes incluidos como esta
colisión vigente; una futura ampliación del dump sería un cambio de contrato, no una corrección documental.

## 22. Dependencias y complejidad

- `AstArena`: O(1) amortizado por make, O(n) destrucción reversa.
- `AstNode`: accessors O(1); hasModifier O(n); prependChild O(n).
- `TypeSyntax`: append O(1) amortizado; prependModifiers O(m+n).
- `AstDumper`: O(nodos + tipos + elementos), stack O(profundidad), más coste de ostream.
- Dependencias directas: Basic.SourceRange, Lexer.TokenKind, STL y ostream.

## 23. Reconstructibilidad

Una implementación compatible debe preservar defaults, orden, aceptación de null/duplicados, no-ownership,
destrucción LIFO, catálogo ordinal y formato byte a byte, incluidas las omisiones del dumper. `RECONSTRUIBLE`
se confirma sólo después de cerrar matrices y segunda auditoría. No se implementaron ni ejecutaron pruebas.
