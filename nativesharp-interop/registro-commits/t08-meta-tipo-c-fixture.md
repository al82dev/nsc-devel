# Fixture de tipo C en superficie pública

Un productor publica un struct NativeSharp con puntero a un struct C importado.
Un consumidor usa solo LibraryReference e intenta acceder al campo C. El script
construye C, productor y consumidor y ejecuta el consumidor en O0; no reconstruye
el compilador. Está preparado, sin ejecutar. La función de inspección se compila,
pero Main no la invoca: aún no es prueba de lectura en ejecución.
