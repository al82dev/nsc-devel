# Validacion 205 - PHI constante en prefijo

## Alcance

`CF-PAS-035`, casos `TF-PAS-273..280`.

## Causa confirmada

El fixture insertaba dos constantes antes del `Phi`. `PhiSimplificationPass`
procesa exclusivamente el prefijo PHI de cada bloque, por lo que el oraculo
pedía una transformacion deliberadamente fuera de la superficie del pase.

## Correccion

El `Phi` se construye ahora al inicio del bloque, conserva su identidad de
resultado y recibe despues las dos constantes bit-a-bit iguales. El oraculo
exige una sola transformacion, opcode `Constant`, identidad de resultado y
valor entero exactos.

No se modifico producto, manifiesto ni plan: el contrato documentado ya era la
simplificacion de un PHI constante valido.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa. La validacion
queda a cargo del usuario cuando termine el conjunto completo de correcciones.
