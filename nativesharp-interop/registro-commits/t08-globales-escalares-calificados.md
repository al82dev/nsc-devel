# T08-004-04a: globales C escalares enteras y flotantes con `const` y `volatile`

Fecha: 2026-11-08. Casilla: T08-004-04a.

## Resultado

Verificada sin cambios en el compilador. Al revisarla apareció una diferencia con C que se registra como
decisión pendiente (T08-004-04a1).

## Prueba

Nueva: `pruebas-tanda-08-globales-c/validar-escalares-calificados.py`. Resultado: aprobada en O0–O3, con
rechazos en O0.

- Globales definidas en C:
  - `signed char`, `unsigned char`, `short`, `unsigned short`, `int`, `unsigned`, `long long`,
    `unsigned long long`, `_Bool`, `float` y `double`;
  - `const int`, `const double` y `const unsigned short`;
  - `volatile int`, `volatile float`, `volatile double` y `volatile long long`;
  - `const volatile int` y `const volatile unsigned char`.
- NativeSharp lee los valores iniciales y escribe las mutables. C comprueba esas escrituras
  (`changed_by_c`). Después C cambia las `volatile` y las `const volatile`, y NativeSharp lee los valores
  nuevos. Las `const volatile` son una vista de solo lectura de un almacenamiento que C modifica.
- Las mismas comprobaciones se hacen:
  - con un control Clang;
  - directo con biblioteca estática;
  - con datos `__declspec(dllimport)` de una DLL publicada junto al ejecutable;
  - desde una plantilla precompilada consumida sin el encabezado.
- IR: `g_vi` y `g_vd` se leen y escriben con `load/store atomic volatile … acquire/release`, que es la
  semántica `volatile` de Microsoft (opción por omisión del destino MSVC). `g_i` no lleva acceso volatile.
- Rechazos (O0):
  - escribir `g_ci`, `g_cd` o `g_cvi`: «left side of assignment is not assignable»;
  - `&g_ci` y `&g_vi`: «cannot take the address of a qualified C/C++ global until qualified pointers are
    supported».

## Decisión pendiente T08-004-04a1

En C, `const int *p = &g_ci;` es válido. NativeSharp sí puede representar ese puntero: `int const*` ya se
obtiene, por ejemplo, con `&g_carr[0]` de un arreglo `const`. Aun así, rechaza `&` sobre una global C
`const`.

Se probó permitirlo: el operador `&` ya marca como de solo lectura el dato apuntado, y una sonda con
`int const* p = &g_ci`, `var p = &g_ci` y `Pair const* p = &g_cp` compilaba. Sin embargo, el rechazo actual
lo fijan pruebas existentes:
- `pruebas-proyecto/ejecutar-cpp.cmd`, caso `addrconst`;
- `tests/unit/CppClangSessionTests.cpp`, escenarios de «cannot take the address of a qualified C/C++
  global»;
- `pruebas-tanda-08-record-c/validar.cmd`, records `const`.

Cambiarlo exige modificar pruebas existentes, así que se revirtió y queda como decisión pendiente.
