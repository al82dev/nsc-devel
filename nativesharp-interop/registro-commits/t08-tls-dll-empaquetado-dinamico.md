# Tanda 8: empaquetado de DLL TLS resueltas dinámicamente

El caso de DLL de nombres diferentes pasaba cuando el ejecutable se generaba junto a las bibliotecas, pero al publicarlo en una carpeta aislada faltaba `api2.dll`. El driver seguía únicamente las DLL presentes en la tabla de importaciones del PE, que no enumera las cargas dinámicas.

El driver identifica las DLL TLS que el generador resolverá dinámicamente por nombre y huella, verifica que estén entre las dependencias declaradas y las incluye en el cierre de runtimes. La validación del enlace reconoce esas DLL como puntos de entrada al recorrer sus dependencias transitivas. No se modifica NSLink ni el formato de metadata.

`validar-empaquetado.cmd` terminó con código 0: generó el ejecutable en una carpeta aislada, comprobó que se publicaron las dos DLL y lo ejecutó desde esa carpeta y desde `A:\`. La prueba incluye el control C y el aislamiento entre dos hilos del consumidor. El recorrido de una dependencia transitiva propia de una DLL cargada dinámicamente todavía requiere una prueba dedicada. Las DLL de igual nombre de archivo siguen pendientes.
