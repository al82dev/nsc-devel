# Validacion 210 - Proveniencia SSA nominal intacta

## Alcance

`TF-SSA-273`, variante nominal de `CF-SSA-035`.

## Evidencia estatica

El contrato inicializa `accepted` como verdadero solo para la variante `0`.
Sin embargo, el `else` final tambien se ejecutaba para esa variante y agregaba
un local duplicado. La compilacion de fixture ya habia producido y verificado
un SSA valido antes de esa mutacion; el rechazo posterior provenia del propio
oraculo, no del producto.

## Correccion

La duplicacion de local queda limitada explicitamente a la variante `7`. La
variante `0` conserva el grafo nominal sin mutaciones, mientras las variantes
`1..7` mantienen una corrupcion dirigida cada una.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa. La validacion
queda a cargo del usuario al cerrar todas las correcciones.
