# T09: tamaño oculto de punteros desplazados

NSC rechazaba `p + 1` en llamadas con tamaño de subobjeto 1 o 3, aunque
Clang admite esa expresión. El backend ahora acepta desplazamientos cuya base
es un puntero local o parámetro y cuyo índice es aritmética sin efectos
laterales. El tamaño se consulta sobre el puntero ya calculado; las bases
directas con información de subobjeto y los índices con efectos no se
generalizan con este cambio.

## Validación

- Construcción incremental de `nsc` y comprobación del runtime del host.
- `pruebas-tanda-09-cpp-tamano-objeto/validar-puntero-desplazado.py` contrasta
  los ocho resultados contra Clang como una tupla codificada sin colisiones.
  Prueba puntero a reserva de pila, puntero a campo de arreglo, puntero recibido
  como parámetro y desplazamiento local variable; enlace estático y DLL,
  O0–O3, con las convenciones predeterminada y SysV. Cada caso se ejecuta
  directamente y desde consumidores precompilados con y sin encabezado.
- La misma prueba usa `--c` para biblioteca C; el resultado de Clang se
  contrasta de forma independiente en cada configuración. Se verificó que
  los códigos de control correspondan a tuplas válidas y no a errores.
- C++ predeterminada: `A:/temp/nsc-t09-pointer-offset-zcdim_e3`; SysV:
  `A:/temp/nsc-t09-pointer-offset-xzvv8xcf`.
- C predeterminada: `A:/temp/nsc-t09-pointer-offset-btu1rhx3`; SysV:
  `A:/temp/nsc-t09-pointer-offset-skr4wj4w`.

## Límites

Los casos están probados en Windows x64 y usan desplazamiento positivo de un
entero. Las pruebas no acreditan todas las operaciones, bases de puntero,
expresiones con efectos ni convenciones. T09-003 permanece abierto.
