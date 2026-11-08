# T08 H3: instantanea interna de global C

Se agrega un codec interno para preservar el modulo, referencia, simbolo de enlace, USR, tipo, enlace, TLS, importacion DLL, alineacion declarada y modo volatile de una global C. El lector verifica el formato canonico y no modifica la salida en caso de error. No se cambia NSMETA ni se instala la global en el consumidor.

Se prepararon controles de ida y vuelta con TLS, alineacion, modo volatile ISO explicito y tipo atomico anidado, ademas del rechazo de datos truncados. Compilacion y ejecucion quedan pendientes para agruparlas. Faltan la seleccion con ambito, aliases, el formato publicado, la restauracion y la validacion de extremo a extremo.
