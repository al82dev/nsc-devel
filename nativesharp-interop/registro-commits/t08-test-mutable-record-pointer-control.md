# Control positivo de función C mutable

La misma función que rechaza un puntero a const debe aceptar un puntero mutable
y producir la modificación observada. El rechazo negativo se comprueba con el
diagnóstico específico de esa función, no con una coincidencia genérica.
Pendiente de repetición del fixture con el compilador ya construido.
