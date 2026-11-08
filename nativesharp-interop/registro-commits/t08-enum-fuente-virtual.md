# Identidad de enum observado desde una fuente virtual

Una comparación de dos copias de la misma declaración C fallaba cuando Clang
había leído su encabezado desde un buffer en memoria: se intentaba verificar
la procedencia exclusivamente mediante un archivo físico.

La sesión conserva una copia inmutable compartida de la fuente virtual para
los enums declarados en ella. La comparación solo acepta esa evidencia si las
dos observaciones comparten el mismo buffer, la ruta y posición coinciden y la
huella del contenido verifica. La evidencia es transitoria y no se serializa en
NSMETA. Las declaraciones de sesiones o fuentes distintas no se equiparan por
el nombre del enum ni por este mecanismo.

La compilación incremental terminó correctamente. La prueba de sesión dejó de
reportar el fallo de comparación del enum virtual; permanecen dos fallos de
lectura del catálogo. El control dirigido con dos DLL y enums precompilados pasó
en O0, así como el rechazo de valores incompatibles. La tanda 8 sigue abierta.
