# Modo de lenguaje del inventario de globales

El inventario conserva si cada global procede de una sesión C. Esto prepara la
proyección de almacenamiento de structs bajo el modelo C sin habilitarla para
clases C++ por coincidencia de nombre o layout. No cambia aún qué globales son
utilizables y requiere validación agrupada junto con la proyección.
