# Tanda 05: errores de apertura C/C++ en el controlador

El fallo de apertura de una sesión incorpora todos los diagnósticos de Clang al
motor de NSC en lugar de añadir únicamente el primer mensaje. Las tres rutas de
generación presentan los diagnósticos cuando falla la preparación del proyecto.
El caso sin diagnósticos conserva el error existente. La validación se agrupa al
finalizar la tanda.
