# Tanda 05: sesión con snapshot semántico real

La prueba de publicación y carga de un snapshot semántico ahora comprueba que
la sesión real de Clang, poseída por la misma ProjectCompilation, sigue utilizable
después de instalar el snapshot. Falta ejecutar el escenario agrupado. No equivale
a habilitar sesiones C++ en el servicio de lenguaje.
