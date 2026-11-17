# T19-001: `_Complex` de C y parámetros C de arreglo de longitud variable

Fixtures nuevos, con y sin `--dll`:

- `pruebas-tanda-19-complejos/validar.py`;
- `pruebas-tanda-19-vla-parametros/validar.py`.

Sondas de desarrollo:

- `tools/t19c/d13/sonda-t19-001*.py` (`logs/sonda1*.log`);
- `tools/t19c/d13/sonda-new-cpp.py` (`logs/sonda-new.log`).

## Defectos

Confirmados con la sonda (`logs/sonda1.log`):

- Un parámetro VLA de una dimensión (`int a[n]`, `int a[static 4]`, `int a[*]`) ya funcionaba: Clang lo ajusta a
  `int*`.
- Uno de varias dimensiones (`double a[n][m]`, `double a[*][*]`, `double (*rows)[m]`) no se importaba:
  - «C/C++ parameter 2 is not yet mapped»;
  - `describeType` no tenía portador para un puntero a VLA ni le daba tamaño, porque el tipo es variablemente
    modificado.
- `_Complex` no tenía portador: no se importaba ninguna función, campo ni global que lo usara («C/C++ return type
  is not yet mapped», «has no compatible member»), ni un typedef de él.

## Decisiones

- **VLA**: un puntero a VLA es un puntero al elemento que queda después de la última dimensión variable, en orden
  por filas, como dispone un VLA NativeSharp `T[n, m]`.
  - Ejemplos: `double a[n][m]` es `double*`, y `double a[n][m][2]` es `double[2]*`.
  - La dirección es la misma, y Clang tampoco comprueba extensiones en una llamada.
  - Un VLA NativeSharp `double[n, m]` se pasa directamente.
- **`_Complex` (decisión del usuario, 2026-11-17)**: un struct C sintético por tipo de elemento, con los campos
  `real` e `imag`, importado como cualquier struct C.
  - El nombre es `__complex_` más el tipo, con `_` por cada espacio: `__complex_double`, `__complex_float`,
    `__complex_long_double`, `__complex_int`, `__complex_unsigned_long_long`…
  - Su representación es la que da C11 6.2.5p13, y Clang lo pasa igual que `_Complex T` en las convenciones del
    destino.
  - El struct no tiene operadores: la aritmética y las conversiones implícitas de C (de real a complejo, entre
    complejos) siguen en T19-039.

## Diseño

- `ClangSession.cpp`:
  - `flattenVariableArrayPointers` reescribe el puntero a VLA antes de describirlo y conserva la ortografía
    original. Un puntero tiene tamaño aunque su dato apuntado sea variablemente modificado, como un callback con un
    parámetro VLA.
  - `synthesizeComplexCarriers` declara en la unidad, tras el análisis, un `RecordDecl` por cada `_Complex T` que
    usan sus funciones, variables, typedefs y records. En C++ es un `CXXRecordDecl` trivial, porque `_Complex` es
    allí una extensión de Clang.
    - Se ubica en la primera declaración que usa el tipo, así que el inventario tiene un origen.
    - No se declara si la unidad ya tiene ese nombre.
    - El mismo struct de otro encabezado es el mismo tipo C por su identidad de Clang (T08-D09).
  - `describeType` describe `_Complex T` como ese record (`complexCarrierRecord`).
  - Una función C++ con un `_Complex` por valor conserva el plan ABI de Clang, como una con un record, para
    `sysv_abi` y `__vectorcall`.
- `CppSemanticFunction.cpp`:
  - Un puntero a un arreglo fijo de escalares también se mapea en un puntero a función C (`cScalarArray`), porque
    es el tipo ajustado de un parámetro de callback `double a[*][3]`.
  - Una función `extern "C"` de un encabezado C++ toma y devuelve por valor los records C++ triviales del
    encabezado, también por sus callbacks (`cCallbackRecordValueOfCFunction`). Antes no se importaba ninguna que
    pasara un struct por valor (`logs/sonda1j.log`, caso `cpp-extern-c-struct`).
- `QualifiedTypeNames.cpp`: el typedef de un callback con un puntero a arreglo fijo de escalares se escribe
  `double[3]*`.

No se modificó Clang ni NSLink. El contrato NSMeta no cambia: el struct sintético viaja en el catálogo como
cualquier record C, y el tipo ajustado de un parámetro VLA como cualquier puntero.

## Qué funciona, como en Clang

**VLA**:

- en funciones:
  - `T a[n]`, `T a[static N]` y `T a[*]`;
  - `double a[n][m]`, `double a[*][*]`, `double (*rows)[m]` y `const double a[n][m]` como `double*`;
  - `double a[n][m][2]` como `double[2]*`;
  - escribir por el parámetro;
  - pasar un VLA NativeSharp `double[n, m]`;
- en callbacks C: `double a[*][*]` como `double*` y `double a[*][3]` como `double[3]*`.

**`_Complex`**:

- parámetros y resultados `float`, `double`, `long double`, `int` y `long long`, con `cdecl`, `__vectorcall` y
  `sysv_abi`;
- campos de records, globales y arreglos globales (también `dllimport`), punteros, callbacks y typedefs (`cplx`);
- leer y escribir `real` e `imag`;
- crear el valor con `new __complex_double()`;
- lo mismo desde un encabezado C++, con funciones `extern "C"` y una función C++ decorada.

## Evidencia

| Fixture | Resultado | Estática | DLL |
|---|---|---|---|
| `pruebas-tanda-19-vla-parametros` | 210522 | `A:/temp/nsc-t19-vla-parametros-zrpp5j7x` | `A:/temp/nsc-t19-vla-parametros-6jljsm4i` |
| `pruebas-tanda-19-complejos` (C) | 211360 | `A:/temp/nsc-t19-complejos-o4zw20jb` | `A:/temp/nsc-t19-complejos-dmh8_jp4` |
| `pruebas-tanda-19-complejos` (C++) | 304205 | `A:/temp/nsc-t19-complejos-o4zw20jb` | `A:/temp/nsc-t19-complejos-dmh8_jp4` |

Todos son O0–O3, directos y desde la plantilla precompilada con y sin encabezado.

Rechazos genuinos, también en Clang:

- pasar un `const double*` a un parámetro `double a[n][m]`, porque descarta `const`;
- pasar un `int*` a un parámetro `double a[n][m]`;
- pasar un `double _Complex*` a un parámetro `float _Complex*`.

Regresión con este cambio:

- Tandas 10 a 19 en estática: 75 de 75 (`logs/regresion-t1*-d53.log`).
- Tanda 9 en estática: pasan 1093 de 1099 y se omiten 2 que exigen parámetros (`logs/regresion-t09-d53.log`).
  - Fallan, como en la línea base, los dos `cpp-variadica-preserve-none` (bloqueo de Clang, T19-032).
  - Bajo carga fallan dos de `cpp-tamano-objeto` (T19-026); repetidos solos, pasan (`logs/regresion-d53r.log`).
- DLL de las tandas 9 a 19: pasan 69 de 74 (`logs/regresion-dll-t*-d53.log`).
  - En los cinco que fallan, el control de Clang tardó unos 30 s y terminó con 0, la falla conocida del entorno:
    `cpp-tamano-objeto`, `tanda-13-especializaciones`, `tanda-19-atomicas-direccion`, `tanda-19-referencias` y
    `tanda-19-uniones`.
  - Repetidos, pasan los cinco (`logs/regresion-dll-d53r.log`).
- `ninja -k 0`, los nueve CTest de interop y la identidad de NSC pasan (`logs/lote-ct2.log`).
- Los tres guiones de T08 (`validar-atomicas.py`, `validar-tls.py` y `validar-atomicas-record.py`) terminan con 0
  (`logs/p53.log`).

## Límites

- `new` de un record C++ trivial en el cuerpo de una plantilla precompilada: «experimental C++ lifetime requires
  direct local or discarded construction».
  - Pasa con cualquier struct C++, no solo con `__complex_T` (`logs/sonda-new.log`).
  - Sigue en T19-016 con los demás objetos C++ en plantilla precompilada.
  - Por eso el fixture C++ no crea el valor con `new` dentro de la plantilla; directo sí funciona.
- Siguen en T19-039:
  - la aritmética y la comparación de C sobre `_Complex`;
  - sus conversiones implícitas, de real a complejo y entre tipos complejos.
