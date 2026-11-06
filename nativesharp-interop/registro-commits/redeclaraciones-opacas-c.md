# Redeclaraciones de tipos C opacos

El encabezado de la prueba incluye una declaración adelantada adicional para
el struct y la union, sin cambiar sus definiciones privadas ni su API. Es C
válido y debe conservar la identidad entre el productor precompilado y el
consumidor que incluye el mismo encabezado. Falta validar y diagnosticar el
resultado; no acredita el caso todavía.

La matriz existente pasó con el compilador `ab06802a4b42fffe`: compilación,
enlace y ejecución directa, mediante productor precompilado y con header
compartido en O0–O3. Se mantuvieron los rechazos de valor incompleto, otro
origen y header modificado. No fue necesario modificar producción para este
caso; no demuestra todas las combinaciones de redeclaraciones.
