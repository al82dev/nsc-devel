# T08-004: codigo de salida del control de matriz

El primer intento que llego a NSC mostro los ocho nombres rechazados, pero el
script devolvio cero al informar el fallo dentro del bloque `for`. Se cambia
el camino de error a una etiqueta que devuelve uno; no afecta al compilador.
