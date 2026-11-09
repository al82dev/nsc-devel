# T08-004-04d: `_Atomic` en globales C externas y `static`, con lectura y escritura cruzadas

Fecha: 2026-11-08. Casilla: T08-004-04d.

## Resultado

Verificada sin cambios en el compilador. Queda registrado un límite: `_Atomic` sobre un record.

## Prueba

Nueva: `pruebas-tanda-08-globales-c/validar-atomicas.py`. Resultado: aprobada en O0–O3, con rechazos en O0.

- Globales atómicas externas: `int`, `long long`, `unsigned`, `unsigned long long`, `short`,
  `unsigned char`, `_Bool`, `float`, `double`, `int *` y un enum.
- Una `static _Atomic int` definida en el encabezado, con accesores C `static inline` que usan
  `__c11_atomic_load`/`fetch_add`.
- Cruce entre los dos lados:
  - NativeSharp escribe todas las globales y C las lee con `__c11_atomic_load`;
  - C escribe con `__c11_atomic_store`/`fetch_add` y NativeSharp lee;
  - NativeSharp lee y escribe la `static`, y los accesores C ven los mismos valores. Es el mismo objeto
    interno del proyecto.
- IR: todas las lecturas y escrituras son `load/store atomic … seq_cst`, también la de la `static`. `double`
  usa el portador `i64` con `bitcast`.
- Las mismas comprobaciones se hacen con un control Clang, directo, y desde una plantilla precompilada
  consumida sin el encabezado.
- Rechazos:
  - `&a_int`: «cannot take the address of C atomic storage until atomic-qualified pointers are supported»;
  - una global `_Atomic Pair` (record): «cannot import C/C++ global 'a_pair': C/C++ global type is not yet
    mapped».

## Límite registrado (T08-004-04d1)

C11 admite `_Atomic` sobre un struct. Clang lo trata sin bloqueos si mide 1, 2, 4 u 8 bytes y está alineado,
y con llamadas a la biblioteca atómica en los demás casos. NativeSharp no importa esas globales. Soportarlas
exige elegir el portador atómico del record y decidir qué hacer con los no libres de bloqueo.
