# T08: corrección del fixture de la suite ThinLTO

La suite recogía con `**/*.ns` archivos creados por sus propios subproyectos;
al volver a usar el proyecto raíz aparecían tipos duplicados. Ahora el proyecto
raíz incluye únicamente su `Program.ns`, conservando los subproyectos aislados.

La comprobación de varios summaries buscaba la cadena `thinlto-summaries: 1`,
que también coincide con 10. Se compara el valor completo del campo: debe ser
distinto de cero y uno. Una sonda aislada mostró 10 unidades para el programa
actual. Falta reconstruir el objetivo de pruebas y repetir la suite agrupada.
