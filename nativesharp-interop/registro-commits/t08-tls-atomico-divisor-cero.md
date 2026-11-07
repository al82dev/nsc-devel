# T08: divisor cero en TLS C atómico

Los controles `ZeroDiv.ns` y `ZeroRem.ns` aplican `/=` y `%=` a TLS atómico,
obteniendo cero de una función C externa para evitar que el operando se
resuelva como una constante en el frontend. El segundo usa almacenamiento
`volatile`. En O0–O3 ambos deben finalizar con el código 129 correspondiente a
la trampa de división por cero de NativeSharp. No se compara con el resultado
en C, donde dividir por cero no tiene comportamiento definido.

El control agrupado `atomic/validar.cmd` pasó con el compilador de A:; registro
`A:/nsc-interop-build/tanda08-atomic-zero-validar.log`. No se construyó de
nuevo el compilador. No implica que la tanda 8 haya pasado su gate completo.
