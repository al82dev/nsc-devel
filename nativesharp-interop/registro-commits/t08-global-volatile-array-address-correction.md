# Dirección de elemento de arreglo C volatile

NSC aceptaba `&volatile_array[1]` como `int*`, perdiendo el calificador. El análisis de
índices ahora identifica el estado global C/C++ resuelto detrás de un arreglo fijo de
escalares o enum y rechaza tomar su dirección mientras no existan punteros calificados.
El recorrido usa el binding resuelto para respetar el sombreado y no proyecta el
calificador de una variable puntero hacia su apuntado. Los accesos normales siguen
permitidos. La prueba y la validación agrupada documentarán los resultados; esto no
afirma soporte general de punteros `volatile`.
