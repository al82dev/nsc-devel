# Tanda de validación 238 — extensión de signo constante

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

`convertConstant` recortaba primero una constante signed al ancho de su tipo
origen y luego la interpretaba con el ancho destino. Al ampliar `int -2` a
`long`, los bits `0xfffffffe` se leían como un positivo de 64 bits en vez de
extender el signo.

## Corrección

Una carga signed se convierte primero a `uint64_t` mediante la conversión
módulo 2^64 definida por C++. La máscara destino común conserva tanto la
truncación hacia anchos menores como la extensión de signo hacia anchos
mayores.

## Alcance

La causa corresponde a `TS-SEMA-285`. Las conversiones de fuentes unsigned y
floating no cambian.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
