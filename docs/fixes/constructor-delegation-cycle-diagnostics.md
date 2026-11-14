# Diagnósticos de ciclos de constructores

LKF-345: la validación marca todos los miembros de ciclos de delegación, pero antes emitía
un solo diagnóstico aunque hubiera ciclos independientes. Cada ciclo detectado ahora registra
un representante; una cadena que entra en un ciclo ya registrado no produce un duplicado.
La selección sigue el orden determinista de miembros y no altera la resolución de constructores.

La prueba dirigida añade un ciclo de dos constructores, una cadena que entra en él y un segundo
ciclo independiente. La prueba existente conserva el control de un único ciclo. El algoritmo
mantiene el recorrido anterior y no introduce recursión; el vector adicional usa O(n) espacio.

En la copia aislada, la regresión de ciclos falló con el código anterior y pasó con la corrección.
El blanco inicialmente conservaba cuatro fallos previos: los ejemplos devolvían un objeto nuevo
sin declarar el retorno propietario y no alcanzaban las comprobaciones que pretendían medir. Se
ajustaron esos ejemplos para llegar a sus comprobaciones originales y se conservó un control del
diagnóstico previo. Con ellos, el blanco pasó sus 74 comprobaciones en la copia aislada. No se
ejecutó una batería general.
