# Tanda 19, cambio 3: retornos `const T&` de funciones y métodos C++

## Defecto

En la [auditoría inicial](../TANDA-19-AUDITORIA-INICIAL.md), `const int& pick(const int&, const int&)` no se
importa (`C++ reference return type is not yet mapped`). Lo mismo pasa con `std::min`, `std::max` y
`std::array::at const` (tanda 18). Un retorno `T&` no `const` ya se proyectaba como retorno `ref`.

Causa en el código: `CppSemanticFunction.cpp` rechaza un retorno por referencia cuyo referente es `const`, en
funciones (`projectCppSemanticFunction`) y en métodos de instancia (`projectCppSemanticInstanceMethod`).

## Diseño

Un retorno `const T&` es una referencia de solo lectura, como un callable NativeSharp `ref readonly T`: el
resultado es un lugar direccionable, no asignable, cuyos miembros solo se leen.

- `ClassMemberSymbol::cppReadonlyReferenceReturn` marca el callable C++.
- `isReadonlyReferenceReturn` lo consulta además del marcador de sintaxis.
- En una función, el adaptador lo deduce de `cppReturnType`; en un método, `CppImportedInstanceMethod` lo lleva.
- El resultado de una llamada a función de namespace deja de ser asignable si es de solo lectura, como ya pasaba
  en los métodos.

La identidad y NSMeta no cambian.

## Cambio

- `CppSemanticFunction.cpp/.h`, `CppImportedInstanceMethod.h`, `ProjectMemberModel.cpp`.
- `ClassMemberSymbol.h`, `ReadonlyReferenceReturn.h`.
- `MethodBodyBuildCoordinator.cpp`, `CallAnalyzer.cpp`.

No se modificó Clang ni NSLink.

## Evidencia

Fixture nuevo `pruebas-tanda-19-referencias`, con y sin `--dll`:

- `A::pick(in a, in b)` y `A::limit()` devuelven `const int&`;
- el método const `A::V::get(i)` devuelve `const int&`;
- `int& A::V::slot(i)` recibe una escritura.

Resultado 4759, igual que Clang, O0–O3. Rechazos genuinos, también en Clang
(`cannot assign to return value`): `A.pick(...) = 1` y `v.get(1) = 2`.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-referencias-5y9x0oam` |
| DLL | `A:/temp/nsc-t19-referencias-xir5rsy_` |

## Pruebas anteriores

Dos negativos esperaban el rechazo de `const T&`, que ya no existe:

- `pruebas-tanda-10-referencias` (`retorno-const`, `T10::view()`);
- `pruebas-tanda-11-metodos` (`retorno-const-ref`, `C::view() const`).

Cada uno se convierte en una comparación positiva con Clang (3 y 5) y conserva un rechazo genuino aparte:
escribir a través del resultado, que Clang también rechaza (`cannot assign to return value`).

## Límites

- La vida de la referencia sigue la regla de T10-003. Queda ligada al único parámetro `ref`/`in`, o a
  almacenamiento estático si hay varios; C++ no da una prueba.
- `T&&` sigue sin proyectarse.
