# Callback C que retorna puntero a record

El fixture prepara `CallbackPayload* (*)(void)` para tres fuentes: un callback
retornado por una función C, uno almacenado en campo C y otro en global C.
NativeSharp los invoca y consulta el valor del record. El caso está preparado
en la matriz de callbacks O0–O3; aún no se ha construido ni ejecutado.
