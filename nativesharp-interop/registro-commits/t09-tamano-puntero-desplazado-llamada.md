# T09: llamada en el desplazamiento de un puntero

El desplazamiento `p + (obtenerIndice() + 0)` se rechazaba en los modos de
subobjeto 1 y 3, aunque Clang lo admite. NSC ahora acepta la llamada directa
como parte del desplazamiento de una variable puntero o un parámetro, y
consulta el tamaño sobre el puntero ya evaluado. Las lecturas volátiles y
otras formas no clasificadas conservan sus restricciones.

## Validación

- Construcción incremental de `nsc` y verificación del runtime del host.
- `validar-puntero-desplazado.py --solo-efecto`, en C y C++, contrasta los
  ocho modos con Clang para punteros procedentes de un campo y de una reserva
  de pila de tamaño variable. Un contador comprueba una llamada por argumento;
  también cuenta la llamada que calcula el tamaño de la reserva. Se probaron
  O0–O3, biblioteca estática y DLL, y dos consumidores precompilados por caso,
  con y sin encabezado, en las convenciones predeterminada y SysV.
- C++ predeterminada: `A:/temp/nsc-t09-pointer-offset-hqs4jqc3`; SysV:
  `A:/temp/nsc-t09-pointer-offset-ns4c9wf_`.
- C predeterminada: `A:/temp/nsc-t09-pointer-offset-5dovvro7`; SysV:
  `A:/temp/nsc-t09-pointer-offset-juwczr_k`.

## Límites

Se acreditan las expresiones y convenciones probadas en Windows x64; no todas
las formas de efectos, bases, parámetros, índices y reservas. T09-003 sigue
abierto.
