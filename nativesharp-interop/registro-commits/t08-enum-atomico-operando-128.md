# Tanda 8: operandos de 128 bits en enum atómico

Se compara con Clang la división de un enum atómico por operandos `i128` y
`u128` de valor 2^80. La primera ejecución no enlazó porque faltaban
`__divti3` y `__udivti3`. Ambas funciones existen en la biblioteca estática
`clang_rt.builtins-x86_64.lib` de A:, que los proyectos del fixture enlazan
ahora explícitamente. Esto no demuestra que NSC la enlace automáticamente.

Global y TLS pasaron O0–O3; también pasaron las regresiones de aislamiento y
actualización cruzada entre hilos. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-enum-global-693-1469`,
`A:\nsc-interop-build\tanda08-atomic-enum-tls-693-1469`,
`A:\nsc-interop-build\tanda08-atomic-enum-hilos-693-1469` y
`A:\nsc-interop-build\tanda08-atomic-enum-contencion-700-22965`. No se
acreditan todas las operaciones con 128 bits ni se cierra la tanda.
