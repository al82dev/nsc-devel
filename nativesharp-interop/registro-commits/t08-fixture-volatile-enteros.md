# Casos compartidos de enteros volatile

El fixture de records declara globales C enteros con y sin signo de 8, 16 y
64 bits. NativeSharp los escribe y lee y una función C comprueba los mismos
valores. Estos casos se ejecutarán junto con las optimizaciones O0–O3 de la
validación agrupada; aún no hay resultado.
