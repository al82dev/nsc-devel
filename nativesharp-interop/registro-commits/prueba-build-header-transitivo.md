# Prueba de recompilación por header transitivo

La prueba prepara un proyecto C/C++ con TLS y un enum declarado en un header
transitivo. Construye y ejecuta con valor cero, cambia únicamente el header
transitivo, vuelve a construir y espera el nuevo resultado. También verifica
que `--dependency-report` rechace esta configuración mientras no se rastree
el cierre de headers. La ejecución está pendiente; la prueba no acredita una
caché de artefactos C/C++.
