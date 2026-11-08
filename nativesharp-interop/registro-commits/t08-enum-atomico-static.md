# Tanda 8: enum C atómico de enlace interno

Se amplía el fixture de globales C estáticas con `_Atomic(enum StaticMood)`.
NativeSharp lee y escribe la variable, y una función `static inline` C comprueba
el mismo estado. El control C y el consumidor pasaron O0–O3.

El verificador de IR exige ahora cargas y escrituras atómicas secuencialmente
consistentes para dos símbolos internos distintos, en lugar de aceptar una
sola variable como evidencia de ambas. La ejecución está en
`A:\nsc-interop-build\tanda08-global-static-tipos-31326-17131`. No se ha
demostrado aquí la variante precompilada ni todas las operaciones o anchos.
