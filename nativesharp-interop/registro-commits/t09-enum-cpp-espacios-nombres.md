# T09-003: enums C++ en espacios de nombres dentro de alias de puntero precompilados

## Defecto

Sonda `build-claude-t08/tools/sonda-ecpp-pre.py`. Un alias de puntero C++ `using P = E (*)(E, int)`,
devuelto por `T09::fetch()`, se usa desde una plantilla precompilada. Si el enum `E` se declara en
el espacio de nombres `T09`, el consumidor rechaza la plantilla con todas las convenciones,
también con `cdecl`, con y sin encabezado:

- `C callback signature contains an unavailable type` (`A:/temp/nsc-t09-sonda-ecpp-pre-r2ngfp2e`).

El mismo enum en el ámbito global pasa (`A:/temp/nsc-t09-sonda-ecpp-pre-ws57btok`). Clang compila
los dos casos.

Causa en el código. Había dos puntos que construían la identidad del enum sin sus espacios de nombres:

- `CppSemanticRecord.cpp`, siembra de enums de los alias (`aliasEnums`). La identidad del catálogo
  se formaba solo con el nombre (`TypeIdentity(módulo, {}, {}, nombre)`). Un enum C++ `T09::E`
  tiene la identidad `T09.E`, así que el alias quedaba sin su enum.
- `collectCppSignatureEnumDependencies` (`CppSemanticInventory.cpp`) marcaba como no proyectable
  cualquier enum cuyo nombre calificado tuviera espacios de nombres. Por eso las funciones con
  ese enum en la firma (`f` y `fetch`, que devuelve el alias) no se restauraban en el consumidor.

Con solo la primera corrección, el error cambió a `unknown identifier 'T09' in callable/initializer
context` (`A:/temp/nsc-t09-sonda-ecpp-pre-w30j8msl`): ya se restauraba el enum, pero no las
funciones del espacio de nombres.

## Cambio

- Siembra de los alias (`CppSemanticRecord.cpp`):
  - un enum C conserva su identidad por nombre;
  - un enum C++ usa `projectCppEnumDeclarationIdentity`, que conserva sus espacios de nombres;
  - la coincidencia por USR de Clang con la declaración no cambia.
- `collectCppSignatureEnumDependencies`:
  - descompone el nombre calificado en espacios de nombres y nombre, y exige segmentos no vacíos y
    que el último sea el nombre del enum;
  - busca el candidato con esa identidad.
- El nombre calificado no distingue un espacio de nombres de una clase contenedora. Por eso, con
  espacios de nombres, la coincidencia exige además:
  - que el tipo tenga USR de Clang;
  - que el candidato sea un enum C++ importado (`CppEnum`) con USR;
  - que los dos USR sean iguales.

  Sin eso, el enum sigue siendo no proyectable.
- Los enums en el ámbito global y los enums C siguen el camino anterior, con la misma
  comprobación de USR.

No se modificó Clang ni NSLink. El catálogo usa sus campos existentes y el contrato NSMeta vigente
no cambia.

## Evidencia

### Sondas tras el cambio

- `sonda-ecpp-pre.py`, con `cdecl` y `regcall` y bases `int` y `unsigned long long`:
  - espacio de nombres: pasan con y sin encabezado (`A:/temp/nsc-t09-sonda-ecpp-pre-s2lne6k1`);
  - ámbito global: siguen pasando (`A:/temp/nsc-t09-sonda-ecpp-pre-eec64axe`).
- `sonda-ecpp-ns.py`, con `cdecl` y `sysv_abi` (`A:/temp/nsc-t09-sonda-ecpp-ns-a2ow6h6p`):
  - pasan con y sin encabezado un enum en `T09::In` y dos enums `E` con el mismo nombre en
    `T09::X` y `T09::Y`;
  - un enum anidado en una clase (`T09::S::E`) sigue rechazado.
- `sonda-ecpp-clase.py` (`A:/temp/nsc-t09-sonda-ecpp-clase-y353miyw`): ese enum de clase tampoco
  se proyecta en sesión viva:
  - en llamada directa, `receiver/type 'type:T09App::T09.S' has no compatible member named 'E'`;
  - por puntero, `C callback signature contains an unavailable type`.

  Es un límite previo de los tipos anidados en clases, no de este cambio.

### Fixture ampliado

`pruebas-tanda-09-cpp-enum-convenciones/validar.py` acepta `--espacios`:

- declara los enums con ámbito en `T09::In`;
- declara el enum sin ámbito, con `INT_MIN` e `INT_MAX`, en `T09::Otro::Mas`;
- las firmas, los cuerpos y las comprobaciones los nombran calificados, en C++ y en NativeSharp.

Lo demás no cambia:

- las 7 convenciones, de O0 a O3;
- el control C++ de Clang 23.1.0;
- en el IR, la convención y las extensiones de cada declaración, de cada llamada directa y de las
  llamadas por puntero;
- la plantilla precompilada con y sin encabezado.

El control negativo nuevo de `--espacios` comprueba que un alias de puntero con un enum anidado en
una clase sigue rechazado con `C callback signature contains an unavailable type`. Así la
identidad con espacios de nombres no confunde una clase con un espacio de nombres. El control de la
unión C solo de `float` con `regcall` se conserva.

Sin `--espacios`, el fixture conserva los enums en el ámbito global y vuelve a pasar en la regresión
completa de la tanda. Los dos controles negativos de `--espacios` pasan en las dos variantes.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática + precompilado + espacios | pasa (7 convenciones × O0–O3) | `A:/temp/nsc-t09-cpp-enum-convenciones-ns-uxcpz7bl` |
| DLL + precompilado + espacios | pasa (7 convenciones × O0–O3) | `A:/temp/nsc-t09-cpp-enum-convenciones-ns-ott8bwx6` |

Regresión de la tanda: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- Los enums anidados en clases (`T09::S::E`) no se proyectan en sesión viva ni en precompilado. Es
  un límite de los nombres anidados C++ y queda para la tanda 10.
