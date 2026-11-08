# Tanda 8: operaciones de enum atómico con signo

Se amplía el fixture con un enum C que comienza en -8. NativeSharp aplica
`/=`, `>>=`, `+=`, `*=`, `%=`, `^=` y `&=` y compara cada resultado intermedio
con una referencia C atómica, tanto para global normal como TLS. Ambas variantes
pasaron O0–O3, con la comprobación de IR existente. La regresión de aislamiento
TLS con dos hilos también pasó.

Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-global-28576-10941`,
`A:\nsc-interop-build\tanda08-atomic-enum-tls-28576-10941` y
`A:\nsc-interop-build\tanda08-atomic-enum-hilos-28605-9372`. Esta secuencia
no cubre todas las conversiones, anchos ni operandos posibles; no cierra la
tanda 8.
