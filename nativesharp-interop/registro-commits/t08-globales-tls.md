# T08-004-04e: `_Thread_local` con y sin `_Atomic` en ejecutable y DLL

Fecha: 2026-11-08. Casilla: T08-004-04e.

## Resultado

Verificada sin cambios en el compilador.

## Prueba

Nueva: `pruebas-tanda-08-globales-c/validar-tls.py`. Resultado: aprobada en O0–O3, con un rechazo en O0.

- Globales locales al hilo: `int`, `_Atomic int`, `double` y `_Atomic long long`. Además, una
  `static _Thread_local int` del encabezado con un accesor C `static inline`.
- El hilo principal escribe sus copias y C las lee en ese mismo hilo (`c_tls_sum`).
- Una lambda NativeSharp corre en un hilo creado por C (`CreateThread`). Esa lambda:
  - ve los valores iniciales;
  - escribe sus propias copias;
  - comprueba que C las lee en su hilo.
- Al volver, el hilo principal conserva sus valores, también en la `static` y en su accesor C.
- Variantes:
  - control Clang con las mismas llamadas y un callback C;
  - ejecutable con biblioteca C estática;
  - variables locales al hilo definidas en una DLL C (exportadas como DATA) y publicada junto al
    ejecutable;
  - plantilla precompilada consumida sin el encabezado.
- Rechazo: `&t_ai` da «cannot take the address of C atomic storage until atomic-qualified pointers are
  supported».

## Nota de enlace

Las variables locales al hilo de C dentro del ejecutable (las de la biblioteca estática y la `static` del
encabezado) necesitan el soporte TLS del CRT estático (`_tls_index`, `_tls_used`). Con NSLink, `libucrt.lib`
falla por COMDAT duplicados («'_guard_dispatch_icall' is NODUPLICATES»). Por eso esta prueba enlaza con
lld-link y `libcmt`, `libcpmt`, `libvcruntime` y `libucrt`, como las pruebas TLS existentes
(`pruebas-tanda-08-tls-c`, `static-dos-bibliotecas-mutables/validar-tls.cmd`). NSLink queda fuera de alcance.
