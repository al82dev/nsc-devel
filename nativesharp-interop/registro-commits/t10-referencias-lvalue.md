# T10-003: referencias lvalue C++ en funciones libres

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md), una función libre C++ con un parámetro
`int&` o `const int&`, o con un retorno `int&`, no se importaba. NativeSharp la daba por
inexistente (`unknown identifier`).

Causa en el código: `projectFunction` (`CppSemanticFunction.cpp`) mapeaba cada parámetro y el
retorno con `mapFunctionType`. Ese mapeo solo conoce escalares, punteros, enums, records y punteros a
función. Para una referencia caía en `mapCppScalarType`, que la rechaza, y la función entera quedaba
fuera.

En Windows x64, Clang pasa y devuelve una referencia como la dirección de su referente (`ptr`). Es la
misma representación física que NativeSharp usa para `ref`, `in` y los retornos `ref`
(`ParameterPassKind.h`).

## Cambio

En `CppSemanticFunction.cpp` (`projectFunction`, solo funciones con enlace C++):

- `cppReferenceCarrier` convierte una referencia lvalue en un puntero de 64 bits al mismo
  referente. El portador se mapea con las reglas existentes de punteros, así que el referente
  admite los mismos tipos: escalares, enums, records triviales y punteros. Se rechazan los
  referentes `void`, función, arreglo, `volatile` o referencia.
- Un parámetro `T&` pasa a ser `ref T` y `const T&`, `in T`. El tipo NativeSharp sigue siendo
  `T*`, como en una declaración NativeSharp. En la llamada se escribe el modificador, igual que con
  una función NativeSharp.
- Un retorno `T&` pasa a ser un retorno `ref T`. Su vida se liga al único parámetro `ref`/`in`, si
  hay exactamente uno; si no, a almacenamiento estático. Es la misma inferencia que NativeSharp
  aplica a sus propias declaraciones.
- La clave de firma lleva el modificador (`ref`/`in`), así que `o(int)` y `o(int&)` son
  sobrecargas distintas. La identidad C++ usa la escritura canónica de la referencia (`int &`), no
  la del puntero.
- Siguen rechazadas las referencias rvalue (`T&&`) y los retornos `const T&`: NativeSharp no tiene
  un retorno `ref` de solo lectura para importarlos.

Los métodos estáticos usan otra proyección (`projectCppSemanticStaticMethod`) y no cambian.

No se modificó Clang ni NSLink. La plantilla precompilada sin encabezado pasa con el catálogo
vigente; el contrato NSMeta no cambia.

## Evidencia

Fixture nuevo `pruebas-tanda-10-referencias/validar.py`, con y sin `--dll`. Biblioteca C++20 con:

- `inc(int&)`, `read(const int&)`, `slot()` que devuelve `int&` y `pick(int&)` que devuelve `int&`;
- `grow(P&)` y `sum(const P&)` con un record trivial;
- `twice(double&)`;
- la sobrecarga `o(int)` / `o(int&)`.

De O0 a O3, el fixture:

- ejecuta un control C++ compilado por Clang 23.1.0 y un consumidor NativeSharp con los mismos
  cálculos, y exige el mismo resultado (239);
- exige que cada una de las 9 declaraciones del IR de NSC tenga los mismos tipos físicos de
  parámetros y retorno que la de Clang (`ptr` para cada referencia);
- consume una plantilla precompilada de una biblioteca NativeSharp, con y sin encabezado, con las
  mismas comprobaciones.

En C++, `o(q)` con un `int` lvalue es ambigua, así que el control elige cada sobrecarga mediante
un puntero a función. En NativeSharp, `T10.o(q)` llama a `o(int)` y `T10.o(ref q)` a `o(int&)`.

Controles negativos:

- `T10.inc(v)` sin `ref` no se enlaza: `no viable overload of namespace function 'T10.inc'`;
- `take(int&&)` y `const int& view()` no se proyectan. Tras `T10.keep()`, que sí se resuelve, el
  nombre falla con `unknown identifier 'T10'`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-referencias-judhqxgr` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-referencias-bxsbwxmd` |

Subconjunto `pruebas-tanda-09-cpp*` con el compilador nuevo (`logs/regresion-r1-cpp.log`): 79
guiones, pasan 73. Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya
documentada, y dos de `cpp-tamano-objeto` en la corrida paralela, que pasan con un proceso
(`logs/regresion-t09-r1-tamano.log`).

## Límites

- Un argumento `const T&` necesita un lugar (`in v`): NativeSharp no materializa temporales para
  `in`, así que `T10.read(6)` no se enlaza.
- Las referencias rvalue y los retornos `const T&` no se proyectan. El diagnóstico es el de un
  nombre desconocido; un diagnóstico propio queda para la tanda 17.
- Los métodos estáticos e instancias con referencias no cambian (tanda 11 para los de instancia).
