# T08-D06: `&` de una global C `const` produce un puntero a dato constante

Fecha: 2026-11-09. Casillas: T08-D06 y T08-004-04a1 ([decisión anterior](t08-globales-escalares-calificados.md)).

## Problema

En C, `const int *p = &g_ci;` es válido, y escribir mediante `p` o guardar `&g_ci` en un `int *` es un error.
NativeSharp rechazaba el `&` con «cannot take the address of a qualified C/C++ global». Para un miembro de
almacenamiento C `const` (`&g_cp.b`, `&ptr_const->a`, `&matriz_const[1][1].value`) daba «cannot take the address
of readonly storage until qualified pointers are supported». Sin embargo, NativeSharp ya representa `T const*`.

## Cambio

- `IdentifierAnalyzer.cpp`: el rechazo del `&` de una global C se limita a las que necesitan un tipo de dirección
  calificado que NativeSharp no tiene:
  - `volatile` y `const volatile`;
  - atómicas;
  - globales cuyo tipo es un puntero a dato `const` o `volatile` (`&g_pci` sería `int const**`).

  Una global `const` es direccionable; su resultado conserva `readonlyReceiver`.
- `MemberAccessAnalyzer.cpp`: un miembro de almacenamiento C `const`, o un miembro `const`, es direccionable,
  salvo que el propio miembro sea un puntero a `const` (calificador anidado).
- `analyzeAddressOf` ya marca `pointerPointeeReadonly` cuando el operando es de solo lectura. Así, quedan
  rechazadas, con los diagnósticos ya existentes:
  - la escritura mediante el puntero: «left side of assignment is not assignable»;
  - la conversión a `T*`: «would discard pointee constness».
- Una referencia mutable (`ref int r = ref g_ci`) sigue rechazada, porque exige almacenamiento asignable.

## Pruebas

**Nueva:** `pruebas-tanda-08-globales-c/validar-direccion-const.py`. Aprobada en O0–O3 (rechazos en O0).

- **Control Clang.** La lectura por puntero `const` funciona; la escritura y la pérdida de `const` son errores
  (`-Werror`).
- **Casos positivos.** En tres formas de consumo: directo con biblioteca estática, datos `__declspec(dllimport)`
  de una DLL, y plantilla precompilada consumida sin encabezado.
  - `&g_ci`, `&g_cd`, `&g_cp`, `&g_cp.b` y `&r->a` (con `r` de tipo `Pair const*`);
  - paso a funciones C `const T *`;
  - igualdad con la dirección que devuelve C.
- **Rechazos.**
  - escritura por el puntero (con tipo explícito y con `var`);
  - `int*` desde `&g_ci`, `Pair*` desde `&g_cp` e `int*` desde `&g_cp.a`;
  - escritura de un miembro mediante el puntero al record y mediante la dirección del miembro;
  - `&` de una global `volatile`, de una `const volatile` y de un puntero a `const`, que conservan
    «cannot take the address of a qualified C/C++ global».

**Actualizadas** (fijaban el rechazo anterior; autorización del usuario, punto 6):

| Prueba | Antes | Ahora |
|---|---|---|
| `pruebas-proyecto/ejecutar-cpp.cmd`, caso `addrconst` (`int* p = &tools.limit`) | «cannot take the address of a qualified C/C++ global» | «would discard pointee constness» |
| `pruebas-tanda-08-record-c/validar.cmd`: `ConstRecordAddress`, `ConstRecordPointerAddress` y `ConstRecordMatrixAddress` (`int* = &…value`) | «cannot take the address of readonly storage…» | «would discard pointee constness» |
| `tests/unit/CppClangSessionTests.cpp`, globales `const`: escenario 2 (`int* p = &tools.cpp_constant`) | rechazo del `&` | «would discard pointee constness» |
| `tests/unit/CppClangSessionTests.cpp`, globales `const`: escenario 0 | solo lectura de la global | lee además por `int const* p = &tools.cpp_constant` |
| `pruebas-tanda-08-globales-c/validar-escalares-calificados.py`: rechazo `direccion-const` | `&` rechazado | caso `int* p = &g_ci` («would discard pointee constness»); el `&` válido se prueba en la suite nueva |

Los casos de `volatile` y de puntero a `const` de `ejecutar-cpp.cmd` y de las pruebas unitarias no cambian.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d06.cmd`, resultado 0:

- la suite nueva;
- `record-c/validar.cmd`;
- `pruebas-proyecto/ejecutar-cpp.cmd`;
- `nsc_cpp_session_tests`, compilado de forma incremental y ejecutado con `ctest -R nsc.cpp-interop.session`;
- el conjunto de D02: calificadores, callbacks, punteros, records, typedef, globales y plantillas.
