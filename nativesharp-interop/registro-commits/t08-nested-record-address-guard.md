# T08-010 / T08-012: dirección de record C anidado

Para un campo record importado, el análisis consulta su alineación natural y
la compara con la alineación efectiva del campo. La restricción de dirección
se propaga a campos interiores cuando el receptor es un valor no direccionable;
un receptor puntero conserva la semántica de su propio apuntado. Esto prepara
la proyección de records subalineados sin habilitarla aún. La prueba funcional
y la validación agrupada quedan pendientes.
