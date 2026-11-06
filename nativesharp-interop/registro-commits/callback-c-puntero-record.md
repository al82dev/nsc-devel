# Callback C con puntero a record

El fixture prepara una firma C `int (*)(CallbackPayload*)`, retornada por una
función y almacenada en un campo y un global. NativeSharp crea el record y
llama el callback desde las tres rutas; la implementación C lee su campo.
Esta primera preparación no demuestra soporte. Se investigará el diagnóstico
antes de cambiar producción y se validará la solución junto a la tanda.
