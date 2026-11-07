# T08: operaciones compuestas de TLS C atómico estrecho

Para un global C TLS atómico de 8 o 16 bits y operando derecho `int`, el
análisis conserva el operando sin estrecharlo. El descenso promociona el valor
observado a `int`, calcula el resultado y lo convierte al ancho de almacenamiento
antes de un `cmpxchg` secuencialmente consistente; si otro hilo cambia el
objeto, reintenta con el valor observado. Se conserva `volatile`. Otros tipos
de operando aún no quedan acreditados. La regla especial se limita al global C
atómico importado: un control local NativeSharp conserva su rechazo anterior.

La construcción incremental de `nsc` y `atomic/validar.cmd` pasaron. Los
controles comparan C con NativeSharp O0–O3 para división y residuo con divisores
mayores que el ancho, y suma, multiplicación, desplazamientos, XOR y residuo.
Registros en `A:/nsc-interop-build/tanda08-atomic-narrow-compound-build.log` y
`A:/nsc-interop-build/tanda08-atomic-narrow-compound-boundary.log`. No se ha
validado toda la matriz de operandos ni el gate completo de T08.
