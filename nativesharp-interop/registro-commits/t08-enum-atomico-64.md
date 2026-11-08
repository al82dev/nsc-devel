# Tanda 8: enum C atómico con base de 64 bits

Se amplía el fixture con un enum de base explícita `unsigned long long`, usando
la extensión de Clang aceptada en C11. Una prueba inicial sin base explícita
no era de 64 bits en este objetivo; el IR de Clang mostró un `i32`, así que
no se considera evidencia de este caso.

La carga, suma y desplazamiento de la variante explícita pasaron de O0 a O3
con global y TLS. El verificador comprueba almacenamiento `i64`, carga y
modificación atómicas secuencialmente consistentes alineadas a ocho en ambos
IR. Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-global-29693-16880`
y `A:\nsc-interop-build\tanda08-atomic-enum-tls-29696-27628`. No se afirman
otras operaciones ni todos los tipos base de 64 bits.
