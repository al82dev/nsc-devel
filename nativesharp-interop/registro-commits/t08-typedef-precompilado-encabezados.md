# Typedef C con biblioteca precompilada y encabezados distintos

El productor publica un método que usa `Alias*`, donde `Alias` identifica un
record C. El consumidor usa un encabezado distinto con el mismo record y alias.
La prueba dirigida `typedefs/precompiled-conflict/validar.cmd` construye el
productor en A: y verifica los consumidores.

- Reutilizar el mismo encabezado pasa el análisis semántico.
- Un segundo encabezado con definición equivalente se rechaza como
  `conflicting C/C++ type alias`: es el fallo pendiente de corregir.
- Un segundo encabezado con campo incompatible también se rechaza, como debe.
- La comprobación usa el compilador de desarrollo y Clang en A:, en O0. No
  acredita ejecución, otras optimizaciones ni variantes con enum.

El primer intento de flexibilizar solo la comparación del USR del alias no
resolvió el caso equivalente y se retiró. El ejecutable de desarrollo se volvió
a construir con el código restaurado; no queda ese cambio en producción. La
prueba deliberadamente devuelve error hasta que el control equivalente funcione.
