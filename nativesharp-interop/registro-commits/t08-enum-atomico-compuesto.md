# Tanda 8: operaciones compuestas de enum C atómico

El análisis acepta operandos enteros en asignaciones compuestas sobre globales
atómicas con enum C importado. El backend realiza la operación en un ciclo
`cmpxchg seq_cst` sobre el tipo subyacente; conserva la identidad del enum y
aplica las conversiones de la operación antes de almacenar el resultado.

Las pruebas de `+=`, `-=` y `|=` pasaron de O0 a O3 para global y TLS, con
comprobación de lectura-modificación-escritura atómica en el IR de Clang y NSC.
Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-global-27704-24700` y
`A:\nsc-interop-build\tanda08-atomic-enum-tls-27714-24177`.
Quedan pendientes otras operaciones, anchos, valores y concurrencia, así como
incremento/decremento. Esta evidencia no cierra la tanda 8.
