# Typedef de función con parámetro enum C

Una firma C variádica que usa un enum no podía conciliarse entre dos productores
precompilados. El catálogo captura ahora el enum que aparece en el typedef y el
resolvedor compara sus definiciones completas, con sus valores, antes de aceptar
la firma. Cuando una plantilla referencia un enumerador, se selecciona el de su
productor; una referencia sin productor identificado conserva la ambigüedad.

Dos DLL homónimas y sus plantillas llaman por un typedef con parámetro `CKind`;
los controles C y NativeSharp pasaron en O0–O3 con los encabezados ocultos y al
restaurar desde caché. En O0 se rechazaron dos enums cuyo segundo enumerador
tiene valores distintos, así como una referencia ambigua desde el consumidor.
La regresión dirigida del record anidado pasó en O0. El control se añadió al
validador del bloque; la batería completa de la tanda sigue pendiente.

No se introdujo una versión nueva de NSMETA. La prueba cubre un parámetro enum
por valor; retornos enum, enums anónimos, otras convenciones y otras variantes
no se consideran acreditados por esta muestra.
