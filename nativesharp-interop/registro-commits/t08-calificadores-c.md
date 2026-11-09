# T08-015: calificadores C

Fecha: 2026-11-08. Casillas: T08-015-06b, 06c y 07a–d.

## Resultado

- **Cerradas** (verificadas sin cambios en el compilador): T08-015-06b, 06c y 07a–d.
- **Bloqueadas** por la decisión pendiente T08-010-05a, sobre el modo volatile de un alias o de la unidad
  consumidora: T08-015-03a–d, 04a–e, 05a–c, 06a, 08, 09 y 10.

  La protección temporal sobre `CppConsumerVolatileMode` sigue activa. 08 pide retirarla solo cuando el contrato
  esté completo, y ese contrato depende de esta decisión.

## Prueba

Nueva: `pruebas-tanda-08-calificadores-c/validar-calificadores.py`. Aprobada en O0–O3, con los rechazos en O0. Se
ejecuta en el camino directo y en una plantilla precompilada consumida sin encabezado.

- **06b — `volatile` nativo sin encabezado.** Un campo `volatile int` de una clase NativeSharp emite accesos
  `volatile` sin orden atómico, en un proyecto sin encabezados C.
- **06c — `volatile` nativo con encabezado.** El mismo campo, en un proyecto que importa un encabezado C en modo
  `Microsoft`, produce exactamente los mismos accesos. Importar C no cambia el contrato nativo.
- **07a — escritura por `const T*`.** Escribir por un `const int*` devuelto por C (guardado con `var`) se rechaza:
  «left side of assignment is not assignable».
- **07b — acceso por `volatile T*`.** Un `volatile int*` devuelto por C se lee, se escribe y se indexa, y C
  comprueba los valores. El IR conserva `volatile` en los cuatro accesos, igual que Clang en tipo y número.
  - **Orden pendiente.** Clang con `-fms-volatile` emite acquire/release. NSC emite accesos simples a través del
    alias NativeSharp (`var`). Es el mismo caso que T08-010-05a; la prueba lo informa y no lo fija.
- **07c — `const volatile T*`.**
  - Se cubre en campos (`cv`, `pcvi`), en globales (`g_cv`, `g_pcv`) y en parámetros (`read_cv`). Agregar
    `const volatile` al pasar `int*` o `volatile int*` es implícito.
  - Se rechazan:
    - escribir el campo, el apuntado o la global: «not assignable»;
    - quitar las calificaciones: «would discard pointee constness», «would discard pointer pointee volatile
      qualification».
- **07d — `T* const` y `T**`.**
  - En un campo `int *const`, escribir por el apuntado se admite y reasignar el puntero se rechaza.
  - Los campos `const int*`, `volatile int*` e `int**` se leen y escriben según su calificación.
