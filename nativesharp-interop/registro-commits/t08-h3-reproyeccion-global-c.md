# T08 H3: reproyección de globales C guardadas

La nueva proyección comprueba módulo, ordinal, destino, USR y símbolo de una
global C capturada. Usa la misma proyección de tipos que la importación viva y
conserva el modo volatile; para almacenamiento interno usa el símbolo guardado.
No instala almacenamiento ni resuelve todavía el origen TLS de una DLL.

Se preparó un control con una declaración Clang real: captura, codificación,
decodificación y comparación de símbolo, tipo y calificadores; también rechaza
un módulo incorrecto. Construcción y ejecución quedan pendientes para agruparse
con la restauración de aliases. No se habilita NSMETA ni se cierra la tanda 8.
