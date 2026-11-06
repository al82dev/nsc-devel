# USR al cerrar dependencias de campos C

El recorrido de dependencias de campos compara identidades conocidas de records
y enums antes de seleccionar el tipo, incluso cuando el record ya estaba en la
salida. El desacuerdo genera error; con USR ausente se conserva la ruta previa.
La prueba de esta ruta y su validación agrupada siguen pendientes.
