# Recarga de campos de bits C volatile

Para una actualización compuesta de un campo de bits volatile importado desde Clang, NSC conserva la primera lectura para calcular el resultado y vuelve a leer la unidad antes de insertar los bits y escribir. Esta secuencia coincide con el IR observado de Clang y evita reutilizar como bits vecinos una lectura anterior. El cambio se condiciona a la procedencia de Clang y no añade operaciones atómicas ni modifica campos nativos. Debe verificarse en conjunto con prefijo, postfijo, asignaciones y regresiones.
