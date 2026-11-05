# Tanda 06: control de fallo de inicio para la prueba

La interfaz interna de la acción permite inyectar una acción Clang no poseída
durante la prueba. En uso normal sigue creando la acción de sintaxis habitual.
Esto permite forzar un fallo real de inicio y observar si se llama al cierre,
sin cambiar el código de Clang. Falta la validación agrupada de la tanda.
