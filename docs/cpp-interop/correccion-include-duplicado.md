# Corrección de include duplicado

Se elimina una inclusión duplicada de `<cstdint>` del encabezado de búsqueda de
nombres. El cambio no modifica el comportamiento ni las dependencias declaradas.

La verificación es la inspección del diff; no requiere reconstruir el proyecto.
