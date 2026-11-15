# T12-004: operadores miembro

Pendiente de la tanda 11 (T11-005): un método `operator…` de un record C++ no se importaba.

## Defecto

`projectCppSemanticInstanceMethod` (`CppSemanticFunction.cpp`) rechaza todo método cuyo nombre empieza
por `operator`. NativeSharp busca los operadores de un tipo C++ entre los operadores de extensión, que
son las funciones operador libres de la tanda 10 (T10-016); un operador miembro no estaba entre ellas.

## Diseño

- Un operador miembro público, no estático, no eliminado y sin calificador de referencia se importa
  como el operador libre equivalente de C++: su primer operando es el objeto, por referencia const si
  el operador es const (`in`), y el resto son sus parámetros. Pertenece al namespace más interno de su
  record, donde lo encuentra la búsqueda dependiente de argumentos de la tanda 10.
- La llamada va a un adaptador que genera Clang, una función libre `extern "C"` cuyo primer parámetro
  es el objeto y cuyo cuerpo llama al miembro por un puntero a miembro de su tipo exacto. El resultado
  sigue el ABI de una función libre que clasifica Clang.
- Las reglas de la tanda 10 se mantienen: los operandos van por valor o `in`, el retorno no es una
  referencia y al menos un operando es un tipo C++ importado. Un operador miembro no const toma el
  objeto como `ref` y queda fuera.
- No se importan `operator=`, `operator()`, `operator[]`, `operator->`, `operator->*`, `new`, `delete`
  ni `co_await`, que no son operadores NativeSharp.

## Cambio

- `CppMemberThunk.h`: `cppMemberOperatorSymbol`.
- `ClangSession.cpp`: `collectMemberOperators` añade a las funciones libres de la sesión un operador por
  cada operador miembro elegible, y `emitCppMemberThunkIR` genera su adaptador de función libre.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-12-operadores-miembro/validar.py`, con y sin `--dll`. En `namespace T12`:

- `V2` declara `operator+`, `operator==`, `operator-` unario y `operator*(int)`, todos const;
- `Acc`, con constructor y destructor, declara `operator+(int) const`: el objeto no trivial va como
  `in`;
- `D : B` declara `operator+` que devuelve `D` por valor, que Clang devuelve en memoria.

El consumidor usa `a + b`, `a == a`, `!(a == b)`, `-a`, `a * 5`, `acc + 5` y `d1 + d1`. Resultado 151
frente al control C++ de Clang 23.1.0, de O0 a O3, con las 3 declaraciones C++ que NSC llama
directamente iguales a las de Clang y los 6 operadores por adaptadores.

Controles negativos:

- un operador miembro no const (`int operator-(int)`): `binary operator requires compatible numeric
  operands`;
- un operador heredado de una base sobre el derivado: `no viable … overload of 'op_Modulus' accepts the
  operand/argument types`.

Contraparte: el mismo operador sobre la base pasa.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t12-operadores-f98rujsq` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t12-operadores-sp0kbekc` |

Regresión: ver [TANDA-12-VALIDACION-AGRUPADA.md](../TANDA-12-VALIDACION-AGRUPADA.md).

## Límites

- Un operador miembro no const, uno heredado de una base y los operadores que NativeSharp no tiene
  (`=`, `()`, `[]`, `->`) no se importan.
- Un operador que devuelve una referencia (`operator+=`) queda fuera, como en la tanda 10.
