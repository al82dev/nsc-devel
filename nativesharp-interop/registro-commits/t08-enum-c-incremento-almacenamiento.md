# Tanda 8: incremento de enum C en distintos almacenamientos

Se contrastan incremento sufijo de un enum C local y de un elemento de arreglo,
y el incremento prefijo en un campo; C comprueba que
el arreglo y el campo compartidos conservan el resultado esperado. La muestra
añadida utiliza un enum con signo de 32 bits y comprueba incremento prefijo en
campo, incremento sufijo en arreglo y local.

El fixture pasó O0–O3 en ambos modos existentes. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-enum-global-1193-7575` y
`A:\nsc-interop-build\tanda08-atomic-enum-tls-1232-5484`. Las regresiones de
hilos también pasaron. Esto no acredita otros tamaños o almacenamientos.
