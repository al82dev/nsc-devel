# T10-001, T10-008, T10-009, T10-011 y T10-017: identidad, sobrecargas, conversiones y símbolos C++

Este registro fija el diseño que ya aplican los cambios de la tanda 10 y lo prueba con una biblioteca
compilada por Clang oficial. No cambia código de producción.

## T10-001: identidad de una declaración C++

NSC identifica cada función C++ importada por cuatro datos que entrega Clang
(`ClangSession.cpp`, `CppSemanticFunction.cpp`):

- el USR de Clang (`clangUsr`), que distingue la declaración entre unidades;
- el símbolo de enlace (`externalSymbol`), con el mangling de Clang;
- el nombre calificado impreso por Clang y sus nombres alternativos (T10-002);
- la identidad canónica `símbolo|resultado(parámetros)` con los tipos canónicos de Clang
  (`cppCanonicalIdentity`). De ella sale la clave de función invocable
  `cpp:<módulo>::<identidad canónica>`.

Reglas:

- El contexto (namespace, record contenedor) forma parte del nombre calificado y del símbolo. Dos
  homónimos en namespaces distintos son dos funciones.
- Las sobrecargas comparten nombre y difieren en la firma NativeSharp (`signatureKey`, con `ref`/`in`
  para las referencias) y en el símbolo.
- Dos observaciones de la misma declaración (dos encabezados, un alias, una declaración using)
  comparten USR y símbolo y se agrupan en una sola función (`CppSemanticFunctionGroups.cpp`). Si el USR
  o el símbolo discrepan, el grupo es un conflicto y no se importa.

## T10-008: resolución de una función sobrecargada

Las sobrecargas C++ importadas son sobrecargas NativeSharp del mismo namespace y nombre, o miembros
estáticos sobrecargados del record. Las resuelve el resolvedor de NativeSharp con los argumentos
NativeSharp:

- por tipo, aridad, puntero frente a escalar y puntero a `const` frente a puntero mutable;
- por el modificador `ref`/`in` de las referencias (T10-003);
- con los argumentos predeterminados que Clang reduce a constantes (T10-010).

Una sobrecarga inaccesible no es candidata (T10-007). Un empate exacto se rechaza con
`ambiguous namespace function call`.

## T10-009: conversiones implícitas

NSC aplica las conversiones implícitas de NativeSharp, no las de C++. Decisión:

- Una llamada que C++ encuentra ambigua puede resolverse en NativeSharp. `narrow(long)` frente a
  `narrow(short)` con `3`: NativeSharp no estrecha `int` a `short` y elige `long`. `widen(long long)`
  frente a `widen(double)` con `4`: elige `long long`.
- Una llamada que NativeSharp encuentra ambigua se rechaza aunque C++ la acepte con sus reglas.
- No se aplican las conversiones de C++ que NativeSharp no tiene: estrechamientos implícitos, puntero a
  `bool`, conversiones definidas por el usuario, temporales para `const T&` (T10-003).

Así el programa NativeSharp se lee con las reglas de su propio lenguaje, y la sobrecarga elegida se ve
en la firma NativeSharp de la llamada.

## T10-011: símbolo C++ desde Clang

NSC no hace mangling de C++. `ClangSession.cpp` toma el símbolo de cada función, método y variable
del `MangleContext` de Clang (`mangleName`) al capturar la declaración. NSC lo usa tal cual en la
declaración LLVM, en la importación de DLL y en el catálogo precompilado. Las funciones `extern "C"` usan
su nombre sin decorar. Cada fixture de la tanda compara las declaraciones del IR de NSC con las de
Clang por ese símbolo.

## Evidencia (T10-017)

Fixture nuevo `pruebas-tanda-10-integral/validar.py`, con y sin `--dll`. Una biblioteca C++20 reúne:

- `L::Geo::area(int, int)` y `area(double)`, el homónimo `L::Text::area(const char*)` y el alias
  `L::Fast = Geo`;
- `extern "C" l_plain` dentro de `namespace L`;
- `pick` con cinco sobrecargas: `int`, `int, int`, `int*`, `const char*` y `char*`;
- `widen` y `narrow`, ambiguas en C++ con un `int`;
- `struct Counter` con un campo estático y `add` sobrecargado (`int`, `int, int` y `double`);
- `scale(int, int = 3)` y `twice(int&)`.

De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp (resultado 581). El
  control fija con `(long long)` y `(long)` las sobrecargas que NativeSharp elige donde C++ es ambiguo;
- exige que las 14 declaraciones del IR de NSC lleven el símbolo y los tipos físicos de Clang;
- consume una plantilla precompilada, con y sin encabezado, con el mismo cuerpo.

Controles negativos y su contraparte:

- `L.tie(1, 1)` con `tie(int, double)` y `tie(double, int)` es un empate también para NativeSharp y se
  rechaza (`ambiguous`); `L.tie(1, 2.0)` y `L.tie(2.0, 1)` eligen cada una su sobrecarga exacta;
- `L.Text.area(3)` no tiene sobrecarga que acepte un entero
  (`no viable overload of namespace function 'L.Text.area'`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-integral-hxgscdvi` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-integral-8vm0fy1p` |

## Límites

- Las plantillas de función C++ y su deducción quedan para la tanda 13.
- Las conversiones definidas por el usuario de C++ no intervienen.
- Los métodos de instancia sobrecargados quedan para la tanda 11.
