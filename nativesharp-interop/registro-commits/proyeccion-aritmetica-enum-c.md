# Aritmética sobre enum C importado

La prueba real rechazó sumas sobre enums C signed y unsigned. Se prepara usar
la representación subyacente capturada por Clang al resolver el tipo numérico,
sin aplicar esa regla a enums NativeSharp. El lowering y el resultado frente a
la operación C todavía requieren validación agrupada.

Con `5bcb2fb4f02458e7`, suma signed y suma, división y residuo unsigned
coincidieron con las funciones C del fixture en O0–O3. Los operadores de
desplazamiento, bitwise y unarios también pasaron el caso de 8 bits. La suma
de enum NativeSharp continuó rechazada y su AND/OR del mismo tipo funcionó.
No se ejecutó la batería completa de la tanda ni todas las combinaciones de C.
