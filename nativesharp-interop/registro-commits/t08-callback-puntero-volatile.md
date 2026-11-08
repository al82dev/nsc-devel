# Calificadores en callbacks C con punteros a records

Dos typedefs equivalentes que aceptan `volatile CRecord*` eran aceptados por
Clang, pero NSC rechazaba su reconstrucción al consumir plantillas de dos
bibliotecas precompiladas. La reconstrucción ahora resuelve el record y
conserva `volatile` y `const` en el tipo del dato apuntado.

`typedef-calificadores/validar.py` contrastó seis pares con Clang y NSC en O0
y O3, sin encabezados originales: `volatile` igual o faltante, `const volatile`
igual o con `volatile` faltante y calificadores superiores del puntero que C
ignora en parámetros. Los casos compatibles compilaron y se ejecutaron; los
incompatibles se rechazaron. También pasó la regresión del puntero `const` en O0.
La construcción fue incremental.

La muestra comprueba la declaración de callbacks y su conciliación; no invoca
todavía los punteros ni demuestra otras combinaciones de calificadores.
