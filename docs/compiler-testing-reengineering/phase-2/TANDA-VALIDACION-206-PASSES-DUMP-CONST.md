# Validacion 206 - Opcode canonico en dump de Passes

## Alcance

`CF-PAS-050`, casos ejecutables `TF-PAS-393..397` y `TF-PAS-399`.

## Causa confirmada

El oraculo buscaba la palabra `constant`, pero `OptimizationDumper` serializa
el nombre canonico proporcionado por `mirOpcodeName(Constant)`: `const`.

## Correccion

La comprobacion exige ahora el fragmento estructural ` = const `, evitando una
coincidencia accidental y conservando las comprobaciones independientes de
identidad MIR, terminador y repetibilidad byte a byte.

Los casos `TF-PAS-398` y `TF-PAS-400` conservan su disposicion de
infraestructura pendiente; no forman parte de esta correccion.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa. La validacion
queda a cargo del usuario al cerrar todas las correcciones.
