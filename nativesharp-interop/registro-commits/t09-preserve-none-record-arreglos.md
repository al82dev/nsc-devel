# T09: records C++ con arreglos y retorno preserve_none

## Evidencia

Se probaron dos records triviales que contienen `int[3]` o `int[2][3]` y un `double`. Clang y NativeSharp llaman una función C++ `preserve_none` que recibe el record por valor y devuelve una copia modificada; el control también llama por callback y comprueba que el original no cambie. No se modificó el compilador.

- Arreglo unidimensional: ocho comparaciones estática/DLL O0–O3, `A:/temp/nsc-t09-preserve-cpp-retorno-record-arreglo-5irluty0`; 16 consumidores precompilados con y sin encabezado, `A:/temp/nsc-t09-preserve-cpp-retorno-record-arreglo-precompilado-zscuc7cz`.
- Arreglo bidimensional: el script reproducible `validar-multidimensional.py` pasó ambos validadores, `A:/temp/nsc-t09-preserve-arreglo2d-r8x7iup7`. Un primer fixture temporal declaró accidentalmente un arreglo tridimensional y Clang lo rechazó; se corrigió antes de la prueba válida.

- Sin plan conciliado, el callback del caso unidimensional se rechaza: A:/temp/nsc-t09-preserve-none-retorno-record-arreglo-sin-plan-ecghx3i9.

## Límites

Son dos layouts concretos en Windows x64. El resultado no acredita todos los arreglos, tipos físicos ni convenciones; T09-003 y T11 siguen abiertos.
