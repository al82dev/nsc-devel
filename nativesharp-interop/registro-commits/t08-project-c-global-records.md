# Proyección inicial de globales C de struct

El inventario proyecta structs C completos, sus punteros y arreglos fijos solo
cuando el tipo seleccionado coincide con el proyecto, tamaño y alineación de
Clang. Los records calificados permanecen rechazados y el modo C de la sesión
es obligatorio. Esto prepara la ruta semántica; no demuestra aún acceso,
asignación por valor, duración de objetos ni compatibilidad con C++.
