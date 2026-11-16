# T17-001 a T17-006: diagnósticos de importación, uso, sobrecarga, plantilla y enlace

## Comprobación

Este cambio comprueba los diagnósticos de los cambios [1](t17-ubicaciones.md), [2](t17-sobrecargas.md) y
[3](t17-simbolos.md); no cambia código de producción.

## Evidencia

Fixture nuevo `pruebas-tanda-17-diagnosticos/validar.py`, con y sin `--dll` (biblioteca estática o DLL con
su biblioteca de importación), con NSLink y lld-link donde enlaza:

- **Importación** (T17-001, T17-002): `Main.ns:4:12: error: ... C/C++ function 'A::take' cannot be imported:
  ... pointer to member` y `note: 'A::take' is declared at .../api.h:10:...`.
- **Sobrecarga** (T17-003): aridad (`takes 2 argument(s)`), tipos (una nota por cada `A::f`, con
  `argument 1 of type 'string literal' does not convert to ...` y su línea) y ambigüedad (`A::g(int)` y
  `A::g(long)`). El control de Clang rechaza la aridad y los tipos; la contraparte correcta devuelve 9 en
  NativeSharp y en C++.
- **Plantilla** (T17-004): las notas `invalid operands to binary expression` e `in instantiation of
  function template specialization` con `api.h:<línea>:<columna>`; Clang rechaza el mismo uso.
- **Ausentes** (T17-005): `CppLibraryReference header does not exist: 'falta.h'` y `referenced library does
  not exist: '.../falta.lib'`, distintos.
- **Símbolos** (T17-005, T17-006): con los dos enlazadores, `A::two` declarada y no definida dice `is not
  defined by any linked library` con su declaración; `A::one(int)` declarada y `A::one(long long)` definida
  dice `the header and the library disagree on it` y `defines 'int __cdecl A::one(__int64)'`. El control de
  Clang no enlaza ninguno de los dos (`undefined symbol`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa | `A:/temp/nsc-t17-diagnosticos-vghv0ncx` |
| DLL | pasa | `A:/temp/nsc-t17-diagnosticos-f1n2ug5g` |

## Límites

- `int` y `long` son ambos `int` en NativeSharp: una sobrecarga C++ `g(int)`/`g(long)` es ambigua desde
  NativeSharp, mientras C++ elige `g(int)` exactamente. El diagnóstico muestra los dos candidatos; distinguir
  esas sobrecargas queda pendiente.
