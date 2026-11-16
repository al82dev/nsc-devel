# Auditoría inicial de la tanda 19

Base: `73673726b`, la tanda 18 con sus pendientes consolidados en
[PENDIENTES-INTEROP-C-CPP.md](PENDIENTES-INTEROP-C-CPP.md). La tanda 19 toma las tareas T19-001 a T19-026 de la
sección 19 de TRACKING. Cada caso se compiló con el compilador de la base (`build-tanda09`, Clang 23.1.0) y,
si compila, se ejecutó. Sondas: `pruebas-tanda-19-auditoria/sonda.py` con `casos-a.py`, `casos-b.py` y `casos-c.py`.

Los controles de Clang están en `pruebas-tanda-19-auditoria/clang.py` (`A:/temp/nsc-t19-clang-bi57o_jx`). Evidencia de las sondas:

- `A:/temp/nsc-t19-sonda-4_id6n5r`;
- `A:/temp/nsc-t19-sonda-w9v9ji12`;
- `A:/temp/nsc-t19-sonda-rvjgvebq`.

## Resultado de cada sonda

| Sonda | Tarea | C++ / C (Clang) | NativeSharp en la base |
|---|---|---|---|
| `g(int)` y `g(long)` con `g(5)` | T19-006 | elige `g(int)` (coincidencia exacta) | `ambiguous namespace function call to 'A.g'` |
| `g(int)` y `g(long)` con un `short` | T19-006 | elige `g(int)` (promoción) | ambigua |
| `g(unsigned)` y `g(unsigned long)` con un `uint` | T19-006 | elige `g(unsigned)` | ambigua |
| `g(long)` y `g(long long)` con un `long` NativeSharp (64 bits) | T19-006 | elige `g(long long)` | elige `g(long long)` (2): correcto |
| `g(long)` y `g(long long)` con `g(5)` | T19-006 | ambigua (dos conversiones) | elige `g(long)` (1): **acepta lo que Clang rechaza** |
| `s.f = null` y `call(null, 3)` con un puntero a función C | T19-002 | válido (`NULL`) | `cannot implicitly convert ... from 'builtin:null' to 'fn:ptr:c:...'` |
| `s.f == null` | T19-002 | válido | `comparison operands have incompatible Stage 34 types` |
| `A.P p;` con `namespace A = B;` | T19-005 | válido | `expected ';', found 'p'` |
| `new A.P()`, `var`, parámetro `A.P` | T19-005 | válido | `unknown type name 'A.P'` |
| `A.get(p)` con el alias | T19-005 | válido | compila y devuelve 7: correcto |
| `const int& pick(const int&, const int&)` | T19-007 | devuelve 9 | `C++ reference return type is not yet mapped` |
| `int& V::at()` | T19-007 | devuelve 6 | devuelve 6: correcto |
| `#define DOBLE(x)` | T19-003 | 42 | rechazo con diagnóstico propio (T14) |
| `union U { int i; float f; }`, `u.i` | T19-004 | 9 | `has no compatible member named 'i'` |
| Bit-field `unsigned v : 6` junto a un bit-field enum | T19-004 | 5 | devuelve 5 (el campo enum no se probó) |
| `~K() noexcept(false)` en un local | T19-009 | válido | `C++ local lifetime currently requires a non-throwing destructor` |
| Miembro privado `k.secret` | T19-012 | error de acceso | `has no compatible member named 'secret'`, sin decir que es privado |
| `int operator[](int) const` | T19-013 | 3 | `reference type has no accessible Stage 38 indexer getter` |
| Campo de una base virtual `d.b` | T19-014 | 4 | `has no compatible member named 'b'` |
| `template <class T> constexpr T cero` | T19-015 | 7 | error de sintaxis |
| `std::pair<int, int>(3, 4)` | T19-010 | 7 | `no accessible constructor accepts 2 argument(s)` |
| `new A.K(4)` con constructor y destructor | T19-011 | 4 | devuelve 4: el local se construye y destruye |
| VLA en un parámetro `int a[n]` | T19-001 | 6 | devuelve 6: correcto |

## Causas confirmadas en el código

- **T19-006.** `selectNamespaceFunctionCandidate` (`Frontend/Bodies/CallAnalyzer.cpp`) ordena las candidatas por
  la conversión NativeSharp. `long` de C++ mide 32 bits en Windows y se proyecta como `int` NativeSharp, así que
  `g(int)` y `g(long)` tienen el mismo parámetro NativeSharp y empatan. El tipo C++ de cada parámetro está en
  `NamespaceFunctionSymbol::cppParameterTypes`, pero la resolución no lo consulta.
- **T19-002.** `classifyNullImplicitConversion` (`Frontend/ProjectTypeConversion.cpp`) solo deja convertir `null` en
  un puntero a función marcado `nullable`. Un puntero a función importado de C tiene convención `c` y no lleva
  esa marca. La comparación usa la misma clasificación.
- **T19-007.** `CppSemanticFunction.cpp` rechaza un retorno `T&` cuyo referente es `const` (`function.returnType.element->isConst`).
  Un retorno `T&` no `const` ya se proyecta como retorno `ref`.
- **T19-005.** Las funciones guardan sus nombres alternativos (`alternateQualifiedNames`, T10-002), pero los
  records no. El nombre de tipo `A.P` no existe para el analizador ni para la búsqueda de nombres.

Las demás filas se investigan en su cambio.
