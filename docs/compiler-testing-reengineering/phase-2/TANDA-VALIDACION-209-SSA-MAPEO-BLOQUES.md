# Validacion 209 - Corrupcion observable del mapeo SSA

## Alcance

`TF-SSA-294`, variante `5` de `CF-SSA-037`.

## Causa confirmada

El fixture intentaba sobrescribir con `nullptr` un bloque fuente ya registrado.
`SsaFunction::mapBlock` usa insercion sin reemplazo, por lo que la operacion era
un no-op y el grafo seguia siendo valido.

## Correccion

La variante sustituye por `nullptr` la primera entrada de la secuencia de
bloques de salida. La corrupcion es observable por el verificador y no depende
de la politica de insercion del indice bidireccional.

No se modifico producto ni ninguna otra variante del contrato.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa. La validacion
queda a cargo del usuario al cerrar todas las correcciones.
