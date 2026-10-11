# Validacion 208 - Orden observable de funciones SSA

## Alcance

`TF-SSA-268`, variante `3` de `CF-SSA-034`.

## Causa confirmada

La mutacion invertia la secuencia de funciones para demostrar que el
verificador rechaza un orden distinto al modulo fuente. El fixture contenia
una sola funcion; invertir una secuencia unitaria no cambiaba el grafo y el
verificador lo aceptaba correctamente.

## Correccion

Solo la variante de orden usa un fixture con dos funciones definidas. La
inversion pasa a ser observable y conserva el contrato original sin modificar
el verificador SSA ni las otras siete variantes.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa. La validacion
queda a cargo del usuario al cerrar todas las correcciones.
