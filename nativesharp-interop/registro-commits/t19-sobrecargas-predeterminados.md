# T19-006: sobrecargas de métodos y constructores C++, sus alternativas y argumentos predeterminados

Fixture: `pruebas-tanda-19-traslados/validar.py`, casos `sobrecargas-metodos`, `predeterminados` y `constructores`.
Cada caso compara un programa NativeSharp con un control C++ de Clang 23.1.0, O0–O3, con la biblioteca estática y,
con `--dll`, como DLL. `sobrecargas-metodos` y `predeterminados` repiten el programa desde una plantilla NativeSharp
precompilada, con y sin el encabezado C++ visible para el consumidor. Sonda de desarrollo:
`tools/t19c/d6/sonda-t19-006.py` (`logs/sonda6.log`).

## Defectos

- `S::g(int)` y `S::g(long)` (también `e(int)`/`e(long)` de instancia) no llegaban a NativeSharp: `long` de C++ mide
  32 bits en Windows y se proyecta como `int`, las dos firmas NativeSharp coincidían y
  `provideExperimentalCppStaticMethods` / `provideExperimentalCppInstanceMethods` descartaban toda la familia por
  ambigua. Con constructores `V(int)` y `V(long)`, `collectCppSemanticLifetimeFamilies` marcaba una colisión y el
  record perdía todos sus constructores.
- Una llamada fallida a un método o constructor C++ solo decía `no overload of 'f' accepts 0 argument(s)` o
  `ambiguous ...`, sin las alternativas que T17-003 da para las funciones de namespace.
- Un argumento predeterminado de enum, de record o no constante (una global, una llamada) exigía su argumento en
  las funciones (T10-010). Los métodos y constructores no llevaban ningún predeterminado a NativeSharp.
- Una plantilla precompilada que declaraba un valor record C++ (`T.S s; s.f(3)`) no guardaba sus métodos de
  instancia: solo se elegían los de un receptor obtenido de una llamada de fábrica.

## Qué funciona ahora, como en Clang

- Sobrecargas de métodos estáticos, de instancia y constructores que NativeSharp ve iguales: `g(int)` frente a
  `g(long)` con un int elige `g(int)`; `m(int, long)` frente a `m(long, int)` con dos int es ambigua. Con un `float`,
  `f(double)` gana por promoción. La regla es la de las funciones de namespace ([sobrecargas](t19-sobrecargas.md)):
  los rangos de [over.ics.scs] solo deciden donde el orden NativeSharp empata. Donde NativeSharp ya resuelve
  (`h(long)` frente a `h(long long)` con un int), se conserva su elección (T10-009).
- Alternativas de una llamada fallida a un método o constructor C++: una nota por candidato con su declaración C++
  y su motivo, como en las funciones de namespace:
  - `candidate: static int T::S::g(long) takes 1 argument(s)`;
  - `candidate: int T::S::f(double) const: argument 1 of type '...' does not convert to 'double'`;
  - en una ambigüedad, las empatadas: `candidate: static int T::S::m(int, long)`;
  - en un constructor: `candidate: T::V::V(int, int)`.
  Solo cuando todos los candidatos son miembros C++; los miembros NativeSharp conservan su diagnóstico.
- Argumentos predeterminados de enum, de record, de una global y de una llamada en funciones; cualquier
  predeterminado en métodos estáticos, de instancia y constructores. Clang evalúa los omitidos en cada llamada: el
  programa ve el valor actual de la global y cada llamada a `counter()`.
- Los métodos de instancia (con sus sobrecargas y adaptadores) que el cuerpo de una plantilla precompilada llama
  sobre un valor C++ que declara, con y sin el encabezado en el consumidor.

## Diseño

- `ClangSession`:
  - Una función con algún predeterminado que NativeSharp no escribe (`defaultArgumentThunkStart`), un método
    público con predeterminados o un constructor público con predeterminados (no de copia ni de movimiento) tiene
    un adaptador de Clang por cada número de argumentos escritos (`cppDefaultArgumentThunkSymbol`, USR
    `cppDefaultArgumentThunkUsr`). La declaración original exige entonces todos sus argumentos.
  - El adaptador nombra la función o el método solo con los argumentos escritos (`self->m(a0)`, `Owner::m(a0)`,
    `::f(a0)`), así que la resolución de sobrecarga y los predeterminados son de Clang. El de un constructor
    construye en `self` con un placement new de una etiqueta propia (`__nsc_cpp_place_t`), que ningún encabezado
    declara, y devuelve `self` como un constructor de Microsoft. Se compilan con la maquinaria de T12-004.
  - Uno estático o de constructor se llama por su símbolo; uno de instancia, por `dispatchSymbol`.
- `CppSemanticFunction`: `describeCppMethodOverload` da a cada método y constructor sus escrituras C++, un sufijo de
  firma (`@0:long`, `@default`) y su declaración C++. El sufijo separa las sobrecargas en las familias, en la firma
  del miembro y en las validaciones de `ProjectMemberModel`.
- `CppOverloadRanking.h` (nuevo): los rangos aritméticos de T19-006, `compareCppArithmeticSpellings` y
  `cppMemberCandidateNotes`. Los usan `CallAnalyzer` (métodos y funciones de namespace) y el modelo de construcción
  (`nonDominatedConstructorCandidates` y sus diagnósticos).
- Catálogo precompilado, sin cambio de formato:
  - el productor elige los métodos de instancia de cada record C++ trivial del catálogo cuyo nombre un cuerpo
    publicado llama por un acceso a miembro, con todas sus sobrecargas; la resolución queda en el consumidor, como
    para los candidatos de fábrica;
  - un método llamado por un adaptador se guarda con el símbolo del adaptador, y el consumidor lo llama por él.

No se modificó Clang ni NSLink. El contrato NSMeta no cambia.

## Evidencia

Resultados: `sobrecargas-metodos` 36442 con cinco rechazos, `predeterminados` 8357, `constructores` 153574342 con
un rechazo. Evidencia:

| Caso | Estática | DLL |
|---|---|---|
| `sobrecargas-metodos` | `A:/temp/nsc-t19-traslados-oh8n77ni` | `A:/temp/nsc-t19-traslados-84ahw3rb` |
| `predeterminados` | `A:/temp/nsc-t19-traslados-uu4mj8xy` | `A:/temp/nsc-t19-traslados-7xximbrl` |
| `constructores` | `A:/temp/nsc-t19-traslados-qqd15e_6` | `A:/temp/nsc-t19-traslados-sn3ex9yw` |

Regresión de las tandas 10 a 19 con este cambio: 69 de 70 en estática (`logs/regresion-d13e`) y 50 de 51 en DLL
(`logs/regresion-dll-d13d`). La única falla era el negativo obsoleto de `pruebas-tanda-10-predeterminados`.

## Negativos obsoletos convertidos

`pruebas-tanda-10-predeterminados` esperaba que `T10.g()` (predeterminado `compute()`), `T10.m()` (enumerador) y
`T10.mix(1)` exigieran su argumento. Ahora cada uso se compara con un control de Clang (3, 2 y 4), igual que las
llamadas explícitas. Queda un rechazo genuino, también en Clang: `T10.g(1, 2)`, más argumentos que parámetros.
Evidencia: `A:/temp/nsc-t10-predeterminados-s1uq0sk2` (estática) y `A:/temp/nsc-t10-predeterminados-rn4frjl2`
(DLL).

## Límites

- Los objetos C++ con constructores en una plantilla precompilada (el productor exige un record trivial) siguen en
  T19-016; el caso `constructores` es solo directo.
- Una plantilla miembro con predeterminados sigue en T19-015.
