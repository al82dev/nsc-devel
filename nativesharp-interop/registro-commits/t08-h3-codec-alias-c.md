# T08 H3: instantanea interna de alias C

Se prepara un codec interno para conservar el modulo, identidad de referencia, nombre, USR y tipo subyacente de un alias C. El lector comprueba la codificacion canonica y deja la salida intacta ante errores. Las ubicaciones son solo diagnosticas y no se transportan. No se modifica NSMETA ni se instala el alias en el consumidor.

Se agregaron controles de ida y vuelta con un tipo atomico anidado y rechazo de datos truncados. Compilacion y ejecucion quedan pendientes para la validacion agrupada con los codecs de tipos, funciones y globales. Siguen pendientes la captura con sus ambitos, la integracion en el formato publicado, la restauracion y las pruebas de extremo a extremo.
