# T08-003: preparar transporte de objetos nativos en el sidecar

El modelo de sidecar incorpora una lista de objetos nativos como bytes. El
codec escribe formato 3 cuando la lista no está vacía y conserva el formato 2
anterior si está vacía; lee ambos formatos, verifica la huella existente y
rechaza conteos, longitudes u objetos vacíos inválidos.

Este paso prepara el formato; aún no conecta los objetos del productor ni los
restaura al consumidor. Se mantiene el fallback de biblioteca nativa hasta
implementar y validar ambos extremos, incluyendo referencias a símbolos.
