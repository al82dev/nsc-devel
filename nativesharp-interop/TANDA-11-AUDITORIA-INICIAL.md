# Tanda 11: auditoría inicial de clases C++ y vida de objetos

Base: `7c7d76fcf` (cierre de la tanda 10). Destino: `x86_64-pc-windows-msvc`, Clang 23.1.0.

## Método

Sonda `pruebas-tanda-11-auditoria/sonda.py`, con 38 casos. Cada caso:

1. compila con Clang una biblioteca C++20 estática a partir de un encabezado y una implementación;
2. la consume desde un proyecto NativeSharp con `CppLibraryReference`, en O0;
3. ejecuta el resultado, que devuelve 0 si los valores coinciden.

La sonda no prueba DLL, plantilla precompilada ni otros niveles de optimización. Evidencia:
`A:/temp/nsc-t11-sonda-fp52zaz2`.

## Modelo existente en NSC (T11-001)

- Un record C++ completo es un struct NativeSharp con el layout de Clang (tamaño, alineación y
  offsets de campos). Un record trivial es un valor copiable; uno con constructor, destructor o copia
  no triviales es un valor move-only (`OwnershipQueries.cpp`, `typeIsImplicitlyCopyable`).
- Los constructores y el destructor C++ son miembros `isCppExternal` del struct. `new A.C(5)` construye
  un local en su almacenamiento y el destructor se llama al salir del ámbito
  (`ProjectCompilation.cpp`, comprobación de vida experimental). Solo se aceptan constructores con
  parámetros escalares o `void*`, y para un local, constructor y destructor no lanzadores.
- Los métodos de instancia C++ se llaman con el receptor como argumento implícito; solo se importan con
  escalares y punteros (`ProjectMemberModel.cpp`, `setExperimentalCppInstanceMethods`).
- Las referencias NativeSharp `ref`/`in` y los punteros no poseen el objeto: no ejecutan su destructor.

## Resultado

### Pasan (compila y ejecuta con 0)

| Ítem | Casos |
|---|---|
| T11-003, T11-008 | `sizeof` de un record `alignas(16)`; un local `alignas(32)` construido queda alineado |
| T11-004, T11-011, T11-012 | campos públicos de `struct` y `class`, también sobre un objeto construido |
| T11-005, T11-010 | métodos de instancia escalares, sobrecargados, definidos en la clase (`inline`) y con puntero a record, también sobre un objeto construido |
| T11-006, T11-009 | constructor `noexcept` con parámetro escalar y destructor `noexcept` |
| T11-019 | puntero no propietario a un objeto de la biblioteca |

### Rechazos y defectos

| Ítem | Caso | Resultado |
|---|---|---|
| T11-007, T11-013 | `~D();` sin `noexcept` explícito, con constructor `noexcept` | `C++ local lifetime currently requires a non-throwing constructor and destructor`. **Defecto**: en C++ un destructor es `noexcept` de forma implícita. `ClangSession.cpp` descarta la especificación no resuelta (`isUnresolvedExceptionSpec`) en vez de resolverla |
| T11-006, T11-013 | constructor sin `noexcept` (`C(int)`, `C()`, sobrecargas, contadores de destrucción, miembro no trivial) | mismo diagnóstico. Un constructor que puede lanzar necesita la limpieza por excepciones de la tanda 15 |
| T11-005 | métodos llamados `get` o `set` | error de sintaxis: `get` y `set` son palabras reservadas de NativeSharp y el analizador exige un identificador tras `.` |
| T11-005 | método con enum, record por valor, `const T&` o retorno `T&` | `no compatible member named …`: solo se importan escalares y punteros |
| T11-004, T11-005 | campo o método privado | `no compatible member named …`, sin mencionar el acceso |
| T11-006 | constructor privado | `struct has only an implicit parameterless constructor`, sin mencionar el acceso |
| T11-007 | destructor privado | `experimental C++ local requires direct automatic construction and cleanup` |
| T11-006, T11-016 | `M(int) noexcept` junto a `M(M&&) noexcept` | `struct has only an implicit parameterless constructor`: un constructor no proyectable descarta toda la familia de constructores |
| T11-015 | `T(int) noexcept` trivialmente copiable, `A.T b = a;` | `move-only value … cannot be copied`. **Defecto**: el tipo es trivialmente copiable en C++ |
| T11-015, T11-017 | copia con constructor de copia propio o eliminado | `move-only value … cannot be copied`; no hay forma de pedir la copia |
| T11-016 | función que devuelve un record no trivial por valor | la función no se importa (`unknown identifier 'A'`) |
| T11-018 | `new C{x}` y `delete` dentro de la biblioteca | NSLink: `unresolved external symbol '??2@YAPEAX_K@Z'` (`operator new` del CRT) |
| T11-004 | campos de una `union` C++ | `no compatible member named 'i'` |
| T11-005 | operador miembro (`V::operator+`) | `binary operator requires compatible numeric operands` |
| T11-006 | constructor que puede lanzar | rechazado con el diagnóstico de vida no lanzadora (tanda 15) |

## Código relevante

| Componente | Archivo |
|---|---|
| Captura de métodos, constructores y destructores | `src/Interop/Cpp/ClangSession.cpp` |
| Proyección de métodos y de la familia de vida | `src/Interop/Cpp/CppSemanticFunction.cpp` (`projectCppSemanticLifetimeMethod`, `collectCppSemanticLifetimeFamilies`) |
| Miembros importados y sus tipos | `src/Frontend/ProjectMemberModel.cpp` |
| Copia y propiedad | `src/Frontend/MemberModel/OwnershipQueries.cpp` |
| Comprobación de vida de locales | `src/Frontend/ProjectCompilation.cpp` |
| Nombre de miembro tras `.` | `src/Parser/ExpressionParser.cpp` |
