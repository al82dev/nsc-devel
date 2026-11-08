# Ámbito de inicializadores que sombrean una función C

El fixture existente comprueba referencias a la función C en inicializadores
de una variable local, un `for` y una desestructuración que luego introducen el
mismo nombre. `validar-o0-inicializadores.cmd` construyó los dos productores y
consumidores y ejecutó ambas muestras correctamente en O0. Es un control
dirigido; no sustituye la validación completa del bloque ni cubre toda sintaxis.
