# Long double en biblioteca NativeSharp precompilada

Se prepara una prueba que publica un puntero a struct C con campo long double,
y lo usa desde un consumidor que importa el mismo encabezado. La matriz O0–O3
se agregó al fixture; todavía no fue ejecutada. La captura del formato IEEE aún
no se serializa y la prueba no supone que esa ruta ya funcione.
