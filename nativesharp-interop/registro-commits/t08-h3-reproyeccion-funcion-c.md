# T08 H3: reproyección de funciones C guardadas

Una declaración C capturada y decodificada puede pasar por la misma proyección
de firmas que una observación viva de Clang. La operación verifica el módulo,
ordinal de proyecto, destino, USR y símbolo; construye los grupos de records
necesarios a partir del cierre suministrado, y conserva los fallos de proyección.
La operación aún no instala el símbolo ni enlaza una biblioteca.

La prueba captura una función de una sesión Clang real, codifica y decodifica
el snapshot y compara símbolo, identidad canónica y USR con la proyección viva;
también rechaza un módulo incorrecto. La construcción incremental y la prueba
de identidad terminaron con código 0. No se probó aquí la proyección de firmas
con records ni la ejecución de un consumidor precompilado.

La misma ejecución comprobó el despacho del lector de tipos para la envoltura
de dependencias preparada. No se habilitó la emisión de NSMETA 1.46 y el gate
completo de la tanda 8 permanece pendiente.
