# Tanda 8: conversión mixta de enum C atómico

Al combinar un enum atómico negativo de 32 bits con un operando de 64 bits,
NSC discrepaba de Clang en división. El IR de Clang para el objetivo fijado
extiende sin signo el portador de 32 bits antes de la operación de 64 bits,
tanto con divisor con signo como sin signo. Se ajusta esa ruta específica.

El fixture compara frente a C división, módulo, suma y desplazamiento con
operandos de 64 bits, además de las pruebas previas. Pasó en O0–O3 tanto para
global como TLS. Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-global-29180-548`
y `A:\nsc-interop-build\tanda08-atomic-enum-tls-29184-11296`.
Esto no verifica todas las anchuras ni todas las conversiones posibles.
