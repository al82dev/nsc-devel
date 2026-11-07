# T08: promoción entera en TLS C atómico estrecho

El control C `Promotion.c` ejecuta división y residuo sobre `_Atomic(short)`,
`_Atomic(signed char)` y `_Atomic(unsigned short)` usando divisores que no
caben en el almacenamiento. Clang devuelve cero para los cocientes y conserva
30000 y 60000 en los residuos: estrechar el divisor antes de operar cambiaría
el resultado y podría producir una división por cero incorrecta.

`Promotion.ns` registra el consumo equivalente. La prueba comprueba el
diagnóstico de que estos TLS aún no se proyectan; ese rechazo no constituye
interoperabilidad. El control agrupado pasó sin reconstruir NSC, registro
`A:/nsc-interop-build/tanda08-atomic-promocion-matriz.log`. El siguiente cambio
debe conservar la promoción de operandos y la atomicidad hasta la conversión
de vuelta al ancho de almacenamiento; queda pendiente su implementación y
validación, junto con el gate completo de T08.
