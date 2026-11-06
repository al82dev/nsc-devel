# Comparación de enum C con enumerador entero

Se conserva como caso independiente la comparación `fromHeader != CGlobalChoice`.
En C el enumerador es entero y el valor enum participa en las conversiones
aritméticas; NSC la rechazó como tipos incompatibles en O0. Con la corrección,
la matriz de metadata C pasó de O0 a O3, incluyendo esta comparación y los
casos dirigidos de signo y anchura. Se usan las reglas numéricas de NativeSharp;
esta prueba no acredita todas las conversiones aritméticas de C.
