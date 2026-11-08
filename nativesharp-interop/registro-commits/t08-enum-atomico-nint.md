# Tanda 8: operandos de ancho de puntero en enum atómico

La selección del tipo de cálculo para operaciones compuestas sobre enum C
atómico ignoraba `nint` y `nuint`; un divisor `nuint` de 64 bits se truncaba
a 32 bits. Se incluyen estos tipos en el cálculo de 64 bits y se preservan los
tipos de 128 bits cuando aparecen como operandos.

Divisiones con `nint` y `nuint` se contrastaron con la referencia C en O0–O3,
para global y TLS. Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-global-233-26039`
y `A:\nsc-interop-build\tanda08-atomic-enum-tls-233-26039`. Las operaciones
con operandos de 128 bits aún no fueron validadas.
