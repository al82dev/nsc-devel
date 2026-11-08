# T08 H3: reproyección de globales C guardadas

La nueva proyección comprueba módulo, ordinal, destino, USR y símbolo de una
global C capturada. Usa la misma proyección de tipos que la importación viva y
conserva el modo volatile; para almacenamiento interno usa el símbolo guardado.
No instala almacenamiento ni resuelve todavía el origen TLS de una DLL.

Se preparó un control con una declaración Clang real: captura, codificación,
decodificación y comparación de símbolo, tipo y calificadores; también rechaza
un módulo incorrecto. La construcción incremental y la ejecución conjunta con
la reconstrucción de aliases terminaron con código 0. Solo se comprobó una
global ordinaria: TLS de DLL y otros tipos siguen sin validar en esta ruta.
No se habilita NSMETA ni se cierra la tanda 8.
