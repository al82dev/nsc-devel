# T10-002: directivas using namespace

## Defecto

`namespace Facade { using namespace Core; }` permite en C++ escribir `Facade::add` para la función
`Core::add`. En NativeSharp, `T10.Facade.add` daba `unknown identifier`.

Causa en el código: `addAlternateFunctionNames` (`ClangSession.cpp`) daba nombres alternativos por
inline namespaces, alias de namespace y declaraciones using
([t10-nombres-namespace.md](t10-nombres-namespace.md)), pero ignoraba `UsingDirectiveDecl`.

## Cambio

`ClangSession.cpp` (`addAlternateFunctionNames`) recoge las directivas using de los namespaces con
nombre. Una directiva en el namespace `N` que nomina `M` da a cada función de `M`, y de los
namespaces anidados en `M`, el nombre `N::…::f`, como la búsqueda calificada de C++:

- es transitiva y se compone con los alias: cada nombre nuevo se expande otra vez, con un límite que
  hace finito un ciclo de directivas;
- un nombre que `N` declara por sí mismo (directamente, por inline namespace o por una declaración
  using) oculta el que llegaría por la directiva.

Una directiva en el ámbito global no da nombres. El resto del camino ya existía: la selección por
demanda, el catálogo precompilado y el índice de funciones de namespace usan los nombres
alternativos sin duplicar el símbolo.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-10-using-namespace/validar.py`, con y sin `--dll`. Encabezado en
`namespace T10`:

- `Core::add`, `Core::twice` y `Core::Sub::deep`;
- `Facade` con `using namespace Core;` y su propia `twice`;
- `Outer` con `using namespace Facade;`;
- el alias `F2 = Outer`.

El consumidor llama a `T10.Facade.add`, `T10.Facade.twice` (la de `Facade`), `T10.Facade.Sub.deep`,
`T10.Outer.add` (dos directivas), `T10.Outer.twice` (la de `Facade`, que oculta la de `Core`),
`T10.F2.add` (alias de un namespace con directiva) y `T10.Core.twice`. De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp (resultado 62);
- exige las 4 declaraciones del IR de NSC con los tipos físicos de Clang;
- consume una plantilla precompilada, con y sin encabezado.

Controles negativos:

- `namespace Shadow { using namespace Core; int add(double); }`: `T10.Shadow.add(1, 2)` se rechaza,
  como en C++, porque la `add` propia oculta la de `Core` (`no overload of namespace function
  'T10.Shadow.add' accepts 2 argument(s)`);
- `using namespace T10::Core;` en el ámbito global no hace visible `add` sin calificar
  (`unknown identifier 'add'`). Es un límite, no un rechazo de C++.

Contrapartes: `T10.Core.add(1, 2)` y `T10.Shadow.add(2.0)` del mismo encabezado pasan.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-using-namespace-b242wl_9` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-using-namespace-fp5_c30w` |

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Solo funciones libres: las variables y los tipos siguen visibles solo por su nombre impreso.
- Una directiva en el ámbito global no da nombres globales.
