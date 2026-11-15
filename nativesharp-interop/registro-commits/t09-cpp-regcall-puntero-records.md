# T09-003: records C++ por valor en punteros `regcall`

## Defecto

Sonda `build-claude-t08/tools/sonda-cpp-regcall-ptr.py`, evidencia
`A:/temp/nsc-t09-sonda-cpp-regcall-ptr-c3gma3d9` y `A:/temp/nsc-t09-sonda-cpp-regcall-ptr-bc019f8k`.
Records C++ triviales por valor en funciones `regcall` de un espacio de nombres, en llamada directa y
por un alias de puntero `using P = … (__regcall *)(…)`:

| Firma | Plan de Clang | Directa | Por puntero |
|---|---|---|---|
| `int f(R{int,int}, int)` | `i64, i32` | pasa | rechazada |
| `R{int,int} f(R, int)` | `i64` / `i64, i32` | pasa | rechazada |
| `E8 f(R{int,int}, E16)` (enums C++) | `i8` / `i64, i16` | pasa | rechazada |
| `int f(R{float,float}, int)` | `float, float, i32` | pasa | pasa |
| `int f(R{int×5}, int)` | `ptr, i32` | pasa | pasa |

Las rechazadas dan `regcall function pointer signature is not yet supported`. Clang compila todos los
casos. Al ampliar el fixture apareció otro caso: la llamada directa `E16 f(RF{float,float}, E8)`,
un enum devuelto junto a un record expandido, se rechazaba con
`regcall currently supports only int scalar signatures …`.

Causa en el código:

- En la sesión, el plan de un alias de puntero C++ `regcall` se adjunta solo si su retorno pasa por
  alguno de los validadores de records expandidos, con o sin retorno indirecto
  (`CppSemanticInventory.cpp`). Lo mismo exigen:
  - la planificación del alias en la sesión viva;
  - la conciliación del plan precompilado (`CppProjectSessions.cpp`).

  Ninguno usaba el validador de records sin expandir, pasados o devueltos como un entero o por
  referencia (`isRegCallWin64NonExpandedRecordsPlan`). Ese validador ya existía para los punteros C
  y para el catálogo.
- Un retorno enum no llegaba a ningún validador en estos sitios:
  - la planificación del alias C++, que solo reconocía enteros, bool, flotantes y punteros;
  - el inventario y el catálogo, que comparaban el tipo del retorno con un entero;
  - la HIR y el emisor (declaración, llamada directa y llamada por puntero), que comparaban la clave
    NativeSharp del retorno, no la de su entero.

## Cambio

- `isRegCallWin64NonExpandedRecordsPlan` se acepta para los alias de puntero C++ `regcall`:
  - al adjuntar el plan en el inventario;
  - al planificar el alias en la sesión viva;
  - al conciliar el plan precompilado.

  Ese validador compara cada parámetro y el retorno con la firma de origen. Antes, la planificación
  ya concilia cada record con el tipo importado: identidad USR, layout, record trivial y un único
  record del catálogo.
- Un retorno enum se lee como su entero subyacente (`sysVCppEnumAsInteger`, o `cppEnumAbiScalarKey`
  para la clave NativeSharp) en estos sitios:
  - las compuertas `regcall` de la HIR (`regCallRecordsReturningScalar`);
  - el emisor (declaración, llamada directa y llamada por puntero);
  - el inventario y la planificación de alias;
  - el catálogo, para funciones y alias.

  Los validadores siguen comprobando el enum contra el plan de Clang (`regCallFloatRecordScalarMatches`).
- En `pruebas-tanda-09-cpp-enum-convenciones/validar.py`, las firmas con record y enums se prueban
  también por puntero `regcall`.

No se modificó Clang ni NSLink. Los planes viajan en los campos existentes y el contrato NSMeta
vigente no cambia.

## Evidencia

### Sondas tras el cambio

- `A:/temp/nsc-t09-sonda-cpp-regcall-ptr-tqxxe2u7`: los once casos de la sonda pasan en directa y
  por puntero. Incluyen enums como parámetro y como retorno junto a records.

### Fixture nuevo

`pruebas-tanda-09-cpp-regcall-puntero-records/validar.py`, con y sin `--dll`, siempre con
`--precompilado`.

Records C++ en el espacio de nombres `T09`:

- de 8, 4, 2 y 1 bytes, pasados como un entero de su tamaño;
- de 20 bytes, por referencia oculta;
- de dos `float`.

Firmas, cada una en llamada directa y por su alias de puntero:

- el record junto a escalares (`int`, `long long`, `double`) o junto a otro record;
- el record como retorno: como entero y por sret;
- un retorno enum (`unsigned char`, `short`) junto a un record entero o a un agregado flotante
  expandido.

De O0 a O3, el fixture:

- ejecuta un control C++ compilado por Clang 23.1.0 y el consumidor NativeSharp con las mismas
  comprobaciones;
- exige en cada declaración del IR de NSC los tipos físicos y las extensiones de cada parámetro y
  del retorno iguales a los de Clang;
- exige `x86_regcallcc` en cada llamada directa y en las diez llamadas por puntero;
- consume una plantilla precompilada con y sin encabezado.

El control negativo comprueba un alias de puntero `regcall` con un struct C++
`{ float a; int : 0; float b; }`. Clang lo expande en dos `float`, y NSC no representa campos de
bits en una expansión, así que lo sigue rechazando con
`regcall function pointer signature is not yet supported`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática + precompilado | pasa (O0–O3) | `A:/temp/nsc-t09-cpp-regcall-puntero-fnmxonx3` |
| DLL + precompilado | pasa (O0–O3) | `A:/temp/nsc-t09-cpp-regcall-puntero-99j2j4bd` |

Antes del cambio, el mismo fixture fallaba en el IR del primer nivel con
`regcall function pointer signature is not yet supported`
(`A:/temp/nsc-t09-cpp-regcall-puntero-tj47yra8`).

### Fixture de enums C++ ampliado

`pruebas-tanda-09-cpp-enum-convenciones/validar.py`, con `regcall` y `--precompilado`:

| Variante | Evidencia |
|---|---|
| estática | `A:/temp/nsc-t09-cpp-enum-convenciones-21ns6xko` |
| DLL | `A:/temp/nsc-t09-cpp-enum-convenciones-7acqis6p` |
| `--espacios` | `A:/temp/nsc-t09-cpp-enum-convenciones-ns-8caovebb` |

Ahora incluye las llamadas por puntero con records.

Regresión de la tanda: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- Los records C++ no triviales (con constructor de copia o destructor propios) no se importan como
  valor. Las funciones que los usan no se proyectan (`sonda-cpp-no-trivial.py`,
  `A:/temp/nsc-t09-sonda-cpp-no-trivial-ve128hn2`). Es un límite previo.
- El acceso a un campo de un record C++ devuelto sin guardarlo antes (`f(4).y`) se rechaza con
  `C++ local storage requires an uncaptured automatic record with layout`. Guardado en una variable
  local, pasa. Es un límite previo del emisor, fuera de este cambio.
