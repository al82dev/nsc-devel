# T09: tamaño de punteros a records importados

NSC rechazaba los modos de tamaño de subobjeto al pasar la dirección de una
variable puntero a un record C o C++, porque su identidad no empieza por
`builtin:`. La emisión acepta ahora tipos puntero y consulta el tamaño del
tipo LLVM, sin confundirlo con el tamaño del record apuntado.

## Validación

Compilación incremental de `nsc`. Las pruebas existentes de subobjetos se
ampliaron con un puntero local y un puntero dentro de un struct; se comparan
los modos 0–3 estáticos y dinámicos contra Clang, por separado con la ABI
predeterminada y SysV, en O0–O3, enlace estático y DLL. Se ejecutan también
dieciséis consumidores precompilados por lenguaje y convención, con y sin
encabezado.

- C: `A:/temp/nsc-t09-c-size-record-z4peyyiy` y
  `A:/temp/nsc-t09-c-size-record-cv1_xjw9`.
- C++: `A:/temp/nsc-t09-size-record-5ztn01z0` y
  `A:/temp/nsc-t09-size-record-92tqck1d`.

## Límites

Las pruebas cubren punteros a records en Windows x64. No acreditan todas las
expresiones, tipos y convenciones. El ítem T09-003 permanece abierto.
