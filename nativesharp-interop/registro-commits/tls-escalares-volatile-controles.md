# Controles de IR y escritura const volatile

La comprobación exige cargas y escrituras `volatile` por cada escalar TLS
mutable y una carga para `const volatile`; además exige el diagnóstico de
asignación prohibida a este último. Los controles esperan validación agrupada.
