# Tanda 10: auditoría inicial de nombres y operaciones C++

Base: `4d40c9373` (cierre de la tanda 9). Destino: `x86_64-pc-windows-msvc`, Clang 23.1.0.

## Método

Sonda `pruebas-tanda-10-auditoria/sonda.py`, con 49 casos. Cada caso:

1. compila con Clang una biblioteca C++20 estática a partir de un encabezado y una implementación;
2. la consume desde un proyecto NativeSharp con `CppLibraryReference`, en O0;
3. ejecuta el resultado, que devuelve 0 si los valores coinciden.

La sonda no prueba DLL, plantilla precompilada ni otros niveles de optimización. Evidencia:
`A:/temp/nsc-t10-sonda-b5bb2s48`.

## Resultado

### Pasan (compila y ejecuta con 0)

| Ítem | Casos |
|---|---|
| T10-001, T10-002 | namespaces anidados (`A::B` y `A::B::C`), homónimos en namespaces distintos, inline namespace por el nombre exterior, `extern "C"` dentro de un namespace, un namespace reabierto con sobrecargas |
| T10-003, T10-012 | funciones libres con escalares, punteros, `bool`, `char` y `noexcept`; función global sin namespace |
| T10-004 | variables globales C++ escalares, mutables y `const` |
| T10-005, T10-013 | métodos estáticos y su sobrecarga; clase con miembros privados y método público |
| T10-006, T10-014, T10-015 | campos estáticos escalares (lectura, escritura y `const`) |
| T10-008 | sobrecarga por tipo, aridad, puntero frente a entero, `const char*` frente a `char*`, promoción y conversión; una llamada exactamente ambigua se rechaza (`ambiguous namespace function call`) |
| T10-011 | todos los casos que pasan enlazan por el símbolo de Clang |

### Rechazos y defectos

| Ítem | Caso | Resultado |
|---|---|---|
| T10-002 | `A.v1.f` (inline namespace por su nombre) | `unknown identifier 'A'` |
| T10-002 | alias de namespace `namespace AB = A::B` | `unknown identifier 'AB'` |
| T10-002 | declaración `using A::f` en `B` | `unknown identifier 'B'` |
| T10-002 | función en namespace anónimo | no visible (`unknown identifier`). Correcto: no tiene enlace externo |
| T10-003 | parámetros `int&`, `const int&` y retorno `int&` | la función no se importa: el namespace entero queda sin miembros |
| T10-004 | global de tipo record | `C/C++ global type is not yet mapped` |
| T10-004 | global `thread_local` | `thread-local C/C++ global is not yet projected` |
| T10-005, T10-013 | método estático de una clase anidada (`A::S::T::f`) | `no compatible member named 'T'` |
| T10-006 | campo estático `static constexpr int k = 12` | error de enlace: el símbolo `?k@S@A@@2HB` no existe en la biblioteca |
| T10-006, T10-014 | campos estáticos de tipo arreglo y record | `no compatible member named …` |
| T10-007 | métodos estáticos privados y protegidos | se rechazan, pero el diagnóstico dice `no compatible member named …` sin mencionar el acceso |
| T10-010 | argumentos predeterminados, literales y `constexpr` | `no overload … accepts 1 argument(s)` |
| T10-012 | función `inline` definida en el encabezado y no emitida por la biblioteca | error de enlace por símbolo no resuelto |
| T10-016 | `operator+` libre y miembro, `operator==` libre | `binary operator requires compatible numeric operands` o `C8 struct root member has no value layout` |
| tanda 9 → 10 | enums anidados en clases (con y sin ámbito) y métodos estáticos que los usan | `no compatible member named 'E'` |

### Conversiones implícitas (T10-009)

NSC resuelve con sus propias conversiones implícitas, no con las de C++:

- `m(long)` frente a `m(short)` con un argumento `int`: en C++ es ambigua. NSC elige `long`, porque
  NativeSharp no estrecha `int` a `short` de forma implícita.
- `z(long long)` frente a `z(double)` con `int`: también ambigua en C++. NSC elige `long long`.

El diseño de T10-009 debe fijar esta regla y probar que un empate según NativeSharp sigue siendo un
error.

## Código relevante

| Componente | Archivo |
|---|---|
| Captura de funciones, métodos, globales y campos estáticos | `src/Interop/Cpp/ClangSession.cpp` |
| Inventario y grupos de sobrecarga | `src/Interop/Cpp/CppSemanticInventory.cpp`, `CppSemanticFunctionGroups.cpp`, `CppSemanticFunction.cpp` |
| Proyección a funciones de namespace NativeSharp | `src/Frontend/ProjectNamespaceFunctionModel.cpp`, `ProjectNameLookup.cpp` |
| Catálogo precompilado | `src/Interop/Internal/CppCRecordMetadataCodec.cpp` |
| Emisión de definiciones por Clang | `CppLifetimeObjectEmitter.cpp` y `DriverArtifactGeneration.cpp`. Ya emiten las definiciones `static inline` de C y los miembros implícitos de C++. Son la vía para las funciones `inline` y los miembros `constexpr` sin símbolo. |
