# T08-002 H8: catálogo C heredado con solo enums

## Causa y corrección

El catálogo heredado 1.42 con bloque vacío de records comienza con un entero
4, igual que el prefijo de una envoltura posterior. El lector trataba esos
bytes como envoltura y rechazaba el archivo. Ahora la decodificación utiliza
el minor declarado: 1.42 se lee como catálogo heredado; la versión también
viaja al módulo precompilado y se usa en verificación, proyección de ABI y
restauración. No se cambió el formato serializado.

## Evidencia

- Construcción incremental de NSC, identidad y sesión: código 0.
- Validador de identidad: código 0. Se leyeron las 16 variantes representables
  de productores reales en 1.42–1.45, incluido el caso 1.42 solo con enum.
- La sección `catalogo` incluye un consumidor real que enlaza la biblioteca
  con su metadata reescrita a 1.42, llama al método que usa el enum y ejecuta
  con código 0. Salida: `A:/nsc-interop-build/tanda08-catalogo-h8-driver-20261107`.
- `nsc_cpp_session_tests` con Clang 23.1.0: código 0.

La validación dirigida también registra rechazos esperados de otras capacidades
pendientes. No acredita el cierre de T08-002 ni el de la tanda 8.
