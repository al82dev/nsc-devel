# Ruta de recursos de Clang en la prueba de sesión

La expectativa de la prueba usa la representación nativa UTF-8 que produce
`std::filesystem` al unir el directorio de recursos con `include`. Esto evita
suponer un separador fijo en Windows y mantiene la comprobación del orden y de
los demás argumentos.

La batería falló con la expectativa anterior; se volverá a ejecutar después de
reconstruir incrementalmente el ejecutable de pruebas.
