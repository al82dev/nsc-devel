# T08 H3: instantanea interna de funcion C

Se prepara un codec interno para una funcion C que conserva modulo, identidad de referencia, simbolo de enlace, nombre, USR, convencion, enlace, firma y parametros. Los tipos usan el codec de dependencias; la lectura exige codificacion canonica y no modifica su salida si falla. No transporta ubicaciones de diagnostico ni modifica NSMETA.

Se agregaron controles de ida y vuelta con un tipo atomico anidado y un resultado `long double`, ademas del rechazo de datos truncados. Aun no se han compilado ni ejecutado: se agruparan con los demas cambios. Falta seleccionar referencias con su ambito, normalizar procedencia y nombres que puedan contener rutas, serializar globales y aliases, integrar versionado y restauracion, y validar bibliotecas reales. La instantanea no se debe publicar como formato definitivo hasta resolver esos puntos.
