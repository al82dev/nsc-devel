# Conservar el enum vivo conciliado

El guard del alias no veía el enum vivo: la observación había seleccionado el
tipo precompilado y la selección final solo agregaba enums vivos referenciados
directamente o por campos. Se prepara agregar el candidato vivo si existe una
conciliación verificada, manteniendo los rechazos por errores y colisiones.
La construcción y validación agrupadas quedan pendientes.
