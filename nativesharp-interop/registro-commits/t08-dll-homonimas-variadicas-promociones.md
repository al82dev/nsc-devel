# Funciones variádicas: promociones de argumentos

Se amplió la muestra de las dos DLL TLS homónimas con una segunda función C
variádica. Cada DLL lee, mediante `va_arg`, argumentos de tipo `int`, `double`,
`int`, `double` y `double` y devuelve su propio estado TLS si son los esperados.
Los controles compilados con Clang y las plantillas NativeSharp pasan valores
equivalentes como `byte`, `float`, `short`, `double` y `float`.

Pasaron los controles C y el consumidor NativeSharp en O0, O1, O2 y O3, con
restauración del consumidor y las DLL desde caché. La muestra incluye argumentos
en registros y en pila del ABI Windows x64. No se modificó el compilador.

Esto no verifica todas las promociones ni argumentos agregados o punteros; la
casilla general continúa pendiente, así como la dirección de función y delegados.
