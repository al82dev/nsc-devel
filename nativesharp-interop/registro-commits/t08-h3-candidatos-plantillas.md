# T08 H3: conservar candidatos de referencias en plantillas

Cuando hay sesiones C/C++, el recorrido de dependencias genericas conserva candidatos sintacticos de llamadas, identificadores, miembros y tipos, junto con namespace, importaciones y aliases visibles. Se capturan antes de que la plantilla se transforme; una restauracion de la cache limpia la lista para volver a descubrirla. Las compilaciones sin sesiones no habilitan esta captura.

Un candidato no es un enlace a C: puede ser una variable local, miembro, alias o llamada sobrecargada. Todavia falta resolver semanticamente esos casos, seleccionar y transportar las declaraciones reales y probar la ruta precompilada. Se preparo un control con funcion, global, typedef y una variable local que sombrea el mismo nombre. Construccion y ejecucion pendientes para agruparlas con los siguientes cambios; NSMETA y la tanda 8 no se dan por terminados.
