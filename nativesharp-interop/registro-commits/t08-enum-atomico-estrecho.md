# Tanda 8: enums C atómicos de 8 y 16 bits

Se añadieron enums con base explícita `signed char` y `unsigned short`, usando
la extensión admitida por Clang en C11. La referencia C reveló que Clang amplía
sin signo el portador atómico al calcular una operación compuesta más ancha,
incluso si el enum tiene base con signo. Se corrige esa promoción en NSC.

Se compararon valores intermedios para división, suma, desplazamiento y
multiplicación con enum estrecho. Las variantes global y TLS pasaron O0–O3,
junto con la regresión del fixture. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-enum-global-30235-31643` y
`A:\nsc-interop-build\tanda08-atomic-enum-tls-30238-9624`.
La prueba no cubre todas las operaciones ni todas las combinaciones de tipos.
