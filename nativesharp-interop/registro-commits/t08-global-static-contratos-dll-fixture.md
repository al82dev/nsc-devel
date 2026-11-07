# T08-004: referencia DLL con globales internas TLS

Se prepara una DLL independiente y un proyecto que asocia el encabezado C a esa
DLL. Las globales static del encabezado deben pertenecer al modulo generado,
incluidas las TLS. El control adicional aun no se ha ejecutado.
