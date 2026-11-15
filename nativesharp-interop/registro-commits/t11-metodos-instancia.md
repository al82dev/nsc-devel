# T11-005 y T11-010: métodos de instancia con enums, records, referencias y nombres reservados

## Defecto

En la [auditoría inicial](../TANDA-11-AUDITORIA-INICIAL.md):

- un método de instancia con un enum, un record por valor, `const T&` o un retorno `T&` daba
  `no compatible member named …`;
- `c.get()` y `c.set(4)` eran errores de sintaxis.

Causa en el código:

- `projectCppSemanticInstanceMethod` (`CppSemanticFunction.cpp`) mapeaba la firma sin búsqueda de
  nombres ni records C++ por valor, y descartaba las referencias.
- `ProjectMemberModel.cpp` solo aceptaba escalares y punteros en un método de instancia.
- `get` y `set` son palabras reservadas de NativeSharp. `parseMemberSuffix`
  (`ExpressionParser.cpp`) exigía un identificador tras `.`, `?.` y `->`.

Al probar los records por valor apareció un defecto de ABI. El ABI C++ de Microsoft devuelve todo
tipo clase desde un método de instancia en memoria, con el puntero oculto después de `this`, aunque
mida 8 bytes. NSC lo devolvía en `i64`: Clang declara `void (ptr, ptr, i64)` para `P C::scaled(P)`
y NSC declaraba `i64 (ptr, i64)`.

## Cambio

- `CppSemanticFunction.cpp` y `.h`:
  - `projectCppSemanticInstanceMethod` recibe la búsqueda de nombres y mapea enums y records C++
    triviales por valor, como una función libre;
  - `T&` es un parámetro `ref` y `const T&` uno `in`, con el portador puntero de T10-003;
  - un retorno `T&` es un retorno `ref` del referente; un retorno `const T&` y `T&&` se rechazan.
- `CppImportedInstanceMethod.h` y `ProjectMemberModel.cpp`:
  - el método importado lleva el modo de paso de cada parámetro y el retorno por referencia;
  - el miembro admite enums y records triviales por valor;
  - un retorno `ref` queda ligado a `this`, como en un método NativeSharp;
  - la firma distingue `ref` e `in`.
- `MirFunctionLowerer.cpp`: un método de instancia C++ que devuelve un tipo clase lo devuelve en
  memoria, tras `this`.
- `OopLlvmCalls.cpp`: la llamada a un método de instancia usa ese retorno en memoria del destino.
- `ExpressionParser.cpp`: tras `.`, `?.` y `->` se acepta como nombre de miembro una palabra
  reservada de NativeSharp (`get`, `set`, `move`…). Ahí solo cabe un nombre, así que no cambia otra
  construcción del lenguaje. Un miembro NativeSharp no puede llamarse así; un miembro C++ sí.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-11-metodos/validar.py`, con y sin `--dll`. `struct C` en
`namespace T11` con:

- `get()` y `set(int)`;
- `scale(E)` y `kind()`, con `enum class E : short`;
- `scaled(P)` (8 bytes) y `grow(Q)` (12 bytes);
- `read(const P&)`, `bump(int&)` e `int& slot()`.

El consumidor llama a cada método sobre un local, `slot()` como destino de una asignación y `get()`
por un puntero (`h->get()`). De O0 a O3, el fixture compara un control C++ compilado por Clang
23.1.0 con el consumidor NativeSharp (resultado 49). También exige las 9 declaraciones del IR de NSC
con los tipos físicos de Clang, incluido el retorno en memoria de `scaled` y `grow`.

Controles negativos, con `no compatible member`: un retorno `const int&` y un parámetro `int&&`.
La contraparte `plain()` del mismo record pasa.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t11-metodos-klhwvgth` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t11-metodos-vy6eakw8` |

Regresión: ver [TANDA-11-VALIDACION-AGRUPADA.md](../TANDA-11-VALIDACION-AGRUPADA.md).

## Límites

- En una plantilla precompilada, los métodos de instancia C++ y los enums que solo usa la plantilla no
  llegan al consumidor (`no compatible member named …`).
- Retornos `const T&` y parámetros `T&&`: tanda 11, con la copia y el movimiento.
- Métodos virtuales: tanda 12.
