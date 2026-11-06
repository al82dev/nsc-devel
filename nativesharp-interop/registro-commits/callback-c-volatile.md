# Callback C volatile almacenado

Se prepara un fixture de lectura y escritura de punteros a función volatile en
un global y un campo C, con ejecución cruzada, y emisión de IR de Clang y NSC
para comparar semántica. Está pendiente de ejecución; producir IR no basta para
acreditar el orden de memoria: debe inspeccionarse y añadirse una comprobación.
La primera ejecución rechazó tanto el campo como el global por tipo no
proyectado. Clang emitió `load atomic volatile ptr` con acquire y alineación 8
para las lecturas observadas. Se añadieron funciones C de escritura para
contrastar también el orden de store. Tras las correcciones, la comparación
automática comprobó acquire/release alineados y volatile ordinario empaquetado
en ambos IR; el fixture ejecutó O0–O3. Otras formas siguen pendientes.
