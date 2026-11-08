# Sombra de función C por parámetro y local

Una biblioteca importa una función C `static inline` y publica dos plantillas
que usan su nombre como parámetro y como variable local, respectivamente. El
consumidor no importa el encabezado y comprueba los valores locales devueltos.
`sombra-funcion/validar.cmd` construyó productor y consumidor y los ejecutó
correctamente en O0. La evidencia es una muestra dirigida; no cierra el bloque
de sombras ni sustituye la validación completa de la tanda.
